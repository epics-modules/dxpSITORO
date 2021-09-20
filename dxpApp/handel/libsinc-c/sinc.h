/********************************************************************
 ***                                                              ***
 ***                       libsinc header                         ***
 ***                                                              ***
 ********************************************************************/

/*
 * libsinc allows all of the features of the Hydra card to be
 * accessed via the network.
 */

#ifndef SINC_H
#define SINC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <time.h>
#include "sinc.pb-c.h"

#ifndef _WIN32
#include <sys/time.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif


// Connection defaults.
#define SINC_PORT 8756


typedef SiToro__Sinc__ListParamDetailsResponse SincListParamDetailsResponse;
typedef SiToro__Sinc__ParamDetails SincParamDetails;


// The maximum length of an error message, or the message is truncated.
#define SINC_ERROR_SIZE_MAX 255

// The maximum number of intensity values.
#define MAX_INTENSITY 255


// Error information structure.
typedef struct
{
    SiToro__Sinc__ErrorCode code;                           // The most recent error code from SiToro__Sinc__ErrorCode.
    char                    msg[SINC_ERROR_SIZE_MAX+1];     // The most recent error message string.
} SincError;


// A buffer for an incoming packet from the array.
typedef struct
{
    ProtobufCBufferSimple cbuf;             // The packet buffer.
    int                   deviceId;         // Which array device this came from. Used only in SINC arrays.
    int                   channelIdOffset;  // What channel id offset to apply to the decoded data. Used only in SINC arrays.
} SincBuffer;


/*
 * Used to initialise or free storage from a buffer.
 *
 * To use these functions do:
 *   uint8_t pad[256];
 *   SincBuffer buf = SINC_BUFFER_INIT(pad);
 *   SincEncodeXXX(&buf, ...);
 *   int success = SincSend(sinc, &buf);
 */

#define SINC_BUFFER_INIT(x)  { PROTOBUF_C_BUFFER_SIMPLE_INIT(x), 0, 0 }
#define SINC_BUFFER_CLEAR(x) PROTOBUF_C_BUFFER_SIMPLE_CLEAR(&(x)->cbuf)



// A channel of communication to a device.
typedef struct
{
    int        fd;               // The socket file descriptor.
    bool       connected;        // Non zero if connected.
    int        timeout;          // How long in milliseconds to wait for a response. -1 for forever. User settable.
    bool       datagramXfer;     // Set false to disable use of datagrams in histogram transfer. Default false. User settable.
    int        datagramFd;       // The socket used for datagram reception.
    int        datagramPort;     // The socket port used for datagram reception.
    bool       datagramIsOpen;   // Datagram communications are open and working.
    bool       inSocketWait;     // Connection is currently waiting on the socket.
    SincBuffer readBuf;          // The read data buffer.
    SincError *err;              // The most recent error.
    SincError  readErr;          // The most recent read error.
    SincError  writeErr;         // The most recent write error.
} Sinc;


// Data from the calibration system.
typedef struct
{
    int          len;
    uint8_t	    *data;
} SincCalibrationData;


// A plot of a calibration pulse.
typedef struct
{
    int          len;
    double      *x;
    double      *y;
} SincCalibrationPlot;


// Oscilloscope data.
// Note that the oscilloscope data is decimated by a factor of two by default.
typedef struct
{
    int          len;
    double      *data;      // Normalised FP version of the data. This data may be decimated.
    int32_t     *intData;   // int version of the data. This data may be decimated.
    int32_t      minRange;  // Range of the integer values.
    int32_t      maxRange;
} SincOscPlot;


// Histogram data.
typedef struct
{
    int          len;
    uint32_t *   data;
} SincHistogram;


// Histogram count statistics.
typedef struct
{
    uint64_t                       dataSetId;
    double                         timeElapsed;
    uint64_t                       samplesDetected;
    uint64_t                       samplesErased;
    uint64_t                       pulsesAccepted;
    uint64_t                       pulsesRejected;
    double                         inputCountRate;
    double                         outputCountRate;
    double                         deadTime;
    int                            gateState;
    uint32_t                       spectrumSelectionMask;
    uint32_t                       subRegionStartIndex;
    uint32_t                       subRegionEndIndex;
    uint32_t                       refreshRate;
    uint32_t                       positiveRailHitCount;
    uint32_t                       negativeRailHitCount;
    SiToro__Sinc__HistogramTrigger trigger;
    uint32_t                       numIntensity;
    uint32_t                       *intensityData;
} SincHistogramCountStats;


// Software update entry.
typedef struct
{
    char *   fileName;
    uint8_t *content;
    int      contentLen;
} SincSoftwareUpdateFile;



/*
 * NAME:        SincInit
 * ACTION:      Initialises a Sinc channel.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincInit(Sinc *sc);


/*
 * NAME:        SincCleanup
 * ACTION:      Closes and frees data used by the channel (but doesn't free the channel structure itself).
 * PARAMETERS:  Sinc *sc  - the sinc connection to clean up.
 */

void SincCleanup(Sinc *sc);


/*
 * NAME:        SincSetTimeout
 * ACTION:      Sets a timeout for following commands.
 * PARAMETERS:  int timeout - timeout in milliseconds. -1 for no timeout.
 */

void SincSetTimeout(Sinc *sc, int timeout);


/*
 * NAME:        SincConnect
 * ACTION:      Connects a Sinc channel to a device on a given host and port.
 * PARAMETERS:  Sinc *sc  - the sinc connection to connect.
 *              const char *host - the host to connect to.
 *              int port         - the port to connect to.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincConnect(Sinc *sc, const char *host, int port);


/*
 * NAME:        SincDisconnect
 * ACTION:      Disconnects a Sinc channel from whatever it's connected to.
 * PARAMETERS:  Sinc *sc  - the sinc connection to disconnect.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincDisconnect(Sinc *sc);


/*
 * NAME:        SincPacketPeek
 * ACTION:      Finds the packet type of the next packet.
 * PARAMETERS:  Sinc *sc  - the sinc connection.
 *              int timeout - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__MessageType *packetType - the found packet type is placed here.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincPacketPeek(Sinc *sc, int timeout, SiToro__Sinc__MessageType *packetType);


/*
 * NAME:        SincPacketPeekMulti
 * ACTION:      Finds the packet type of the next packet.
 * PARAMETERS:  Sinc **channelSet - an array of pointers to channels.
 *              int numChannels - the number of channels in the array.
 *              int timeout - in milliseconds. 0 to poll. -1 to wait forever.
 *              SiToro__Sinc__MessageType *packetType - the found packet type is placed here.
 *              int *packetChannel - set to the channel that a packet was found on.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincPacketPeekMulti(Sinc **channelSet, int numChannels, int timeout, SiToro__Sinc__MessageType *packetType, int *packetChannel);


/*
 * Calls to read various types of packet after identifying the type with SyncPacketPeek().
 * The responses should be free()d after use using:
 *      si_toro__sinc__success_response__free_unpacked(resp, NULL);
 * and other calls in this family.
 *
 * Destination parameters can be set to NULL if the given value isn't required.
 * Errors returned in packets will cause the Sinc error codes to be set and false returned.
 *
 * Note that the channel ids of parameters returned by SincReadGetParamResponse() are
 * included with each returned parameter.
 *
 * It's not necessary to use the dynamically allocated SiToro__Sinc__XXXResponse response.
 * If unused set this to NULL and it will be ignored. If it is used it must be free with
 * the corresponding si_toro__sinc__xxx_response__free_unpacked() auto-generated call
 * in sinc.pc-c.h.
 */

bool SincReadSuccessResponse(Sinc *sc, int timeout, SiToro__Sinc__SuccessResponse **resp, int *fromChannelId);
bool SincReadGetParamResponse(Sinc *sc, int timeout, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId);
bool SincReadParamUpdatedResponse(Sinc *sc, int timeout, SiToro__Sinc__ParamUpdatedResponse **resp, int *fromChannelId);
bool SincReadCalibrationProgressResponse(Sinc *sc, int timeout, SiToro__Sinc__CalibrationProgressResponse **resp, double *progress, int *complete, char **stage, int *fromChannelId);
bool SincReadGetCalibrationResponse(Sinc *sc, int timeout, SiToro__Sinc__GetCalibrationResponse **resp, int *fromChannelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);
bool SincReadCalculateDCOffsetResponse(Sinc *sc, int timeout, SiToro__Sinc__CalculateDcOffsetResponse **resp, double *dcOffset, int *fromChannelId);
bool SincReadListParamDetailsResponse(Sinc *sc, int timeout, SiToro__Sinc__ListParamDetailsResponse **resp, int *fromChannelId);
bool SincReadCheckParamConsistencyResponse(Sinc *sc, int timeout, SiToro__Sinc__CheckParamConsistencyResponse **resp, int *fromChannelId);
bool SincReadDownloadCrashDumpResponse(Sinc *sc, int timeout, bool *newDump, uint8_t **dumpData, size_t *dumpSize);
bool SincReadAsynchronousErrorResponse(Sinc *sc, int timeout, SiToro__Sinc__AsynchronousErrorResponse **resp, int *fromChannelId);
bool SincReadAndDiscardPacket(Sinc *sc, int timeout);
bool SincReadSynchronizeLogResponse(Sinc *sc, int timeout, SiToro__Sinc__SynchronizeLogResponse **resp);


/*
 * NAME:        SincPing
 * ACTION:      Checks if the device is responding.
 *              SincRequestPing() variant sends the request but doesn't wait for a reply.
 * PARAMETERS:  Sinc *sc  - the sinc connection to ping.
 *              int showOnConsole - not used, set to 0.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincPing(Sinc *sc, int showOnConsole);
bool SincRequestPing(Sinc *sc, int showOnConsole);


/*
 * NAME:        SincGetParam
 * ACTION:      Gets a named parameter from the device.
 *              Note that the channel ids of parameters are included with each returned parameter.
 * PARAMETERS:  Sinc *sc                        - the channel to request from.
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

bool SincGetParam(Sinc *sc, int channelId, char *name, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId);
bool SincGetParams(Sinc *sc, int *channelIds, const char **names, int numNames, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId);


/*
 * NAME:        SincSetParam
 * ACTION:      Sets a named parameter on the device.
 * PARAMETERS:  Sinc *sc                            - the channel to request from.
 *              int channelId                       - which channel to use.
 *              SiToro__Sinc__KeyValue *param       - the key and value to set.
 *                  Set the key in param->key.
 *                  Set the value type in param->valueCase and the value in one of intval,
 *                  floatval, boolval, strval or optionval.
 *
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincSetParam(Sinc *sc, int channelId, SiToro__Sinc__KeyValue *param);
bool SincSetParams(Sinc *sc, int channelId, SiToro__Sinc__KeyValue *params, int numParams);


/*
 * NAME:        SincSetAllParams
 * ACTION:      Sets all of the parameters on the device. If any parameters on the
 *              device aren't set by this command they'll automatically be set to
 *              sensible defaults. This is useful when loading a project file which
 *              is intended to set all the values in a single lot. It ensures that
 *              the device's parameters are upgraded automatically along with any
 *              firmware upgrades.
 *
 *              If a set of saved device parameters are loaded after a firmware
 *              update using SincSetParams() there may be faulty behavior. This
 *              Is due to new parameters not being set as they're not defined in
 *              the set of saved parameters. Using this call instead of
 *              SincSetParams() when loading a complete device state ensures that
 *              the device's parameters are upgraded automatically along with any
 *              firmware upgrades.
 * PARAMETERS:  Sinc *sc                            - the channel to request from.
 *              int channelId                       - which channel to use.
 *              SiToro__Sinc__KeyValue *param       - the key and value to set.
 *                  Set the key in param->key.
 *                  Set the value type in param->valueCase and the value in one of intval,
 *                  floatval, boolval, strval or optionval.
 *              const char *fromFirmwareVersion     - the instrument.firmwareVersion
 *                                                    of the saved parameters being
 *                                                    restored.
 *
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincSetAllParams(Sinc *sc, int channelId, SiToro__Sinc__KeyValue *params, int numParams, const char *fromFirmwareVersion);


/*
 * NAME:        SincCalibrate
 * ACTION:      Performs a calibration and returns calibration data. May take several seconds.
 * PARAMETERS:  Sinc *sc                            - the channel to request from.
 *              int channelId                       - which channel to use.
 *              SincCalibrationData *calibData      - the calibration data.
 *              SincPlot *example, model, final     - the pulse shapes.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincCalibrate(Sinc *sc, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);


/*
 * NAME:        SincStartCalibration
 * ACTION:      Requests a calibration but doesn't wait for it to complete. Use SincCalibrate()
 *              instead to wait for calibration to complete or use SincWaitCalibrationComplete()
 *              with this call.
 * PARAMETERS:  Sinc *sc                        - the channel to request from.
 *              int channelId                   - which channel to use. -1 to use the default channel for this port.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincStartCalibration(Sinc *sc, int channelId);


/*
 * NAME:        SincWaitCalibrationComplete
 * ACTION:      Waits for calibration to be complete. Use with SincRequestCalibration().
 * PARAMETERS:  Sinc *sc                            - the channel to request from.
 *              int channelId                       - which channel to use.
 *              SincCalibrationData *calibData      - the calibration data.
 *              SincPlot *example, model, final     - the pulse shapes.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincWaitCalibrationComplete(Sinc *sc, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);


/*
 * NAME:        SincGetCalibration
 * ACTION:      Gets the calibration data from a previous calibration.
 * PARAMETERS:  Sinc *sc                        - the channel to request from.
 *              int channelId                   - which channel to use.
 *              SincCalibrationData *calibData  - the calibration data is stored here.
 *              calibData must be free()d after use.
 *              SincPlot *example, model, final - the pulse shapes are set here.
 *              The fields x and y of each pulse must be free()d after use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincGetCalibration(Sinc *sc, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);


/*
 * NAME:        SincSetCalibration
 * ACTION:      Sets the calibration data on the device from a previously acquired data set.
 * PARAMETERS:  Sinc *sc                            - the channel to request from.
 *              int channelId                       - which channel to use.
 *              SincCalibrationData *calibData      - the calibration data to set.
 *              SincPlot *example, model, final     - the pulse shapes.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincSetCalibration(Sinc *sc, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);


/*
 * NAME:        SincSFreeCalibration
 * ACTION:      Free the calibration waveforms.
 * PARAMETERS:  SincCalibrationData *calibData      - the calibration data to free.
 *              SincPlot *example, model, final     - the pulse shapes.
 */

void SincSFreeCalibration(SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);


/*
 * NAME:        SincCalculateDcOffset
 * ACTION:      Calculates the DC offset on the device. May take a couple of seconds.
 * PARAMETERS:  Sinc *sc         - the sinc connection to request from.
 *              int channelId    - which channel to use.
 *              double *dcOffset - the calculated DC offset is placed here.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincCalculateDcOffset(Sinc *sc, int channelId, double *dcOffset);


/*
 * NAME:        SincStartOscilloscope
 * ACTION:      Starts the oscilloscope.
 * PARAMETERS:  Sinc *sc            - the sinc connection.
 *              int channelId       - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincStartOscilloscope(Sinc *sc, int channelId);


/*
 * NAME:        SincReadOscilloscope
 * ACTION:      Gets a curve from the oscilloscope. Waits for the next oscilloscope update to
 *              arrive if timeout is non-zero.
 * PARAMETERS:  Sinc *sc                  - the sinc connection.
 *              int timeout               - in milliseconds. 0 to poll. -1 to wait forever.
 *              int *fromChannelId        - if non-NULL this is set to the channel the histogram was received from.
 *              SincOscPlot *resetBlanked - coordinates of the reset blanked oscilloscope plot. Will allocate resetBlanked->data and resetBlanked->intData so you must free them.
 *              SincOscPlot *rawCurve     - coordinates of the raw oscilloscope plot. Will allocate rawCurve->data and resetBlanked->intData so you must free them.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status. There's no need to free
 *                  resetBlanked or rawCurve data on failure.
 */

bool SincReadOscilloscope(Sinc *sc, int timeout, int *fromChannelId, uint64_t *dataSetId, SincOscPlot *resetBlanked, SincOscPlot *rawCurve);


/*
 * NAME:        SincStartHistogram
 * ACTION:      Starts the histogram. Note that if you want to use TCP only you should set
 *              sc->datagramXfer to false. Otherwise UDP will be used.
 * PARAMETERS:  Sinc *sc               - the sinc connection.
 *              int channelId          - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincStartHistogram(Sinc *sc, int channelId);


/*
 * NAME:        SincReadHistogram
 * ACTION:      Gets an update from the histogram. Waits for the next histogram update to
 *              arrive if timeout is non-zero. Formats for stream and datagram types.
 * PARAMETERS:  Sinc *sc                - the sinc connection.
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

bool SincReadHistogram(Sinc *sc, int timeout, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats);
bool SincReadHistogramDatagram(Sinc *sc, int timeout, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats);


/*
 * NAME:        SincWaitReady
 * ACTION:      Waits for a packet indicating that the channel has returned to a
 *              ready state. Do this after a single shot oscilloscope run.
 * PARAMETERS:  Sinc *sc      - the sinc connection.
 *              int channelId - which channel to use.
 *              int timeout   - timeout in milliseconds. -1 for no timeout.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincWaitReady(Sinc *sc, int channelId, int timeout);


/*
 * NAME:        SincClearHistogramData
 * ACTION:      Clears the histogram counts.
 * PARAMETERS:  Sinc *sc       - the sinc connection.
 *              int channelId  - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincClearHistogramData(Sinc *sc, int channelId);


/*
 * NAME:        SincStartListMode
 * ACTION:      Starts list mode.
 * PARAMETERS:  Sinc *sc      - the sinc connection.
 *              int channelId - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincStartListMode(Sinc *sc, int channelId);


/*
 * NAME:        SincReadListMode
 * ACTION:      Gets an update from the list mode data stream. Waits for the next list mode update to
 *              arrive if timeout is non-zero.
 * PARAMETERS:  Sinc *sc                     - the sinc connection.
 *              int timeout                  - in milliseconds. 0 to poll. -1 to wait forever.
 *              int *fromChannelId           - if non-NULL this is set to the channel the data was received from.
 *              uint8_t **data, int *dataLen - filled in with a dynamically allocated buffer containing list mode data. Must be freed on success.
 *              uint64_t *dataSetId          - if non-NULL this is set to the data set id.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status. There's no need to free
 *                  accepted or rejected data on failure.
 */

bool SincReadListMode(Sinc *sc, int timeout, int *fromChannelId, uint8_t **data, int *dataLen, uint64_t *dataSetId);


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

bool SincReadCheckParamConsistencyResponse(Sinc *sc, int timeout, SiToro__Sinc__CheckParamConsistencyResponse **resp, int *fromChannelId);


/*
 * NAME:        SincStopDataAcquisition
 * ACTION:      Stops oscilloscope / histogram / list mode / calibration. Deprecated in favor of
 *              SincStop().
 * PARAMETERS:  Sinc *sc      - the sinc connection.
 *              int channelId - which channel to use.
 *              int timeout   - timeout in milliseconds. -1 for no timeout.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincStopDataAcquisition(Sinc *sc, int channelId, int timeout);


/*
 * NAME:        SincStop
 * ACTION:      Stops oscilloscope / histogram / list mode / calibration. Allows skipping of the
 *              optional optimisation phase of calibration.
 * PARAMETERS:  Sinc *sc      - the sinc connection.
 *              int channelId - which channel to use.
 *              int timeout   - timeout in milliseconds. -1 for no timeout.
 *              bool skip     - true to skip the optimisation phase of calibration but still keep
 *                              the calibration.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincStop(Sinc *sc, int channelId, int timeout, bool skip);


/*
 * NAME:        SincListParamDetails
 * ACTION:      Returns a list of matching device parameters and their details.
 * PARAMETERS:  Sinc *sc                  - the connection to use.
 *              int channelId             - which channel to use.
 *              char *matchPrefix         - a key prefix to match. Only matching keys are returned. Empty string for all keys.
 *              SiToro__Sinc__ListParamDetailsResponse **resp - where to put the response received.
 *                  This message should be freed with si_toro__sinc__list_param_details_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincListParamDetails(Sinc *sc, int channelId, char *matchPrefix, SiToro__Sinc__ListParamDetailsResponse **resp);


/*
 * NAME:        SincRestart
 * ACTION:      Restarts the instrument.
 * PARAMETERS:  Sinc *sc      - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincRestart(Sinc *sc);


/*
 * NAME:        SincResetSpatialSystem
 * ACTION:      Resets the spatial system to its origin position.
 * PARAMETERS:  Sinc *sc      - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincResetSpatialSystem(Sinc *sc);


/*
 * NAME:        SincTriggerHistogram
 * ACTION:      Manually triggers a single histogram data collection if:
 *                  * histogram.mode is "gated".
 *                  * gate.source is "software".
 *                  * gate.statsCollectionMode is "always".
 *                  * histograms must be started first using SincStartHistogram().
 * PARAMETERS:  Sinc *sc      - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincTriggerHistogram(Sinc *sc);


/*
 * NAME:        SincSoftwareUpdate
 * ACTION:      Updates the software on the device.
 * PARAMETERS:  Sinc *sc  - the sinc connection.
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

bool SincSoftwareUpdate(Sinc *sc, uint8_t *appImage, int appImageLen, char *appChecksum, uint8_t *fpgaImage, int fpgaImageLen, char *fpgaChecksum, SincSoftwareUpdateFile *updateFiles, int updateFilesLen, int autoRestart);


/*
 * NAME:        SincSaveConfiguration
 * ACTION:      Saves the board's current configuration to use as default settings on startup.
 * PARAMETERS:  Sinc *sc          - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincSaveConfiguration(Sinc *sc);


/*
 * NAME:        SincDeleteSavedConfiguration
 * ACTION:      Deletes any saved configuration to return to system defaults on the next startup.
 * PARAMETERS:  Sinc *sc          - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincDeleteSavedConfiguration(Sinc *sc);


/*
 * NAME:        SincProbeDatagram
 * ACTION:      Requests a probe datagram to be sent to the configured IP and port.
 *              Waits a timeout period to see if it's received and reports success.
 *              This is called by SincInitDatagramComms() but can be called
 *              separately if you wish.
 * PARAMETERS:  Sinc *sc               - the sinc connection.
 *              bool *datagramsOk      - set to true or false depending if the probe datagram was received ok.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincProbeDatagram(Sinc *sc, bool *datagramsOk);


/*
 * NAME:        SincInitDatagramComms
 * ACTION:      Initialises the histogram datagram communications. Creates the
 *              socket if necessary and readys the datagram comms if possible.
 *              This is called automatically by SincSendHistogram() if
 *              sc->datagramXfer is set.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincInitDatagramComms(Sinc *sc);


/*
 * NAME:        SincMonitorChannels
 * ACTION:      Tells the card which channels this connection is interested in.
 *              Asynchronous events like oscilloscope and histogram data will only
 *              be sent for monitored channels.
 * PARAMETERS:  Sinc *sc        - the sinc connection.
 *              int *channelSet - a list of the channels to monitor.
 *              int numChannels - the number of channels in the list.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincMonitorChannels(Sinc *sc, int *channelSet, int numChannels);


/*
 * NAME:        SincCheckParamConsistency
 * ACTION:      Check parameters for consistency.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to check. -1 for all channels.
 *              SiToro__Sinc__CheckParamConsistencyResponse **resp - where to put the response received.
 *                  This message should be freed with si_toro__sinc__check_param_consistency_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincCheckParamConsistency(Sinc *sc, int channelId, SiToro__Sinc__CheckParamConsistencyResponse **resp);


/*
 * NAME:        SincDownloadCrashDump
 * ACTION:      Downloads the most recent crash dump, if one exists.
 * PARAMETERS:  Sinc *sc           - the sinc connection.
 *              bool *newDump      - set true if this crash dump is new.
 *              uint8_t **dumpData - where to put a pointer to the newly allocated crash dump data.
 *                                   This should be free()d after use.
 *              size_t *dumpSize   - where to put the size of the crash dump data.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincDownloadCrashDump(Sinc *sc, bool *newDump, uint8_t **dumpData, size_t *dumpSize);


/*
 * NAME:        SincSynchronizeLog
 * ACTION:      Get all the log entries since the specified log sequence number. 0 for all.
 * PARAMETERS:  Sinc *sc            - the connection to use.
 *              uint64_t sequenceNo - the log sequence number to start from. 0 for all log entries.
 *              SiToro__Sinc__SynchronizeLogResponse **resp - where to put the response received.
 *                  This message should be freed with si_toro__sinc__synchronize_log_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincSynchronizeLog(Sinc *sc, uint64_t sequenceNo, SiToro__Sinc__SynchronizeLogResponse **resp);


/*
 * NAME:        SincSetTime
 * ACTION:      Set the time on the device's real time clock. This is useful
 *              to get logs with correct timestamps.
 * PARAMETERS:  Sinc *sc            - the connection to use.
 *              struct timeval *tv  - the time to set. Includes seconds and milliseconds.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincSetTime(Sinc *sc, struct timeval *tv);


/*
 * NAME:        SincSend
 * ACTION:      Send a send buffer. Automatically clears the buffer afterwards.
 * PARAMETERS:  Sinc *sc            - the sinc connection.
 *              SincBuffer *sendBuf - the buffer to send.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincSend(Sinc *sc, SincBuffer *buf);
bool SincSendNoFree(Sinc *sc, SincBuffer *buf);


/*
 * NAME:        SincCheckSuccess
 * ACTION:      Check for a simple success response.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincCheckSuccess(Sinc *sc);


/*
 * NAME:        SincInterpretSuccess / SincInterpretSuccessError
 * ACTION:      Intepret a success response and assign channel error codes accordingly.
 *              Versions to return errors to the Sinc structure or the SincError structure.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincInterpretSuccess(Sinc *sc, SiToro__Sinc__SuccessResponse *success);
bool SincInterpretSuccessError(SincError *err, SiToro__Sinc__SuccessResponse *success);


/*
 * Various packet request functions which can be used stand-alone.
 * Parameters are the same as the above versions.
 */

bool SincRequestPing(Sinc *sc, int showOnConsole);
bool SincRequestGetParam(Sinc *sc, int channelId, char *name);
bool SincRequestGetParams(Sinc *sc, int *channelIds, const char **names, int numNames);
bool SincRequestSetParam(Sinc *sc, int channelId, SiToro__Sinc__KeyValue *param);
bool SincRequestSetParams(Sinc *sc, int channelId, SiToro__Sinc__KeyValue *param, int numParams);
bool SincRequestSetAllParams(Sinc *sc, int channelId, SiToro__Sinc__KeyValue *param, int numParams, const char *fromFirmwareVersion);
bool SincRequestStartCalibration(Sinc *sc, int channelId);
bool SincRequestGetCalibration(Sinc *sc, int channelId);
bool SincRequestSetCalibration(Sinc *sc, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);
bool SincRequestCalculateDcOffset(Sinc *sc, int channelId);
bool SincRequestStartOscilloscope(Sinc *sc, int channelId);
bool SincRequestStartHistogram(Sinc *sc, int channelId);
bool SincRequestStartFFT(Sinc *sc, int channelId);
bool SincRequestClearHistogramData(Sinc *sc, int channelId);
bool SincRequestStartListMode(Sinc *sc, int channelId);
bool SincRequestStopDataAcquisition(Sinc *sc, int channelId); // Deprecated - use SincRequestStop() instead.
bool SincRequestStop(Sinc *sc, int channelId, bool skip);
bool SincRequestListParamDetails(Sinc *sc, int channelId, char *matchPrefix);
bool SincRequestRestart(Sinc *sc);
bool SincRequestResetSpatialSystem(Sinc *sc);
bool SincRequestTriggerHistogram(Sinc *sc);
bool SincRequestSoftwareUpdate(Sinc *sc, uint8_t *appImage, int appImageLen, char *appChecksum, uint8_t *fpgaImage, int fpgaImageLen, char *fpgaChecksum, SincSoftwareUpdateFile *updateFiles, int updateFilesLen, int autoRestart);
bool SincRequestSaveConfiguration(Sinc *sc);
bool SincRequestDeleteSavedConfiguration(Sinc *sc);
bool SincRequestMonitorChannels(Sinc *sc, int *channelSet, int numChannels);
bool SincRequestProbeDatagram(Sinc *sc);
bool SincRequestCheckParamConsistency(Sinc *sc, int channelId);
bool SincRequestDownloadCrashDump(Sinc *sc);
bool SincRequestSynchronizeLog(Sinc *sc, uint64_t sequenceNo);
bool SincRequestSetTime(Sinc *sc, struct timeval *tv);


/*
 * Various packet encoding functions which can be used stand-alone.
 * Parameters are the same as the above versions.
 *
 * To use these functions do:
 *   uint8_t pad[256];
 *   SincBuffer buf = SINC_BUFFER_INIT(pad);
 *   SincEncodeXXX(&buf, ...);
 *   int success = SincSend(sinc, &buf);
 *
 * Boolean variants of these functions may return false if memory
 * allocation fails.
 */

void SincEncodePing(SincBuffer *buf, int showOnConsole);
void SincEncodeGetParam(SincBuffer *buf, int channelId, const char *name);
bool SincEncodeGetParams(SincBuffer *buf, int *channelIds, const char **names, int numNames);
void SincEncodeSetParam(SincBuffer *buf, int channelId, SiToro__Sinc__KeyValue *param);
bool SincEncodeSetParams(SincBuffer *buf, int channelId, SiToro__Sinc__KeyValue *params, int numParams);
bool SincEncodeSetAllParams(SincBuffer *buf, int channelId, SiToro__Sinc__KeyValue *params, int numParams, const char *fromFirmwareVersion);
void SincEncodeStartCalibration(SincBuffer *buf, int channelId);
void SincEncodeGetCalibration(SincBuffer *buf, int channelId);
void SincEncodeSetCalibration(SincBuffer *buf, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);
void SincEncodeCalculateDcOffset(SincBuffer *buf, int channelId);
void SincEncodeStartOscilloscope(SincBuffer *buf, int channelId);
void SincEncodeStartHistogram(SincBuffer *buf, int channelId);
void SincEncodeStartFFT(SincBuffer *buf, int channelId);
void SincEncodeClearHistogramData(SincBuffer *buf, int channelId);
void SincEncodeStartListMode(SincBuffer *buf, int channelId);
void SincEncodeStopDataAcquisition(SincBuffer *buf, int channelId); // Deprecated - use SincEncodeStop() instead.
void SincEncodeStop(SincBuffer *buf, int channelId, bool skip);
void SincEncodeListParamDetails(SincBuffer *buf, int channelId, const char *matchPrefix);
void SincEncodeRestart(SincBuffer *buf);
void SincEncodeResetSpatialSystem(SincBuffer *buf);
void SincEncodeTriggerHistogram(SincBuffer *buf);
bool SincEncodeSoftwareUpdate(SincBuffer *buf, const uint8_t *appImage, int appImageLen, const char *appChecksum, const uint8_t *fpgaImage, int fpgaImageLen, const char *fpgaChecksum, const SincSoftwareUpdateFile *updateFiles, int updateFilesLen, int autoRestart);
void SincEncodeSaveConfiguration(SincBuffer *buf);
void SincEncodeDeleteSavedConfiguration(SincBuffer *buf);
bool SincEncodeMonitorChannels(SincBuffer *buf, const int *channelSet, int numChannels);
void SincEncodeSuccessResponse(SincBuffer *buf, SiToro__Sinc__ErrorCode errorCode, char *message, int channelId);
void SincEncodeProbeDatagram(SincBuffer *buf);
void SincEncodeCheckParamConsistency(SincBuffer *buf, int channelId);
void SincEncodeDownloadCrashDump(SincBuffer *buf);
void SincEncodeSynchronizeLog(SincBuffer *buf, uint64_t sequenceNo);
void SincEncodeSetTime(SincBuffer *buf, struct timeval *tv);


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
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincReadMessage(Sinc *sc, int timeout, SincBuffer *buf, SiToro__Sinc__MessageType *msgType);


/*
 * NAME:        SincGetNextPacketFromBuffer
 * ACTION:      Gets the next packet in the read buffer and de-encapsulates it.
 *              The resulting packet can be handed directly to the appropriate SincDecodeXXX()
 *              function.
 *              Use this function to read the next message from a buffer. If you want to read
 *              from the input stream use SincReadMessage() instead.
 * PARAMETERS:  SincBuffer *readBuf   - the buffer to read from.
 *              SiToro__Sinc__MessageType *packetType - the found packet type is placed here.
 *              SincBuffer *packetBuf - where to put the resultant de-encapsulated packet.
 *                                      If NULL it won't make a buffer or consume data (this is useful
 *                                      for peeking ahead for the packet type).
 *              int *packetFound      - set to true if a complete packet was found, false otherwise.
 * RETURNS:     true on success. But this function never errors.
 */

bool SincGetNextPacketFromBuffer(SincBuffer *readBuf, SiToro__Sinc__MessageType *packetType, SincBuffer *packetBuf, int *packetFound);


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

bool SincDecodeSuccessResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__SuccessResponse **resp, int *fromChannelId);
bool SincDecodeGetParamResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId);
bool SincDecodeParamUpdatedResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__ParamUpdatedResponse **resp, int *fromChannelId);
bool SincDecodeCalibrationProgressResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CalibrationProgressResponse **resp, double *progress, int *complete, char **stage, int *fromChannelId);
bool SincDecodeGetCalibrationResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__GetCalibrationResponse **resp, int *fromChannelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final);
bool SincDecodeCalculateDCOffsetResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CalculateDcOffsetResponse **resp, double *dcOffset, int *fromChannelId);
bool SincDecodeListParamDetailsResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__ListParamDetailsResponse **resp, int *fromChannelId);
bool SincDecodeOscilloscopeDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, uint64_t *dataSetId, SincOscPlot *resetBlanked, SincOscPlot *rawCurve);
bool SincDecodeOscilloscopeDataResponseAsPlotArray(SincError *err, SincBuffer *packet, int *fromChannelId, uint64_t *dataSetId, SincOscPlot *plotArray, int maxPlotArray, int *plotArraySize);
bool SincDecodeHistogramDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats);
bool SincDecodeHistogramDatagramResponse(SincError *err, SincBuffer *packet, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats);
bool SincDecodeListModeDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, uint8_t **data, int *dataLen, uint64_t *dataSetId);
bool SincDecodeMonitorChannelsCommand(SincError *err, SincBuffer *packet, uint64_t *channelBitSet);
bool SincDecodeCheckParamConsistencyResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CheckParamConsistencyResponse **resp, int *fromChannelId);
bool SincDecodeAsynchronousErrorResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__AsynchronousErrorResponse **resp, int *fromChannelId);
bool SincDecodeSoftwareUpdateCompleteResponse(SincError *err, SincBuffer *packet);
bool SincDecodeDownloadCrashDumpResponse(SincError *err, SincBuffer *packet, bool *newDump, uint8_t **dumpData, size_t *dumpSize);
bool SincDecodeSynchronizeLogResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__SynchronizeLogResponse **resp);


/*
 * NAME:        SincIsConnected
 * ACTION:      Returns the connected/disconnected state of the channel.
 * PARAMETERS:  Sinc *sc  - the sinc connection.
 * RETURNS:     int - true if connected, false otherwise.
 */

int SincIsConnected(Sinc *sc);


/*
 * NAME:        SincListModeEncodeHeader
 * ACTION:      Encodes a list mode header.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 *              int channelId - which channel to use.
 *              uint8_t **headerData - memory will be allocated for the result and returned here.
 *                  You must free() this data.
 *              size_t *headerLen - the length of the result data will be returned here.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincListModeEncodeHeader(Sinc *sc, int channelId, uint8_t **headerData, size_t *headerLen);


/*
 * NAME:        SincProjectLoad
 * ACTION:      Loads the device settings from a file and restores them to the device.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 *              const char *fileName - the name of the file to load from.
 */

bool SincProjectLoad(Sinc *sc, const char *fileName);


/*
 * NAME:        SincProjectSave
 * ACTION:      Saves the device settings to a file from the device.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 *              const char *fileName - the name of the file to save to.
 */

bool SincProjectSave(Sinc *sc, const char *fileName);


/*
 * NAME:        SincCurrentErrorCode / SincReadErrorCode / SincWriteErrorCode
 * ACTION:      Get the most recent error code. SincCurrentErrorCode() gets the most
 *              recent error code. SincReadErrorCode() gets the most recent read error.
 *              SincWriteErrorCode() gets the most recent write error.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 */

SiToro__Sinc__ErrorCode SincCurrentErrorCode(Sinc *sc);
SiToro__Sinc__ErrorCode SincReadErrorCode(Sinc *sc);
SiToro__Sinc__ErrorCode SincWriteErrorCode(Sinc *sc);


/*
 * NAME:        SincCurrentErrorMessage / SincReadErrorMessage / SincWriteErrorMessage
 * ACTION:      Get the most recent error message in alphanumeric form. SincCurrentErrorMessage()
 *              gets the most recent error code. SincReadErrorMessage() gets the most recent read error.
 *              SincWriteErrorMessage() gets the most recent write error.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 */

const char *SincCurrentErrorMessage(Sinc *sc);
const char *SincReadErrorMessage(Sinc *sc);
const char *SincWriteErrorMessage(Sinc *sc);

#ifdef __cplusplus
}
#endif

#endif /* SINC_H */
