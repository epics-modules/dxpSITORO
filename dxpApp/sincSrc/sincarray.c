/********************************************************************
 ***                                                              ***
 ***                     libsinc device array                     ***
 ***                                                              ***
 ********************************************************************/

/*
 * This module provides a way to access an entire array of cards
 * in a convenient way.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "sincarray.h"
#include "sinc.h"
#include "sinc_internal.h"



/* A response we're waiting for from multiple places. */
typedef struct _SincArrayWaitReponse
{
    int   deviceId;         // Which SINC device we want a response from.
    int   deviceChannelId;  // Which channel on this device.
    bool  keepResponse;     // Whether to keep the response for later use.
    void *resp;             // The response will be cast to void * and put here.
    bool  gotResponse;      // Set to whether a response was received.
    bool  gotError;         // Set true if the response was an error.
} SincArrayWaitResponse;


/* Forward declarations. */
bool SincArraySendToEachDevice(SincArray *sa, SincBuffer *sendBuf);
void SincArrayErrorSetMessage(SincArray *sa, SiToro__Sinc__ErrorCode code, const char *msg);
void SincArrayErrorSetCode(SincArray *sa, SiToro__Sinc__ErrorCode code);
void SincArrayChannelTranslate(SincArray *sa, int *deviceId, int *deviceChannelId, int channelId);


/*
 * NAME:        SincArrayInit
 * ACTION:      Initialises a SincArray.
 * RETURNS:     true on success, false otherwise. On failure use SincArrayErrno() and
 *                  SincArrayStrError() to get the error status.
 */

bool SincArrayInit(SincArray *sa)
{
    memset(sa, 0, sizeof(*sa));
    sa->numDevices = 0;
    sa->devices = NULL;
    sa->channelsPerDevice = 0;
    sa->timeout = -1;
    sa->err = NULL;

    return true;
}


/*
 * NAME:        SincArrayCleanup
 * ACTION:      Closes and frees data used by the device array (but doesn't free the SincArray structure itself).
 * PARAMETERS:  SincArray *sa - the device array to clean up.
 */

void SincArrayCleanup(SincArray *sa)
{
    int i;

    for (i = 0; i < sa->numDevices; i++)
    {
        SincCleanup(&sa->devices[i]);
    }

    if (sa->devices)
    {
        free(sa->devices);
        sa->devices = NULL;
        sa->numDevices = 0;
    }
}


/*
 * NAME:        SincArraySetTimeout
 * ACTION:      Sets a timeout for following commands.
 * PARAMETERS:  int timeoutMs - timeout in milliseconds. -1 for no timeout.
 */

void SincArraySetTimeout(SincArray *sa, int timeoutMs)
{
    int i;

    sa->timeout = timeoutMs;

    for (i = 0; i < sa->numDevices; i++)
    {
        SincSetTimeout(&sa->devices[i], timeoutMs);
    }
}


/*
 * NAME:        SincArrayConnect
 * ACTION:      Connects a SincArray channel to a device on a given host.
 * PARAMETERS:  SincArray *sa         - the sinc device array to connect.
 *              const char **hosts    - an array of hosts to connect to.
 *              int numHosts          - the number of hosts in the array.
 *              int channelsPerDevice - the number of channels there are in each array device.
 * RETURNS:     true on success, false otherwise. On failure use SincArrayErrno() and
 *                  SincArrayStrError() to get the error status.
 */

bool SincArrayConnect(SincArray *sa, const char **hosts, int numHosts, int channelsPerDevice)
{
    if (sa->numDevices > 0)
    {
        SincArrayCleanup(sa);
    }

    /* Allocate the Sinc structures. */
    sa->devices = calloc(numHosts, sizeof(Sinc));
    if (!sa->devices)
    {
        SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    /* Initialise the Sinc structures. */
    int i;
    for (i = 0; i < numHosts; i++)
    {
        SincInit(&sa->devices[i]);
        SincSetTimeout(&sa->devices[i], sa->timeout);
    }

    sa->numDevices = numHosts;
    sa->channelsPerDevice = channelsPerDevice;

    /* Connect to each host. */
    bool ok = true;
    for (i = 0; i < sa->numDevices; i++)
    {
        Sinc *sc = &sa->devices[i];
        if (!SincConnect(sc, hosts[i], SINC_PORT))
        {
            ok = false;
            sa->err = sc->err;
        }
    }

    return ok;
}


/*
 * NAME:        SincArrayConnectPort
 * ACTION:      Connects a SincArray channel to a device on a given host and port.
 * PARAMETERS:  SincArray *sa      - the sinc device array to connect.
 *              const char **hosts - an array of hosts to connect to.
 *              int *ports         - an array of port corresponding to the hosts.
 *              int numHosts       - the number of hosts in the array.
 * RETURNS:     true on success, false otherwise. On failure use SincArrayErrno() and
 *                  SincArrayStrError() to get the error status.
 */

bool SincArrayConnectPort(SincArray *sa, const char **hosts, int *ports, int numHosts, int channelsPerDevice)
{
    int i;

    /* Allocate the Sinc structures. */
    sa->devices = calloc(numHosts, sizeof(Sinc));
    if (!sa->devices)
    {
        SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    /* Initialise the Sinc structures. */
    for (i = 0; i < sa->numDevices; i++)
    {
        SincInit(&sa->devices[i]);
        SincSetTimeout(&sa->devices[i], sa->timeout);
    }

    sa->numDevices = numHosts;
    sa->channelsPerDevice = channelsPerDevice;

    /* Connect to each host. */
    bool ok = true;
    for (i = 0; i < sa->numDevices; i++)
    {
        Sinc *sc = &sa->devices[i];
        if (!SincConnect(sc, hosts[i], ports[i]))
        {
            ok = false;
            sa->err = sc->err;
        }
    }

    return ok;
}


/*
 * NAME:        SincArrayDisconnect
 * ACTION:      Disconnects a SincArray channel from whatever it's connected to.
 * PARAMETERS:  SincArray *sa         - the sinc device array to disconnect.
 * RETURNS:     true on success, false otherwise. On failure use SincArrayErrno() and
 *                  SincArrayStrError() to get the error status.
 */

bool SincArrayDisconnect(SincArray *sa)
{
    int i;
    bool success = true;

    for (i = 0; i < sa->numDevices; i++)
    {
        Sinc *sc = &sa->devices[i];
        if (!SincDisconnect(sc))
        {
            success = false;
            sa->err = sc->err;
        }
    }

    return success;
}


/*
 * NAME:        SincArrayIsConnected
 * ACTION:      Returns the connected/disconnected state of the channel.
 * PARAMETERS:  SincArray *sa         - the sinc connection.
 * RETURNS:     bool - true if connected, false otherwise.
 */

bool SincArrayIsConnected(SincArray *sa)
{
    int i;

    for (i = 0; i < sa->numDevices; i++)
    {
        if (!sa->devices[i].connected)
            return false;
    }

    return true;
}

/*
 * NAME:        SincArrayReadMessage
 * ACTION:      Reads or polls for the next message. The received packet can then be decoded
 *              with one of the SincDecodeXXX() calls.
 * PARAMETERS:  SincArray *sa                         - the sinc device array.
 *              int timeout                           - in milliseconds. 0 to poll. -1 to wait forever.
 *              SincBuffer *packetBuf                 - the received packet data is placed here.
 *              SiToro__Sinc__MessageType *packetType - the found packet type is placed here.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayReadMessage(SincArray *sa, int timeout, SincBuffer *packetBuf, SiToro__Sinc__MessageType *packetType)
{
#if 0
    int i;

    // Check if there's a packet already buffered on any channel.
    for (i = 0; i < sa->numDevices; i++)
    {
        Sinc *sc = &sa->devices[i];

        if (SincReadMessage(sc, 0, packetBuf, packetType))
        {
            // We found a packet.
            packetBuf->deviceId = i;
            packetBuf->channelIdOffset = sa->channelsPerDevice * i;
            return true;
        }
        else if (sc->err->code != SI_TORO__SINC__ERROR_CODE__TIMEOUT)
        {
            // Got an error.
            return false;
        }
    }

    // We need to read more data. Wait on data from any channel.
    int *fdSet = calloc((size_t)sa->numDevices, sizeof(int));
    if (fdSet == NULL)
    {
        SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    bool *readOk = calloc((size_t)sa->numDevices, sizeof(bool));
    if (readOk == NULL)
    {
        free(fdSet);
        SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    for (i = 0; i < sa->numDevices; i++)
    {
        fdSet[i] = sa->devices[i].fd;
    }

    while (true)
    {
        // Wait for network activity.
        int err = SincSocketWaitMulti(fdSet, sa->numDevices, timeout, readOk);
        if (err != 0)
        {
            packetBuf->deviceId = 0;
            packetBuf->channelIdOffset = 0;
            SincArrayErrorSetCode(sa, (SiToro__Sinc__ErrorCode)err);
            free(fdSet);
            free(readOk);
            return false;
        }

        // Check each channel for activity.
        for (i = 0; i < sa->numDevices; i++)
        {
            if (readOk[i])
            {
                // Got something on this channel - poll the channel to try to get a packet.
                Sinc *sc = &sa->devices[i];
                if (SincReadMessage(sc, timeout, packetBuf, packetType))
                {
                    // Got a packet.
                    packetBuf->channelIdOffset = i;
                    free(fdSet);
                    free(readOk);
                    return true;
                }
                else if (sc->readErr.code != SI_TORO__SINC__ERROR_CODE__TIMEOUT)
                {
                    // Got an error.
                    free(fdSet);
                    free(readOk);
                    return false;
                }
            }
        }
    }

    return true;
#else
    return false;
#endif
}

/*
 * NAME:        SincArrayReadMessageFromResponseSet
 * ACTION:      Reads or polls for the next message. The received packet can then be decoded
 *              with one of the SincDecodeXXX() calls. Only the boards in the responseSet
 *              are checked.
 * PARAMETERS:  SincArray *sa                         - the sinc device array.
 *              int timeout                           - in milliseconds. 0 to poll. -1 to wait forever.
 *              SincArrayWaitResponse *responseSet    - the set of devices and channels we're monitoring.
 *              size_t responseSetSize                - the size of the responseSet.
 *              SincBuffer *packetBuf                 - the received packet data is placed here.
 *              SiToro__Sinc__MessageType *packetType - the found packet type is placed here.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

static bool SincArrayReadMessageFromResponseSet(SincArray *sa, int timeout, SincArrayWaitResponse *responseSet, int responseSetSize, SincBuffer *packetBuf, SiToro__Sinc__MessageType *packetType)
{
    int i;
    uint64_t deviceBitSet = 0;
    int numDevicesMonitored = 0;
    int maxUsedDeviceId = 0;

    // Create a bit set of the devices we're interested in.
    for (i = 0; i < responseSetSize; i++)
    {
        // Do we already have this one?
        if (!responseSet[i].gotResponse && (deviceBitSet & (1 << responseSet[i].deviceId)) == 0)
        {
            // No - add it.
            numDevicesMonitored++;
            deviceBitSet |= (1 << responseSet[i].deviceId);

            if (responseSet[i].deviceId >= maxUsedDeviceId)
            {
                maxUsedDeviceId = responseSet[i].deviceId + 1;
            }
        }
    }

    // Check if there's a packet already buffered on any channel.
    for (i = 0; i < maxUsedDeviceId; i++)
    {
        // Is it one of the devices we care about?
        if (deviceBitSet & (1 << i))
        {
            Sinc *sc = &sa->devices[i];

            if (SincReadMessage(sc, 0, packetBuf, packetType))
            {
                // We found a packet.
                packetBuf->deviceId = i;
                packetBuf->channelIdOffset = sa->channelsPerDevice * i;
                return true;
            }
            else if (sc->err->code != SI_TORO__SINC__ERROR_CODE__TIMEOUT)
            {
                // Got an error.
                return false;
            }
        }
    }

    // We need to read more data. Wait on data from any channel.
    int fdSet[numDevicesMonitored];
    bool readOk[numDevicesMonitored];
    int deviceId[numDevicesMonitored];
    size_t index = 0;
    for (i = 0; i < maxUsedDeviceId; i++)
    {
        if (deviceBitSet & (1 << responseSet[i].deviceId))
        {
            fdSet[index] = sa->devices[i].fd;
            deviceId[index] = i;
            index++;
        }
    }

    while (true)
    {
        // Wait for network activity.
        int err = SincSocketWaitMulti(fdSet, numDevicesMonitored, timeout, readOk);
        if (err != 0)
        {
            packetBuf->deviceId = 0;
            packetBuf->channelIdOffset = 0;
            SincArrayErrorSetCode(sa, (SiToro__Sinc__ErrorCode)err);
            return false;
        }

        // Check each channel for activity.
        for (i = 0; i < numDevicesMonitored; i++)
        {
            if (readOk[i])
            {
                // Got something on this channel - poll the channel to try to get a packet.
                Sinc *sc = &sa->devices[deviceId[i]];
                if (SincReadMessage(sc, timeout, packetBuf, packetType))
                {
                    // Got a packet.
                    packetBuf->channelIdOffset = i;
                    return true;
                }
                else if (sc->readErr.code != SI_TORO__SINC__ERROR_CODE__TIMEOUT)
                {
                    // Got an error.
                    return false;
                }
            }
        }
    }

    return true;
}


/*
 * NAME:        SincArrayFindResponse
 * ACTION:      Find a given device/channel combo in the list of responses we're looking for.
 * PARAMETERS:  SincArrayWaitResponse *responseSet - a list of responses we're looking for.
 *              int responseSetSize                - how long the list is.
 *              int deviceId                       - the device id we found.
 *              int fromChannelId                  - the device channel id we found.
 * RETURNS:     int - returns the response list offset of the matching response or -1 if not found.
 */

int SincArrayFindResponse(SincArrayWaitResponse *responseSet, int responseSetSize, int deviceId, int fromChannelId)
{
    int i;

    for (i = 0; i < responseSetSize; i++)
    {
        if (responseSet[i].deviceId == deviceId && responseSet[i].deviceChannelId == fromChannelId)
            return i;
    }

    return -1;
}


/*
 * NAME:        SincArrayWaitResponseSet
 * ACTION:      Wait for a set of responses from a set of devices.
 * PARAMETERS:  SincArray *sa - the sinc array.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincArrayWaitResponseSet(SincArray *sa, SincArrayWaitResponse *responseSet, int responseSetSize, SiToro__Sinc__MessageType mt)
{
    int   i;
    bool  ok = true;
    int   numResponses = 0;

    for (i = 0; i < responseSetSize; i++)
    {
        responseSet[i].resp = NULL;
        responseSet[i].gotError = false;
        responseSet[i].gotResponse = false;
    }

    // Wait for responses from all the devices.
    while (numResponses < responseSetSize)
    {
        // Read messages until we've seen enough success responses.
        uint8_t pad[256];
        SincBuffer buf = SINC_BUFFER_INIT(pad);
        SiToro__Sinc__MessageType msgType = SI_TORO__SINC__MESSAGE_TYPE__NO_MESSAGE_TYPE;
        if (!SincArrayReadMessageFromResponseSet(sa, sa->timeout, responseSet, responseSetSize, &buf, &msgType))
        {
            return false;
        }

        // Is it a success response?
        if (msgType == mt || msgType == SI_TORO__SINC__MESSAGE_TYPE__ASYNCHRONOUS_ERROR_RESPONSE)
        {
            // Decode it.
            bool gotError = false;
            int fromChannelId = -1;

            switch (msgType)
            {
            case SI_TORO__SINC__MESSAGE_TYPE__ASYNCHRONOUS_ERROR_RESPONSE:
                if (!SincDecodeAsynchronousErrorResponse(&sa->arrayErr, &buf, NULL, &fromChannelId))
                {
                    sa->err = &sa->arrayErr;
                    gotError = true;
                }
                break;

            case SI_TORO__SINC__MESSAGE_TYPE__SUCCESS_RESPONSE:
                if (!SincDecodeSuccessResponse(&sa->arrayErr, &buf, NULL, &fromChannelId))
                {
                    sa->err = &sa->arrayErr;
                    gotError = true;
                }
                break;

            case SI_TORO__SINC__MESSAGE_TYPE__GET_PARAM_RESPONSE:
            {
                SiToro__Sinc__GetParamResponse *resp = NULL;
                if (!SincDecodeGetParamResponse(&sa->arrayErr, &buf, &resp, &fromChannelId))
                {
                    sa->err = &sa->arrayErr;
                    gotError = true;
                }

                int responseId = SincArrayFindResponse(responseSet, responseSetSize, buf.deviceId, fromChannelId);
                if (responseId >= 0 && responseSet[responseId].keepResponse)
                {
                    responseSet[responseId].resp = (void *)resp;
                }
                else
                {
                    si_toro__sinc__get_param_response__free_unpacked(resp, NULL);
                }
                break;
            }

            case SI_TORO__SINC__MESSAGE_TYPE__GET_CALIBRATION_RESPONSE:
            {
                SiToro__Sinc__GetCalibrationResponse *resp = NULL;
                if (!SincDecodeGetCalibrationResponse(&sa->arrayErr, &buf, &resp, &fromChannelId, NULL, NULL, NULL, NULL))
                {
                    sa->err = &sa->arrayErr;
                    gotError = true;
                }

                int responseId = SincArrayFindResponse(responseSet, responseSetSize, buf.deviceId, fromChannelId);
                if (responseId >= 0 && responseSet[responseId].keepResponse)
                {
                    responseSet[responseId].resp = (void *)resp;
                }
                else
                {
                    si_toro__sinc__get_calibration_response__free_unpacked(resp, NULL);
                }
                break;
            }

            case SI_TORO__SINC__MESSAGE_TYPE__CALCULATE_DC_OFFSET_RESPONSE:
            {
                SiToro__Sinc__CalculateDcOffsetResponse *resp = NULL;
                if (!SincDecodeCalculateDCOffsetResponse(&sa->arrayErr, &buf, &resp, NULL, &fromChannelId))
                {
                    sa->err = &sa->arrayErr;
                    gotError = true;
                }

                int responseId = SincArrayFindResponse(responseSet, responseSetSize, buf.deviceId, fromChannelId);
                if (responseId >= 0 && responseSet[responseId].keepResponse)
                {
                    responseSet[responseId].resp = (void *)resp;
                }
                else
                {
                    si_toro__sinc__calculate_dc_offset_response__free_unpacked(resp, NULL);
                }
                break;
            }

            case SI_TORO__SINC__MESSAGE_TYPE__LIST_PARAM_DETAILS_RESPONSE:
            {
                SiToro__Sinc__ListParamDetailsResponse *resp = NULL;
                if (!SincDecodeListParamDetailsResponse(&sa->arrayErr, &buf, &resp, &fromChannelId))
                {
                    sa->err = &sa->arrayErr;
                    gotError = true;
                }

                int responseId = SincArrayFindResponse(responseSet, responseSetSize, buf.deviceId, fromChannelId);
                if (responseId >= 0 && responseSet[responseId].keepResponse)
                {
                    responseSet[responseId].resp = (void *)resp;
                }
                else
                {
                    si_toro__sinc__list_param_details_response__free_unpacked(resp, NULL);
                }
                break;
            }

            case SI_TORO__SINC__MESSAGE_TYPE__SOFTWARE_UPDATE_COMPLETE_RESPONSE:
            {
                if (!SincDecodeSoftwareUpdateCompleteResponse(&sa->arrayErr, &buf))
                {
                    sa->err = &sa->arrayErr;
                    gotError = true;
                }

                int responseId = SincArrayFindResponse(responseSet, responseSetSize, buf.deviceId, fromChannelId);
                if (responseId >= 0 && responseSet[responseId].keepResponse)
                {
                    responseSet[responseId].resp = (void *)1;
                }
                break;
            }

            case SI_TORO__SINC__MESSAGE_TYPE__CHECK_PARAM_CONSISTENCY_RESPONSE:
            {
                SiToro__Sinc__CheckParamConsistencyResponse *resp = NULL;
                if (!SincDecodeCheckParamConsistencyResponse(&sa->arrayErr, &buf, &resp, &fromChannelId))
                {
                    sa->err = &sa->arrayErr;
                    gotError = true;
                }

                int responseId = SincArrayFindResponse(responseSet, responseSetSize, buf.deviceId, fromChannelId);
                if (responseId >= 0 && responseSet[responseId].keepResponse)
                {
                    responseSet[responseId].resp = (void *)resp;
                }
                else
                {
                    si_toro__sinc__check_param_consistency_response__free_unpacked(resp, NULL);
                }
                break;
            }

#if 0
            case SI_TORO__SINC__MESSAGE_TYPE__SYNCHRONIZE_LOG_RESPONSE:
            {
                SiToro__Sinc__SynchronizeLogResponse *resp = NULL;
                if (!SincDecodeSynchronizeLogResponse(&sa->arrayErr, &buf, &resp, &fromChannelId))
                {
                    sa->err = &sa->arrayErr;
                    gotError = true;
                }

                int responseId = SincArrayFindResponse(responseSet, responseSetSize, buf.deviceId, fromChannelId);
                if (responseId >= 0 && responseSet[responseId].keepResponse)
                {
                    responseSet[responseId].resp = (void *)resp;
                }
                else
                {
                    si_toro__sinc__synchronize_log_response__free_unpacked(resp, NULL);
                }
                break;
            }
#endif

            default:
                break;
            }

            // Mark as received.
            int responseId = SincArrayFindResponse(responseSet, responseSetSize, buf.deviceId, fromChannelId);
            if (responseId >= 0 && !responseSet[responseId].gotResponse)
            {
                if (gotError)
                {
                    responseSet[responseId].gotError = true;
                }

                responseSet[responseId].gotResponse = true;
                numResponses++;
            }
        }
    }

    return ok;
}


/*
 * NAME:        SincArrayCheckSuccess
 * ACTION:      Check for a simple success response.
 * PARAMETERS:  SincArray *sa - the sinc array.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincArrayCheckSuccess(SincArray *sa)
{
    int   i;
    bool  ok = true;
    int   numResponses = 0;
    bool *gotResponse = calloc(sa->numDevices, sizeof(bool));

    if (!gotResponse)
    {
        SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    for (i = 0; i < sa->numDevices; i++)
    {
        gotResponse[i] = false;
    }

    // Wait for responses from all the devices.
    while (numResponses < sa->numDevices)
    {
        // Read messages until we've seen enough success responses.
        uint8_t pad[256];
        SincBuffer buf = SINC_BUFFER_INIT(pad);
        SiToro__Sinc__MessageType msgType;
        if (!SincArrayReadMessage(sa, sa->timeout, &buf, &msgType))
        {
            free(gotResponse);
            return false;
        }

        // Is it a success response?
        if (msgType == SI_TORO__SINC__MESSAGE_TYPE__SUCCESS_RESPONSE)
        {
            // Decode it.
            SiToro__Sinc__SuccessResponse *resp = NULL;
            if (!SincDecodeSuccessResponse(&sa->arrayErr, &buf, &resp, NULL))
            {
                sa->err = &sa->arrayErr;
                free(gotResponse);
                return false;
            }

            if (resp != NULL)
            {
                // Is this a success response from a device we haven't had a success response from yet?
                if (!gotResponse[buf.deviceId])
                {
                    numResponses++;
                    gotResponse[buf.deviceId] = true;
                }

                si_toro__sinc__success_response__free_unpacked(resp, NULL);
            }
        }
    }

    free(gotResponse);

    return ok;
}


/*
 * NAME:        SincArrayPing
 * ACTION:      Checks if the device is responding.
 *              SincRequestPing() variant sends the request but doesn't wait for a reply.
 * PARAMETERS:  SincArray *sa  - the sinc device array to ping.
 *              int showOnConsole - not used, set to 0.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayPing(SincArray *sa, int showOnConsole)
{
    // Send the request.
    if (!SincArrayRequestPing(sa, showOnConsole))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}


/*
 * NAME:        SincArrayGetParam
 * ACTION:      Gets a named parameter from the device.
 *              Note that the channel ids of parameters are included with each returned parameter.
 * PARAMETERS:  SincArray *sa                        - the sinc device array to request from.
 *              int channelId                   - which channel to use.
 *              char *name                      - the name of the parameter to get.
 *              SiToro__Sinc__GetParamResponse **resp - where to put the response received.
 *
 *                  (*resp)->results[0] will contain the result as a SiToro__Sinc__KeyValue.
 *                  Get the type of response from value_case and the value from one of intval,
 *                  floatval, boolval, strval or optionval.
 *
 *                  resp should be freed with si_toro__sinc__get_param_response__free_unpacked(resp, NULL) after use.
 *
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayGetParam(SincArray *sa, int channelId, char *name, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}

bool SincArrayGetParams(SincArray *sa, int *channelIds, const char **names, int numNames, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}



/*
 * NAME:        SincArraySetParam
 * ACTION:      Sets a named parameter on the device.
 * PARAMETERS:  SincArray *sa                            - the sinc device array to request from.
 *              SiToro__Sinc__KeyValue *param       - the key and value to set.
 *                  Set the key in param->key.
 *                  Set the value type in param->valueCase and the value in one of intval,
 *                  floatval, boolval, strval or optionval.
 *
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArraySetParam(SincArray *sa, SiToro__Sinc__KeyValue *param)
{
    // Send the request.
    if (!SincArrayRequestSetParam(sa, param))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}

bool SincArraySetParams(SincArray *sa, SiToro__Sinc__KeyValue *params, int numParams)
{
    // Send the request.
    if (!SincArrayRequestSetParams(sa, params, numParams))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



/*
 * NAME:        SincArrayCalibrate
 * ACTION:      Performs a calibration and returns calibration data. May take several seconds.
 * PARAMETERS:  SincArray *sa                            - the sinc device array to request from.
 *              int channelId                       - which channel to use.
 *              SincCalibrationData *calibData      - the calibration data.
 *              SincPlot *example, model, final     - the pulse shapes.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayCalibrate(SincArray *sa, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}



/*
 * NAME:        SincArrayStartCalibration
 * ACTION:      Starts a calibration. Use SincArrayWaitCalibrationComplete() to wait for
 *              completion.
 * PARAMETERS:  SincArray *sa                            - the sinc device array to request from.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayStartCalibration(SincArray *sa, int channelId)
{
    // Send the request.
    if (!SincArrayRequestStartCalibration(sa, channelId))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



/*
 * NAME:        SincArrayWaitCalibrationComplete
 * ACTION:      Waits for calibration to be complete. Use with SincArrayRequestCalibration().
 * PARAMETERS:  SincArray *sa                            - the sinc device array to request from.
 *              int channelId                       - which channel to use.
 *              SincCalibrationData *calibData      - the calibration data.
 *              SincPlot *example, model, final     - the pulse shapes.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayWaitCalibrationComplete(SincArray *sa, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}



/*
 * NAME:        SincArrayGetCalibration
 * ACTION:      Gets the calibration data from a previous calibration.
 * PARAMETERS:  SincArray *sa                        - the sinc device array to request from.
 *              int channelId                   - which channel to use.
 *              SincCalibrationData *calibData  - the calibration data is stored here.
 *              calibData must be free()d after use.
 *              SincPlot *example, model, final - the pulse shapes are set here.
 *              The fields x and y of each pulse must be free()d after use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayGetCalibration(SincArray *sa, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}



/*
 * NAME:        SincArraySetCalibration
 * ACTION:      Sets the calibration data on the device from a previously acquired data set.
 * PARAMETERS:  SincArray *sa                            - the sinc device array to request from.
 *              int channelId                       - which channel to use.
 *              SincCalibrationData *calibData      - the calibration data to set.
 *              SincPlot *example, model, final     - the pulse shapes.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArraySetCalibration(SincArray *sa, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    // Send the request.
    if (!SincArrayRequestSetCalibration(sa, channelId, calibData, example, model, final))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



/*
 * NAME:        SincArrayCalculateDcOffset
 * ACTION:      Calculates the DC offset on the device. May take a couple of seconds.
 * PARAMETERS:  SincArray *sa         - the the sinc device array to request from.
 *              int channelId    - which channel to use.
 *              double *dcOffset - the calculated DC offset is placed here.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayCalculateDcOffset(SincArray *sa, int channelId, double *dcOffset)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}



/*
 * NAME:        SincArrayStartOscilloscope
 * ACTION:      Starts the oscilloscope.
 * PARAMETERS:  SincArray *sa            - the sinc device array.
 *              int channelId       - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayStartOscilloscope(SincArray *sa, int channelId)
{
    // Send the request.
    if (!SincArrayRequestStartOscilloscope(sa, channelId))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



/*
 * NAME:        SincArrayReadOscilloscope
 * ACTION:      Gets a curve from the oscilloscope. Waits for the next oscilloscope update to
 *              arrive if timeout is non-zero.
 * PARAMETERS:  SincArray *sa                  - the sinc device array.
 *              int timeout               - in milliseconds. 0 to poll. -1 to wait forever.
 *              int *fromChannelId        - if non-NULL this is set to the channel the histogram was received from.
 *              SincOscPlot *resetBlanked - coordinates of the reset blanked oscilloscope plot. Will allocate resetBlanked->data and resetBlanked->intData so you must free them.
 *              SincOscPlot *rawCurve     - coordinates of the raw oscilloscope plot. Will allocate rawCurve->data and resetBlanked->intData so you must free them.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status. There's no need to free
 *                  resetBlanked or rawCurve data on failure.
 */

bool SincArrayReadOscilloscope(SincArray *sa, int timeout, int *fromChannelId, uint64_t *dataSetId, SincOscPlot *resetBlanked, SincOscPlot *rawCurve)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}



/*
 * NAME:        SincArrayStartHistogram
 * ACTION:      Starts the histogram. Note that if you want to use TCP only you should set
 *              sc->datagramXfer to false. Otherwise UDP will be used.
 * PARAMETERS:  SincArray *sa               - the sinc device array.
 *              int channelId          - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayStartHistogram(SincArray *sa, int channelId)
{
    // Send the request.
    if (!SincArrayRequestStartHistogram(sa, channelId))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



/*
 * NAME:        SincArrayReadHistogram
 * ACTION:      Gets an update from the histogram. Waits for the next histogram update to
 *              arrive if timeout is non-zero. Formats for stream and datagram types.
 * PARAMETERS:  SincArray *sa                - the sinc device array.
 *              int timeout             - in milliseconds. 0 to poll. -1 to wait forever.
 *              int *fromChannelId      - if non-NULL this is set to the channel the histogram was received from.
 *              SincHistogram *accepted - the accepted histogram plot. Will allocate accepted->data so you must free it.
 *              SincHistogram *rejected - the rejected histogram plot. Will allocate rejected->data so you must free it.
 *              SincHistogramCountStats *stats - various statistics about the histogram. Can be NULL if not needed.
 *                                        May allocate stats->intensity so you should free it if non-NULL.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status. There's no need to free
 *                  accepted or rejected data on failure.
 */

bool SincArrayReadHistogram(SincArray *sa, int timeout, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}

bool SincArrayReadHistogramDatagram(SincArray *sa, int timeout, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}



/*
 * NAME:        SincArrayWaitReady
 * ACTION:      Waits for a packet indicating that the channel has returned to a
 *              ready state. Do this after a single shot oscilloscope run.
 * PARAMETERS:  SincArray *sa      - the sinc device array.
 *              int channelId - which channel to use.
 *              int timeout   - timeout in milliseconds. -1 for no timeout.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayWaitReady(SincArray *sa, int channelId, int timeout)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}



/*
 * NAME:        SincArrayStartListMode
 * ACTION:      Starts list mode.
 * PARAMETERS:  SincArray *sa      - the sinc device array.
 *              int channelId - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayStartListMode(SincArray *sa, int channelId)
{
    // Send the request.
    if (!SincArrayRequestStartListMode(sa, channelId))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



/*
 * NAME:        SincArrayStop
 * ACTION:      Stops oscilloscope / histogram / list mode / calibration. Allows skipping of the
 *              optional optimisation phase of calibration.
 * PARAMETERS:  SincArray *sa      - the sinc device array.
 *              int channelId - which channel to use. -1 for all.
 *              bool skip     - true to skip the optimisation phase of calibration but still keep
 *                              the calibration.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayStop(SincArray *sa, int channelId, bool skip)
{
    // Send the request.
    if (!SincArrayRequestStop(sa, channelId, skip))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



/*
 * NAME:        SincArrayListParamDetails
 * ACTION:      Returns a list of matching device parameters and their details.
 * PARAMETERS:  SincArray *sa                  - the channel.
 *              int channelId             - which channel to use.
 *              char *matchPrefix         - a key prefix to match. Only matching keys are returned. Empty string for all keys.
 *              SiToro__Sinc__ListParamDetailsResponse **resp - where to put the response received.
 *                  This message should be freed with si_toro__sinc__list_param_details_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayListParamDetails(SincArray *sa, int channelId, char *matchPrefix, SiToro__Sinc__ListParamDetailsResponse **resp)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}



/*
 * NAME:        SincArrayRestart
 * ACTION:      Restarts the instrument.
 * PARAMETERS:  SincArray *sa      - the sinc device array.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayRestart(SincArray *sa)
{
    // Send the request.
    if (!SincArrayRequestRestart(sa))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



/*
 * NAME:        SincArrayResetSpatialSystem
 * ACTION:      Resets the spatial system to its origin position.
 * PARAMETERS:  SincArray *sa      - the sinc device array.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayResetSpatialSystem(SincArray *sa)
{
    // Send the request.
    if (!SincArrayRequestResetSpatialSystem(sa))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



/*
 * NAME:        SincArraySoftwareUpdate
 * ACTION:      Updates the software on the device.
 * PARAMETERS:  SincArray *sa  - the sinc device array.
 *              uint8_t *appImage, int appImageLen - pointer to the binary image and its
 *                  length. NULL/0 if not to be updated.
 *              char *appChecksum - a string form seven hex digit md5 checksum of the
 *                  appImage - eg. "3ecd091".
 *              SincSoftwareUpdateFile *updateFiles, int updateFilesLen - a set of
 *                  additional files to update.
 *              int autoRestart - if true will reboot when the update is complete.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArraySoftwareUpdate(SincArray *sa, uint8_t *appImage, int appImageLen, char *appChecksum, int autoRestart)
{
    // Send the request.
    if (!SincArrayRequestSoftwareUpdate(sa, appImage, appImageLen, appChecksum, autoRestart))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



/*
 * NAME:        SincArrayProbeDatagram
 * ACTION:      Requests a probe datagram to be sent to the configured IP and port.
 *              Waits a timeout period to see if it's received and reports success.
 *              This is called by SincInitDatagramComms() but can be called
 *              separately if you wish.
 * PARAMETERS:  SincArray *sa          - the sinc device array.
 *              bool *datagramsOk      - set to true or false depending if the probe datagram was received ok.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincArrayProbeDatagram(SincArray *sa, bool *datagramsOk)
{
    // Send the request.
    if (!SincArrayRequestProbeDatagram(sa))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



#if 0
/*
 * NAME:        SincArrayInitDatagramComms
 * ACTION:      Initialises the histogram datagram communications. Creates the
 *              socket if necessary and readys the datagram comms if possible.
 *              This is called automatically by SincSendHistogram() if
 *              sc->datagramXfer is set.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincArrayInitDatagramComms(SincArray *sa)
{
    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}
#endif


/*
 * NAME:        SincArrayMonitorChannels
 * ACTION:      Tells the card which channels this connection is interested in.
 *              Asynchronous events like oscilloscope and histogram data will only
 *              be sent for monitored channels.
 * PARAMETERS:  SincArray *sa        - the sinc device array.
 *              uint32_t *channelSet - a list of the channels to monitor.
 *              int numChannels      - the number of channels in the list.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayMonitorChannels(SincArray *sa, int *channelSet, int numChannels)
{
    // Send the request.
    if (!SincArrayRequestMonitorChannels(sa, channelSet, numChannels))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
}



/*
 * NAME:        SincArrayCheckParamConsistency
 * ACTION:      Check parameters for consistency.
 * PARAMETERS:  SincArray *sa         - the sinc device array.
 *              int channelId    - which channel to check. -1 for all channels.
 *              SiToro__Sinc__CheckParamConsistencyResponse **resp - where to put the response received.
 *                  This message should be freed with si_toro__sinc__check_param_consistency_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayCheckParamConsistency(SincArray *sa, int channelId, SiToro__Sinc__CheckParamConsistencyResponse **resp)
{
#if 0
    // Send the request.
    if (!SincArrayRequestCheckParamConsistency(sa, channelId))
        return false;

    // Get the response;
    return SincArrayCheckSuccess(sa);
#endif

    SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED);
    return false;
}



/*
 * Various packet request functions which can be used stand-alone.
 * Parameters are the same as the above versions.
 */

bool SincArrayRequestPing(SincArray *sa, int showOnConsole)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodePing(&sendBuf, showOnConsole);

    // Send it to each device.
    bool success = SincArraySendToEachDevice(sa, &sendBuf);
    SINC_BUFFER_CLEAR(&sendBuf);

    return success;
}


bool SincArrayRequestGetParam(SincArray *sa, int channelId, char *name)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeGetParam(&sendBuf, -1, name);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestGetParam(&sa->devices[deviceId], deviceChannelId, name);
    }

    return success;
}


bool SincArrayRequestGetParams(SincArray *sa, int *channelIds, const char **names, int numNames)
{
    /* Allocate memory for temporary per-device lists. */
    int i;
    int j;
    bool success = true;
    int *deviceChannelIds = malloc(sizeof(int) * numNames);
    if (!deviceChannelIds)
    {
        SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    const char **deviceNames = malloc(sizeof(const char *) * numNames);
    if (!deviceNames)
    {
        free(deviceChannelIds);
        SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    /* Get parameters card by card. */
    for (i = 0; i < sa->numDevices; i++)
    {
        int deviceNumNames = 0;
        int deviceLowChannelId = i * sa->channelsPerDevice;
        int deviceHighChannelId = deviceLowChannelId + sa->channelsPerDevice;

        /* Which parameters are on this device? */
        for (j = 0; j < numNames; j++)
        {
            if (channelIds[j] >= deviceLowChannelId && channelIds[j] < deviceHighChannelId)
            {
                deviceChannelIds[deviceNumNames] = channelIds[j];
                deviceNames[deviceNumNames] = names[j];
                deviceNumNames++;
            }
        }

        /* Make the request. */
        if (deviceNumNames > 0)
        {
            Sinc *sc = &sa->devices[i];
            success = SincRequestGetParams(sc, deviceChannelIds, deviceNames, deviceNumNames);
            if (!success)
            {
                sa->err = sc->err;
            }
        }
    }

    /* Clean up. */
    free(deviceChannelIds);
    free(deviceNames);

    return success;
}


bool SincArrayRequestSetParam(SincArray *sa, SiToro__Sinc__KeyValue *param)
{
    int channelId = param->channelid;
    int deviceId;
    int deviceChannelId;

    if (!param->has_channelid)
    {
        SincArrayErrorSetMessage(sa, SI_TORO__SINC__ERROR_CODE__BAD_PARAMETERS, "missing channel id");
        return false;
    }

    SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, param->channelid);
    param->channelid = deviceChannelId;
    bool success = SincRequestSetParam(&sa->devices[deviceId], deviceChannelId, param);
    param->channelid = channelId;

    return success;
}


bool SincArrayRequestSetParams(SincArray *sa, SiToro__Sinc__KeyValue *param, int numParams)
{
    /* Allocate memory for temporary per-device lists. */
    int i;
    int j;
    bool success = true;
    SiToro__Sinc__KeyValue *deviceKvs = malloc(sizeof(SiToro__Sinc__KeyValue) * numParams);

    if (!deviceKvs)
    {
        SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    /* Set parameters card by card. */
    for (i = 0; i < sa->numDevices; i++)
    {
        int deviceNumParams = 0;
        int deviceLowChannelId = i * sa->channelsPerDevice;
        int deviceHighChannelId = deviceLowChannelId + sa->channelsPerDevice;

        /* Which parameters are on this device? */
        for (j = 0; j < numParams; j++)
        {
            if (param[j].has_channelid && param[j].channelid >= deviceLowChannelId && param[j].channelid < deviceHighChannelId)
            {
                memcpy(&deviceKvs[deviceNumParams], &param[j], sizeof(SiToro__Sinc__KeyValue));
                deviceNumParams++;
            }
        }

        /* Make the request. */
        if (deviceNumParams > 0)
        {
            Sinc *sc = &sa->devices[i];
            success = SincRequestSetParams(sc, 0, deviceKvs, deviceNumParams);
            if (!success)
            {
                sa->err = sc->err;
            }
        }
    }

    /* Clean up. */
    free(deviceKvs);

    return success;
}

bool SincArrayRequestStartCalibration(SincArray *sa, int channelId)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeStartCalibration(&sendBuf, -1);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestStartCalibration(&sa->devices[deviceId], deviceChannelId);
    }

    return success;
}


bool SincArrayRequestGetCalibration(SincArray *sa, int channelId)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeGetCalibration(&sendBuf, -1);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestGetCalibration(&sa->devices[deviceId], deviceChannelId);
    }

    return success;
}


bool SincArrayRequestSetCalibration(SincArray *sa, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeSetCalibration(&sendBuf, -1, calibData, example, model, final);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestSetCalibration(&sa->devices[deviceId], deviceChannelId, calibData, example, model, final);
    }

    return success;
}


bool SincArrayRequestCalculateDcOffset(SincArray *sa, int channelId)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeCalculateDcOffset(&sendBuf, -1);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestCalculateDcOffset(&sa->devices[deviceId], deviceChannelId);
    }

    return success;
}


bool SincArrayRequestStartOscilloscope(SincArray *sa, int channelId)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeStartOscilloscope(&sendBuf, -1);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestStartOscilloscope(&sa->devices[deviceId], deviceChannelId);
    }

    return success;
}


bool SincArrayRequestStartHistogram(SincArray *sa, int channelId)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeStartHistogram(&sendBuf, -1);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestStartHistogram(&sa->devices[deviceId], deviceChannelId);
    }

    return success;
}


bool SincArrayRequestStartFFT(SincArray *sa, int channelId)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeStartFFT(&sendBuf, -1);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestStartFFT(&sa->devices[deviceId], deviceChannelId);
    }

    return success;
}


bool SincArrayRequestStartListMode(SincArray *sa, int channelId)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeStartListMode(&sendBuf, -1);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestStartListMode(&sa->devices[deviceId], deviceChannelId);
    }

    return success;
}


bool SincArrayRequestStop(SincArray *sa, int channelId, bool skip)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeStop(&sendBuf, -1, skip);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestStop(&sa->devices[deviceId], deviceChannelId, skip);
    }

    return success;
}


bool SincArrayRequestListParamDetails(SincArray *sa, int channelId, char *matchPrefix)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeListParamDetails(&sendBuf, -1, matchPrefix);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestListParamDetails(&sa->devices[deviceId], deviceChannelId, matchPrefix);
    }

    return success;
}


bool SincArrayRequestRestart(SincArray *sa)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeRestart(&sendBuf);

    // Send it to each device.
    bool success = SincArraySendToEachDevice(sa, &sendBuf);
    SINC_BUFFER_CLEAR(&sendBuf);

    return success;
}

bool SincArrayRequestResetSpatialSystem(SincArray *sa)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeResetSpatialSystem(&sendBuf);

    // Send it to each device.
    bool success = SincArraySendToEachDevice(sa, &sendBuf);
    SINC_BUFFER_CLEAR(&sendBuf);

    return success;
}

bool SincArrayRequestSoftwareUpdate(SincArray *sa, uint8_t *appImage, int appImageLen, char *appChecksum, int autoRestart)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    if (!SincEncodeSoftwareUpdate(&sendBuf, appImage, appImageLen, appChecksum, NULL, 0, NULL, NULL, 0, autoRestart))
    {
        SincArrayErrorSetCode(sa, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    // Send it to each device.
    bool success = SincArraySendToEachDevice(sa, &sendBuf);
    SINC_BUFFER_CLEAR(&sendBuf);

    return success;
}

bool SincArrayRequestSaveConfiguration(SincArray *sa, int channelId)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeSaveConfiguration(&sendBuf);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestSaveConfiguration(&sa->devices[deviceId]);
    }

    return success;
}

bool SincArrayRequestDeleteSavedConfiguration(SincArray *sa, int channelId)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeDeleteSavedConfiguration(&sendBuf);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestDeleteSavedConfiguration(&sa->devices[deviceId]);
    }

    return success;
}


bool SincArrayRequestMonitorChannels(SincArray *sa, int *channelSet, int numChannels)
{
    int i;
    int j;
    bool success = true;
    int deviceChannelSet[sa->channelsPerDevice];

    for (i = 0; i < sa->numDevices; i++)
    {
        int dcsCount = 0;
        Sinc *sc = &sa->devices[i];
        int chanStart = sa->channelsPerDevice * i;

        for (j = 0; j < numChannels; j++)
        {
            if (channelSet[i] >= chanStart && channelSet[i] < chanStart + sa->channelsPerDevice)
            {
                deviceChannelSet[dcsCount] = channelSet[i] - chanStart;
                dcsCount++;
            }
        }

        if (!SincRequestMonitorChannels(sc, (int *)&deviceChannelSet, dcsCount))
        {
            sa->err = sc->err;
            success = false;
        }
    }

    return success;
}


bool SincArrayRequestProbeDatagram(SincArray *sa)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeProbeDatagram(&sendBuf);

    // Send it to each device.
    bool success = SincArraySendToEachDevice(sa, &sendBuf);
    SINC_BUFFER_CLEAR(&sendBuf);

    return success;
}

bool SincArrayRequestCheckParamConsistency(SincArray *sa, int channelId)
{
    bool success = true;

    if (channelId < 0)
    {
        // Request all channels.
        uint8_t pad[256];
        SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
        SincEncodeCheckParamConsistency(&sendBuf, -1);
        success = SincArraySendToEachDevice(sa, &sendBuf);
        SINC_BUFFER_CLEAR(&sendBuf);
    }
    else
    {
        // Just request a single channel.
        int deviceId;
        int deviceChannelId;
        SincArrayChannelTranslate(sa, &deviceId, &deviceChannelId, channelId);
        success = SincRequestCheckParamConsistency(&sa->devices[deviceId], deviceChannelId);
    }

    return success;
}

#if 0
/* Simplified access to SincDecodeXXX() calls from a SincArray. */
bool SincArrayDecodeSuccessResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__SuccessResponse **resp, int *fromChannelId)
{
    int deviceChannelId = 0;
    bool success = SincDecodeSuccessResponse(err, packet, resp, &deviceChannelId);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeGetParamResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId)
{
    int deviceChannelId = 0;
    bool success = SincDecodeGetParamResponse(err, packet, resp, &deviceChannelId);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeParamUpdatedResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__ParamUpdatedResponse **resp, int *fromChannelId)
{
    int deviceChannelId = 0;
    bool success = SincDecodeParamUpdatedResponse(err, packet, resp, &deviceChannelId);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeCalibrationProgressResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CalibrationProgressResponse **resp, int *fromChannelId, double *progress, int *complete, char **stage)
{
    int deviceChannelId = 0;
    bool success = SincDecodeCalibrationProgressResponse(err, packet, resp, progress, complete, stage, &deviceChannelId);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeGetCalibrationResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__GetCalibrationResponse **resp, int *fromChannelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    int deviceChannelId = 0;
    bool success = SincDecodeGetCalibrationResponse(err, packet, resp, &deviceChannelId, calibData, example, model, final);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeCalculateDCOffsetResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CalculateDcOffsetResponse **resp, int *fromChannelId, double *dcOffset)
{
    int deviceChannelId = 0;
    bool success = SincDecodeCalculateDCOffsetResponse(err, packet, resp, dcOffset, &deviceChannelId);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeListParamDetailsResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__ListParamDetailsResponse **resp, int *fromChannelId)
{
    int deviceChannelId = 0;
    bool success = SincDecodeListParamDetailsResponse(err, packet, resp, &deviceChannelId);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeOscilloscopeDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, uint64_t *dataSetId, SincOscPlot *resetBlanked, SincOscPlot *rawCurve)
{
    int deviceChannelId = 0;
    bool success = SincDecodeOscilloscopeDataResponse(err, packet, &deviceChannelId, dataSetId, resetBlanked, rawCurve);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeHistogramDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats)
{
    int deviceChannelId = 0;
    bool success = SincDecodeHistogramDataResponse(err, packet, &deviceChannelId, accepted, rejected, stats);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeHistogramDatagramResponse(SincError *err, SincBuffer *packet, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats)
{
    int deviceChannelId = 0;
    bool success = SincDecodeHistogramDatagramResponse(err, packet, &deviceChannelId, accepted, rejected, stats);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeListModeDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, uint8_t **data, int *dataLen, uint64_t *dataSetId)
{
    int deviceChannelId = 0;
    bool success = SincDecodeListModeDataResponse(err, packet, &deviceChannelId, data, dataLen, dataSetId);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeCheckParamConsistencyResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CheckParamConsistencyResponse **resp, int *fromChannelId)
{
    int deviceChannelId = 0;
    bool success = SincDecodeCheckParamConsistencyResponse(err, packet, resp, &deviceChannelId);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeAsynchronousErrorResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__AsynchronousErrorResponse **resp, int *fromChannelId)
{
    int deviceChannelId = 0;
    bool success = SincDecodeAsynchronousErrorResponse(err, packet, resp, &deviceChannelId);
    if (fromChannelId)
    {
        *fromChannelId = deviceChannelId + packet->channelIdOffset;
    }

    return success;
}


bool SincArrayDecodeSoftwareUpdateCompleteResponse(SincError *err, SincBuffer *packet)
{
    return SincDecodeSoftwareUpdateCompleteResponse(err, packet);
}
#endif


/*
 * NAME:        SincArraySendToEachDevice
 * ACTION:      Send a send buffer.
 * PARAMETERS:  SincArray *sa       - the sinc array.
 *              SincBuffer *sendBuf - the buffer to send.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincArraySendToEachDevice(SincArray *sa, SincBuffer *sendBuf)
{
    int i;
    bool success = true;

    for (i = 0; i < sa->numDevices; i++)
    {
        Sinc *sc = &sa->devices[i];
        if (sc->connected)
        {
            int errCode = SincSocketWrite(sc->fd, sendBuf->cbuf.data, (int)sendBuf->cbuf.len);
            if (errCode != 0)
            {
                SincWriteErrorSetCode(sc, (SiToro__Sinc__ErrorCode)errCode);
                sa->err = sc->err;
                success = false;
            }
        }
    }

    return success;
}


/*
 * NAME:        SincArrayChannelTranslate
 * PARAMETERS:  SincArray *sa        - the sinc array.
 *              int *deviceId        - set to which device in the array.
 *              int *deviceChannelId - set to the channel on the device.
 *              int channelId        - the channel to find in the array.
 */

void SincArrayChannelTranslate(SincArray *sa, int *deviceId, int *deviceChannelId, int channelId)
{
    if (channelId >= sa->numDevices * sa->channelsPerDevice)
    {
        *deviceId = 0;
        *deviceChannelId = channelId;
    }
    else
    {
        *deviceId = channelId / sa->channelsPerDevice;
        *deviceChannelId = channelId % sa->channelsPerDevice;
    }
}


/*
 * NAME:        writeUint64
 * ACTION:      Writes a uint64_t value to a binary save file.
 */

static void writeUint64(FILE *outFile, uint64_t v)
{
    fwrite((void *)&v, sizeof(v), 1, outFile);
}


/*
 * NAME:        SincArraySaveScan
 * ACTION:      Saves received histograms in a ".scan" file format. Continues
 *              for the given duration or the given count, whichever is shorter.
 *              The data will be appended to an existing file if the append
 *              option is set. This allows the call to be used repeatedly.
 * PARAMETERS:  SincArray *sa        - the sinc array.
 *              const char *fileName - the name of the file to save.
 *              int durationMs       - the number of millseconds to save for. -1 to not do this.
 *              int histogramCount   - the number of histograms to save. -1 to not do this.
 * RETURNS:     true on success, false otherwise. On failure use SincArrayErrorCode() and
 *                  SincArrayErrorMessage() to get the error status.
 */

bool SincArraySaveScan(SincArray *sa, const char *fileName, int durationMs, int histogramCount, bool appendMode)
{
    int i;

    // Open the file.
    FILE *outFile = fopen(fileName, appendMode ? "w+" : "w");
    if (outFile == NULL)
    {
        SincArrayErrorSetMessage(sa, SI_TORO__SINC__ERROR_CODE__WRITE_FAILED, "can't open scan file for writing");
        return false;
    }

    // Write the start header.
    fprintf(outFile, "SiToro__ScanData" "00000000");

    uint64_t numChannels = sa->numDevices * sa->channelsPerDevice;
    writeUint64(outFile, numChannels);
    writeUint64(outFile, sa->numDevices);
    for (i = 0; i < sa->numDevices; i++)
    {
        writeUint64(outFile, sa->channelsPerDevice);
    }

    // When do we have to be done by?
    uint64_t endNsec = ~(uint64_t)0;
    if (durationMs >= 0)
    {
        struct timespec startTime;
        clock_gettime(CLOCK_REALTIME, &startTime);
        uint64_t startNsec = (uint64_t)startTime.tv_sec * 1000000000 + startTime.tv_nsec;
        endNsec = startNsec + (uint64_t)durationMs * 1000000;
    }

    // Get histograms until we're done.
    int savedHistograms = 0;
    uint8_t pad[32768];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    bool done = false;
    while (!done)
    {
        // Read and handle packets.
        SiToro__Sinc__MessageType packetType;
        packet.cbuf.len = 0;
        if (!SincArrayReadMessage(sa, 100, &packet, &packetType))
        {
            if (sa->err->code != SI_TORO__SINC__ERROR_CODE__TIMEOUT)
            {
                SINC_BUFFER_CLEAR(&packet);
                fclose(outFile);
                return false;
            }
            else
            {
                // Just skip checking this non-packet.
                packetType = 0;
            }
        }

        switch (packetType)
        {
        case SI_TORO__SINC__MESSAGE_TYPE__HISTOGRAM_DATA_RESPONSE:
        case SI_TORO__SINC__MESSAGE_TYPE__HISTOGRAM_DATAGRAM_RESPONSE:
        {
            // Work out the channel id.
            int fromChannelId = 0;
            bool decodedOk = false;
            if (packetType == SI_TORO__SINC__MESSAGE_TYPE__HISTOGRAM_DATA_RESPONSE)
            {
                decodedOk = SincDecodeHistogramDataResponse(&sa->arrayErr, &packet, &fromChannelId, NULL, NULL, NULL);
            }
            else
            {
                decodedOk = SincDecodeHistogramDatagramResponse(&sa->arrayErr, &packet, &fromChannelId, NULL, NULL, NULL);
            }

            if (!decodedOk)
            {
                sa->err = &sa->arrayErr;

                SINC_BUFFER_CLEAR(&packet);
                fclose(outFile);
                return false;
            }

            // Write the record.
            struct timespec packetTime;
            clock_gettime(CLOCK_REALTIME, &packetTime);
            writeUint64(outFile, 0xfeedfacecafebabeULL);
            writeUint64(outFile, packetTime.tv_sec);
            writeUint64(outFile, packetTime.tv_nsec);
            writeUint64(outFile, fromChannelId);
            writeUint64(outFile, 0);
            writeUint64(outFile, packet.cbuf.len);
            fwrite(packet.cbuf.data, 1, packet.cbuf.len, outFile);
            savedHistograms++;
            break;
        }
        case SI_TORO__SINC__MESSAGE_TYPE__ASYNCHRONOUS_ERROR_RESPONSE:
            if (!SincDecodeAsynchronousErrorResponse(&sa->arrayErr, &packet, NULL, NULL))
            {
                SincArrayErrorSetMessage(sa, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "can't decode asynchronous error");

                SINC_BUFFER_CLEAR(&packet);
                fclose(outFile);
                return false;
            }

            sa->err = &sa->arrayErr;

            SINC_BUFFER_CLEAR(&packet);
            fclose(outFile);
            return false;

        default:
            // It's a packet we don't care about - ignore it.
            break;
        }

        // Are we out of time?
        if (durationMs >= 0)
        {
            struct timespec nowTime;
            clock_gettime(CLOCK_REALTIME, &nowTime);
            uint64_t nowNsec = (uint64_t)nowTime.tv_sec * 1000000000 + nowTime.tv_nsec;
            if (nowNsec > endNsec)
            {
                done = true;
            }
        }

        // Have we received enough histograms?
        if (histogramCount >= 0 && savedHistograms >= histogramCount)
        {
            done = true;
        }

        // Free the packet buffer.
        SINC_BUFFER_CLEAR(&packet);
    }

    fclose(outFile);
    return true;
}


/*
 * NAME:        SincArraySetMasterSynchronisation
 * ACTION:      Sets one of the devices in the array as a master.
 * PARAMETERS:  SincArray *sa - the sinc array.
 *              int masterId  - which board to make master.
 * RETURNS:     true on success, false otherwise. On failure use SincArrayErrorCode() and
 *                  SincArrayErrorMessage() to get the error status.
 */

bool SincArraySetMasterSynchronisation(SincArray *sa, int masterId, int clocksPerCount, int countsPerGate, bool useMasterClock)
{
    int i;

    // Set the clock parameters once per board.
    SiToro__Sinc__KeyValue kvs[4 * sa->numDevices];
    for (i = 0; i < sa->numDevices; i++)
    {
        int instrumentChannelId = i * sa->channelsPerDevice;

        // Set "clocks per count".
        SiToro__Sinc__KeyValue *cpcKv = &kvs[i*4];
        si_toro__sinc__key_value__init(cpcKv);
        cpcKv->key = "triggerClock.clocksPerCount";
        cpcKv->has_intval = true;
        cpcKv->intval = clocksPerCount;
        cpcKv->has_paramtype = true;
        cpcKv->paramtype = SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE;
        cpcKv->has_channelid = true;
        cpcKv->channelid = instrumentChannelId;

        // Set "counts per gate".
        SiToro__Sinc__KeyValue *cpgKv = &kvs[i*4+1];
        si_toro__sinc__key_value__init(cpgKv);
        cpgKv->key = "triggerClock.countsPerGate";
        cpgKv->has_intval = true;
        cpgKv->intval = countsPerGate;
        cpgKv->has_paramtype = true;
        cpgKv->paramtype = SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE;
        cpgKv->has_channelid = true;
        cpgKv->channelid = instrumentChannelId;

        // Set "master".
        SiToro__Sinc__KeyValue *mKv = &kvs[i*4+2];
        si_toro__sinc__key_value__init(mKv);
        mKv->key = "triggerClock.master";
        mKv->has_boolval = true;
        mKv->boolval = false;
        mKv->has_paramtype = true;
        mKv->paramtype = SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__BOOL_TYPE;
        mKv->has_channelid = true;
        mKv->channelid = instrumentChannelId;

        // Set "use internal".
        SiToro__Sinc__KeyValue *uiKv = &kvs[i*4+3];
        si_toro__sinc__key_value__init(uiKv);
        uiKv->key = "triggerClock.countsPerGate";
        uiKv->has_boolval = true;
        uiKv->boolval = !useMasterClock;
        uiKv->has_paramtype = true;
        uiKv->paramtype = SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__BOOL_TYPE;
        uiKv->has_channelid = true;
        uiKv->channelid = instrumentChannelId;
    }

    if (!SincArraySetParams(sa, &kvs[0], 4 * sa->numDevices))
        return false;

    if (useMasterClock)
    {
        // Set the actual master.
        SiToro__Sinc__KeyValue mKv;
        si_toro__sinc__key_value__init(&mKv);
        mKv.key = "triggerClock.master";
        mKv.has_boolval = true;
        mKv.boolval = true;
        mKv.has_paramtype = true;
        mKv.paramtype = SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__BOOL_TYPE;
        mKv.has_channelid = true;
        mKv.channelid = masterId * sa->channelsPerDevice;

        if (!SincArraySetParam(sa, &mKv))
            return false;
    }

    return true;
}


/*
 * NAME:        SincArrayErrorSetMessage / SincArrayErrorSetCode
 * ACTION:      Sets an error state.
 */

void SincArrayErrorSetMessage(SincArray *sa, SiToro__Sinc__ErrorCode code, const char *msg)
{
    SincErrorSetMessage(&sa->arrayErr, code, msg);
    sa->err = &sa->arrayErr;
}

void SincArrayErrorSetCode(SincArray *sa, SiToro__Sinc__ErrorCode code)
{
    SincErrorSetCode(&sa->arrayErr, code);
    sa->err = &sa->arrayErr;
}


/*
 * NAME:        SincArrayCurrentErrorCode
 * ACTION:      Get the most recent error code. SincArrayCurrentErrorCode() gets the most
 *              recent error code.
 * PARAMETERS:  SincArray *sa - the sinc connection.
 */

SiToro__Sinc__ErrorCode SincArrayErrorCode(SincArray *sa)
{
    if (sa->err == NULL)
        return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
    else
        return sa->err->code;
}


/*
 * NAME:        SincArrayCurrentErrorMessage
 * ACTION:      Get the most recent error message in alphanumeric form. SincArrayCurrentErrorMessage()
 *              gets the most recent error code.
 * PARAMETERS:  SincArray *sa - the sinc connection.
 */

const char *SincArrayErrorMessage(SincArray *sa)
{
    if (sa->err == NULL)
        return "";
    else
        return sa->err->msg;
}
