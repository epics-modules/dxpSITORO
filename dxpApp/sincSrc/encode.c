/********************************************************************
 ***                                                              ***
 ***                   libsinc api implementation                 ***
 ***                                                              ***
 ********************************************************************/

/*
 * This module provides the encoding API for libsinc.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "sinc.h"
#include "sinc_internal.h"


/*
 * NAME:        SincEncodePing
 * ACTION:      Encodes a packet to check if the device is responding.
 * PARAMETERS:  SincBuffer *buf   - where to put the encoded packet.
 *              int showOnConsole - the number of channels in the array.
 */

void SincEncodePing(SincBuffer *buf, int showOnConsole)
{
    // Create the packet.
    SiToro__Sinc__PingCommand pingCmd;
    si_toro__sinc__ping_command__init(&pingCmd);
    if (showOnConsole)
    {
        pingCmd.has_verbose = true;
        pingCmd.verbose = true;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__ping_command__get_packed_size(&pingCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__PING_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__ping_command__pack_to_buffer(&pingCmd, cBuf);
}


/*
 * NAME:        SincEncodeGetParam
 * ACTION:      Gets a named parameter from the device. Encode only version.
 * PARAMETERS:  SincBuffer *buf    - where to put the encoded packet.
 *              int channelId      - which channel to use. -1 to use the default channel for this port.
 *              const char *name   - the name of the parameter to get.
 */

void SincEncodeGetParam(SincBuffer *buf, int channelId, const char *name)
{
    // Create the packet.
    SiToro__Sinc__GetParamCommand getParamMsg;
    si_toro__sinc__get_param_command__init(&getParamMsg);
    getParamMsg.key = (char *)name;
    if (channelId >= 0)
    {
        getParamMsg.has_channelid = true;
        getParamMsg.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__get_param_command__get_packed_size(&getParamMsg);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__GET_PARAM_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__get_param_command__pack_to_buffer(&getParamMsg, cBuf);
}


/*
 * NAME:        SincEncodeGetParams
 * ACTION:      Gets named parameters from the device. Encode only version.
 * PARAMETERS:  SincBuffer *buf    - where to put the encoded packet.
 *              int *channelIds    - which channel to use for each name.
 *              const char **names - the names of the parameters to get.
 *              int numNames       - the number of names in "names".
 */

void SincEncodeGetParams(SincBuffer *buf, int *channelIds, const char **names, int numNames)
{
    int i;
    SiToro__Sinc__KeyValue *kvs = malloc(sizeof(SiToro__Sinc__KeyValue) * numNames);
    SiToro__Sinc__KeyValue **chanKeys = malloc(sizeof(SiToro__Sinc__KeyValue *) * numNames);

    // Create the packet.
    SiToro__Sinc__GetParamCommand getParamMsg;
    si_toro__sinc__get_param_command__init(&getParamMsg);
    getParamMsg.n_chankeys = numNames;
    getParamMsg.chankeys = chanKeys;

    for (i = 0; i < numNames; i++)
    {
        si_toro__sinc__key_value__init(&kvs[i]);

        if (channelIds[i] >= 0)
        {
            kvs[i].has_channelid = true;
            kvs[i].channelid = channelIds[i];
        }

        kvs[i].key = (char *)names[i];
        chanKeys[i] = &kvs[i];
    }

    if (numNames > 0 && channelIds[0] >= 0)
    {
        getParamMsg.has_channelid = true;
        getParamMsg.channelid = channelIds[0];
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__get_param_command__get_packed_size(&getParamMsg);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__GET_PARAM_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__get_param_command__pack_to_buffer(&getParamMsg, cBuf);
    free(kvs);
    free(chanKeys);
}


/*
 * NAME:        SincEncodeSetParam
 * ACTION:      Requests setting a named parameter on the device but doesn't wait for a response.
 *              Encode only version.
 * PARAMETERS:  SincBuffer *buf          - where to put the encoded packet.
 *              int channelId            - which channel to use. -1 to use the default channel for this port.
 *              SiToro__Sinc__KeyValue *param       - the key and value to set.
 *                  Use si_toro__sinc__key_value__init() to initialise the struct.
 *                  Set the key in param->key.
 *                  Set the value type in param->valueCase and the value in one of intval,
 *                  floatval, boolval, strval or optionval.
 */

void SincEncodeSetParam(SincBuffer *buf, int channelId, SiToro__Sinc__KeyValue *param)
{
    // Create the packet.
    SiToro__Sinc__SetParamCommand setParamMsg;
    si_toro__sinc__set_param_command__init(&setParamMsg);
    setParamMsg.param = param;

    if (channelId >= 0)
    {
        setParamMsg.has_channelid = true;
        setParamMsg.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__set_param_command__get_packed_size(&setParamMsg);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__SET_PARAM_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__set_param_command__pack_to_buffer(&setParamMsg, cBuf);
}


/*
 * NAME:        SincEncodeSetParams
 * ACTION:      Requests setting named parameters on the device but doesn't wait for a response.
 *              Encode only version.
 * PARAMETERS:  SincBuffer *buf          - where to put the encoded packet.
 *              int channelId            - which channel to use. -1 to use the default channel for this port.
 *              SiToro__Sinc__KeyValue *param       - the key and value to set.
 *                  Use si_toro__sinc__key_value__init() to initialise the struct.
 *                  Set the key in param->key.
 *                  Set the value type in param->valueCase and the value in one of intval,
 *                  floatval, boolval, strval or optionval.
 *              int numParams            - the number of parameters to set.
 */

void SincEncodeSetParams(SincBuffer *buf, int channelId, SiToro__Sinc__KeyValue *params, int numParams)
{
    // Create the packet.
    int i;
    SiToro__Sinc__KeyValue **paramBuf;
    SiToro__Sinc__SetParamCommand setParamMsg;
    si_toro__sinc__set_param_command__init(&setParamMsg);

    paramBuf = calloc((size_t)numParams, sizeof(SiToro__Sinc__KeyValue *));
    for (i = 0; i < numParams; i++)
    {
        paramBuf[i] = &params[i];
    }

    setParamMsg.params = paramBuf;
    setParamMsg.n_params = (size_t)numParams;

    if (channelId >= 0)
    {
        setParamMsg.has_channelid = true;
        setParamMsg.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__set_param_command__get_packed_size(&setParamMsg);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__SET_PARAM_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__set_param_command__pack_to_buffer(&setParamMsg, cBuf);

    free(paramBuf);
}


/*
 * NAME:        SincEncodeStartCalibration
 * ACTION:      Requests a calibration. Encode only version.
 * PARAMETERS:  SincBuffer *buf                 - where to put the encoded packet.
 *              int channelId                   - which channel to use. -1 to use the default channel for this port.
 */

void SincEncodeStartCalibration(SincBuffer *buf, int channelId)
{
    // Create the packet.
    SiToro__Sinc__StartCalibrationCommand startCalibrationMsg;
    si_toro__sinc__start_calibration_command__init(&startCalibrationMsg);
    if (channelId >= 0)
    {
        startCalibrationMsg.has_channelid = true;
        startCalibrationMsg.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__start_calibration_command__get_packed_size(&startCalibrationMsg);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__START_CALIBRATION_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__start_calibration_command__pack_to_buffer(&startCalibrationMsg, cBuf);
}


/*
 * NAME:        SincEncodeGetCalibration
 * ACTION:      Gets the calibration data from a previous calibration. Encode only version.
 * PARAMETERS:  SincBuffer *buf                 - where to put the encoded packet.
 *              int channelId                   - which channel to use. -1 to use the default channel for this port.
 */

void SincEncodeGetCalibration(SincBuffer *buf, int channelId)
{
    // Create the packet.
    SiToro__Sinc__GetCalibrationCommand getCalibrationMsg;
    si_toro__sinc__get_calibration_command__init(&getCalibrationMsg);
    if (channelId >= 0)
    {
        getCalibrationMsg.has_channelid = true;
        getCalibrationMsg.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__get_calibration_command__get_packed_size(&getCalibrationMsg);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__GET_CALIBRATION_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__get_calibration_command__pack_to_buffer(&getCalibrationMsg, cBuf);
}


/*
 * NAME:        SincEncodeSetCalibration
 * ACTION:      Sets the calibration data on the device from a previously acquired data set.
 *              Encode only version.
 * PARAMETERS:  SincBuffer *buf                     - where to put the encoded packet.
 *              int channelId                       - which channel to use. -1 to use the default channel for this port.
 *              SincCalibrationData *calibData      - the calibration data to set.
 *              SincPlot *example, model, final     - the pulse shapes.
 */

void SincEncodeSetCalibration(SincBuffer *buf, int channelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    // Create the packet.
    SiToro__Sinc__SetCalibrationCommand setCalibrationMsg;
    si_toro__sinc__set_calibration_command__init(&setCalibrationMsg);
    setCalibrationMsg.has_data = true;
    setCalibrationMsg.data.len = (size_t)calibData->len;
    setCalibrationMsg.data.data = calibData->data;
    setCalibrationMsg.n_examplex = (size_t)example->len;
    setCalibrationMsg.examplex = example->x;
    setCalibrationMsg.n_exampley = (size_t)example->len;
    setCalibrationMsg.exampley = example->y;
    setCalibrationMsg.n_modelx = (size_t)model->len;
    setCalibrationMsg.modelx = model->x;
    setCalibrationMsg.n_modely = (size_t)model->len;
    setCalibrationMsg.modely = model->y;
    setCalibrationMsg.n_finalx = (size_t)final->len;
    setCalibrationMsg.finalx = final->x;
    setCalibrationMsg.n_finaly = (size_t)final->len;
    setCalibrationMsg.finaly = final->y;

    if (channelId >= 0)
    {
        setCalibrationMsg.has_channelid = true;
        setCalibrationMsg.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__set_calibration_command__get_packed_size(&setCalibrationMsg);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__SET_CALIBRATION_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__set_calibration_command__pack_to_buffer(&setCalibrationMsg, cBuf);
}


/*
 * NAME:        SincEncodeCalculateDcOffset
 * ACTION:      Calculates the DC offset on the device. Encode only version.
 * PARAMETERS:  SincBuffer *buf  - where to put the encoded packet.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 */

void SincEncodeCalculateDcOffset(SincBuffer *buf, int channelId)
{
    // Create the packet.
    SiToro__Sinc__CalculateDcOffsetCommand calculateDcOffsetMsg;
    si_toro__sinc__calculate_dc_offset_command__init(&calculateDcOffsetMsg);

    if (channelId >= 0)
    {
        calculateDcOffsetMsg.has_channelid = true;
        calculateDcOffsetMsg.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__calculate_dc_offset_command__get_packed_size(&calculateDcOffsetMsg);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__CALCULATE_DC_OFFSET_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__calculate_dc_offset_command__pack_to_buffer(&calculateDcOffsetMsg, cBuf);
}


/*
 * NAME:        SincEncodeStartOscilloscope
 * ACTION:      Starts the oscilloscope. Encode only version.
 * PARAMETERS:  SincBuffer *buf     - where to put the encoded packet.
 *              int channelId       - which channel to use. -1 to use the default channel for this port.
 */

void SincEncodeStartOscilloscope(SincBuffer *buf, int channelId)
{
    // Create the packet.
    SiToro__Sinc__StartOscilloscopeCommand startOscilloscopeCmd;
    si_toro__sinc__start_oscilloscope_command__init(&startOscilloscopeCmd);
    startOscilloscopeCmd.reserved = 8192;   // For backward compatibility.

    if (channelId >= 0)
    {
        startOscilloscopeCmd.has_channelid = true;
        startOscilloscopeCmd.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__start_oscilloscope_command__get_packed_size(&startOscilloscopeCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__START_OSCILLOSCOPE_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__start_oscilloscope_command__pack_to_buffer(&startOscilloscopeCmd, cBuf);
}


/*
 * NAME:        SincEncodeStartHistogram
 * ACTION:      Starts the histogram. Encode only version.
 * PARAMETERS:  SincBuffer *buf              - where to put the encoded packet.
 *              int channelId                - which channel to use. -1 to use the default channel for this port.
 */

void SincEncodeStartHistogram(SincBuffer *buf, int channelId)
{
    // Create the packet.
    SiToro__Sinc__StartHistogramCommand startHistogramCmd;
    si_toro__sinc__start_histogram_command__init(&startHistogramCmd);
    startHistogramCmd.reserved = 4096;   // For backward compatibility.

    if (channelId >= 0)
    {
        startHistogramCmd.has_channelid = true;
        startHistogramCmd.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__start_histogram_command__get_packed_size(&startHistogramCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__START_HISTOGRAM_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__start_histogram_command__pack_to_buffer(&startHistogramCmd, cBuf);
}


/*
 * NAME:        SincEncodeStartFFT
 * ACTION:      Starts FFT histogram capture. Encode only version.
 * PARAMETERS:  SincBuffer *buf  - where to put the encoded packet.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 */

void SincEncodeStartFFT(SincBuffer *buf, int channelId)
{
    // Create the packet.
    SiToro__Sinc__StartFFTCommand startFFTCmd;
    si_toro__sinc__start_fftcommand__init(&startFFTCmd);

    if (channelId >= 0)
    {
        startFFTCmd.has_channelid = true;
        startFFTCmd.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__start_fftcommand__get_packed_size(&startFFTCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__START_FFT_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__start_fftcommand__pack_to_buffer(&startFFTCmd, cBuf);
}


/*
 * NAME:        SincEncodeClearHistogramData
 * ACTION:      Clears the histogram counts. Encode only version.
 * PARAMETERS:  SincBuffer *buf  - where to put the encoded packet.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 */

void SincEncodeClearHistogramData(SincBuffer *buf, int channelId)
{
    // Create the packet.
    SiToro__Sinc__ClearHistogramCommand clearHistogramCmd;
    si_toro__sinc__clear_histogram_command__init(&clearHistogramCmd);

    if (channelId >= 0)
    {
        clearHistogramCmd.has_channelid = true;
        clearHistogramCmd.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__clear_histogram_command__get_packed_size(&clearHistogramCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__CLEAR_HISTOGRAM_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__clear_histogram_command__pack_to_buffer(&clearHistogramCmd, cBuf);
}


/*
 * NAME:        SincEncodeStartListMode
 * ACTION:      Starts list mode. Encode only version.
 * PARAMETERS:  SincBuffer *buf  - where to put the encoded packet.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 */

void SincEncodeStartListMode(SincBuffer *buf, int channelId)
{
    // Create the packet.
    SiToro__Sinc__StartListModeCommand startListModeCmd;
    si_toro__sinc__start_list_mode_command__init(&startListModeCmd);

    if (channelId >= 0)
    {
        startListModeCmd.has_channelid = true;
        startListModeCmd.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__start_list_mode_command__get_packed_size(&startListModeCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__START_LIST_MODE_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__start_list_mode_command__pack_to_buffer(&startListModeCmd, cBuf);
}


/*
 * NAME:        SincEncodeStopDataAcquisition
 * ACTION:      Deprecated in favor of SincEncodeStop.
 *              Stops oscilloscope / histogram / list mode / calibration. Encode only version.
 * PARAMETERS:  SincBuffer *buf  - where to put the encoded packet.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 */

void SincEncodeStopDataAcquisition(SincBuffer *buf, int channelId)
{
    SincEncodeStop(buf, channelId, false);
}


/*
 * NAME:        SincEncodeStop
 * ACTION:      Stops oscilloscope / histogram / list mode / calibration.  Allows skipping of the
 *              optional optimisation phase of calibration. Encode only version.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 *              bool skip        - true to skip the optimisation phase of calibration but still keep
 *                                 the calibration.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

void SincEncodeStop(SincBuffer *buf, int channelId, bool skip)
{
    // Create the packet.
    SiToro__Sinc__StopDataAcquisitionCommand stopCmd;
    si_toro__sinc__stop_data_acquisition_command__init(&stopCmd);

    if (channelId >= 0)
    {
        stopCmd.has_channelid = true;
        stopCmd.channelid = channelId;
    }

    if (skip)
    {
        stopCmd.has_skip = true;
        stopCmd.skip = true;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__stop_data_acquisition_command__get_packed_size(&stopCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__STOP_DATA_ACQUISITION_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__stop_data_acquisition_command__pack_to_buffer(&stopCmd, cBuf);
}


/*
 * NAME:        SincEncodeListParamDetails
 * ACTION:      Returns a list of matching device parameters and their details. Encode only version.
 * PARAMETERS:  SincBuffer *buf          - where to put the encoded packet.
 *              int channelId            - which channel to use. -1 to use the default channel for this port.
 *              const char *matchPrefix  - a key prefix to match. Only matching keys are returned. Empty for all keys.
 */

void SincEncodeListParamDetails(SincBuffer *buf, int channelId, const char *matchPrefix)
{
    // Create the packet.
    SiToro__Sinc__ListParamDetailsCommand listDetailsMsg;
    si_toro__sinc__list_param_details_command__init(&listDetailsMsg);
    listDetailsMsg.matchprefix = (char *)matchPrefix;

    if (channelId >= 0)
    {
        listDetailsMsg.has_channelid = true;
        listDetailsMsg.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__list_param_details_command__get_packed_size(&listDetailsMsg);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__LIST_PARAM_DETAILS_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__list_param_details_command__pack_to_buffer(&listDetailsMsg, cBuf);
}


/*
 * NAME:        SincEncodeRestart
 * ACTION:      Restarts the instrument. Encode only version.
 * PARAMETERS:  SincBuffer *buf  - where to put the encoded packet.
 */

void SincEncodeRestart(SincBuffer *buf)
{
    // Create the packet.
    SiToro__Sinc__RestartCommand restartCmd;
    si_toro__sinc__restart_command__init(&restartCmd);

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__restart_command__get_packed_size(&restartCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__RESTART_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__restart_command__pack_to_buffer(&restartCmd, cBuf);
}


/*
 * NAME:        SincEncodeResetSpatialSystem
 * ACTION:      Resets the spatial system. Encode only version.
 * PARAMETERS:  SincBuffer *buf  - where to put the encoded packet.
 */

void SincEncodeResetSpatialSystem(SincBuffer *buf)
{
    // Create the packet.
    SiToro__Sinc__ResetSpatialSystemCommand rssCmd;
    si_toro__sinc__reset_spatial_system_command__init(&rssCmd);

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__reset_spatial_system_command__get_packed_size(&rssCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__RESET_SPATIAL_SYSTEM_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__reset_spatial_system_command__pack_to_buffer(&rssCmd, cBuf);
}


/*
 * NAME:        SincEncodeSoftwareUpdate
 * ACTION:      Updates the software on the device. Encode only version.
 * PARAMETERS:  SincBuffer *buf   - where to put the encoded packet.
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
 *              int autoRestart    - if true will reboot when the update is complete.
 */

void SincEncodeSoftwareUpdate(SincBuffer *buf, const uint8_t *appImage, int appImageLen, const char *appChecksum, const uint8_t *fpgaImage, int fpgaImageLen, const char *fpgaChecksum, const SincSoftwareUpdateFile *updateFiles, int updateFilesLen, int autoRestart)
{
    SiToro__Sinc__SoftwareUpdateCommand suCmd;
    SiToro__Sinc__SoftwareUpdateFile *protoUpdateFiles;
    SiToro__Sinc__SoftwareUpdateFile **protoUpdateFilePtrs;

    // Create the packet.
    si_toro__sinc__software_update_command__init(&suCmd);
    if (appImage != NULL && appChecksum != NULL)
    {
        suCmd.appimage.data = (uint8_t *)appImage;
        suCmd.appimage.len = (size_t)appImageLen;
        suCmd.has_appimage = true;
        suCmd.appchecksum = (char *)appChecksum;
    }

    if (fpgaImage != NULL && fpgaChecksum != NULL)
    {
        suCmd.fpgaimage.data = (uint8_t *)fpgaImage;
        suCmd.fpgaimage.len = (size_t)fpgaImageLen;
        suCmd.has_fpgaimage = true;
        suCmd.fpgachecksum = (char *)fpgaChecksum;
    }

    suCmd.autorestart = autoRestart;
    suCmd.has_autorestart = true;

    protoUpdateFiles = malloc((size_t)updateFilesLen * sizeof(SiToro__Sinc__SoftwareUpdateFile));
    protoUpdateFilePtrs = malloc((size_t)updateFilesLen * sizeof(SiToro__Sinc__SoftwareUpdateFile*));

    int i;
    for (i = 0; i < updateFilesLen; i++)
    {
        protoUpdateFiles[i].filename = updateFiles->fileName;
        protoUpdateFiles[i].has_content = true;
        protoUpdateFiles[i].content.data = updateFiles->content;
        protoUpdateFiles[i].content.len = (size_t)updateFiles->contentLen;
        protoUpdateFilePtrs[i] = &protoUpdateFiles[i];
    }

    suCmd.n_updatefiles = (size_t)updateFilesLen;
    suCmd.updatefiles = protoUpdateFilePtrs;

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__software_update_command__get_packed_size(&suCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__SOFTWARE_UPDATE_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__software_update_command__pack_to_buffer(&suCmd, cBuf);

    free(protoUpdateFiles);
    free(protoUpdateFilePtrs);
}


/*
 * NAME:        SincEncodeSaveConfiguration
 * ACTION:      Saves the channel's current configuration to use as default settings on startup.
 *              Encode only version.
 * PARAMETERS:  SincBuffer *buf  - where to put the encoded packet.
 *              int channelId    - which channel to use. -1 to use the default channel for this port.
 */

void SincEncodeSaveConfiguration(SincBuffer *buf, int channelId)
{
    // Create the packet.
    SiToro__Sinc__SaveConfigurationCommand scCmd;
    si_toro__sinc__save_configuration_command__init(&scCmd);

    if (channelId >= 0)
    {
        scCmd.has_channelid = true;
        scCmd.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__save_configuration_command__get_packed_size(&scCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__SAVE_CONFIGURATION_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__save_configuration_command__pack_to_buffer(&scCmd, cBuf);
}


/*
 * NAME:        SincEncodeMonitorChannels
 * ACTION:      Tells the card which channels this connection is interested in. Encode only version.
 *              Asynchronous events like oscilloscope and histogram data will only
 *              be sent for monitored channels.
 * PARAMETERS:  SincBuffer *buf  - where to put the encoded packet.
 *              int *channelSet - a list of the channels to monitor.
 *              int numChannels - the number of channels in the list.
 */

void SincEncodeMonitorChannels(SincBuffer *buf, const int *channelSet, int numChannels)
{
    // Create a buffer to write into.
    int32_t *channelid;
    int i;

    channelid = malloc((size_t)numChannels * sizeof(int32_t));

    // Create the packet.
    SiToro__Sinc__MonitorChannelsCommand mcCmd;
    si_toro__sinc__monitor_channels_command__init(&mcCmd);
    for (i = 0; i < numChannels; i++)
        channelid[i] = channelSet[i];

    mcCmd.n_channelid = (size_t)numChannels;
    mcCmd.channelid = channelid;

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__monitor_channels_command__get_packed_size(&mcCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__MONITOR_CHANNELS_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__monitor_channels_command__pack_to_buffer(&mcCmd, cBuf);

    // Free memory.
    free(channelid);
}


/*
 * NAME:        SincEncodeSuccessResponse
 * ACTION:      Encodes a response packet indicating success or failure.
 * PARAMETERS:  SincBuffer *buf   - where to put the encoded packet.
 *              SiToro__Sinc__ErrorCode errorCode - the error code or SI_TORO__SINC__ERROR_CODE__NO_ERROR if none.
 *              char *message - the message or NULL if none.
 *              int channelId - the channel id or -1 if none.
 */

void SincEncodeSuccessResponse(SincBuffer *buf, SiToro__Sinc__ErrorCode errorCode, char *message, int channelId)
{
    // Create the packet.
    SiToro__Sinc__SuccessResponse successResp;
    si_toro__sinc__success_response__init(&successResp);
    if (errorCode != SI_TORO__SINC__ERROR_CODE__NO_ERROR)
    {
        successResp.has_errorcode = true;
        successResp.errorcode = errorCode;
    }

    if (message != NULL)
    {
        successResp.message = message;
    }

    if (channelId >= 0)
    {
        successResp.has_channelid = true;
        successResp.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__success_response__get_packed_size(&successResp);
    SincProtocolEncodeHeaderGeneric(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__SUCCESS_RESPONSE, SINC_RESPONSE_MARKER);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__success_response__pack_to_buffer(&successResp, cBuf);
}


/*
 * NAME:        SincEncodeProbeDatagram
 * ACTION:      Encodes a message to request a probe datagram to be sent back.
 * PARAMETERS:  SincBuffer *buf  - where to put the encoded packet.
 */

void SincEncodeProbeDatagram(SincBuffer *buf)
{
    // Create the packet.
    SiToro__Sinc__ProbeDatagramCommand pdCmd;
    si_toro__sinc__probe_datagram_command__init(&pdCmd);

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__probe_datagram_command__get_packed_size(&pdCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__PROBE_DATAGRAM_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__probe_datagram_command__pack_to_buffer(&pdCmd, cBuf);
}


/*
 * NAME:        SincEncodeCheckParamConsistency
 * ACTION:      Encodes a message to check parameters for consistency.
 *              int channelId    - which channel to check. -1 for all channels.
 * PARAMETERS:  SincBuffer *buf  - where to put the encoded packet.
 */

void SincEncodeCheckParamConsistency(SincBuffer *buf, int channelId)
{
    // Create the packet.
    SiToro__Sinc__CheckParamConsistencyCommand cpcCmd;
    si_toro__sinc__check_param_consistency_command__init(&cpcCmd);

    if (channelId >= 0)
    {
        cpcCmd.has_channelid = true;
        cpcCmd.channelid = channelId;
    }

    // Encode it.
    uint8_t headerBuf[SINC_HEADER_LENGTH];
    size_t payloadLen = si_toro__sinc__check_param_consistency_command__get_packed_size(&cpcCmd);
    SincProtocolEncodeHeader(headerBuf, (int)payloadLen, SI_TORO__SINC__MESSAGE_TYPE__CHECK_PARAM_CONSISTENCY_COMMAND);

    ProtobufCBuffer *cBuf = &buf->base;
    cBuf->append(cBuf, SINC_HEADER_LENGTH, headerBuf);
    si_toro__sinc__check_param_consistency_command__pack_to_buffer(&cpcCmd, cBuf);
}
