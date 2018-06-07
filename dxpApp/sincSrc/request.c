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
 * NAME:        SincSend
 * ACTION:      Send a send buffer. Automatically clears the buffer afterwards.
 * PARAMETERS:  Sinc *sc            - the sinc connection.
 *              SincBuffer *sendBuf - the buffer to send.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincSend(Sinc *sc, SincBuffer *sendBuf)
{
    // Check that we're connected.
    if (!sc->connected)
    {
        SincWriteErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__NOT_CONNECTED);
        return false;
    }

    // Send it.
    int errCode = SincSocketWrite(sc->fd, sendBuf->cbuf.data, (int)sendBuf->cbuf.len);
    SINC_BUFFER_CLEAR(sendBuf);
    if (errCode != 0)
    {
        SincWriteErrorSetCode(sc, (SiToro__Sinc__ErrorCode)errCode);
        return false;
    }

    return true;
}


/*
 * NAME:        SincSendNoFree
 * ACTION:      Send a send buffer. Doesn't clear the buffer afterwards.
 * PARAMETERS:  Sinc *sc            - the sinc connection.
 *              SincBuffer *sendBuf - the buffer to send.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincSendNoFree(Sinc *sc, SincBuffer *sendBuf)
{
    // Check that we're connected.
    if (!sc->connected)
    {
        SincWriteErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__NOT_CONNECTED);
        return false;
    }

    // Send it.
    int errCode = SincSocketWrite(sc->fd, sendBuf->cbuf.data, (int)sendBuf->cbuf.len);
    if (errCode != 0)
    {
        SincWriteErrorSetCode(sc, (SiToro__Sinc__ErrorCode)errCode);
        return false;
    }

    return true;
}


/*
 * NAME:        SincRequestPing
 * ACTION:      Checks if the device is responding. Request only version.
 * PARAMETERS:  Sinc *sc         - the channel to ping.
 *              int showOnConsole - the number of channels in the array.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestPing(Sinc *sc, int showOnConsole)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodePing(&sendBuf, showOnConsole);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestGetParam
 * ACTION:      Gets a named parameter from the device. Request only version.
 * PARAMETERS:  Sinc *sc                 - the channel to request from.
 *              int channelId            - which channel to use.
 *              char *name               - the name of the parameter to get.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestGetParam(Sinc *sc, int channelId, char *name)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeGetParam(&sendBuf, channelId, name);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestGetParams
 * ACTION:      Gets named parameters from the device. Request only version.
 * PARAMETERS:  Sinc *sc           - the channel to request from.
 *              int *channelIds    - which channel to use for each name.
 *              const char **names - the names of the parameters to get.
 *              int numNames       - the number of names in "names".
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestGetParams(Sinc *sc, int *channelIds, const char **names, int numNames)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    if (!SincEncodeGetParams(&sendBuf, channelIds, names, numNames))
    {
        SincWriteErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestSetParam
 * ACTION:      Requests setting a named parameter on the device but doesn't wait for a response.
 * PARAMETERS:  Sinc *sc                 - the channel to request from.
 *              int channelId            - which channel to use.
 *              SiToro__Sinc__KeyValue *param       - the key and value to set.
 *                  Use si_toro__sinc__key_value__init() to initialise the struct.
 *                  Set the key in param->key.
 *                  Set the value type in param->valueCase and the value in one of intval,
 *                  floatval, boolval, strval or optionval.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestSetParam(Sinc *sc, int channelId, SiToro__Sinc__KeyValue *param)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeSetParam(&sendBuf, channelId, param);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestSetParams
 * ACTION:      Requests setting named parameters on the device but doesn't wait for a response.
 * PARAMETERS:  Sinc *sc                 - the channel to request from.
 *              int channelId            - which channel to use.
 *              SiToro__Sinc__KeyValue *param       - the key and value to set.
 *                  Use si_toro__sinc__key_value__init() to initialise the struct.
 *                  Set the key in param->key.
 *                  Set the value type in param->valueCase and the value in one of intval,
 *                  floatval, boolval, strval or optionval.
 *              int numParams            - the number of parameters to set.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestSetParams(Sinc *sc, int channelId, SiToro__Sinc__KeyValue *params, int numParams)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    if (!SincEncodeSetParams(&sendBuf, channelId, params, numParams))
    {
        SincWriteErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestSetAllParams
 * ACTION:      Requests setting all of the parameters on the device. If any parameters on the
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

bool SincRequestSetAllParams(Sinc *sc, int channelId, SiToro__Sinc__KeyValue *params, int numParams, const char *fromFirmwareVersion)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    if (!SincEncodeSetAllParams(&sendBuf, channelId, params, numParams, fromFirmwareVersion))
    {
        SincWriteErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestStartCalibration
 * ACTION:      Requests a calibration but doesn't wait for a response. Use SincCheckSuccess()
 *              and then SincCalibrate() instead to wait for calibration to complete or use
 *              SincWaitCalibrationComplete() with this call. Request only version.
 * PARAMETERS:  Sinc *sc                        - the channel to request from.
 *              int channelId                   - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestStartCalibration(Sinc *sc, int channelId)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeStartCalibration(&sendBuf, channelId);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestGetCalibration
 * ACTION:      Gets the calibration data from a previous calibration. Request only version.
 * PARAMETERS:  Sinc *sc                        - the channel to request from.
 *              int channelId                   - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestGetCalibration(Sinc *sc, int channelId)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeGetCalibration(&sendBuf, channelId);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestSetCalibration
 * ACTION:      Sets the calibration data on the device from a previously acquired data set.
 * PARAMETERS:  Sinc *sc                            - the channel to request from.
 *              int channelId                       - which channel to use.
 *              SincCalibrationData *calibData      - the calibration data to set.
 *              SincPlot *example, model, final     - the pulse shapes.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestSetCalibration(Sinc *sc, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeSetCalibration(&sendBuf, channelId, calibData, example, model, final);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestCalculateDcOffset
 * ACTION:      Calculates the DC offset on the device. Request only version.
 * PARAMETERS:  Sinc *sc         - the channel to request from.
 *              int channelId    - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestCalculateDcOffset(Sinc *sc, int channelId)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeCalculateDcOffset(&sendBuf, channelId);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestStartOscilloscope
 * ACTION:      Starts the oscilloscope. Request only version.
 * PARAMETERS:  Sinc *sc            - the sinc connection.
 *              int channelId       - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestStartOscilloscope(Sinc *sc, int channelId)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeStartOscilloscope(&sendBuf, channelId);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestStartHistogram
 * ACTION:      Starts the histogram. Request only version. Note that if you want to use TCP only you should set
 *              sc->datagramXfer to false. Otherwise UDP will be used.
 * PARAMETERS:  Sinc *sc               - the sinc connection.
 *              int channelId          - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestStartHistogram(Sinc *sc, int channelId)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeStartHistogram(&sendBuf, channelId);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestStartFFT
 * ACTION:      Starts FFT histogram capture. Request only version.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestStartFFT(Sinc *sc, int channelId)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeStartFFT(&sendBuf, channelId);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestClearHistogramData
 * ACTION:      Clears the histogram counts. Request only version.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestClearHistogramData(Sinc *sc, int channelId)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeClearHistogramData(&sendBuf, channelId);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestStartListMode
 * ACTION:      Starts list mode. Request only version.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestStartListMode(Sinc *sc, int channelId)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeStartListMode(&sendBuf, channelId);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestStopDataAcquisition
 * ACTION:      Deprecated. Use instead.
 *              Stops oscilloscope / histogram / list mode / calibration. Request only version.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestStopDataAcquisition(Sinc *sc, int channelId)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeStopDataAcquisition(&sendBuf, channelId);

    // Send it.
    return SincSend(sc, &sendBuf);
}



/*
 * NAME:        SincRequestStop
 * ACTION:      Stops oscilloscope / histogram / list mode / calibration.  Allows skipping of the
 *              optional optimisation phase of calibration. Request only version.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use.
 *              bool skip        - true to skip the optimisation phase of calibration but still keep
 *                                 the calibration.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestStop(Sinc *sc, int channelId, bool skip)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeStop(&sendBuf, channelId, skip);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestListParamDetails
 * ACTION:      Returns a list of matching device parameters and their details. Request only version.
 * PARAMETERS:  Sinc *sc                                - the sinc connection.
 *              int channelId                           - which channel to use.
 *              char *matchPrefix                       - a key prefix to match. Only matching keys are returned.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestListParamDetails(Sinc *sc, int channelId, char *matchPrefix)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeListParamDetails(&sendBuf, channelId, matchPrefix);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestRestart
 * ACTION:      Restarts the instrument.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestRestart(Sinc *sc)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeRestart(&sendBuf);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestResetSpatialSystem
 * ACTION:      Resets the spatial system to its origin position.
 * PARAMETERS:  Sinc *sc      - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincRequestResetSpatialSystem(Sinc *sc)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeResetSpatialSystem(&sendBuf);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestTriggerHistogram
 * ACTION:      Manually triggers a single histogram data collection if:
 *                  * histogram.mode is "gated".
 *                  * gate.source is "software".
 *                  * gate.statsCollectionMode is "always".
 *                  * histograms must be started first using SincStartHistogram().
 * PARAMETERS:  Sinc *sc      - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincRequestTriggerHistogram(Sinc *sc)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeTriggerHistogram(&sendBuf);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestSoftwareUpdate
 * ACTION:      Updates the software on the device. Request only version.
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

bool SincRequestSoftwareUpdate(Sinc *sc, uint8_t *appImage, int appImageLen, char *appChecksum, uint8_t *fpgaImage, int fpgaImageLen, char *fpgaChecksum, SincSoftwareUpdateFile *updateFiles, int updateFilesLen, int autoRestart)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    if (!SincEncodeSoftwareUpdate(&sendBuf, appImage, appImageLen, appChecksum, fpgaImage, fpgaImageLen, fpgaChecksum, updateFiles, updateFilesLen, autoRestart))
    {
        SincWriteErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestSaveConfiguration
 * ACTION:      Saves the channel's current configuration to use as default settings on startup.
 *              Request only version.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestSaveConfiguration(Sinc *sc)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeSaveConfiguration(&sendBuf);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestDeleteSavedConfiguration
 * ACTION:      Deletes any saved configuration to return to system defaults on the next startup.
 *              Request only version.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestDeleteSavedConfiguration(Sinc *sc)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeDeleteSavedConfiguration(&sendBuf);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestMonitorChannels
 * ACTION:      Tells the card which channels this connection is interested in.
 *              Asynchronous events like oscilloscope and histogram data will only
 *              be sent for monitored channels. Request only version.
 * PARAMETERS:  Sinc *sc        - the sinc connection.
 *              int *channelSet - a list of the channels to monitor.
 *              int numChannels - the number of channels in the list.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestMonitorChannels(Sinc *sc, int *channelSet, int numChannels)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    if (!SincEncodeMonitorChannels(&sendBuf, channelSet, numChannels))
    {
        SincWriteErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestProbeDatagram
 * ACTION:      Requests a datagram probe packet to be sent. Request only version.
 * PARAMETERS:  Sinc *sc            - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestProbeDatagram(Sinc *sc)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeProbeDatagram(&sendBuf);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestCheckParamConsistency
 * ACTION:      Requests a check of parameters for consistency. Request only version.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to check. -1 for all channels.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincRequestCheckParamConsistency(Sinc *sc, int channelId)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeCheckParamConsistency(&sendBuf, channelId);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestDownloadCrashDump
 * ACTION:      Downloads the most recent crash dump, if one exists. Request only version.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincRequestDownloadCrashDump(Sinc *sc)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeDownloadCrashDump(&sendBuf);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestSynchronizeLog
 * ACTION:      Send a request to get all the log entries since the specified
 *              log sequence number. 0 for all.
 * PARAMETERS:  Sinc *sc            - the connection to use.
 *              uint64_t sequenceNo - the log sequence number to start from. 0 for all log entries.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincRequestSynchronizeLog(Sinc *sc, uint64_t sequenceNo)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeSynchronizeLog(&sendBuf, sequenceNo);

    // Send it.
    return SincSend(sc, &sendBuf);
}


/*
 * NAME:        SincRequestSetTime
 * ACTION:      Send a request to set the time on the device's real time
 *              clock. This is useful to get logs with correct timestamps.
 * PARAMETERS:  Sinc *sc            - the connection to use.
 *              struct timeval *tv  - the time to set. Includes seconds and milliseconds.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincRequestSetTime(Sinc *sc, struct timeval *tv)
{
    // Encode it.
    uint8_t pad[256];
    SincBuffer sendBuf = SINC_BUFFER_INIT(pad);
    SincEncodeSetTime(&sendBuf, tv);

    // Send it.
    return SincSend(sc, &sendBuf);
}
