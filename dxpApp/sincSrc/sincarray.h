/********************************************************************
 ***                                                              ***
 ***                     SINC array header                        ***
 ***                                                              ***
 ********************************************************************/

/*
 * SINC array allow all of the features of an array of Hydra cards
 * to be accessed via the network.
 */

#ifndef SINC_ARRAY_H
#define SINC_ARRAY_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include "sinc.h"


// A channel of communication to a device.
typedef struct
{
    int        numDevices;           // The number of devices in the array.
    Sinc      *devices;              // The devices.
    uint64_t   waitBits;             // Which devices to wait for.
    int        channelsPerDevice;    // The number of channels in each device.

    int        timeout;              // How long in milliseconds to wait for a response. -1 for forever. User settable.

    SincError *err;                  // The most recent error.
    SincError  arrayErr;             // An error originating from the array itself.
} SincArray;


/*
 * NAME:        SincArrayInit
 * ACTION:      Initialises a Sinc channel.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayInit(SincArray *sa);


/*
 * NAME:        SincArrayCleanup
 * ACTION:      Closes and frees data used by the device array (but doesn't free the SincArray structure itself).
 * PARAMETERS:  SincArray *sa - the sinc device array to clean up.
 */

void SincArrayCleanup(SincArray *sa);


/*
 * NAME:        SincArraySetTimeout
 * ACTION:      Sets a timeout for following commands.
 * PARAMETERS:  int timeout - timeout in milliseconds. -1 for no timeout.
 */

void SincArraySetTimeout(SincArray *sa, int timeout);


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

bool SincArrayConnect(SincArray *sa, const char **hosts, int numHosts, int channelsPerDevice);
bool SincArrayConnectPort(SincArray *sa, const char **hosts, int *ports, int numHosts, int channelsPerDevice);


/*
 * NAME:        SincArrayDisconnect
 * ACTION:      Disconnects a Sinc channel from whatever it's connected to.
 * PARAMETERS:  SincArray *sa  - the sinc device array to disconnect.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayDisconnect(SincArray *sa);


/*
 * NAME:        SincArrayReadMessage
 * ACTION:      Reads or polls for the next message. The received packet can then be decoded
 *              with one of the SincDecodeXXX() calls.
 * PARAMETERS:  SincArray *sa                         - the sinc device array.
 *              int timeout                           - in milliseconds. 0 to poll. -1 to wait forever.
 *              SincBuffer *packetBuf            - the received packet data is placed here.
 *              SiToro__Sinc__MessageType *packetType - the found packet type is placed here.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayReadMessage(SincArray *sa, int timeout, SincBuffer *packetBuf, SiToro__Sinc__MessageType *packetType);


/*
 * NAME:        SincArrayPing
 * ACTION:      Checks if the device is responding.
 *              SincRequestPing() variant sends the request but doesn't wait for a reply.
 * PARAMETERS:  SincArray *sa  - the sinc device array to ping.
 *              int showOnConsole - not used, set to 0.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayPing(SincArray *sa, int showOnConsole);
bool SincArrayRequestPing(SincArray *sa, int showOnConsole);


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

bool SincArrayGetParam(SincArray *sa, int channelId, char *name, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId);
bool SincArrayGetParams(SincArray *sa, int *channelIds, const char **names, int numNames, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId);


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

bool SincArraySetParam(SincArray *sa, SiToro__Sinc__KeyValue *param);
bool SincArraySetParams(SincArray *sa, SiToro__Sinc__KeyValue *params, int numParams);


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

bool SincArrayCalibrate(SincArray *sa, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);


/*
 * NAME:        SincArrayStartCalibration
 * ACTION:      Starts a calibration. Use SincArrayWaitCalibrationComplete() to wait for
 *              completion.
 * PARAMETERS:  SincArray *sa                            - the sinc device array to request from.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayStartCalibration(SincArray *sa, int channelId);


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

bool SincArrayWaitCalibrationComplete(SincArray *sa, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);


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

bool SincArrayGetCalibration(SincArray *sa, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);


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

bool SincArraySetCalibration(SincArray *sa, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);


/*
 * NAME:        SincArrayCalculateDcOffset
 * ACTION:      Calculates the DC offset on the device. May take a couple of seconds.
 * PARAMETERS:  SincArray *sa         - the the sinc device array to request from.
 *              int channelId    - which channel to use.
 *              double *dcOffset - the calculated DC offset is placed here.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayCalculateDcOffset(SincArray *sa, int channelId, double *dcOffset);


/*
 * NAME:        SincArrayStartOscilloscope
 * ACTION:      Starts the oscilloscope.
 * PARAMETERS:  SincArray *sa            - the sinc device array.
 *              int channelId       - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayStartOscilloscope(SincArray *sa, int channelId);


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

bool SincArrayReadOscilloscope(SincArray *sa, int timeout, int *fromChannelId, uint64_t *dataSetId, SincOscPlot *resetBlanked, SincOscPlot *rawCurve);


/*
 * NAME:        SincArrayStartHistogram
 * ACTION:      Starts the histogram. Note that if you want to use TCP only you should set
 *              sc->datagramXfer to false. Otherwise UDP will be used.
 * PARAMETERS:  SincArray *sa               - the sinc device array.
 *              int channelId          - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayStartHistogram(SincArray *sa, int channelId);


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

bool SincArrayReadHistogram(SincArray *sa, int timeout, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats);
bool SincArrayReadHistogramDatagram(SincArray *sa, int timeout, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats);


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

bool SincArrayWaitReady(SincArray *sa, int channelId, int timeout);


/*
 * NAME:        SincArrayStartListMode
 * ACTION:      Starts list mode.
 * PARAMETERS:  SincArray *sa      - the sinc device array.
 *              int channelId - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayStartListMode(SincArray *sa, int channelId);


/*
 * NAME:        SincArrayStop
 * ACTION:      Stops oscilloscope / histogram / list mode / calibration. Allows skipping of the
 *              optional optimisation phase of calibration.
 * PARAMETERS:  SincArray *sa      - the sinc device array.
 *              int channelId - which channel to use.
 *              bool skip     - true to skip the optimisation phase of calibration but still keep
 *                              the calibration.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayStop(SincArray *sa, int channelId, bool skip);


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

bool SincArrayListParamDetails(SincArray *sa, int channelId, char *matchPrefix, SiToro__Sinc__ListParamDetailsResponse **resp);


/*
 * NAME:        SincArrayRestart
 * ACTION:      Restarts the instrument.
 * PARAMETERS:  SincArray *sa      - the sinc device array.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayRestart(SincArray *sa);


/*
 * NAME:        SincArrayResetSpatialSystem
 * ACTION:      Resets the spatial system to its origin position.
 * PARAMETERS:  SincArray *sa      - the sinc device array.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayResetSpatialSystem(SincArray *sa);


/*
 * NAME:        SincArraySoftwareUpdate
 * ACTION:      Updates the software on the device.
 * PARAMETERS:  SincArray *sa  - the sinc device array.
 *              uint8_t *appImage, int appImageLen - pointer to the binary image and its
 *                  length. NULL/0 if not to be updated.
 *              char *appChecksum - a string form seven hex digit md5 checksum of the
 *                  appImage - eg. "3ecd091".
 *              uint8_t *fpgaImage, int fpgaImageLen - pointer to the binary image of the
 *                  FPGA firmware and its length. NULL/0 if not to be updated.
 *              char *fpgaChecksum - a string form eight hex digit md5 checksum of the
 *                  appImage - eg. "54166011".
 *              SincSoftwareUpdateFile *updateFiles, int updateFilesLen - a set of
 *                  additional files to update.
 *              int autoRestart - if true will reboot when the update is complete.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArraySoftwareUpdate(SincArray *sa, uint8_t *appImage, int appImageLen, char *appChecksum, int autoRestart);


/*
 * NAME:        SincArrayProbeDatagram
 * ACTION:      Requests a probe datagram to be sent to the configured IP and port.
 *              Waits a timeout period to see if it's received and reports success.
 *              This is called by SincInitDatagramComms() but can be called
 *              separately if you wish.
 * PARAMETERS:  SincArray *sa               - the sinc device array.
 *              bool *datagramsOk      - set to true or false depending if the probe datagram was received ok.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincArrayProbeDatagram(SincArray *sa, bool *datagramsOk);


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

bool SincArrayInitDatagramComms(SincArray *sa);
#endif


/*
 * NAME:        SincArrayMonitorChannels
 * ACTION:      Tells the card which channels this connection is interested in.
 *              Asynchronous events like oscilloscope and histogram data will only
 *              be sent for monitored channels.
 * PARAMETERS:  SincArray *sa        - the sinc device array.
 *              int *channelSet      - a list of the channels to monitor.
 *              int numChannels      - the number of channels in the list.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayMonitorChannels(SincArray *sa, int *channelSet, int numChannels);


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

bool SincArrayCheckParamConsistency(SincArray *sa, int channelId, SiToro__Sinc__CheckParamConsistencyResponse **resp);


/*
 * NAME:        SincArrayIsConnected
 * ACTION:      Returns the connected/disconnected state of the channel.
 * PARAMETERS:  SincArray *sa  - the sinc device array.
 * RETURNS:     bool - true if connected, false otherwise.
 */

bool SincArrayIsConnected(SincArray *sa);


/*
 * NAME:        SincArrayReadMessage
 * ACTION:      Reads the next message. This may block waiting for a message to be received.
 *              The resulting packet can be handed directly to the appropriate SincDecodeXXX()
 *              function.
 *              Use this function to read the next message from the input stream. If you want to
 *              read from a buffer use SincGetNextPacketFromBuffer() instead.
 * PARAMETERS:  SincArray *sa   - the sinc device array.
 *              int timeout     - in milliseconds. 0 to poll. -1 to wait forever.
 *              SincBuffer *buf - where to put the message.
 *              SiToro__Sinc__MessageType *msgType - which type of message we got.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincArrayReadMessage(SincArray *sa, int timeout, SincBuffer *buf, SiToro__Sinc__MessageType *msgType);


/*
 * Various packet request functions which can be used stand-alone.
 * Parameters are the same as the above versions.
 */

bool SincArrayRequestPing(SincArray *sa, int showOnConsole);
bool SincArrayRequestGetParam(SincArray *sa, int channelId, char *name);
bool SincArrayRequestGetParams(SincArray *sa, int *channelIds, const char **names, int numNames);
bool SincArrayRequestSetParam(SincArray *sa, SiToro__Sinc__KeyValue *param);
bool SincArrayRequestSetParams(SincArray *sa, SiToro__Sinc__KeyValue *param, int numParams);
bool SincArrayRequestStartCalibration(SincArray *sa, int channelId);
bool SincArrayRequestGetCalibration(SincArray *sa, int channelId);
bool SincArrayRequestSetCalibration(SincArray *sa, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);
bool SincArrayRequestCalculateDcOffset(SincArray *sa, int channelId);
bool SincArrayRequestStartOscilloscope(SincArray *sa, int channelId);
bool SincArrayRequestStartHistogram(SincArray *sa, int channelId);
bool SincArrayRequestStartFFT(SincArray *sa, int channelId);
bool SincArrayRequestClearHistogramData(SincArray *sa, int channelId);
bool SincArrayRequestStartListMode(SincArray *sa, int channelId);
bool SincArrayRequestStopDataAcquisition(SincArray *sa, int channelId); // Deprecated - use SincRequestStop() instead.
bool SincArrayRequestStop(SincArray *sa, int channelId, bool skip);
bool SincArrayRequestListParamDetails(SincArray *sa, int channelId, char *matchPrefix);
bool SincArrayRequestRestart(SincArray *sa);
bool SincArrayRequestResetSpatialSystem(SincArray *sa);
bool SincArrayRequestSoftwareUpdate(SincArray *sa, uint8_t *appImage, int appImageLen, char *appChecksum, int autoRestart);
bool SincArrayRequestSaveConfiguration(SincArray *sa, int channelId);
bool SincArrayRequestDeleteSavedConfiguration(SincArray *sa, int channelId);
bool SincArrayRequestMonitorChannels(SincArray *sa, int *channelSet, int numChannels);
bool SincArrayRequestProbeDatagram(SincArray *sa);
bool SincArrayRequestCheckParamConsistency(SincArray *sa, int channelId);


#if 0
/*
 * Various packet decoding functions. Use these on packets returned from SincGetNextPacketFromBuffer().
 * Parameters are similar to the SincXXX() calls.
 * Destination parameters can be set to NULL if the given value isn't required.
 * Errors returned in packets will cause the Sinc error codes to be set and false returned.
 *
 * Note that the channel ids of parameters returned by SincDecodeGetParamResponse() are
 * included with each returned parameter.
 *
 * It's not necessary to use the dynamically allocated SiToro__Sinc__XXXResponse response.
 * If unused set this to NULL and it will be ignored. If it is used it must be free with
 * the corresponding si_toro__sinc__xxx_response__free_unpacked() auto-generated call
 * in sinc.pc-c.h.
 */

bool SincArrayDecodeSuccessResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__SuccessResponse **resp, int *fromChannelId);
bool SincArrayDecodeGetParamResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId);
bool SincArrayDecodeParamUpdatedResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__ParamUpdatedResponse **resp, int *fromChannelId);
bool SincArrayDecodeCalibrationProgressResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CalibrationProgressResponse **resp, int *fromChannelId, double *progress, int *complete, char **stage);
bool SincArrayDecodeGetCalibrationResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__GetCalibrationResponse **resp, int *fromChannelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);
bool SincArrayDecodeCalculateDCOffsetResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CalculateDcOffsetResponse **resp, int *fromChannelId, double *dcOffset);
bool SincArrayDecodeListParamDetailsResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__ListParamDetailsResponse **resp, int *fromChannelId);
bool SincArrayDecodeOscilloscopeDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, uint64_t *dataSetId, SincOscPlot *resetBlanked, SincOscPlot *rawCurve);
bool SincArrayDecodeHistogramDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats);
bool SincArrayDecodeHistogramDatagramResponse(SincError *err, SincBuffer *packet, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats);
bool SincArrayDecodeListModeDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, uint8_t **data, int *dataLen, uint64_t *dataSetId);
bool SincArrayDecodeMonitorChannelsCommand(SincError *err, SincBuffer *packet, uint64_t *channelBitSet);
bool SincArrayDecodeCheckParamConsistencyResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CheckParamConsistencyResponse **resp, int *fromChannelId);
bool SincArrayDecodeAsynchronousErrorResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__AsynchronousErrorResponse **resp, int *fromChannelId);
bool SincArrayDecodeSoftwareUpdateCompleteResponse(SincError *err, SincBuffer *packet);
#endif

/*
 * NAME:        SincArrayCurrentErrorCode
 * ACTION:      Get the most recent error code. SincArrayErrorCode() gets the most
 *              recent error code.
 * PARAMETERS:  SincArray *sa - the sinc device array.
 */

SiToro__Sinc__ErrorCode SincArrayErrorCode(SincArray *sa);


/*
 * NAME:        SincCurrentErrorMessage
 * ACTION:      Get the most recent error message in alphanumeric form. SincArrayErrorMessage()
 *              gets the most recent error code.
 * PARAMETERS:  SincArray *sa - the sinc device array.
 */

const char *SincArrayErrorMessage(SincArray *sa);

#ifdef __cplusplus
}
#endif

#endif /* SINC_ARRAY_H */
