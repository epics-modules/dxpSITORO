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


/* Forward declarations. */
int SincSendAndCheckSuccess(Sinc *sc, ProtobufCBufferSimple *sendBuf);
bool SincStartCalibration(Sinc *sc, int channelId);


/*
 * NAME:        SincCheckSuccess
 * ACTION:      Check for a simple success response.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincCheckSuccess(Sinc *sc)
{
    // Wait for the response.
    SiToro__Sinc__SuccessResponse *resp = NULL;
    if (!SincReadSuccessResponse(sc, sc->timeout, &resp, NULL))
    {
        if (resp != NULL)
        {
            si_toro__sinc__success_response__free_unpacked(resp, NULL);
        }

        return false;
    }

    // Handle errors and clean up.
    int ok = SincInterpretSuccess(sc, resp);
    si_toro__sinc__success_response__free_unpacked(resp, NULL);

    return ok;
}


/*
 * NAME:        SincSendAndCheckSuccess
 * ACTION:      Send a send buffer and check for a simple success response.
 * PARAMETERS:  Sinc *sc                - the sinc connection.
 *              ProtobufCBufferSimple *sendBuf - the buffer to send.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

int SincSendAndCheckSuccess(Sinc *sc, ProtobufCBufferSimple *sendBuf)
{
    // Send it.
    if (!SincSend(sc, sendBuf))
        return false;

    // Wait for the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincPing
 * ACTION:      Checks if the device is responding.
 * PARAMETERS:  Sinc *sc         - the channel to ping.
 *              int showOnConsole - the number of channels in the array.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincPing(Sinc *sc, int showOnConsole)
{
    // Send the request.
    if (!SincRequestPing(sc, showOnConsole))
        return false;

    // Get the response;
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincGetParam
 * ACTION:      Gets a named parameter from the device.
 * PARAMETERS:  Sinc *sc                 - the channel to request from.
 *              int channelId            - which channel to use. -1 to use the default channel for this port.
 *              char *name               - the name of the parameter to get. "channel.allSettings" provides
 *                  a JSON list of all the available parameters and their values.
 *              SiToro__Sinc__GetParamResponse **resp - where to put the response received.
 *
 *                  (*resp)->results[0] will contain the result as a SiToro__Sinc__KeyValue.
 *                  Get the type of response from value_case and the value from one of intval,
 *                  floatval, boolval, strval or optionval.
 *
 *                  resp should be freed with si_toro__sinc__get_param_response__free_unpacked(resp, NULL) after use.
 *
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincGetParam(Sinc *sc, int channelId, char *name, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId)
{
    // Request the parameter.
    if (!SincRequestGetParam(sc, channelId, name))
        return false;

    // Wait for the response.
    if (!SincReadGetParamResponse(sc, sc->timeout, resp, fromChannelId))
        return false;

    // Handle errors and clean up.
    return SincInterpretSuccess(sc, (*resp)->success);
}


/*
 * NAME:        SincGetParams
 * ACTION:      Gets named parameters from the device.
 * PARAMETERS:  Sinc *sc           - the channel to request from.
 *              int *channelIds    - which channel to use for each name.
 *              const char **names - the names of the parameters to get.
 *              int numNames       - the number of names in "names".
 *              SiToro__Sinc__GetParamResponse **resp - where to put the response received.
 *
 *                  (*resp)->results and (*resp)->n_results will contain the results as a set
 *                  of SiToro__Sinc__KeyValue. Get the type of response from value_case and
 *                  the value from one of intval, floatval, boolval, strval or optionval.
 *
 *                  resp should be freed with si_toro__sinc__get_param_response__free_unpacked(resp, NULL) after use.
 *
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincGetParams(Sinc *sc, int *channelIds, const char **names, int numNames, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId)
{
    // Request the parameter.
    if (!SincRequestGetParams(sc, channelIds, names, numNames))
        return false;

    // Wait for the response.
    if (!SincReadGetParamResponse(sc, sc->timeout, resp, fromChannelId))
        return false;

    // Handle errors and clean up.
    return SincInterpretSuccess(sc, (*resp)->success);
}


/*
 * NAME:        SincSetParam
 * ACTION:      Sets a named parameter on the device.
 * PARAMETERS:  Sinc *sc                 - the channel to request from.
 *              int channelId            - which channel to use. -1 to use the default channel for this port.
 *              SiToro__Sinc__KeyValue *param       - the key and value to set.
 *                  Use si_toro__sinc__key_value__init() to initialise the struct.
 *                  Set the key in param->key.
 *                  Set the value type in param->valueCase and the value in one of intval,
 *                  floatval, boolval, strval or optionval.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincSetParam(Sinc *sc, int channelId, SiToro__Sinc__KeyValue *param)
{
    // Send the request.
    if (!SincRequestSetParam(sc, channelId, param))
        return false;

    // Send it.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincSetParams
 * ACTION:      Sets named parameters on the device.
 * PARAMETERS:  Sinc *sc                 - the channel to request from.
 *              int channelId            - which channel to use. -1 to use the default channel for this port.
 *              SiToro__Sinc__KeyValue *param       - the array of keys and values to set.
 *                  Use si_toro__sinc__key_value__init() to initialise the struct.
 *                  Set the key in param->key.
 *                  Set the value type in param->valueCase and the value in one of intval,
 *                  floatval, boolval, strval or optionval.
 *              int numParams            - the number of parameters to set.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincSetParams(Sinc *sc, int channelId, SiToro__Sinc__KeyValue *params, int numParams)
{
    // Send the request.
    if (!SincRequestSetParams(sc, channelId, params, numParams))
        return false;

    // Send it.
    return SincCheckSuccess(sc);
}


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

bool SincStartCalibration(Sinc *sc, int channelId)
{
    // Send the request.
    if (!SincRequestStartCalibration(sc, channelId))
        return false;

    // Get the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincCalibrate
 * ACTION:      Performs a calibration and returns calibration data. May take several seconds.
 * PARAMETERS:  Sinc *sc                            - the channel to request from.
 *              int channelId            - which channel to use. -1 to use the default channel for this port.
 *              SincCalibrationData *calibData      - the calibration data.
 *              SincPlot *example, model, final     - the pulse shapes.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincCalibrate(Sinc *sc, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    // Request the calibration.
    if (!SincStartCalibration(sc, channelId))
        return false;

    // Wait for calibration to be complete.
    return SincWaitCalibrationComplete(sc, channelId, calibData, example, model, final);
}


/*
 * NAME:        SincGetCalibration
 * ACTION:      Gets the calibration data from a previous calibration.
 * PARAMETERS:  Sinc *sc                        - the channel to request from.
 *              int channelId                   - which channel to use. -1 to use the default channel for this port.
 *              SincCalibrationData *calibData  - the calibration data is stored here.
 *              calibData must be free()d after use.
 *              SincPlot *example, model, final - the pulse shapes are set here.
 *              The fields x and y of each pulse must be free()d after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincGetCalibration(Sinc *sc, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    // Request the calibration.
    if (!SincRequestGetCalibration(sc, channelId))
        return false;

    // Wait for the response.
    return SincReadGetCalibrationResponse(sc, sc->timeout, NULL, NULL, calibData, example, model, final);
}


/*
 * NAME:        SincSetCalibration
 * ACTION:      Sets the calibration data on the device from a previously acquired data set.
 * PARAMETERS:  Sinc *sc                            - the channel to request from.
 *              int channelId                       - which channel to use. -1 to use the default channel for this port.
 *              SincCalibrationData *calibData      - the calibration data to set.
 *              SincPlot *example, model, final     - the pulse shapes.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincSetCalibration(Sinc *sc, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    // Send the request.
    if (!SincRequestSetCalibration(sc, channelId, calibData, example, model, final))
        return false;

    // Get the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincCalculateDcOffset
 * ACTION:      Calculates the DC offset on the device. May take a couple of seconds.
 * PARAMETERS:  Sinc *sc         - the channel to request from.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 *              double *dcOffset - the calculated DC offset is placed here.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincCalculateDcOffset(Sinc *sc, int channelId, double *dcOffset)
{
    // Send the request.
    if (!SincRequestCalculateDcOffset(sc, channelId))
        return false;

    // Wait for the immediate success response.
    if (!SincCheckSuccess(sc))
        return false;

    // Wait for the dc offset response.
    return SincReadCalculateDCOffsetResponse(sc, sc->timeout, NULL, dcOffset, NULL);
}


/*
 * NAME:        SincStartOscilloscope
 * ACTION:      Starts the oscilloscope.
 * PARAMETERS:  Sinc *sc            - the sinc connection.
 *              int channelId       - which channel to use. -1 to use the default channel for this port.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincStartOscilloscope(Sinc *sc, int channelId)
{
    // Send the request.
    if (!SincRequestStartOscilloscope(sc, channelId))
        return false;

    // Get the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincProbeDatagram
 * ACTION:      Requests a probe datagram to be sent to the configured IP and port.
 *              Waits a timeout period to see if it's received and reports success.
 * PARAMETERS:  Sinc *sc               - the sinc connection.
 *              bool *datagramsOk      - set to true or false depending if the probe datagram was received ok.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincProbeDatagram(Sinc *sc, bool *datagramsOk)
{
    size_t buflen;
    int fds[2];
    int err;
    bool readOk[2];

    // Send the request.
    if (!SincRequestProbeDatagram(sc))
        return false;

    // Wait for something to happen.
    *datagramsOk = false;
    err = SI_TORO__SINC__ERROR_CODE__NO_ERROR;
    readOk[0] = false;

    while (!readOk[0])
    {
        // Wait for something to happen.
        fds[0] = sc->fd;
        fds[1] = sc->datagramFd;
        readOk[0] = false;
        readOk[1] = false;
        err = SincSocketWaitMulti(fds, 2, sc->timeout, readOk);
        if (err != (int)SI_TORO__SINC__ERROR_CODE__NO_ERROR)
        {
            SincReadErrorSetMessage(sc, (SiToro__Sinc__ErrorCode)err, "can't read histogram probe datagram");
            return false;
        }

        if (readOk[1])
        {
            // Read the datagram.
            buflen = sc->readBuf.alloced;
            err = SincSocketReadDatagram(sc->datagramFd, sc->readBuf.data, &buflen, true);
            if (err != (int)SI_TORO__SINC__ERROR_CODE__NO_ERROR)
            {
                // Got an error.
                SincReadErrorSetMessage(sc, (SiToro__Sinc__ErrorCode)err, "can't read histogram probe datagram");
                return false;
            }
            else
            {
                *datagramsOk = true;
            }
        }
    }

    // Get the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincOpenDatagramComms
 * ACTION:      Initialises datagram communications. Creates the socket if necessary.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincOpenDatagramComms(Sinc *sc)
{
    // Don't re-do the comms if we've already done it.
    if (sc->datagramFd >= 0)
        return true;

    // Create the datagram socket.
    int err = SincSocketBindDatagram(&sc->datagramFd, &sc->datagramPort);
    if (err != SI_TORO__SINC__ERROR_CODE__NO_ERROR)
    {
        sc->datagramIsOpen = false;
        SincReadErrorSetMessage(sc, (SiToro__Sinc__ErrorCode)err, "can't bind histogram datagram socket");
        return false;
    }

    return true;
}


/*
 * NAME:        SincInitDatagramComms
 * ACTION:      Initialises the histogram datagram communications. Creates the
 *              socket if necessary and readys the datagram comms if possible.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincInitDatagramComms(Sinc *sc)
{
    SiToro__Sinc__KeyValue params[2];
    SiToro__Sinc__KeyValue enableParam;

    // Don't re-do the comms if we've already done it.
    if (sc->datagramFd >= 0)
        return true;

    // Open the datagram socket.
    if (!SincOpenDatagramComms(sc))
        return false;

    // Set the datagram destination ip and port.
    si_toro__sinc__key_value__init(&params[0]);
    si_toro__sinc__key_value__init(&params[1]);
    params[0].key = (char *)"histogram.datagram.ip";
    params[0].strval = (char *)"";  // Use the connection's source ip.
    params[1].key = (char *)"histogram.datagram.port";
    params[1].has_intval = true;
    params[1].intval = sc->datagramPort;
    if (!SincSetParams(sc, -1, params, 2))
        return false;

    // Check the datagram path.
    if (!SincProbeDatagram(sc, &sc->datagramIsOpen))
    {
        return false;
    }

    // Turn datagrams on if we can use them.
    si_toro__sinc__key_value__init(&enableParam);
    enableParam.key = (char *)"histogram.datagram.enable";
    enableParam.has_boolval = true;
    enableParam.boolval = sc->datagramIsOpen;
    if (!SincSetParam(sc, -1, &enableParam))
        return false;

    printf("datagram mode enabled.\n");
    return true;
}


/*
 * NAME:        SincStartHistogram
 * ACTION:      Starts the histogram. Note that if you want to use TCP only you should set
 *              sc->datagramXfer to false. Otherwise UDP will be used.
 * PARAMETERS:  Sinc *sc               - the sinc connection.
 *              int channelId          - which channel to use. -1 to use the default channel for this port.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincStartHistogram(Sinc *sc, int channelId)
{
    // Try starting datagram comms.
    if (sc->datagramXfer && sc->datagramFd < 0)
    {
        if (!SincInitDatagramComms(sc))
        {
            printf("can't negotiate datagram mode - %s\n", SincCurrentErrorMessage(sc));
        }
    }

    // Send the request.
    if (!SincRequestStartHistogram(sc, channelId))
        return false;

    // Get the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincClearHistogramData
 * ACTION:      Clears the histogram counts.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincClearHistogramData(Sinc *sc, int channelId)
{
    // Send the request.
    if (!SincRequestClearHistogramData(sc, channelId))
        return false;

    // Get the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincStartListMode
 * ACTION:      Starts list mode.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincStartListMode(Sinc *sc, int channelId)
{
    // Send the request.
    if (!SincRequestStartListMode(sc, channelId))
        return false;

    // Get the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincStopDataAcquisition
 * ACTION:      Stops oscilloscope / histogram / list mode / calibration.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 *              int timeout      - in milliseconds. 0 to poll. -1 to wait forever.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincStopDataAcquisition(Sinc *sc, int channelId, int timeout)
{
    // Send the request.
    if (!SincRequestStopDataAcquisition(sc, channelId))
        return false;

    // Get the response.
    if (!SincCheckSuccess(sc))
        return false;

    // Wait for the "channel.state=ready" state.
    return SincWaitReady(sc, channelId, timeout);
}


/*
 * NAME:        SincStop
 * ACTION:      Stops oscilloscope / histogram / list mode / calibration. Allows skipping of the
 *              optional optimisation phase of calibration.
 * PARAMETERS:  Sinc *sc      - the sinc connection.
 *              int channelId - which channel to use. -1 to use the default channel for this port.
 *              int timeout   - timeout in milliseconds. -1 for no timeout.
 *              bool skip     - true to skip the optimisation phase of calibration but still keep
 *                              the calibration.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincStop(Sinc *sc, int channelId, int timeout, bool skip)
{
    // Send the request.
    if (!SincRequestStop(sc, channelId, skip))
        return false;

    // Get the response.
    if (!SincCheckSuccess(sc))
        return false;

    // Wait for the "channel.state=ready" state.
    return SincWaitReady(sc, channelId, timeout);
}


/*
 * NAME:        SincListParamDetails
 * ACTION:      Returns a list of matching device parameters and their details.
 * PARAMETERS:  Sinc *sc                                      - the sinc connection.
 *              int channelId                                 - which channel to use. -1 to use the default channel for this port.
 *              const char *matchPrefix                       - a key prefix to match. Only matching keys are returned.
 *              SiToro__Sinc__ListParamDetailsResponse **resp - where to put the response received.
 *                  This message should be freed with si_toro__sinc__list_param_details_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincListParamDetails(Sinc *sc, int channelId, char *matchPrefix, SiToro__Sinc__ListParamDetailsResponse **resp)
{
    // Send the request.
    if (!SincRequestListParamDetails(sc, channelId, matchPrefix))
        return false;

    // Wait for the response.
    if (!SincReadListParamDetailsResponse(sc, sc->timeout, resp, NULL))
        return false;

    // Handle errors and clean up.
    return SincInterpretSuccess(sc, (*resp)->success);
}


/*
 * NAME:        SincRestart
 * ACTION:      Restarts the instrument.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRestart(Sinc *sc)
{
    // Send the request.
    if (!SincRequestRestart(sc))
        return false;

    // Get the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincResetSpatialSystem
 * ACTION:      Resets the spatial system to its origin position.
 * PARAMETERS:  Sinc *sc      - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincResetSpatialSystem(Sinc *sc)
{
    // Send the request.
    if (!SincRequestResetSpatialSystem(sc))
        return false;

    // Get the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincSoftwareUpdate
 * ACTION:      Updates the software on the device.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
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
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincSoftwareUpdate(Sinc *sc, uint8_t *appImage, int appImageLen, char *appChecksum, uint8_t *fpgaImage, int fpgaImageLen, char *fpgaChecksum, SincSoftwareUpdateFile *updateFiles, int updateFilesLen, int autoRestart)
{
    // Send the request.
    if (!SincRequestSoftwareUpdate(sc, appImage, appImageLen, appChecksum, fpgaImage, fpgaImageLen, fpgaChecksum, updateFiles, updateFilesLen, autoRestart))
        return false;

    // Get the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincSaveConfiguration
 * ACTION:      Saves the channel's current configuration to use as default settings on startup.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincSaveConfiguration(Sinc *sc, int channelId)
{
    // Send the request.
    if (!SincRequestSaveConfiguration(sc, channelId))
        return false;

    // Get the response.
    return SincCheckSuccess(sc);
}


/*
 * NAME:        SincMonitorChannels
 * ACTION:      Tells the card which channels this connection is interested in.
 *              Asynchronous events like oscilloscope and histogram data will only
 *              be sent for monitored channels.
 * PARAMETERS:  Sinc *sc        - the sinc connection.
 *              int *channelSet - a list of the channels to monitor.
 *              int numChannels - the number of channels in the list.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincMonitorChannels(Sinc *sc, int *channelSet, int numChannels)
{
    // Send the request.
    if (!SincRequestMonitorChannels(sc, channelSet, numChannels))
        return false;

    // Get the response.
    return SincCheckSuccess(sc);
}


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

bool SincCheckParamConsistency(Sinc *sc, int channelId, SiToro__Sinc__CheckParamConsistencyResponse **resp)
{
    // Send the request.
    if (!SincRequestCheckParamConsistency(sc, channelId))
        return false;

    // Wait for the response.
    if (!SincReadCheckParamConsistencyResponse(sc, sc->timeout, resp, NULL))
        return false;

    // Handle errors and clean up.
    return SincInterpretSuccess(sc, (*resp)->success);
}
