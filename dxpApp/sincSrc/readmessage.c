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

#include "sinc.h"
#include "sinc_internal.h"


/*
 * NAME:        SincReadAndDiscardPacket
 * ACTION:      Reads and discards the next packet, which we've presumably already identified
 *              with SincPacketPeek().
 * PARAMETERS:  Sinc *sc                            - the channel to listen to.
 *              SincBuffer *packet                  - the de-encapsulated packet to decode.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadAndDiscardPacket(Sinc *sc, int timeout)
{
    // Read a message at a time.
    uint8_t pad[256];
    SincBuffer packetBuf = SINC_BUFFER_INIT(pad);
    SiToro__Sinc__MessageType msgType;
    int success = SincReadMessage(sc, timeout, &packetBuf, &msgType);
    SINC_BUFFER_CLEAR(&packetBuf);

    return success;
}


/*
 * NAME:        SincReadSuccessResponse
 * ACTION:      Reads a success response from the device. May or may not wait depending on
 *              the timeout.
 * PARAMETERS:  Sinc *sc                                  - the channel to listen to.
 *              int timeout                               - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__SuccessResponse **resp      - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__success_response__free_unpacked(resp, NULL) after use.
 *              int *fromChannelId                        - set to the received channel id. NULL to not use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadSuccessResponse(Sinc *sc, int timeout, SiToro__Sinc__SuccessResponse **resp, int *fromChannelId)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__SUCCESS_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeSuccessResponse(&sc->readErr, &packet, resp, fromChannelId);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadGetParamResponse
 * ACTION:      Reads a get parameters response from the device. May or may not wait depending on
 *              the timeout.
 * PARAMETERS:  Sinc *sc                              - the channel to listen to.
 *              int timeout                           - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__GetParamResponse **resp - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__get_param_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadGetParamResponse(Sinc *sc, int timeout, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__GET_PARAM_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeGetParamResponse(&sc->readErr, &packet, resp, fromChannelId);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadParamUpdatedResponse
 * ACTION:      Reads an asynchronous parameter update message from the device. May or may not wait depending on
 *              the timeout.
 * PARAMETERS:  Sinc *sc                                  - the channel to listen to.
 *              int timeout                               - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__ParamUpdatedResponse **resp - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__param_updated_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadParamUpdatedResponse(Sinc *sc, int timeout, SiToro__Sinc__ParamUpdatedResponse **resp, int *fromChannelId)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__PARAM_UPDATED_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeParamUpdatedResponse(&sc->readErr, &packet, resp, fromChannelId);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadCalibrationProgressResponse
 * ACTION:      Waits for a get parameters response from the device.
 * PARAMETERS:  Sinc *sc                            - the channel to listen to.
 *              int timeout                                - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__CalibrationProgressResponse **resp      - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__calbration_progress_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadCalibrationProgressResponse(Sinc *sc, int timeout, SiToro__Sinc__CalibrationProgressResponse **resp, double *progress, int *complete, char **stage, int *fromChannelId)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__CALIBRATION_PROGRESS_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeCalibrationProgressResponse(&sc->readErr, &packet, resp, progress, complete, stage, fromChannelId);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadGetCalibrationResponse
 * ACTION:      Waits for a get calibration response from the device.
 * PARAMETERS:  Sinc *sc                            - the channel to listen to.
 *              int timeout                                - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__GetCalibrationResponse **resp      - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__get_calibration_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadGetCalibrationResponse(Sinc *sc, int timeout, SiToro__Sinc__GetCalibrationResponse **resp, int *fromChannelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__GET_CALIBRATION_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeGetCalibrationResponse(&sc->readErr, &packet, resp, fromChannelId, calibData, example, model, final);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadListParamDetailsResponse
 * ACTION:      Reads a get parameters response from the device. May or may not wait depending on
 *              the timeout.
 * PARAMETERS:  Sinc *sc                               - the channel to listen to.
 *              int timeout                                   - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__ListParamDetailsResponse **resp - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__list_param_details_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadListParamDetailsResponse(Sinc *sc, int timeout, SiToro__Sinc__ListParamDetailsResponse **resp, int *fromChannelId)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__LIST_PARAM_DETAILS_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeListParamDetailsResponse(&sc->readErr, &packet, resp, fromChannelId);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadSynchronizeLogResponse
 * ACTION:      Reads a synchronize log response from the device. May or may not wait depending on
 *              the timeout.
 * PARAMETERS:  Sinc *sc     - the channel to listen to.
 *              int timeout  - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__SynchronizeLogResponse **resp - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__synchronize_log_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadSynchronizeLogResponse(Sinc *sc, int timeout, SiToro__Sinc__SynchronizeLogResponse **resp)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__SYNCHRONIZE_LOG_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeSynchronizeLogResponse(&sc->readErr, &packet, resp);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadAsynchronousErrorResponse
 * ACTION:      Reads a get parameters response from the device. May or may not wait depending on
 *              the timeout.
 * PARAMETERS:  Sinc *sc                               - the channel to listen to.
 *              int timeout                                   - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__AsynchronousErrorResponse **resp - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__list_param_details_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadAsynchronousErrorResponse(Sinc *sc, int timeout, SiToro__Sinc__AsynchronousErrorResponse **resp, int *fromChannelId)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__ASYNCHRONOUS_ERROR_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeAsynchronousErrorResponse(&sc->readErr, &packet, resp, fromChannelId);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadCalculateDCOffsetResponse
 * ACTION:      Waits for a calculate DC offset response from the device.
 * PARAMETERS:  Sinc *sc                            - the channel to listen to.
 *              int timeout                                - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__CalculateDCOffsetResponse **resp      - where to put the response received.
 *                  This message should be freed with si_toro__sinc__calculate_dc_offset_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadCalculateDCOffsetResponse(Sinc *sc, int timeout, SiToro__Sinc__CalculateDcOffsetResponse **resp, double *dcOffset, int *fromChannelId)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__CALCULATE_DC_OFFSET_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeCalculateDCOffsetResponse(&sc->readErr, &packet, resp, dcOffset, fromChannelId);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincPacketPeek
 * ACTION:      Finds the packet type of the next packet.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int timeout - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__MessageType *packetType - the found packet type is placed here.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincPacketPeek(Sinc *sc, int timeout, SiToro__Sinc__MessageType *packetType)
{
    return SincReadMessage(sc, timeout, NULL, packetType);
}


/*
 * NAME:        SincPacketPeekMulti
 * ACTION:      Finds the packet type of the next packet.
 * PARAMETERS:  Sinc **channelSet - an array of pointers to channels.
 *              int numChannels - the number of channels in the array.
 *              int timeout - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__MessageType *packetType - the found packet type is placed here.
 *              int *packetChannel - set to the channel that a packet was found on.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status. *packetChannel will
 *                  indicate which channel had the error.
 */

bool SincPacketPeekMulti(Sinc **channelSet, int numChannels, int timeout, SiToro__Sinc__MessageType *packetType, int *packetChannel)
{
    int i;

    // Check if there's a packet already buffered on any channel.
    for (i = 0; i < numChannels; i++)
    {
        // Try to get a message from the read buffer.
        int packetFound = false;
        SincGetNextPacketFromBuffer(&channelSet[i]->readBuf, packetType, NULL, &packetFound);
        if (packetFound)
        {
            // We found a packet.
            *packetChannel = i;
            return true;
        }
    }

    // We need to read more data. Wait on data from any channel.
    int *fdSet = malloc(sizeof(int) * (size_t)numChannels * 2);
    if (fdSet == NULL)
    {
        *packetChannel = 0;
        SincReadErrorSetCode(channelSet[*packetChannel], SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    int *fdSetToChannel = malloc(sizeof(int) * (size_t)numChannels * 2);
    if (fdSetToChannel == NULL)
    {
        *packetChannel = 0;
        SincReadErrorSetCode(channelSet[*packetChannel], SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    bool *readOk = malloc(sizeof(bool) * (size_t)numChannels * 2);
    if (readOk == NULL)
    {
        *packetChannel = 0;
        free(fdSet);
        SincReadErrorSetCode(channelSet[*packetChannel], SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    /* Make the set of fds to watch. */
    int numFds = 0;
    for (i = 0; i < numChannels; i++)
    {
        /* Add the channel's TCP fd. */
        fdSet[numFds] = channelSet[i]->fd;
        fdSetToChannel[numFds] = i;
        numFds++;

        /* Add the optional UDP fd. */
        if (channelSet[i]->datagramIsOpen)
        {
            fdSet[numFds] = channelSet[i]->datagramFd;
            fdSetToChannel[numFds] = i;
            numFds++;
        }
    }

    while (true)
    {
        // Check for multiple threads waiting.
        for (i = 0; i < numChannels; i++)
        {
            if (channelSet[i]->inSocketWait)
            {
                *packetChannel = 0;
                SincReadErrorSetCode(channelSet[i], SI_TORO__SINC__ERROR_CODE__MULTIPLE_THREAD_WAIT);
                free(fdSet);
                free(fdSetToChannel);
                free(readOk);
                return false;
            }
        }

        for (i = 0; i < numChannels; i++)
        {
            channelSet[i]->inSocketWait = true;
        }

        // Wait for network activity.
        int err = SincSocketWaitMulti(fdSet, numFds, timeout, readOk);

        for (i = 0; i < numChannels; i++)
        {
            channelSet[i]->inSocketWait = false;
        }

        if (err != 0)
        {
            *packetChannel = 0;
            SincReadErrorSetCode(channelSet[*packetChannel], (SiToro__Sinc__ErrorCode)err);
            free(fdSet);
            free(fdSetToChannel);
            free(readOk);
            return false;
        }

        // Check each channel for activity.
        for (i = 0; i < numFds; i++)
        {
            if (readOk[i])
            {
                // Got something on this channel - poll the channel to try to get a packet.
                int channelId = fdSetToChannel[i];
                if (SincPacketPeek(channelSet[channelId], 0, packetType))
                {
                    // Got a packet.
                    *packetChannel = channelId;
                    free(fdSet);
                    free(fdSetToChannel);
                    free(readOk);
                    return true;
                }
                else if (SincReadErrorCode(channelSet[i]) != SI_TORO__SINC__ERROR_CODE__TIMEOUT)
                {
                    // Got an error.
                    free(fdSet);
                    free(fdSetToChannel);
                    free(readOk);
                    return false;
                }
            }
        }
    }
}


/*
 * NAME:        SincReadOscilloscope
 * ACTION:      Gets a curve from the oscilloscope. Waits for the next oscilloscope update to arrive.
 * PARAMETERS:  Sinc *sc                  - the sinc connection.
 *              int timeout               - in milliseconds. 0 to poll. -1 to wait forever.
 *              int *fromChannelId        - if non-NULL this is set to the channel the histogram was received from.
 *              SincOscPlot *resetBlanked - coordinates of the reset blanked oscilloscope plot. Will allocate resetBlanked->data and resetBlanked->intData so you must free them.
 *              SincOscPlot *rawCurve     - coordinates of the raw oscilloscope plot. Will allocate rawCurve->data and resetBlanked->intData so you must free them.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status. There's no need to free
 *                  resetBlanked or rawCurve data on failure.
 */

bool SincReadOscilloscope(Sinc *sc, int timeout, int *fromChannelId, uint64_t *dataSetId, SincOscPlot *resetBlanked, SincOscPlot *rawCurve)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__OSCILLOSCOPE_DATA_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeOscilloscopeDataResponse(&sc->readErr, &packet, fromChannelId, dataSetId, resetBlanked, rawCurve);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadHistogram
 * ACTION:      Gets an update from the histogram. Waits for the next histogram update to
 *              arrive if timeout is non-zero.
 * PARAMETERS:  Sinc *sc                - the sinc connection.
 *              int timeout             - in milliseconds. 0 to poll. -1 to wait forever.
 *              int *fromChannelId      - if non-NULL this is set to the channel the histogram was received from.
 *              SincHistogram *accepted - the accepted histogram plot. Will allocate accepted->data so you must free it.
 *              SincHistogram *rejected - the rejected histogram plot. Will allocate rejected->data so you must free it.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status. There's no need to free
 *                  accepted or rejected data on failure.
 */

bool SincReadHistogram(Sinc *sc, int timeout, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__HISTOGRAM_DATA_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeHistogramDataResponse(&sc->readErr, &packet, fromChannelId, accepted, rejected, stats);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadHistogramDatagram
 * ACTION:      Gets an datagram-format update from the histogram. Waits for the next histogram update to
 *              arrive if timeout is non-zero.
 * PARAMETERS:  Sinc *sc                - the sinc connection.
 *              int timeout             - in milliseconds. 0 to poll. -1 to wait forever.
 *              int *fromChannelId      - if non-NULL this is set to the channel the histogram was received from.
 *              SincHistogram *accepted - the accepted histogram plot. Will allocate accepted->data so you must free it.
 *              SincHistogram *rejected - the rejected histogram plot. Will allocate rejected->data so you must free it.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status. There's no need to free
 *                  accepted or rejected data on failure.
 */

bool SincReadHistogramDatagram(Sinc *sc, int timeout, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__HISTOGRAM_DATAGRAM_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeHistogramDatagramResponse(&sc->readErr, &packet, fromChannelId, accepted, rejected, stats);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadListMode
 * ACTION:      Gets an update from the list mode data stream. Waits for the next list mode update to
 *              arrive if timeout is non-zero.
 * PARAMETERS:  Sinc *sc                     - the sinc connection.
 *              int timeout                  - in milliseconds. 0 to poll. -1 to wait forever.
 *              int *fromChannelId           - if non-NULL this is set to the channel the histogram was received from.
 *              uint8_t **data, int *dataLen - filled in with a dynamically allocated buffer containing list mode data. Must be freed on success.
 *              uint64_t *dataSetId          - if non-NULL this is set to the data set id.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status. There's no need to free
 *                  accepted or rejected data on failure.
 */

bool SincReadListMode(Sinc *sc, int timeout, int *fromChannelId, uint8_t **data, int *dataLen, uint64_t *dataSetId)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__LIST_MODE_DATA_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeListModeDataResponse(&sc->readErr, &packet, fromChannelId, data, dataLen, dataSetId);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadCheckParamConsistencyResponse
 * ACTION:      Reads a response to a check parameter consistency command. May or may not wait depending on
 *              the timeout.
 * PARAMETERS:  Sinc *sc                               - the channel to listen to.
 *              int timeout                            - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__ListParamDetailsResponse **resp - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__list_param_details_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadCheckParamConsistencyResponse(Sinc *sc, int timeout, SiToro__Sinc__CheckParamConsistencyResponse **resp, int *fromChannelId)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__CHECK_PARAM_CONSISTENCY_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeCheckParamConsistencyResponse(&sc->readErr, &packet, resp, fromChannelId);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}


/*
 * NAME:        SincReadDownloadCrashDumpResponse
 * ACTION:      Reads a response to a download crash dump command. May or may not wait depending on
 *              the timeout.
 * PARAMETERS:  Sinc *sc           - the channel to listen to.
 *              int timeout        - in milliseconds. 0 to poll. -1 to wait forever.
 *              bool *newDump      - set true if this crash dump is new.
 *              uint8_t **dumpData - where to put a pointer to the newly allocated crash dump data.
 *                                   This should be free()d after use.
 *              size_t *dumpSize   - where to put the size of the crash dump data.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincReadDownloadCrashDumpResponse(Sinc *sc, int timeout, bool *newDump, uint8_t **dumpData, size_t *dumpSize)
{
    uint8_t pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    int success;

    if (!SincWaitForMessageType(sc, timeout, &packet, SI_TORO__SINC__MESSAGE_TYPE__DOWNLOAD_CRASH_DUMP_RESPONSE))
    {
        SINC_BUFFER_CLEAR(&packet);
        return false;
    }

    success = SincDecodeDownloadCrashDumpResponse(&sc->readErr, &packet, newDump, dumpData, dumpSize);
    if (!success)
        sc->err = &sc->readErr;

    SINC_BUFFER_CLEAR(&packet);
    return success;
}
