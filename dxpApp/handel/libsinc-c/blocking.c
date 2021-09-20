/********************************************************************
 ***                                                              ***
 ***                   libsinc api implementation                 ***
 ***                                                              ***
 ********************************************************************/

/*
 * This module provides the caller API for libsinc.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#endif

#include "sinc.h"
#include "sinc_internal.h"


/*
 * NAME:        SincWaitForData
 * ACTION:      Waits until some data is available and notifies the
 *              caller. Can be used to poll for data if timeout=0.
 * PARAMETERS:  Sinc *sc        - the sinc connection.
 *              int timeout     - in milliseconds. 0 to poll. -1 to wait forever.
 *              bool *readOk[2] - readOk[0] indicates that stream data was available.
 *                                readOk[1] indicates that datagram data was available.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

static bool SincWaitForData(Sinc *sc, int timeout, bool *readOk)
{
    int errCode;
    int fds[2];
    bool readAvailable[2];

    if (sc->inSocketWait)
    {
        SincReadErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__MULTIPLE_THREAD_WAIT);
        return false;
    }

    sc->inSocketWait = true;
    if (sc->datagramFd >= 0)
    {
        // We're interested in either stream data or datagrams.
        fds[0] = sc->fd;
        fds[1] = sc->datagramFd;

        errCode = SincSocketWaitMulti(fds, 2, timeout, readAvailable);
        readOk[0] = readAvailable[0];
        readOk[1] = readAvailable[1];
    }
    else
    {
        // Datagrams aren't enabled - just read the stream.
        errCode = SincSocketWait(sc->fd, timeout, &readAvailable[0]);
        readOk[0] = readAvailable[0];
    }

    sc->inSocketWait = false;

    if (errCode != SI_TORO__SINC__ERROR_CODE__NO_ERROR)
    {
        SincReadErrorSetCode(sc, (SiToro__Sinc__ErrorCode)errCode);
        return false;
    }

    return true;
}


/*
 * NAME:        SincReadMessage
 * ACTION:      Reads the next message. This may block waiting for a message to be received.
 *              The resulting packet can be handed directly to the appropriate SincDecodeXXX()
 *              function.
 *              Use this function to read the next message from the input stream. If you want to
 *              read from a buffer use SincGetNextPacketFromBuffer() instead.
 * PARAMETERS:  Sinc *sc                           - the sinc connection.
 *              int timeout                        - in milliseconds. 0 to poll. -1 to wait forever.
 *              SincBuffer *buf                    - where to put the message.
 *              SiToro__Sinc__MessageType *msgType - which type of message we got.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadMessage(Sinc *sc, int timeout, SincBuffer *buf, SiToro__Sinc__MessageType *msgType)
{
    // Try to get a message from the read buffer.
    int packetFound = false;
    SincGetNextPacketFromBuffer(&sc->readBuf, msgType, buf, &packetFound);
    if (packetFound)
        return true;

    // Check that we're connected to something.
    if (!sc->connected)
    {
        SincReadErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__NOT_CONNECTED);
        return false;
    }

    // We'll have to read some more data.
    while (true)
    {
        // Read any data which is currently available.
        int readSomeData = false;
        bool readAvailable[2];
        readAvailable[0] = false;
        readAvailable[1] = false;

        do
        {
            int errCode;
            int readBufAvailable;

            // Check for data being available.
            if (!SincWaitForData(sc, 0, readAvailable))
                return false;

            // Is there stream data available?
            if (readAvailable[0])
            {
                int readBufBytesAvailable;
                int bytesRead = 0;

                if (sc->readBuf.cbuf.len == sc->readBuf.cbuf.alloced)
                {
                    // Out of buffer space - read and expand the buffer.
                    uint8_t tempReadBuf[65536];
                    errCode = SincSocketRead(sc->fd, tempReadBuf, sizeof(tempReadBuf), &bytesRead);
                    if (errCode == 0 && bytesRead > 0)
                    {
                        // Append to the buffer.
                        sc->readBuf.cbuf.base.append(&sc->readBuf.cbuf.base, (size_t)bytesRead, tempReadBuf);
                    }
                }
                else
                {
                    // Read some data into our existing buffer.
                    readBufBytesAvailable = (int)(sc->readBuf.cbuf.alloced - sc->readBuf.cbuf.len);
                    errCode = SincSocketRead(sc->fd, &sc->readBuf.cbuf.data[sc->readBuf.cbuf.len], readBufBytesAvailable, &bytesRead);
                    if (errCode == 0 && bytesRead > 0)
                    {
#ifdef PROTOCOL_VERBOSE_DEBUG
                        printf("SincReadMessage() %d bytes from fd %d\n", bytesRead, sc->fd);
                        printf("read data: ");

                        int i;
                        for (i = 0; i < bytesRead; i++)
                            printf("%02x ", sc->readBuf.buf.data[sc->readBuf.buf.len + i]);

                        printf("\n");
#endif

                        sc->readBuf.cbuf.len += (size_t)bytesRead;
                    }
                }

                if (errCode != 0)
                {
                    SincReadErrorSetCode(sc, (SiToro__Sinc__ErrorCode)errCode);
                    return false;
                }

                if (bytesRead < 0)
                {
                    SincReadErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__READ_FAILED);
                    return false;
                }
                else if (bytesRead == 0)
                {
                    SincReadErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__SOCKET_CLOSED_UNEXPECTEDLY);
                    return false;
                }

                readSomeData = true;
            }

            // Is there datagram data available?
            if (readAvailable[1])
            {
                uint8_t *bufPos;
                size_t bytesRead;

                // Make sure we have at least 64k available in the buffer for reading (datagrams can't be bigger than this).
                if (sc->readBuf.cbuf.len + SINC_MAX_DATAGRAM_BYTES + SINC_HEADER_LENGTH > sc->readBuf.cbuf.alloced)
                {
                    // Expand the buffer.
                    size_t newSize = sc->readBuf.cbuf.len + SINC_MAX_DATAGRAM_BYTES + SINC_HEADER_LENGTH;
                    uint8_t *mem = realloc(sc->readBuf.cbuf.data, newSize);
                    if (!mem)
                    {
                        SincReadErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                        return false;
                    }

                    sc->readBuf.cbuf.data = mem;
                    sc->readBuf.cbuf.alloced = newSize;
                }

                // Read the datagram.
                readBufAvailable = (int)(sc->readBuf.cbuf.alloced - sc->readBuf.cbuf.len);
                bufPos = &sc->readBuf.cbuf.data[sc->readBuf.cbuf.len];
                bytesRead = (size_t)readBufAvailable - SINC_HEADER_LENGTH;
                errCode = SincSocketReadDatagram(sc->datagramFd, bufPos + SINC_HEADER_LENGTH, &bytesRead, true);
                if (errCode != SI_TORO__SINC__ERROR_CODE__NO_ERROR)
                {
                    SincReadErrorSetCode(sc, (SiToro__Sinc__ErrorCode)errCode);
                    return false;
                }

                if (bytesRead > 0)
                {
                    // Make a fake SINC header since we don't get them with datagrams.
                    uint8_t fakeMsgType = SI_TORO__SINC__MESSAGE_TYPE__HISTOGRAM_DATAGRAM_RESPONSE;
                    if (bytesRead >= 4)
                        fakeMsgType = bufPos[SINC_HEADER_LENGTH + 6];

                    SincProtocolEncodeHeaderGeneric(bufPos, (int)bytesRead, fakeMsgType, SINC_RESPONSE_MARKER);

                    sc->readBuf.cbuf.len += bytesRead + SINC_HEADER_LENGTH;
                    readSomeData = true;
                }
            }

        } while (readAvailable[0] || readAvailable[1]);

        // Try to get a message from the read buffer.
        if (readSomeData)
        {
#ifdef PROTOCOL_VERBOSE_DEBUG
            printf("read some data: %ld bytes in buffer\n", sc->readBuf.buf.len);
#endif
            SincGetNextPacketFromBuffer(&sc->readBuf, msgType, buf, &packetFound);
            if (packetFound)
                return true;
        }

        // Wait for more data?
        if (!SincWaitForData(sc, timeout, readAvailable))
            return false;

        if (timeout == 0)
        {
            SincReadErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__TIMEOUT);
            break;
        }
    }

    return false;
}


/*
 * NAME:        SincWaitForMessageType
 * ACTION:      Waits for a specific protobuf message type from the device.
 * PARAMETERS:  Sinc *sc                           - the sinc connection.
 *              int timeout                        - in milliseconds. 0 to poll. -1 to wait forever.
 *              SincBuffer *buf                    - where to put the message.
 *              _SiToro__Sinc__MessageType msgType - the type of response to wait for.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincWaitForMessageType(Sinc *sc, int timeout, SincBuffer *buf, SiToro__Sinc__MessageType seekMsgType)
{
    while (true)
    {
        // Read a message at a time.
        SiToro__Sinc__MessageType msgType;
        if (!SincReadMessage(sc, timeout, buf, &msgType))
        {
            // Discard any message.
            return false;
        }

        if (msgType == seekMsgType)
        {
            // This is the one we were looking for.
            return true;
        }
        else if (msgType == SI_TORO__SINC__MESSAGE_TYPE__ASYNCHRONOUS_ERROR_RESPONSE)
        {
            // It's an async error instead. Treat it as an error response.
            if (!SincDecodeAsynchronousErrorResponse(&sc->readErr, buf, NULL, NULL))
            {
                sc->err = &sc->readErr;
                return false;
            }
        }
        else if (msgType == SI_TORO__SINC__MESSAGE_TYPE__SUCCESS_RESPONSE)
        {
            // It might be a failure response.
            if (!SincDecodeSuccessResponse(&sc->readErr, buf, NULL, NULL))
            {
                sc->err = &sc->readErr;
                return false;
            }
        }
    }
}


/*
 * NAME:        SincWaitCalibrationComplete
 * ACTION:      Waits for calibration to be complete. Use with SincRequestCalibration().
 * PARAMETERS:  Sinc *sc                            - the channel to request from.
 *              int channelId                       - which channel to use. -1 to use the default channel for this port.
 *              SincCalibrationData *calibData      - the calibration data.
 *              SincPlot *example, model, final     - the pulse shapes.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincWaitCalibrationComplete(Sinc *sc, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    // Wait for calibration to be complete.
    int complete = false;
    while (!complete)
    {
        if (!SincReadCalibrationProgressResponse(sc, sc->timeout, NULL, NULL, &complete, NULL, NULL))
            return false;
    }

    // Get the calibration data.
    if (!SincGetCalibration(sc, channelId, calibData, example, model, final))
        return false;

    return true;
}


/*
 * NAME:        SincWaitReady
 * ACTION:      Waits for a packet indicating that the channel has returned to a
 *              ready state.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 *              int timeout - timeout in milliseconds. -1 for no timeout.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincWaitReady(Sinc *sc, int channelId, int timeout)
{
    uint8_t pad[256];
    SincBuffer buf = SINC_BUFFER_INIT(pad);
    int fromChannelId;
    SiToro__Sinc__ParamUpdatedResponse *puResp;
    SiToro__Sinc__GetParamResponse *gpResp;

    // Request the "channel.state".
    if (!SincRequestGetParam(sc, channelId, (char *)"channel.state"))
        return false;

    // Keep getting messages until we find the right ones.
    int done = false;
    int gotGetParamResponse = false;
    while (!done && !gotGetParamResponse)
    {
        // Get a message at a time.
        SiToro__Sinc__MessageType msgType;
        if (!SincReadMessage(sc, timeout, &buf, &msgType))
        {
            SINC_BUFFER_CLEAR(&buf);
            return false;
        }

        // We're looking for a "param updated" or a "get param" response.
        if (msgType == SI_TORO__SINC__MESSAGE_TYPE__PARAM_UPDATED_RESPONSE)
        {
            // Is it a "channel.state=ready"?
            fromChannelId = -1;
            if (SincDecodeParamUpdatedResponse(&sc->readErr, &buf, &puResp, &fromChannelId))
            {
                if (channelId < 0 || fromChannelId < 0 || fromChannelId == channelId)
                {
                    unsigned int i;
                    for (i = 0; i < puResp->n_params; i++)
                    {
                        SiToro__Sinc__KeyValue *kv = puResp->params[i];
                        if (kv->key != NULL && strcmp(kv->key, "channel.state") == 0 && kv->optionval != NULL && strcmp(kv->optionval, "ready") == 0)
                        {
                            if (channelId < 0 || (kv->has_channelid && kv->channelid == channelId) )
                            {
                                // We got the response we were looking for.
                                done = true;
                            }
                        }
                    }
                }

                // Clean up the response.
                si_toro__sinc__param_updated_response__free_unpacked(puResp, NULL);
            }
        }
        else if (msgType == SI_TORO__SINC__MESSAGE_TYPE__GET_PARAM_RESPONSE)
        {
            fromChannelId = -1;
            if (SincDecodeGetParamResponse(&sc->readErr, &buf, &gpResp, &fromChannelId) && channelId == fromChannelId)
            {
                // Is it a "channel.state=ready"?
                unsigned int i;
                for (i = 0; i < gpResp->n_results; i++)
                {
                    SiToro__Sinc__KeyValue *kv = gpResp->results[i];
                    if (kv->key != NULL && strcmp(kv->key, "channel.state") == 0 && kv->optionval != NULL && strcmp(kv->optionval, "ready") == 0)
                    {
                        if (channelId < 0 || (kv->has_channelid && kv->channelid == channelId) )
                        {
                            // We got the response we were looking for.
                            done = true;
                        }
                    }
                }

                // Clean up the response.
                si_toro__sinc__get_param_response__free_unpacked(gpResp, NULL);

                gotGetParamResponse = true;
            }
        }
        else if (msgType == SI_TORO__SINC__MESSAGE_TYPE__ASYNCHRONOUS_ERROR_RESPONSE)
        {
            // It's an async error instead. Treat it as an error response.
            if (!SincDecodeAsynchronousErrorResponse(&sc->readErr, &buf, NULL, NULL))
            {
                sc->err = &sc->readErr;
                SINC_BUFFER_CLEAR(&buf);
                return false;
            }
        }
    }

    // If we haven't get received a GetParamResponse wait for it or it'll throw everything else out of sync.
    if (!gotGetParamResponse)
    {
        SincReadGetParamResponse(sc, timeout, NULL, NULL);
    }

    // Clean up the buffer.
    SINC_BUFFER_CLEAR(&buf);

    return true;
}
