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


#define SINC_UDP_HISTOGRAM_HEADER_SIZE_PROTOCOL_0 110

#define MIN(a,b) (((a)<(b))?(a):(b))


/*
 * NAME:        SincDecodeSuccessResponse
 * ACTION:      Decodes a success response from the device. May or may not wait depending on
 *              the timeout.
 * PARAMETERS:  SincError *err                            - the sinc error structure.
 *              SincBuffer *packet                        - the de-encapsulated packet to decode.
 *              SiToro__Sinc__SuccessResponse **resp      - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__success_response__free_unpacked(resp, NULL) after use.
 *              int *fromChannelId                        - set to the received channel id. NULL to not use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeSuccessResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__SuccessResponse **resp, int *fromChannelId)
{
    int ok;
    SiToro__Sinc__SuccessResponse *r = si_toro__sinc__success_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);
    if (resp != NULL)
        *resp = r;

    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted success packet");
        return false;
    }

    if (fromChannelId != NULL && r->has_channelid)
        *fromChannelId = r->channelid + packet->channelIdOffset;

    ok = SincInterpretSuccessError(err, r);

    if (resp == NULL)
        si_toro__sinc__success_response__free_unpacked(r, NULL);

    return ok;
}


/*
 * NAME:        SincDecodeGetParamResponse
 * ACTION:      Decodes a get parameters response from the device.
 * PARAMETERS:  SincError *err                            - the sinc error structure.
 *              SincBuffer *packet                        - the de-encapsulated packet to decode.
 *              SiToro__Sinc__GetParamResponse **resp     - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__get_param_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeGetParamResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__GetParamResponse **resp, int *fromChannelId)
{
    int ok;
    SiToro__Sinc__GetParamResponse *r = si_toro__sinc__get_param_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);
    if (resp != NULL)
        *resp = r;

    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted get parameter packet");
        return false;
    }

    if (fromChannelId != NULL && r->has_channelid)
        *fromChannelId = r->channelid + packet->channelIdOffset;

    ok = SincInterpretSuccessError(err, r->success);

    if (resp == NULL)
        si_toro__sinc__get_param_response__free_unpacked(r, NULL);

    return ok;
}


/*
 * NAME:        SincDecodeParamUpdatedResponse
 * ACTION:      Decodes a "parameter updated" asynchronous message.
 * PARAMETERS:  SincError *err                            - the sinc error structure.
 *              SincBuffer *packet                        - the de-encapsulated packet to decode.
 *              SiToro__Sinc__GetParamResponse **resp     - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__param_updated_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeParamUpdatedResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__ParamUpdatedResponse **resp, int *fromChannelId)
{
    SiToro__Sinc__ParamUpdatedResponse *r = si_toro__sinc__param_updated_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);
    if (resp != NULL)
        *resp = r;

    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted parameter updated packet");
        return false;
    }

    if (fromChannelId != NULL && r->has_channelid)
        *fromChannelId = r->channelid + packet->channelIdOffset;

    if (resp == NULL)
        si_toro__sinc__param_updated_response__free_unpacked(r, NULL);

    return true;
}


/*
 * NAME:        SincDecodeCalibrationProgressResponse
 * ACTION:      Decodes a get parameters response from the device.
 * PARAMETERS:  SincError *err                            - the sinc error structure.
 *              SincBuffer *packet                        - the de-encapsulated packet to decode.
 *              SiToro__Sinc__CalibrationProgressResponse **resp      - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__calbration_progress_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeCalibrationProgressResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CalibrationProgressResponse **resp, double *progress, int *complete, char **stage, int *fromChannelId)
{
    int ok;
    SiToro__Sinc__CalibrationProgressResponse *r = si_toro__sinc__calibration_progress_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);
    if (resp != NULL)
        *resp = r;

    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted calibration progress packet");
        return false;
    }

    if (progress != NULL && r->has_progress)
        *progress = r->progress;

    if (complete != NULL && r->has_complete)
        *complete = r->complete;

    if (stage != NULL && r->stage != NULL)
        *stage = strdup(r->stage);

    if (fromChannelId != NULL && r->has_channelid)
        *fromChannelId = r->channelid + packet->channelIdOffset;

    ok = SincInterpretSuccessError(err, r->success);

    if (resp == NULL)
        si_toro__sinc__calibration_progress_response__free_unpacked(r, NULL);

    return ok;
}


/*
 * NAME:        SincDecodeGetCalibrationResponse
 * ACTION:      Decode a get calibration response from the device.
 * PARAMETERS:  Sinc *sc                                     - the sinc connection.
 *              SincBuffer *packet                           - the de-encapsulated packet to decode.
 *              SiToro__Sinc__GetCalibrationResponse **resp  - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__get_calibration_response__free_unpacked(resp, NULL) after use.
 *              SincCalibrationData *calibData  - the calibration data is stored here.
 *              calibData must be free()d after use.
 *              SincPlot *example, model, final - the pulse shapes are set here.
 *              The fields x and y of each pulse must be free()d after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeGetCalibrationResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__GetCalibrationResponse **resp, int *fromChannelId, SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    // Clear the results.
    if (calibData != NULL)
        memset(calibData, 0, sizeof(*calibData));

    if (model != NULL)
        memset(model, 0, sizeof(*model));

    if (example != NULL)
        memset(example, 0, sizeof(*example));

    if (final != NULL)
        memset(final, 0, sizeof(*final));

    // Unpack the packet.
    SiToro__Sinc__GetCalibrationResponse *r = si_toro__sinc__get_calibration_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);
    if (resp != NULL)
        *resp = r;

    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted calibration data packet");
        return false;
    }

    // Check success.
    if (!SincInterpretSuccessError(err, r->success))
    {
        si_toro__sinc__get_calibration_response__free_unpacked(r, NULL);
        if (resp)
        {
            *resp = NULL;
        }

        return false;
    }

    if (fromChannelId != NULL && r->has_channelid)
        *fromChannelId = r->channelid + packet->channelIdOffset;

    // Convert the data.
    if (calibData != NULL)
    {
        if (r->has_data)
        {
            calibData->len = (int)r->data.len;
            calibData->data = malloc((size_t)calibData->len);
            if (calibData->data == NULL)
            {
                SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                si_toro__sinc__get_calibration_response__free_unpacked(r, NULL);
                return false;
            }

            memcpy(calibData->data, r->data.data, (size_t)calibData->len);
        }
    }

    // Copy the calibration pulses.
    if ( (example != NULL && !SincCopyCalibrationPulse(err, example, (int)r->n_exampley, r->examplex, r->exampley)) ||
         (model != NULL   && !SincCopyCalibrationPulse(err, model,   (int)r->n_modely,   r->modelx,   r->modely)) ||
         (final != NULL   && !SincCopyCalibrationPulse(err, final,   (int)r->n_finaly,   r->finalx,   r->finaly)) )
    {
        si_toro__sinc__get_calibration_response__free_unpacked(r, NULL);
        SincSFreeCalibration(calibData, example, model, final);
        return false;
    }

    // Clean up.
    if (resp == NULL)
        si_toro__sinc__get_calibration_response__free_unpacked(r, NULL);

    return true;
}


/*
 * NAME:        SincDecodeCalculateDCOffsetResponse
 * ACTION:      Decodes a calculate DC offset response from the device.
 * PARAMETERS:  Sinc *sc                            - the channel to listen to.
 *              SincBuffer *packet                  - the de-encapsulated packet to decode.
 *              SiToro__Sinc__CalculateDCOffsetResponse **resp      - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__calculate_dc_offset_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeCalculateDCOffsetResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CalculateDcOffsetResponse **resp, double *dcOffset, int *fromChannelId)
{
    int ok;
    SiToro__Sinc__CalculateDcOffsetResponse *r = si_toro__sinc__calculate_dc_offset_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);
    if (resp != NULL)
        *resp = r;

    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted calculate dc offset packet");
        return false;
    }

    if (dcOffset != NULL && r->has_dcoffset)
        *dcOffset = r->dcoffset;

    if (fromChannelId != NULL && r->has_channelid)
        *fromChannelId = r->channelid + packet->channelIdOffset;

    ok = SincInterpretSuccessError(err, r->success);

    // Clean up.
    if (resp == NULL)
        si_toro__sinc__calculate_dc_offset_response__free_unpacked(r, NULL);

    return ok;
}


/*
 * NAME:        SincDecodeListParamDetailsResponse
 * ACTION:      Decodes a get parameters response from the device.
 * PARAMETERS:  Sinc *sc                                      - the channel to listen to.
 *              SincBuffer *packet                            - the de-encapsulated packet to decode.
 *              SiToro__Sinc__ListParamDetailsResponse **resp - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__list_param_details_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeListParamDetailsResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__ListParamDetailsResponse **resp, int *fromChannelId)
{
    int ok;
    SiToro__Sinc__ListParamDetailsResponse *r = si_toro__sinc__list_param_details_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);
    if (resp != NULL)
        *resp = r;

    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted parameter details packet");
        return false;
    }

    if (fromChannelId != NULL && r->has_channelid)
        *fromChannelId = r->channelid + packet->channelIdOffset;

    ok = SincInterpretSuccessError(err, r->success);

    if (resp == NULL)
        si_toro__sinc__list_param_details_response__free_unpacked(r, NULL);

    return ok;
}


/*
 * NAME:        SincDecodeSynchronizeLogResponse
 * ACTION:      Decodes a synchronize log response from the device.
 * PARAMETERS:  Sinc *sc                                    - the channel to listen to.
 *              SincBuffer *packet                          - the de-encapsulated packet to decode.
 *              SiToro__Sinc__SynchronizeLogResponse **resp - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__synchronize_log_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeSynchronizeLogResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__SynchronizeLogResponse **resp)
{
    int ok;
    SiToro__Sinc__SynchronizeLogResponse *r = si_toro__sinc__synchronize_log_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);
    if (resp != NULL)
        *resp = r;

    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted synchronize log packet");
        return false;
    }

    ok = SincInterpretSuccessError(err, r->success);

    if (resp == NULL)
        si_toro__sinc__synchronize_log_response__free_unpacked(r, NULL);

    return ok;
}


/*
 * NAME:        SincDecodeMonitorChannelsCommand
 * ACTION:      Decodes a monitor channels command from the device.
 * PARAMETERS:  Sinc *sc                                      - the channel to listen to.
 *              SincBuffer *packet                            - the de-encapsulated packet to decode.
 *              SiToro__Sinc__ListParamDetailsResponse **resp - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__list_param_details_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeMonitorChannelsCommand(SincError *err, SincBuffer *packet, uint64_t *channelBitSet)
{
    unsigned int i;
    SiToro__Sinc__MonitorChannelsCommand *cmd = si_toro__sinc__monitor_channels_command__unpack(NULL, packet->cbuf.len, packet->cbuf.data);

    if (cmd == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted monitor channels packet");
        return false;
    }

    *channelBitSet = 0;
    for (i = 0; i < cmd->n_channelid; i++)
        *channelBitSet |= (((uint64_t)1) << cmd->channelid[i]);

    si_toro__sinc__monitor_channels_command__free_unpacked(cmd, NULL);

    return true;
}


/*
 * NAME:        SincInterpretSuccessError
 * ACTION:      Intepret a success response and assign channel error codes accordingly.
 * PARAMETERS:  SincError *err - the sinc error structure.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincInterpretSuccessError(SincError *err, SiToro__Sinc__SuccessResponse *success)
{
    SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__NO_ERROR);
    if (success->has_errorcode)
    {
        // Set the new error code.
        if (success->message == NULL)
            SincErrorSetCode(err, success->errorcode);
        else
            SincErrorSetMessage(err, success->errorcode, success->message);

        return false;
    }

    return true;
}


/*
 * NAME:        SincInterpretSuccess
 * ACTION:      Intepret a success response and assign channel error codes accordingly.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincInterpretSuccess(Sinc *sc, SiToro__Sinc__SuccessResponse *success)
{
    int ok = SincInterpretSuccessError(&sc->readErr, success);
    if (!ok)
        sc->err = &sc->readErr;

    return ok;
}


/*
 * NAME:        SincCopyCalibrationPulse
 * ACTION:      Copies calibration pulse data across to a SincCalibrationPulse structure.
 *              Will allocate memory for fields x and y so you must free it.
 * PARAMETERS:  SincPlot *pulse - the pulse to copy it across to.
 *              int len - the number of entries in the data.
 *              double *x, y - the data to copy from.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincCopyCalibrationPulse(SincError *err, SincCalibrationPlot *pulse, int len, double *x, double *y)
{
    pulse->len = len;
    pulse->x = malloc((size_t)len * sizeof(double));
    if (pulse->x == NULL)
    {
        SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    pulse->y = malloc((size_t)len * sizeof(double));
    if (pulse->y == NULL)
    {
        free(pulse->x);
        SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    memcpy(pulse->x, x, (size_t)len * sizeof(double));
    memcpy(pulse->y, y, (size_t)len * sizeof(double));

    return true;
}


/*
 * NAME:        SincSFreeCalibrationPlot
 * ACTION:      Free a calibration plot.
 * PARAMETERS:  SincPlot *plot     - the pulse shape.
 */

static void SincSFreeCalibrationPlot(SincCalibrationPlot *plot)
{
    if (plot != NULL)
    {
        if (plot->x != NULL)
            free(plot->x);

        if (plot->y != NULL)
            free(plot->y);

        memset(plot, 0, sizeof(*plot));
    }
}


/*
 * NAME:        SincSFreeCalibration
 * ACTION:      Free the calibration waveforms.
 * PARAMETERS:  SincCalibrationData *calibData      - the calibration data to free.
 *              SincPlot *example, model, final     - the pulse shapes.
 */

void SincSFreeCalibration(SincCalibrationData *calibData, SincCalibrationPlot *example, SincCalibrationPlot *model, SincCalibrationPlot *final)
{
    if (calibData != NULL)
    {
        if (calibData->data != NULL)
            free(calibData->data);

        memset(calibData, 0, sizeof(*calibData));
    }

    SincSFreeCalibrationPlot(example);
    SincSFreeCalibrationPlot(model);
    SincSFreeCalibrationPlot(final);
}


/*
 * NAME:        SincDecodeOscilloscopeDataResponse
 * ACTION:      Decodes a curve from the oscilloscope. Waits for the next oscilloscope update to arrive.
 * PARAMETERS:  Sinc *sc                  - the sinc connection.
 *              SincBuffer *packet        - the de-encapsulated packet to decode.
 *              int *fromChannelId        - if non-NULL this is set to the channel the histogram was received from.
 *              SincOscPlot *resetBlanked - coordinates of the reset blanked oscilloscope plot. Will allocate resetBlanked->data and resetBlanked->intData so you must free them.
 *              SincOscPlot *rawCurve     - coordinates of the raw oscilloscope plot. Will allocate rawCurve->data and resetBlanked->intData so you must free them.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status. There's no need to free
 *                  resetBlanked or rawCurve data on failure.
 */

bool SincDecodeOscilloscopeDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, uint64_t *dataSetId, SincOscPlot *resetBlanked, SincOscPlot *rawCurve)
{
    uint16_t val_u16;
    uint32_t val_u32;

    if (rawCurve != NULL)
    {
        rawCurve->data = NULL;
        rawCurve->intData = NULL;
    }

    if (resetBlanked != NULL)
    {
        resetBlanked->data = NULL;
        resetBlanked->intData = NULL;
    }

    if (packet->cbuf.len < 2)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted oscilloscope packet");
        goto errorExitDecodeOsc;
    }

    uint32_t protobufHeaderLen = SINC_PROTOCOL_READ_UINT16(packet->cbuf.data);
    unsigned int startPos = 2;
    if (protobufHeaderLen == 0xffff)
    {
        // Extended length protobuf data.
        protobufHeaderLen = SINC_PROTOCOL_READ_UINT32(&packet->cbuf.data[startPos]);
        startPos += sizeof(uint32_t);
    }

    if (protobufHeaderLen + startPos > packet->cbuf.len)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted oscilloscope packet");
        goto errorExitDecodeOsc;
    }

    // Unpack it.
    SiToro__Sinc__OscilloscopeDataResponse *resp = si_toro__sinc__oscilloscope_data_response__unpack(NULL, protobufHeaderLen, &packet->cbuf.data[startPos]);
    if (resp == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted oscilloscope packet");
        goto errorExitDecodeOsc;
    }

    // Get some fields.
    if (fromChannelId != NULL)
    {
        *fromChannelId = -1;
        if (resp->has_channelid)
            *fromChannelId = resp->channelid;
    }

    if (dataSetId != NULL)
    {
        *dataSetId = 0;
        if (resp->has_datasetid)
            *dataSetId = resp->datasetid;
    }

    // Get the int plots.
    size_t numIntPlots = resp->n_plots;
    if (rawCurve != NULL)
    {
        if (numIntPlots > 0 && resp->plots[0]->n_val > 0)
        {
            // Get the "raw adc" int plot.
            rawCurve->len = (int)resp->plots[0]->n_val;
            rawCurve->intData = malloc((size_t)rawCurve->len * sizeof(int32_t));
            if (rawCurve->intData == NULL)
            {
                SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                goto errorExitDecodeOsc;
            }

            memcpy(rawCurve->intData, resp->plots[0]->val, (size_t)rawCurve->len * sizeof(*rawCurve->intData));
        }

        // Get the int value ranges.
        if (resp->has_minvaluerange)
            rawCurve->minRange = resp->minvaluerange;

        if (resp->has_maxvaluerange)
            rawCurve->maxRange = resp->maxvaluerange;
    }

    if (resetBlanked != NULL)
    {
        if (numIntPlots > 1 && resp->plots[1]->n_val > 0)
        {
            // Get the "reset blanked" int plot.
            resetBlanked->len = (int)resp->plots[1]->n_val;
            resetBlanked->intData = malloc((size_t)resetBlanked->len * sizeof(int32_t));
            if (resetBlanked->intData == NULL)
            {
                SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                goto errorExitDecodeOsc;
            }

            memcpy(resetBlanked->intData, resp->plots[1]->val, (size_t)resetBlanked->len * sizeof(*resetBlanked->intData));
        }

        // Get the int value ranges.
        if (resp->has_minvaluerange)
            resetBlanked->minRange = resp->minvaluerange;

        if (resp->has_maxvaluerange)
            resetBlanked->maxRange = resp->maxvaluerange;
    }

    // Get FP plots.
    size_t numFpPlots = resp->n_plotlen;
    if (numFpPlots < 2 && numIntPlots < 2)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted oscilloscope header");
        si_toro__sinc__oscilloscope_data_response__free_unpacked(resp, NULL);
        return false;
    }

    if (numFpPlots >= 2)
    {
        // Get optional FP-format data.
        uint32_t rawDataSamples = resp->plotlen[0];
        uint32_t resetBlankedSamples = resp->plotlen[1];
        si_toro__sinc__oscilloscope_data_response__free_unpacked(resp, NULL);

        uint8_t *rawData = &packet->cbuf.data[protobufHeaderLen + startPos];  // Skip the initial protocol buffer info.
        uint8_t *dpos = rawData;

        // Copy the raw data.
        if (rawCurve != NULL)
        {
            rawCurve->len = (int)rawDataSamples;
            rawCurve->data = malloc(rawDataSamples * sizeof(double));
            if (rawCurve->data == NULL)
            {
                SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                goto errorExitDecodeOsc;
            }

            memcpy(rawCurve->data, dpos, rawDataSamples * sizeof(double));
            dpos += rawDataSamples * sizeof(double);
        }

        // Copy the reset blanked data.
        if (resetBlanked != NULL)
        {
            resetBlanked->len = (int)resetBlankedSamples;
            resetBlanked->data = malloc(resetBlankedSamples * sizeof(double));
            if (resetBlanked->data == NULL)
            {
                SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                goto errorExitDecodeOsc;
            }

            memcpy(resetBlanked->data, dpos, resetBlankedSamples * sizeof(double));
            dpos += resetBlankedSamples * sizeof(double);
        }
    }

    return true;

    /* Using a linux kernel-style cleanup method to make sure resources are freed on any failure. */
errorExitDecodeOsc:
    if (rawCurve != NULL && rawCurve->data != NULL)
    {
        free(rawCurve->data);
        rawCurve->data = NULL;
    }

    if (rawCurve != NULL && rawCurve->intData != NULL)
    {
        free(rawCurve->intData);
        rawCurve->intData = NULL;
    }

    if (resetBlanked != NULL && resetBlanked->data != NULL)
    {
        free(resetBlanked->data);
        resetBlanked->data = NULL;
    }

    if (resetBlanked != NULL && resetBlanked->intData != NULL)
    {
        free(resetBlanked->intData);
        resetBlanked->intData = NULL;
    }

    return false;
}


/*
 * NAME:        SincDecodeOscilloscopeDataResponseAsPlotArray
 * ACTION:      Decodes a capture from the oscilloscope as an array of plots with different data in each plot.
 * PARAMETERS:  Sinc *sc               - the sinc connection.
 *              SincBuffer *packet     - the de-encapsulated packet to decode.
 *              int *fromChannelId     - if non-NULL this is set to the channel the histogram was received from.
 *              SincOscPlot *plotArray - points to an array of oscilloscope plots. Plot 0 is the raw data. Plot 1 is reset blanked.
 *                                       Will allocate plotArray[i]->intData so you must free it.
 *              int maxPlotArray       - the number of SincOscPlots in the array.
 *              int *plotArraySize     - will be set to the number of oscilloscope plots received.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status. There's no need to free
 *                  resetBlanked or rawCurve data on failure.
 */

bool SincDecodeOscilloscopeDataResponseAsPlotArray(SincError *err, SincBuffer *packet, int *fromChannelId, uint64_t *dataSetId, SincOscPlot *plotArray, int maxPlotArray, int *plotArraySize)
{
    uint16_t val_u16;
    uint32_t val_u32;
    size_t   i;
    size_t   numIntPlots = 0;

    // Clear the plots.
    memset(plotArray, 0, sizeof(SincOscPlot) * maxPlotArray);

    if (packet->cbuf.len < 2)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted oscilloscope packet");
        goto errorExitDecodeOsc;
    }

    uint32_t protobufHeaderLen = SINC_PROTOCOL_READ_UINT16(packet->cbuf.data);
    unsigned int startPos = 2;
    if (protobufHeaderLen == 0xffff)
    {
        // Extended length protobuf data.
        protobufHeaderLen = SINC_PROTOCOL_READ_UINT32(&packet->cbuf.data[startPos]);
        startPos += sizeof(uint32_t);
    }

    if (protobufHeaderLen + startPos > packet->cbuf.len)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted oscilloscope packet");
        goto errorExitDecodeOsc;
    }

    // Unpack it.
    SiToro__Sinc__OscilloscopeDataResponse *resp = si_toro__sinc__oscilloscope_data_response__unpack(NULL, protobufHeaderLen, &packet->cbuf.data[startPos]);
    if (resp == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted oscilloscope packet");
        goto errorExitDecodeOsc;
    }

    // Get some fields.
    if (fromChannelId != NULL)
    {
        *fromChannelId = -1;
        if (resp->has_channelid)
            *fromChannelId = resp->channelid;
    }

    if (dataSetId != NULL)
    {
        *dataSetId = 0;
        if (resp->has_datasetid)
            *dataSetId = resp->datasetid;
    }

    // Get the int plots.
    numIntPlots = MIN(resp->n_plots, (size_t)maxPlotArray);
    for (i = 0; i < numIntPlots; i++)
    {
        if (resp->plots[i]->n_val > 0)
        {
            // Get the int plot.
            plotArray[i].len = (int)resp->plots[i]->n_val;
            plotArray[i].intData = malloc((size_t)plotArray[i].len * sizeof(int32_t));
            if (plotArray[i].intData == NULL)
            {
                SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                goto errorExitDecodeOsc;
            }

            memcpy(plotArray[i].intData, resp->plots[i]->val, (size_t)plotArray[i].len * sizeof(*plotArray[i].intData));

            // Get the int value ranges.
            if (resp->has_minvaluerange)
                plotArray[i].minRange = resp->minvaluerange;

            if (resp->has_maxvaluerange)
                plotArray[i].maxRange = resp->maxvaluerange;
        }
    }

    if (plotArraySize)
    {
        *plotArraySize = numIntPlots;
    }

    return true;

    /* Using a linux kernel-style cleanup method to make sure resources are freed on any failure. */
errorExitDecodeOsc:
    for (i = 0; i < numIntPlots; i++)
    {
        if (plotArray[i].intData)
        {
            free(plotArray[i].intData);
            plotArray[i].intData = NULL;
        }

        plotArray[i].len = 0;
    }

    return false;
}


/*
 * NAME:        SincDecodeHistogramDataResponse
 * ACTION:      Decodes an update from the histogram. Waits for the next histogram update to
 *              arrive if timeout is non-zero.
 * PARAMETERS:  Sinc *sc                - the sinc connection.
 *              SincBuffer *packet      - the de-encapsulated packet to decode.
 *              int *fromChannelId      - if non-NULL this is set to the channel the histogram was received from.
 *              SincHistogram *accepted - the accepted histogram plot. Will allocate accepted->data so you must free it.
 *              SincHistogram *rejected - the rejected histogram plot. Will allocate rejected->data so you must free it.
 *              SincHistogramCountStats *stats - various statistics about the histogram. Can be NULL if not needed.
 *                                        May allocate stats->intensity so you should free it if non-NULL.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status. There's no need to free
 *                  accepted or rejected data on failure.
 */

bool SincDecodeHistogramDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats)
{
    uint16_t val_u16;
    uint32_t val_u32;

    if (packet->cbuf.len < 2)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted histogram packet");
        return false;
    }

    uint32_t protobufHeaderLen = SINC_PROTOCOL_READ_UINT16(packet->cbuf.data);
    unsigned int startPos = 2;

    if (protobufHeaderLen == 0xffff)
    {
        // Extended length protobuf data.
        protobufHeaderLen = SINC_PROTOCOL_READ_UINT32(&packet->cbuf.data[startPos]);
        startPos += sizeof(uint32_t);
    }

    if (protobufHeaderLen > 200 || protobufHeaderLen + startPos > packet->cbuf.len)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted histogram packet");
        return false;
    }

    SiToro__Sinc__HistogramDataResponse *resp = si_toro__sinc__histogram_data_response__unpack(NULL, protobufHeaderLen, &packet->cbuf.data[startPos]);
    if (resp == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted histogram header");
        si_toro__sinc__histogram_data_response__free_unpacked(resp, NULL);
        return false;
    }

    // Get the channel.
    if (fromChannelId != NULL)
    {
        *fromChannelId = -1;
        if (resp->has_channelid)
            *fromChannelId = resp->channelid;
    }

    // Get the plots.
    unsigned int plotCount = 0;
    uint32_t acceptedSamples = 0;
    uint32_t rejectedSamples = 0;
    if (resp->has_spectrumselectionmask)
    {
        if ((resp->spectrumselectionmask & 0x01) && resp->n_plotlen > plotCount)
        {
            acceptedSamples = resp->plotlen[plotCount];
            plotCount++;
        }

        if ((resp->spectrumselectionmask & 0x02) && resp->n_plotlen > plotCount)
        {
            rejectedSamples = resp->plotlen[plotCount];
            plotCount++;
        }
    }

    // Read the header.
    if (stats != NULL)
    {
        memset(stats, 0, sizeof(*stats));
        if (resp->has_datasetid)
            stats->dataSetId = resp->datasetid;

        if (resp->has_timeelapsed)
            stats->timeElapsed = resp->timeelapsed;

        if (resp->has_samplesdetected)
            stats->samplesDetected = resp->samplesdetected;

        if (resp->has_sampleserased)
            stats->samplesErased = resp->sampleserased;

        if (resp->has_pulsesaccepted)
            stats->pulsesAccepted = resp->pulsesaccepted;

        if (resp->has_pulsesrejected)
            stats->pulsesRejected = resp->pulsesrejected;

        if (resp->has_inputcountrate)
            stats->inputCountRate = resp->inputcountrate;

        if (resp->has_outputcountrate)
            stats->outputCountRate = resp->outputcountrate;

        if (resp->has_deadtimepercent)
            stats->deadTime = resp->deadtimepercent;

        if (resp->has_gatestate)
            stats->gateState = (int)resp->gatestate;

        if (resp->has_spectrumselectionmask)
            stats->spectrumSelectionMask = resp->spectrumselectionmask;

        if (resp->has_subregionstartindex)
            stats->subRegionStartIndex = resp->subregionstartindex;

        if (resp->has_subregionendindex)
            stats->subRegionEndIndex = resp->subregionendindex;

        if (resp->has_refreshrate)
            stats->refreshRate = resp->refreshrate;

        if (resp->has_trigger)
            stats->trigger = resp->trigger;

        if (resp->n_intensity > 0)
        {
            stats->numIntensity = resp->n_intensity;
            stats->intensityData = calloc(resp->n_intensity, sizeof(uint32_t));
            if (stats->intensityData == NULL)
            {
                SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                goto errorExit;
            }

            memcpy(stats->intensityData, resp->intensity, resp->n_intensity * sizeof(uint32_t));
        }
    }

    si_toro__sinc__histogram_data_response__free_unpacked(resp, NULL);

    // Copy the accepted data.
    uint8_t *bPos = &packet->cbuf.data[protobufHeaderLen + 2];  // Skip the initial protocol buffer info.
    int bLeft = (int)(packet->cbuf.len - protobufHeaderLen - 2);

    if (accepted != NULL)
    {
        accepted->len = (int)acceptedSamples;
        accepted->data = NULL;
        if (acceptedSamples > 0)
        {
            accepted->data = calloc(acceptedSamples, sizeof(uint32_t));
            if (accepted->data == NULL)
            {
                SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                goto errorExit;
            }

            memcpy(accepted->data, bPos, acceptedSamples * sizeof(uint32_t));
            bPos += acceptedSamples * sizeof(uint32_t);
            bLeft -= acceptedSamples * sizeof(uint32_t);
        }
    }

    // Copy the rejected data.
    if (rejected != NULL)
    {
        rejected->len = (int)rejectedSamples;
        rejected->data = NULL;
        if (rejectedSamples > 0)
        {
            rejected->data = calloc(rejectedSamples, sizeof(uint32_t));
            if (rejected->data == NULL)
            {
                SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                goto errorExit;
            }

            memcpy(rejected->data, bPos, rejectedSamples * sizeof(uint32_t));
            bPos += rejectedSamples * sizeof(uint32_t);
            bLeft -= rejectedSamples * sizeof(uint32_t);
        }
    }

    return true;

errorExit:
    if (accepted && accepted->data != NULL)
    {
        free(accepted->data);
        accepted->data = NULL;
    }

    if (rejected && rejected->data != NULL)
    {
        free(rejected->data);
        rejected->data = NULL;
    }

    if (stats && stats->intensityData != NULL)
    {
        free(stats->intensityData);
        stats->intensityData = NULL;
        stats->numIntensity = 0;
    }

    return false;
}


/*
 * NAME:        SincDecodeHistogramDatagramResponse
 * ACTION:      Decodes an update from the histogram. Waits for the next histogram update to
 *              arrive if timeout is non-zero.
 * PARAMETERS:  Sinc *sc                - the sinc connection.
 *              SincBuffer *packet      - the de-encapsulated packet to decode.
 *              int *fromChannelId      - if non-NULL this is set to the channel the histogram was received from.
 *              SincHistogram *accepted - the accepted histogram plot. Will allocate accepted->data so you must free it.
 *              SincHistogram *rejected - the rejected histogram plot. Will allocate rejected->data so you must free it.
 *              SincHistogramCountStats *stats - various statistics about the histogram. Can be NULL if not needed.
 *                                        May allocate stats->intensity so you should free it if non-NULL.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status. There's no need to free
 *                  accepted or rejected data on failure.
 */

bool SincDecodeHistogramDatagramResponse(SincError *err, SincBuffer *packet, int *fromChannelId, SincHistogram *accepted, SincHistogram *rejected, SincHistogramCountStats *stats)
{
    uint8_t *bufPos;
    size_t headerLen;
    uint32_t samples, spectrumSelectionMask;
    uint16_t msgType, protocolVersion;
    int bufLeft;
    uint16_t val_u16;
    uint32_t val_u32;
    uint64_t val_u64;
    double   val_double;

    if (packet->cbuf.len < SINC_UDP_HISTOGRAM_HEADER_SIZE_PROTOCOL_0)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted histogram datagram packet");
        return false;
    }

    bufPos = packet->cbuf.data;
    headerLen = SINC_PROTOCOL_READ_UINT32(bufPos);
    bufPos += sizeof(uint32_t);

    protocolVersion = SINC_PROTOCOL_READ_UINT16(bufPos);
    bufPos += sizeof(uint16_t);
    if (protocolVersion != 0)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "unknown histogram datagram protocol");
        return false;
    }

    msgType = SINC_PROTOCOL_READ_UINT16(bufPos);
    bufPos += sizeof(uint16_t);
    if (msgType != SI_TORO__SINC__MESSAGE_TYPE__HISTOGRAM_DATAGRAM_RESPONSE)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted histogram datagram packet");
        return false;
    }

    if (fromChannelId != NULL)
        *fromChannelId = (int)SINC_PROTOCOL_READ_UINT32(bufPos);

    bufPos += sizeof(uint32_t);

    samples = SINC_PROTOCOL_READ_UINT32(bufPos);
    bufPos += sizeof(uint32_t);

    spectrumSelectionMask = SINC_PROTOCOL_READ_UINT32(bufPos);
    bufPos += sizeof(uint32_t);

    if (stats != NULL)
    {
        uint8_t *sPos = bufPos;

        memset(stats, 0, sizeof(*stats));
        stats->spectrumSelectionMask = spectrumSelectionMask;

        stats->dataSetId = SINC_PROTOCOL_READ_UINT64(sPos);
        sPos += sizeof(uint64_t);
        stats->timeElapsed = SINC_PROTOCOL_READ_DOUBLE(sPos);
        sPos += sizeof(double);
        stats->samplesDetected = SINC_PROTOCOL_READ_UINT64(sPos);
        sPos += sizeof(uint64_t);
        stats->samplesErased = SINC_PROTOCOL_READ_UINT64(sPos);
        sPos += sizeof(uint64_t);
        stats->pulsesAccepted = SINC_PROTOCOL_READ_UINT64(sPos);
        sPos += sizeof(uint64_t);
        stats->pulsesRejected = SINC_PROTOCOL_READ_UINT64(sPos);
        sPos += sizeof(uint64_t);
        stats->inputCountRate = SINC_PROTOCOL_READ_DOUBLE(sPos);
        sPos += sizeof(double);
        stats->outputCountRate = SINC_PROTOCOL_READ_DOUBLE(sPos);
        sPos += sizeof(double);
        stats->deadTime = SINC_PROTOCOL_READ_DOUBLE(sPos);
        sPos += sizeof(double);
        stats->subRegionStartIndex = SINC_PROTOCOL_READ_UINT32(sPos);
        sPos += sizeof(uint32_t);
        stats->subRegionEndIndex = SINC_PROTOCOL_READ_UINT32(sPos);
        sPos += sizeof(uint32_t);
        stats->refreshRate = SINC_PROTOCOL_READ_UINT32(sPos);
        sPos += sizeof(uint32_t);
        stats->gateState = (int)SINC_PROTOCOL_READ_UINT32(sPos);
        sPos += sizeof(uint32_t);
        stats->positiveRailHitCount = SINC_PROTOCOL_READ_UINT32(sPos);
        sPos += sizeof(uint32_t);
        stats->negativeRailHitCount = SINC_PROTOCOL_READ_UINT32(sPos);
        sPos += sizeof(uint32_t);

        stats->trigger = SI_TORO__SINC__HISTOGRAM_TRIGGER__REFRESH_UPDATE;
        if (headerLen > (size_t)(sPos - packet->cbuf.data))
        {
            // Get the trigger.
            stats->trigger = SINC_PROTOCOL_READ_UINT32(sPos);
            sPos += sizeof(uint32_t);
        }

        if (headerLen - 5 * sizeof(uint32_t) >= (size_t)(sPos - packet->cbuf.data))
        {
            // Get intensity data.
            sPos += sizeof(uint32_t) * 4;
            stats->numIntensity = SINC_PROTOCOL_READ_UINT32(sPos);
            sPos += sizeof(uint32_t);

            if (stats->numIntensity > 0)
            {
                if (headerLen < sPos - packet->cbuf.data - stats->numIntensity * sizeof(uint32_t))
                {
                    SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted histogram intensity packet");
                    goto errorExit;
                }

                stats->intensityData = calloc(stats->numIntensity, sizeof(uint32_t));
                if (stats->intensityData == NULL)
                {
                    SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                    goto errorExit;
                }

                memcpy(stats->intensityData, sPos, stats->numIntensity * sizeof(uint32_t));
                sPos += stats->numIntensity * sizeof(uint32_t);
            }
        }
    }

    //bufPos += 5 * sizeof(uint64_t) + 4 * sizeof(double) + 6 * sizeof(uint32_t);

    if (headerLen > packet->cbuf.len)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted histogram datagram packet");
        return false;
    }

    bufPos = &packet->cbuf.data[headerLen];
    bufLeft = (int)(packet->cbuf.len - headerLen);

    if (accepted != NULL)
    {
        accepted->len = 0;
        accepted->data = NULL;
        if (samples > 0 && (spectrumSelectionMask & SINC_SPECTRUMSELECT_ACCEPTED) != 0 && bufLeft >= (int)(samples * sizeof(uint32_t)))
        {
            accepted->len = (int)samples;
            accepted->data = calloc(samples, sizeof(uint32_t));
            if (accepted->data == NULL)
            {
                SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                goto errorExit;
            }

            memcpy(accepted->data, bufPos, samples * sizeof(uint32_t));
            bufPos += samples * sizeof(uint32_t);
            bufLeft -= samples * sizeof(uint32_t);
        }
    }

    // Copy the rejected data.
    if (rejected != NULL)
    {
        rejected->len = 0;
        rejected->data = NULL;
        if (samples > 0 && (spectrumSelectionMask & SINC_SPECTRUMSELECT_REJECTED) != 0 && bufLeft >= (int)(samples * sizeof(uint32_t)))
        {
            rejected->len = (int)samples;
            rejected->data = calloc(samples, sizeof(uint32_t));
            if (rejected->data == NULL)
            {
                SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
                goto errorExit;
            }

            memcpy(rejected->data, bufPos, samples * sizeof(uint32_t));
            bufPos += samples * sizeof(uint32_t);
            bufLeft -= samples * sizeof(uint32_t);
        }
    }

    return true;

errorExit:
    // Clean up and exit.
    if (accepted && accepted->data != NULL)
    {
        free(accepted->data);
        accepted->data = NULL;
    }

    if (rejected && rejected->data != NULL)
    {
        free(accepted->data);
        accepted->data = NULL;
    }

    if (stats && stats->intensityData != NULL)
    {
        free(stats->intensityData);
        stats->intensityData = NULL;
        stats->numIntensity = 0;
    }

    return false;
}


/*
 * NAME:        SincDecodeListModeDataResponse
 * ACTION:      Decodes a list mode packet.
 * PARAMETERS:  Sinc *sc                - the sinc connection.
 *              SincBuffer *packet      - the de-encapsulated packet to decode.
 *              int *fromChannelId      - if non-NULL this is set to the channel the histogram was received from.
 *              uint8_t **data, int *dataLen - filled in with a dynamically allocated buffer containing list mode data. Must be freed on success.
 *              uint64_t *dataSetId     - if non-NULL this is set to the data set id.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status. There's no need to free
 *                  accepted or rejected data on failure.
 */

bool SincDecodeListModeDataResponse(SincError *err, SincBuffer *packet, int *fromChannelId, uint8_t **data, int *dataLen, uint64_t *dataSetId)
{
    uint16_t val_u16;
    uint32_t val_u32;

    if (packet->cbuf.len < 2)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted list mode packet");
        return false;
    }

    uint32_t protobufHeaderLen = SINC_PROTOCOL_READ_UINT16(packet->cbuf.data);
    unsigned int startPos = 2;

    if (protobufHeaderLen == 0xffff)
    {
        // Extended length protobuf data.
        protobufHeaderLen = SINC_PROTOCOL_READ_UINT32(&packet->cbuf.data[startPos]);
        startPos += sizeof(uint32_t);
    }

    if (protobufHeaderLen > 200 || protobufHeaderLen + startPos > packet->cbuf.len)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted list mode packet");
        return false;
    }

    SiToro__Sinc__ListModeDataResponse *resp = si_toro__sinc__list_mode_data_response__unpack(NULL, protobufHeaderLen, &packet->cbuf.data[startPos]);
    if (resp == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted list mode header");
        si_toro__sinc__list_mode_data_response__free_unpacked(resp, NULL);
        return false;
    }

    // Get the channel.
    if (fromChannelId != NULL)
    {
        *fromChannelId = -1;
        if (resp->has_channelid)
            *fromChannelId = resp->channelid;
    }

    // Get the data set id.
    if (dataSetId != NULL && resp->has_datasetid)
        *dataSetId = resp->datasetid;

    si_toro__sinc__list_mode_data_response__free_unpacked(resp, NULL);

    // Get the list mode data.
    uint8_t *bPos = &packet->cbuf.data[protobufHeaderLen + 2];  // Skip the initial protocol buffer info.
    int bLen = (int)(packet->cbuf.len - protobufHeaderLen - 2);

    if (data != NULL)
    {
        *data = malloc((size_t)bLen);
        if (*data == NULL)
        {
            SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
            return false;
        }

        memcpy(*data, bPos, (size_t)bLen);
    }

    if (dataLen != NULL)
    {
        *dataLen = bLen;
    }

    return true;
}


/*
 * NAME:        SincDecodeAsynchronousErrorResponse
 * ACTION:      Decodes an asynchronous error response from the device. May or may not wait depending on
 *              the timeout.
 * PARAMETERS:  SincError *err                                 - the sinc error structure.
 *              SincBuffer *packet                             - the de-encapsulated packet to decode.
 *              SiToro__Sinc__AsynchronousErrorResponse **resp - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__asynchronous_error_response__free_unpacked(resp, NULL) after use.
 *              int *fromChannelId                        - set to the received channel id. NULL to not use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeAsynchronousErrorResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__AsynchronousErrorResponse **resp, int *fromChannelId)
{
    SiToro__Sinc__AsynchronousErrorResponse *r = si_toro__sinc__asynchronous_error_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);
    if (resp != NULL)
        *resp = r;

    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted async error packet");
        return false;
    }

    if (fromChannelId != NULL && r->success && r->success->has_channelid)
        *fromChannelId = r->success->channelid;

    int ok = true;
    if (r->success)
    {
        ok = SincInterpretSuccessError(err, r->success);
    }

    if (resp == NULL)
        si_toro__sinc__asynchronous_error_response__free_unpacked(r, NULL);

    return ok;
}


/*
 * NAME:        SincDecodeSoftwareUpdateCompleteResponse
 * ACTION:      Decodes a "software update complete" message.
 * PARAMETERS:  SincError *err                            - the sinc error structure.
 *              SincBuffer *packet                        - the de-encapsulated packet to decode.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeSoftwareUpdateCompleteResponse(SincError *err, SincBuffer *packet)
{
    SiToro__Sinc__SoftwareUpdateCompleteResponse *r = si_toro__sinc__software_update_complete_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);
    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted software update complete packet");
        return false;
    }

    int ok = true;
    if (r->success)
    {
        ok = SincInterpretSuccessError(err, r->success);
    }

    si_toro__sinc__software_update_complete_response__free_unpacked(r, NULL);

    return ok;
}


/*
 * NAME:        SincDecodeCheckParamConsistencyResponse
 * ACTION:      Reads a response to a check parameter consistency command.
 * PARAMETERS:  SincError *err                                 - the sinc error structure.
 *              SincBuffer *packet                             - the de-encapsulated packet to decode.
 *              SiToro__Sinc__ListParamDetailsResponse **resp - where to put the response received. NULL to not use.
 *                  This message should be freed with si_toro__sinc__list_param_details_response__free_unpacked(resp, NULL) after use.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeCheckParamConsistencyResponse(SincError *err, SincBuffer *packet, SiToro__Sinc__CheckParamConsistencyResponse **resp, int *fromChannelId)
{
    SiToro__Sinc__CheckParamConsistencyResponse *r = si_toro__sinc__check_param_consistency_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);
    if (resp != NULL)
        *resp = r;

    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted check param consistency packet");
        return false;
    }

    if (fromChannelId != NULL && r->success && r->success->has_channelid)
        *fromChannelId = r->success->channelid;

    int ok = true;
    if (r->success)
    {
        ok = SincInterpretSuccessError(err, r->success);
    }

    if (resp == NULL)
        si_toro__sinc__check_param_consistency_response__free_unpacked(r, NULL);

    return ok;
}

/*
 * NAME:        SincDecodeDownloadCrashDumpResponse
 * ACTION:      Reads a response to a check parameter consistency command.
 * PARAMETERS:  SincError *err     - the sinc error structure.
 *              SincBuffer *packet - the de-encapsulated packet to decode.
 *              bool *newDump      - set true if this crash dump is new.
 *              uint8_t **dumpData - where to put a pointer to the newly allocated crash dump data.
 *                                   This should be free()d after use.
 *              size_t *dumpSize   - where to put the size of the crash dump data.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDecodeDownloadCrashDumpResponse(SincError *err, SincBuffer *packet, bool *newDump, uint8_t **dumpData, size_t *dumpSize)
{
    SiToro__Sinc__DownloadCrashDumpResponse *r = si_toro__sinc__download_crash_dump_response__unpack(NULL, packet->cbuf.len, packet->cbuf.data);

    if (r == NULL)
    {
        SincErrorSetMessage(err, SI_TORO__SINC__ERROR_CODE__READ_FAILED, "corrupted download crash dump packet");
        return false;
    }

    if (r->success)
    {
        if (!SincInterpretSuccessError(err, r->success))
        {
            si_toro__sinc__download_crash_dump_response__free_unpacked(r, NULL);
            return false;
        }
    }

    *newDump = true;
    if (r->has_new_)
    {
        *newDump = r->new_;
    }

    *dumpData = NULL;
    *dumpSize = 0;

    if (r->has_content)
    {
        *dumpData = calloc(r->content.len, 1);
        if (!*dumpData)
        {
            si_toro__sinc__download_crash_dump_response__free_unpacked(r, NULL);
            SincErrorSetCode(err, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
            return false;
        }

        memcpy(*dumpData, r->content.data, r->content.len);
        *dumpSize = r->content.len;
    }

    si_toro__sinc__download_crash_dump_response__free_unpacked(r, NULL);

    return true;
}

/*
 * NAME:        SincMessageTypeFromCodes
 * ACTION:      Convert incoming packet codes into a message type.
 * PARAMETERS:  int responseCode - the packet's response code.
 *              SiToro__Sinc__MessageType msgType - the packet's message type.
 * RETURNS:     SiToro__Sinc__MessageType - the processed message type.
 */

static SiToro__Sinc__MessageType SincMessageTypeFromCodes(int responseCode, SiToro__Sinc__MessageType msgType)
{
    switch (responseCode)
    {
    case SINC_RESPONSE_CODE_PROTOBUF:
        return msgType;

    default:
        return SI_TORO__SINC__MESSAGE_TYPE__NO_MESSAGE_TYPE;
    }
}


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

bool SincGetNextPacketFromBuffer(SincBuffer *readBuf, SiToro__Sinc__MessageType *packetType, SincBuffer *packetBuf, int *packetFound)
{
    return SincGetNextPacketFromBufferGeneric(readBuf, SINC_RESPONSE_MARKER, packetType, packetBuf, packetFound);
}


/*
 * NAME:        SincGetNextPacketFromBufferGeneric
 * ACTION:      Gets the next packet in the read buffer and de-encapsulates it.
 *              The resulting packet can be handed directly to the appropriate SincDecodeXXX()
 *              function.
 *              Use this function to read the next message from a buffer. If you want to read
 *              from the input stream use SincReadMessage() instead.
 * PARAMETERS:  SincBuffer *readBuf   - the buffer to read from.
 *              uint32_t marker       - which packet marker we're looking for. Either SINC_RESPONSE_MARKER
 *                                      or SINC_COMMAND_MARKER. Usually SINC_RESPONSE_MARKER to receive
 *                                      responses from a server.
 *              SiToro__Sinc__MessageType *packetType - the found packet type is placed here.
 *              SincBuffer *packetBuf - where to put the resultant de-encapsulated packet.
 *                                      If NULL it won't make a buffer or consume data (this is useful
 *                                      for peeking ahead for the packet type).
 *              int *packetFound      - set to true if a complete packet was found, false otherwise.
 * RETURNS:     true on success. But this function never errors.
 */

bool SincGetNextPacketFromBufferGeneric(SincBuffer *readBuf, uint32_t marker, SiToro__Sinc__MessageType *packetType, SincBuffer *packetBuf, int *packetFound)
{
    int bytesConsumed;
    int gotMessage;
    int responseCode;

    *packetFound = false;

    if (readBuf->cbuf.len == 0)
        return true;

    // Clear the destination buffer.
    if (packetBuf != NULL)
        packetBuf->cbuf.len = 0;

    // Try to get a message from the read buffer.
    do
    {
        bytesConsumed = 0;
        gotMessage = SincDecodePacketEncapsulation(readBuf, &bytesConsumed, &responseCode, packetType, packetBuf, marker);

        // Remove the consumed data from the buffer.
        if (bytesConsumed > 0 && (packetBuf != NULL || !gotMessage))
        {
            memmove(readBuf->cbuf.data, &readBuf->cbuf.data[bytesConsumed], readBuf->cbuf.len - (size_t)bytesConsumed);
            readBuf->cbuf.len -= (size_t)bytesConsumed;
        }

        // Is there a response?
        if (gotMessage)
        {
            *packetFound = true;
            *packetType = SincMessageTypeFromCodes(responseCode, *packetType);
            return true;
        }

    } while (bytesConsumed > 0);

    return true;
}
