/*
 * Copyright (c) 2016 XIA LLC
 * All rights reserved
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above
 *     copyright notice, this list of conditions and the
 *     following disclaimer.
 *   * Redistributions in binary form must reproduce the
 *     above copyright notice, this list of conditions and the
 *     following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *   * Neither the name of XIA LLC
 *     nor the names of its contributors may be used to endorse
 *     or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * FalconXn Platform Specific Layer
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

#include <inttypes.h>

#include "handel_log.h"

#include "psldef.h"
#include "psl_common.h"

#include "xia_handel.h"
#include "xia_system.h"
#include "xia_common.h"
#include "xia_assert.h"

#include "handel_constants.h"
#include "handel_errors.h"

#include "handel_file.h"
#include "xia_file.h"

#include "md_threads.h"

#include "handel_mapping_modes.h"

#include "falconxn_psl.h"

#ifdef _MSC_VER
#pragma warning(once: 4100)    /* unreferenced formal parameter */
#pragma warning(once: 4127)    /* conditional expression is constant */

/* Some msvc standard extensions pertaining to struct initializers do conform
 * with gcc features and are allowed by the msvc default flag /Ze but generate
 * warnings. We need them for initializing local SincBuffers (i.e.
 * SincBufferInit).
 */
#pragma warning(disable: 4204) /* non-constant aggregate initializer */
#pragma warning(disable: 4221) /* cannot be initialized using address of
                                * automatic variable */
#endif

/*
 * Statistic data types for SiToro list mode data. This is to
 * clean up and work around an API wart where SiToro does not abstract
 * the interface and presents the actual data sizes of the data in
 * the list mode stream. As a result we need to provide 32bit and 64bit
 * variants to map to the API calls.
 */
typedef struct {
    uint32_t samplesDetected;
    uint32_t samplesErased;
    uint32_t pulsesDetected;
    uint32_t pulsesAccepted;
} psl__FalconXNListModeStats32;

typedef struct {
    uint8_t  statsType;
    uint64_t samplesDetected;
    uint64_t samplesErased;
    uint64_t pulsesDetected;
    uint64_t pulsesAccepted;
    double   inputCountRate;
    double   outputCountRate;
    double   deadTimePercent;
} psl__FalconXNListModeStats;

/* A helper type to pull parameter value information out of a Sinc key-value
 * response packet.
 */
typedef union {
    boolean_t boolval;
    int64_t intval;
    double floatval;
    struct {
        size_t len;
        char *str;
    } str;
} psl__SincParamValue;

/* The most we will print for debug SINC param values. */
#define MAX_PARAM_STR_LEN 256


/* Required PSL hooks */
PSL_STATIC int psl__IniWrite(FILE* fp, const char* section, const char* path,
                             void *value, const int index, Module *module);
PSL_STATIC int psl__SetupModule(Module *module);
PSL_STATIC int psl__EndModule(Module *module);
PSL_STATIC int psl__SetupDetChan(int detChan, Detector *detector, Module *module);
PSL_STATIC int psl__EndDetChan(int detChan, Detector *detector, Module *module);
PSL_STATIC int psl__UserSetup(int detChan, Detector *detector, Module *module);

PSL_STATIC int psl__BoardOperation(int detChan, Detector* detector, Module* module,
                                   const char *name, void *value);

PSL_STATIC int psl__GetDefaultAlias(char *alias, char **names, double *values);
PSL_STATIC unsigned int psl__GetNumDefaults(void);

PSL_STATIC int psl__SetDetectorTypeValue(int detChan, Detector *det);

PSL_STATIC int psl__SetAcquisitionValues(int detChan, Detector* detector, Module* module,
                                         const char *name, void *value);
PSL_STATIC int psl__GetAcquisitionValues(int detChan, Detector* detector, Module* module,
                                         const char *name, void *value);

#if 0
PSL_STATIC int psl__UpdateGain(Detector* detector, FalconXNDetector* fDetector,
                               XiaDefaults* defaults, boolean_t gain, boolean_t scaling);
PSL_STATIC int psl__CalculateGain(FalconXNDetector* fDetector, double* gaindac,
                                  double* gaincoarse, double* scalefactor);
#endif
PSL_STATIC int psl__GainCalibrate(int detChan, Detector *det, int modChan,
                                  Module *m, XiaDefaults *def, double* delta);
PSL_STATIC int psl__GainOperation(int detChan, const char *name, void *value,
                                  Detector *det, int modChan, Module *m, XiaDefaults *defs);
PSL_STATIC int psl__StartRun(int detChan, unsigned short resume,
                             XiaDefaults *defs, Detector *detector, Module *m);
PSL_STATIC int psl__StopRun(int detChan, Detector *detector, Module *m);
PSL_STATIC int psl__GetRunData(int detChan, const char *name, void *value,
                               XiaDefaults *defs, Detector *detector, Module *m);
PSL_STATIC int psl__SpecialRun(int detChan, const char *name, void *info,
                               XiaDefaults *defaults, Detector *detector, Module *module);
PSL_STATIC int psl__GetSpecialRunData(int detChan, const char *name, void *value, XiaDefaults *defaults,
                                      Detector *detector, Module *module);

PSL_STATIC int psl__ModuleTransactionSend(Module* module, SincBuffer* packet);
PSL_STATIC int psl__ModuleTransactionReceive(Module* module, Sinc_Response* response);
PSL_STATIC int psl__ModuleTransactionEnd(Module* module);

PSL_STATIC boolean_t psl__CanRemoveName(const char *name);

PSL_STATIC int psl__DetCharacterizeStart(int detChan, Detector* detector, Module* module);
PSL_STATIC int psl__UnloadDetCharacterization(Module* module, Detector* detector, const char *filename);
PSL_STATIC int psl__LoadDetCharacterization(Detector *detector, Module *module);

PSL_STATIC int psl__RefreshChannelState(Module* module, Detector* detector);
PSL_STATIC int psl__UpdateChannelState(SiToro__Sinc__KeyValue* kv, Detector* detector);

/* Board operations */
PSL_STATIC int psl__BoardOp_Apply(int detChan, Detector* detector, Module* module,
                                  const char *name, void *value);
PSL_STATIC int psl__BoardOp_BufferDone(int detChan, Detector* detector, Module* module,
                                       const char *name, void *value);
PSL_STATIC int psl__BoardOp_MappingPixelNext(int detChan, Detector* detector, Module* module,
                                       const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetBoardInfo(int detChan, Detector* detector, Module* module,
                                         const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetConnected(int detChan, Detector* detector, Module* module,
                                         const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetChannelCount(int detChan, Detector* detector, Module* module,
                                            const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetSerialNumber(int detChan, Detector* detector, Module* module,
                                            const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetFirmwareVersion(int detChan, Detector* detector, Module* module,
                                               const char *name, void *value);

/* Helpers */
PSL_STATIC bool psl__AcqRemoved(const char *name);
PSL_STATIC Detector* psl__FindDetector(Module* module, int channel);
PSL_STATIC int psl__GetParam(Module* module, int channel, const char* name,
                             SiToro__Sinc__GetParamResponse** resp);
PSL_STATIC int psl__SetParam(Module* module, Detector* detector,
                             SiToro__Sinc__KeyValue* param);
PSL_STATIC int psl__GetParamValue(Module* module, int channel, const char* name,
                                  int paramType, psl__SincParamValue* val);
PSL_STATIC int psl__GetMaxNumberSca(int detChan, Module* module, int *value);
PSL_STATIC int psl__SetDigitalConf(int detChan, Detector* detector, Module* module);
PSL_STATIC int psl__SyncMCARefresh(Module *module, Detector *detector);
PSL_STATIC int psl__SetMCARefresh(Module *module, Detector *detector, double period);
PSL_STATIC int psl__SyncPresetType(Module *module, Detector *detector);
PSL_STATIC int psl__SyncPixelAdvanceMode(Module *module, Detector *detector);
PSL_STATIC int psl__SetHistogramMode(Module *module, Detector *detector,
                                     const char *mode);
PSL_STATIC int psl__SyncNumberMCAChannels(Module *module, Detector *detector);
PSL_STATIC int psl__SyncGateCollectionMode(Module *module, Detector *detector);

/* Acquisition value handler */
#define ACQ_HANDLER_DECL(_n) \
    PSL_STATIC int psl__Acq_ ## _n (Module*           module, \
                                    Detector*         detector, \
                                    int               channel, \
                                    FalconXNDetector* fDetector, \
                                    XiaDefaults*      defaults, \
                                    AcquisitionValue* acq, \
                                    const char        *name, \
                                    double*           value, \
                                    boolean_t         read)
#define ACQ_HANDLER(_n) psl__Acq_ ## _n
/* #define ACQ_HANDLER_NAME(_n) "psl__Acq_" # _n */
#define ACQ_HANDLER_CALL(_n) \
    ACQ_HANDLER(_n)(module, detector, channel, fDetector, defaults, acq, name, value, read)
#define ACQ_HANDLER_LOG(_n) \
    pslLog(PSL_LOG_DEBUG, "ACQ %s: %s (%s:%d)",   \
           read ? "read": "write", name, module->alias, channel)

#define ACQ_SYNC_DECL(_n) \
    PSL_STATIC int psl__Sync_ ## _n (int               detChan, \
                                     int               channel, \
                                     Module*           module, \
                                     Detector*         detector, \
                                     XiaDefaults*      defaults)
#define ACQ_SYNC(_n) psl__Sync_ ## _n
/* #define ACQ_SYNC_NAME(_n) "psl__Sync_" # _n */
/* #define ACQ_SYNC_CALL(_n) ACQ_SYNC(_n)(detChan, channel, module, detector, fDetector, defaults) */
#define ACQ_SYNC_LOG(_n, _v) \
    pslLog(PSL_LOG_DEBUG, "%s = %5.3f", # _n, _v)

ACQ_HANDLER_DECL(analog_gain);
ACQ_HANDLER_DECL(analog_offset);
ACQ_HANDLER_DECL(detector_polarity);
ACQ_SYNC_DECL(detector_polarity);
ACQ_HANDLER_DECL(termination);
ACQ_HANDLER_DECL(attenuation);
ACQ_HANDLER_DECL(coupling);
ACQ_HANDLER_DECL(decay_time);
ACQ_HANDLER_DECL(dc_offset);
ACQ_HANDLER_DECL(reset_blanking_enable);
ACQ_HANDLER_DECL(reset_blanking_threshold);
ACQ_HANDLER_DECL(reset_blanking_presamples);
ACQ_HANDLER_DECL(reset_blanking_postsamples);
ACQ_HANDLER_DECL(clock_speed);
ACQ_HANDLER_DECL(detection_threshold);
ACQ_HANDLER_DECL(min_pulse_pair_separation);
ACQ_HANDLER_DECL(detection_filter);
ACQ_HANDLER_DECL(scale_factor);
ACQ_HANDLER_DECL(number_mca_channels);
ACQ_HANDLER_DECL(mca_spectrum_accepted);
ACQ_HANDLER_DECL(mca_spectrum_rejected);
ACQ_HANDLER_DECL(mca_start_channel);
ACQ_HANDLER_DECL(mca_refresh);
ACQ_HANDLER_DECL(mca_bin_width);
ACQ_HANDLER_DECL(preset_type);
ACQ_HANDLER_DECL(preset_value);
ACQ_HANDLER_DECL(mapping_mode);
ACQ_HANDLER_DECL(sca_trigger_mode);
ACQ_HANDLER_DECL(sca_pulse_duration);
ACQ_HANDLER_DECL(number_of_scas);
ACQ_HANDLER_DECL(sca);
ACQ_HANDLER_DECL(num_map_pixels);
ACQ_HANDLER_DECL(num_map_pixels_per_buffer);
ACQ_HANDLER_DECL(pixel_advance_mode);
ACQ_HANDLER_DECL(input_logic_polarity);
ACQ_HANDLER_DECL(gate_ignore);
ACQ_HANDLER_DECL(sync_count);


/* The default acquisition values. */
#define ACQ_DEFAULT(_n, _t, _d, _f, _s) \
    { # _n, (_d), { (_t), { (double) 0 } }, (_f), ACQ_HANDLER(_n), _s}

/* Compact the flags to make the table narrower. */
/*#define PSL_ACQ_E    PSL_ACQ_EMPTY */
#define PSL_ACQ_HD   PSL_ACQ_HAS_DEFAULT
#define PSL_ACQ_L    PSL_ACQ_LOCAL
#define PSL_ACQ_RO   PSL_ACQ_READ_ONLY
#define PSL_ACQ_L_HD PSL_ACQ_LOCAL | PSL_ACQ_HAS_DEFAULT

/*
 * When adding a new acquisition value, be sure to add the proper call to
 * pslSetParset/pslSetGenset to invalidate the cached value, as required. Note
 * that the matching function uses STRNEQ and will return the first match,
 * thus, "gain_trim" must precede "gain", etc.
 */
static const AcquisitionValue DEFAULT_ACQ_VALUES[] = {
    /* analog settings */
    ACQ_DEFAULT(analog_gain,                 acqFloat,   1.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(analog_offset,               acqFloat,   0.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(detector_polarity,           acqBool,    0.0, PSL_ACQ_HD, ACQ_SYNC(detector_polarity)),
    ACQ_DEFAULT(termination,                 acqInt,     0.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(attenuation,                 acqInt,     0.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(coupling,                    acqInt,     0.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(decay_time,                  acqInt,     0.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(dc_offset,                   acqFloat,   0.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(reset_blanking_enable,       acqBool,    1.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(reset_blanking_threshold,    acqFloat, -0.05, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(reset_blanking_presamples,   acqInt,      50, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(reset_blanking_postsamples,  acqInt,      50, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(detection_threshold,         acqFloat,  0.01, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(min_pulse_pair_separation,   acqInt,      25, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(detection_filter,            acqInt, XIA_FILTER_MID_RATE, PSL_ACQ_HD, NULL),
    /* system settings */
    ACQ_DEFAULT(clock_speed,                 acqInt,     0.0, PSL_ACQ_RO,   NULL),
    ACQ_DEFAULT(mapping_mode,                acqInt,     0.0, PSL_ACQ_L_HD, NULL),
    /* MCA mode */
    ACQ_DEFAULT(number_mca_channels,         acqInt,  4096.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(mca_spectrum_accepted,       acqInt,     1.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(mca_spectrum_rejected,       acqInt,     0.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(mca_start_channel,           acqInt,     0.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(mca_refresh,                 acqFloat,   0.1, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(preset_type,                 acqInt,     0.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(preset_value,                acqFloat,   0.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(scale_factor,                acqFloat,   2.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(mca_bin_width,               acqFloat,  10.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(sca_trigger_mode,            acqInt,     SCA_TRIGGER_ALWAYS, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(sca_pulse_duration,          acqInt,   400.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(number_of_scas,              acqInt,     0.0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(sca,                         acqFloat,   0.0, PSL_ACQ_L,  NULL),
    ACQ_DEFAULT(num_map_pixels_per_buffer,   acqInt,    1024, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(num_map_pixels,              acqInt,       0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(pixel_advance_mode,          acqInt,       0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(input_logic_polarity,        acqInt,       0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(gate_ignore,                 acqInt,       0, PSL_ACQ_HD, NULL),
    ACQ_DEFAULT(sync_count,                  acqInt,       0, PSL_ACQ_HD, NULL),
};

#define SI_DET_NUM_OF_ACQ_VALUES (sizeof(DEFAULT_ACQ_VALUES) / sizeof(const AcquisitionValue))

/* These are allowed in old ini files but not from the API. */
static const char* REMOVED_ACQ_VALUES[] = {
    "coarse_bin_scale",
    "pulse_scale_factor",
    "mca_end_channel",
    "adc_trace_length"
};

/* These are the allowed board operations for this hardware */
static  BoardOperation boardOps[] =
  {
    { "apply",                psl__BoardOp_Apply },
    { "buffer_done",          psl__BoardOp_BufferDone },
    { "mapping_pixel_next",   psl__BoardOp_MappingPixelNext },

    { "get_board_info",       psl__BoardOp_GetBoardInfo },

    /* FalconXn Specific board operations. */
    { "get_connected",        psl__BoardOp_GetConnected },
    { "get_channel_count",    psl__BoardOp_GetChannelCount },
    { "get_serial_number",    psl__BoardOp_GetSerialNumber },
    { "get_firmware_version", psl__BoardOp_GetFirmwareVersion }
  };

/* The PSL Handlers table. This is exported to Handel. */
static PSLHandlers handlers;

PSL_SHARED int falconxn_PSLInit(const PSLHandlers** psl);
PSL_SHARED int falconxn_PSLInit(const PSLHandlers** psl)
{
    handlers.iniWrite = psl__IniWrite;
    handlers.setupModule = psl__SetupModule;
    handlers.endModule = psl__EndModule;
    handlers.setupDetChan = psl__SetupDetChan;
    handlers.endDetChan = psl__EndDetChan;
    handlers.userSetup = psl__UserSetup;
    handlers.boardOperation = psl__BoardOperation;
    handlers.getNumDefaults = psl__GetNumDefaults;
    handlers.getDefaultAlias = psl__GetDefaultAlias;
    handlers.setDetectorTypeValue = psl__SetDetectorTypeValue;
    handlers.setAcquisitionValues = psl__SetAcquisitionValues;
    handlers.getAcquisitionValues = psl__GetAcquisitionValues;
    handlers.gainOperation = psl__GainOperation;
    handlers.startRun = psl__StartRun;
    handlers.stopRun = psl__StopRun;
    handlers.getRunData = psl__GetRunData;
    handlers.doSpecialRun = psl__SpecialRun;
    handlers.getSpecialRunData = psl__GetSpecialRunData;
    handlers.canRemoveName = psl__CanRemoveName;
    handlers.freeSCAs = pslDestroySCAs;

    *psl = &handlers;
    return XIA_SUCCESS;
}

PSL_STATIC void falconXNClearCalibrationData(SincCalibrationPlot* plot)
{
    if (plot->x != NULL) {
        free(plot->x);
        plot->x = NULL;
    }
    if (plot->y != NULL) {
        free(plot->y);
        plot->y = NULL;
    }
    plot->len = 0;
}

/*
 * Clean out the calibration data.
 */
PSL_STATIC void falconXNClearDetectorCalibrationData(FalconXNDetector* fDetector)
{
    if (fDetector->calibData.data != NULL) {
        free(fDetector->calibData.data);
        fDetector->calibData.data = NULL;
        fDetector->calibData.len = 0;
    }
    falconXNClearCalibrationData(&fDetector->calibExample);
    falconXNClearCalibrationData(&fDetector->calibModel);
    falconXNClearCalibrationData(&fDetector->calibFinal);
}

/*
 * Clean out the stats.
 */
PSL_STATIC void falconXNClearDetectorStats(FalconXNDetector* fDetector)
{
    int i;
    for (i = 0; i < FALCONXN_STATS_NUMOF; ++i)
        fDetector->stats[i] = 0.0;
}

/*
 * Convert SINC stats to Handel stats.
 */
PSL_STATIC void falconXNSetDetectorStats(FalconXNDetector* fDetector,
                                         SincHistogramCountStats* stats)
{
    fDetector->stats[FALCONXN_STATS_TIME_ELAPSED] =
        stats->timeElapsed;
    fDetector->stats[FALCONXN_STATS_TRIGGERS] =
        (double) (stats->pulsesAccepted + stats->pulsesRejected);

    /* inputCountRate=NaN has been observed when there is no signal. */
    fDetector->stats[FALCONXN_STATS_TRIGGER_LIVETIME] =
        isnormal(stats->inputCountRate)
        ? fDetector->stats[FALCONXN_STATS_TRIGGERS] / stats->inputCountRate
        : 0.0;

    fDetector->stats[FALCONXN_STATS_MCA_EVENTS] =
        (double) stats->pulsesAccepted;
    fDetector->stats[FALCONXN_STATS_INPUT_COUNT_RATE] =
        isnormal(stats->inputCountRate) ? stats->inputCountRate : 0.0;
    fDetector->stats[FALCONXN_STATS_OUTPUT_COUNT_RATE] =
        stats->outputCountRate;

    fDetector->stats[FALCONXN_STATS_SAMPLES_DETECTED] =
        (double) stats->samplesDetected;
    fDetector->stats[FALCONXN_STATS_SAMPLES_ERASED] =
        (double) stats->samplesErased;
    fDetector->stats[FALCONXN_STATS_PULSES_ACCEPTED] =
        (double) stats->pulsesAccepted;
    fDetector->stats[FALCONXN_STATS_PULSES_REJECTED] =
        (double) stats->pulsesRejected;
    fDetector->stats[FALCONXN_STATS_DEADTIME] =
        stats->deadTime;
}

/*
 * Handle the SINC API result.
 */
PSL_STATIC int falconXNSincResultToHandel(int code, const char* msg)
{
    int handelError = XIA_SUCCESS;
    if (code != 0) {
        handelError = XIA_FN_BASE_CODE + code;
        pslLog(PSL_LOG_ERROR, handelError, "%s", msg);
    }
    return handelError;
}

/*
 * Handle the SINC Error result.
 */
PSL_STATIC int falconXNSincErrorToHandel(SincError* se)
{
    return falconXNSincResultToHandel(se->code, se->msg);
}

/*
 * Get the acquisition value reference given the label.
 * the given name could have additional parameters appended to it
 * e.g. scalo_0 can match "sca"
 */
PSL_STATIC AcquisitionValue* psl__GetAcquisition(FalconXNDetector* fDetector,
                                                 const char*       name)
{
    int i;
    for (i = 0; i < fDetector->numOfAcqValues; i++) {
        if (STRNEQ(name, fDetector->acqValues[i].name)) {
            return &fDetector->acqValues[i];
        }
    }
    return NULL;
}

/*
 * Convert the Handel standard double to the specified type.
 */
#define PSL__CONVERT_TO(_t, _T, _R, _m, _M) \
PSL_STATIC int psl__ConvertTo_ ## _t (AcquisitionValue* acq, \
                                      const double      value) { \
    if (acq->value.type != _T) \
        return XIA_UNKNOWN_VALUE; \
    if ((value < (double) _m) || (value > (double) _M))  \
        return XIA_ACQ_OOR; \
    acq->value.ref._R = (_t) value; \
    return XIA_SUCCESS; \
}

PSL__CONVERT_TO(int64_t, acqInt,   i,       LLONG_MIN,               LLONG_MAX)
PSL__CONVERT_TO(bool,    acqBool,  b,                0,                      1)
PSL__CONVERT_TO(MM_Mode, acqInt,   i, MAPPING_MODE_MCA, MAPPING_MODE_COUNT - 1)

PSL_STATIC PSL_INLINE int psl__SetAcqValue(AcquisitionValue* acq,
                                           const double      value)
{
    int status = XIA_SUCCESS;
    if (acq) {
        switch (acq->value.type)
        {
            case acqFloat:
                acq->value.ref.f = value;
                break;
            case acqInt:
                status = psl__ConvertTo_int64_t(acq, value);
                break;
            case acqBool:
                status = psl__ConvertTo_bool(acq, value);
                break;
            case acqString:
            default:
                status = XIA_BAD_TYPE;
                break;
        }
    }
    else {
        status = XIA_BAD_VALUE;
    }
    return status;
}

/* Converts a SiToro__Sinc__KeyValue boolval response to a Handel boolean_t.
 * Use this to avoid size warnings.
 */
#define PSL_BOOL_OF_KV(kv) (kv->boolval ? TRUE_ : FALSE_)

/* Acquisition value bool value to double. */
#define PSL_ACQ_GET_BOOL(acq) (acq->value.ref.b ? 1.0 : 0.0)

/*
 * Update the acq value's default.
 */
PSL_STATIC int psl__UpdateDefault(XiaDefaults*      defaults,
                                  const char*       name,
                                  AcquisitionValue* acq)
{
    if (!PSL_ACQ_FLAG_SET(acq, PSL_ACQ_READ_ONLY)) {
        int status;

        double value = 0;

        switch (acq->value.type)
        {
            case acqFloat:
            default:
                value = acq->value.ref.f;
                break;
            case acqInt:
                value = (double) acq->value.ref.i;
                break;
            case acqBool:
                value = (double) acq->value.ref.b;
                break;
            case acqString:
                status = XIA_BAD_VALUE;
                pslLog(PSL_LOG_ERROR, status,
                       "Bad type for value");
                return status;
        }

        pslLog(PSL_LOG_INFO, "Name: %s = %0.3f", name, value);

        status = pslSetDefault(name, &value, defaults);

        if (status != XIA_SUCCESS) {
            if (status == XIA_NOT_FOUND) {
                boolean_t adding = TRUE_;
                while (adding) {
                    pslLog(PSL_LOG_DEBUG,
                           "Adding default entry %s to %s", name, defaults->alias);

                    status = xiaAddDefaultItem(defaults->alias, name, &value);
                    if (status == XIA_SUCCESS) {
                        adding = FALSE_;
                    } else {
                        if (status == XIA_NO_ALIAS) {
                            pslLog(PSL_LOG_DEBUG,
                                   "Adding defaults %s", defaults->alias);

                            status = xiaNewDefault(defaults->alias);
                            if (status != XIA_SUCCESS) {
                                pslLog(PSL_LOG_ERROR, status,
                                       "Error creating new default alias: %s",
                                       defaults->alias);
                                return status;
                            }
                        }
                        else {
                            pslLog(PSL_LOG_ERROR, status,
                                   "Error adding  default item to %s: %s",
                                   defaults->alias, name);
                            return status;
                        }
                    }
                }
            }
            else {
                pslLog(PSL_LOG_ERROR, status,
                       "Error setting default: %s", name);
                return status;
            }
        }

        acq->flags |= PSL_ACQ_HAS_DEFAULT;
    }

    return XIA_SUCCESS;
}

/*
 * Check the defaults and if they have are
*/
PSL_STATIC int psl__ReloadDefaults(FalconXNDetector* fDetector)
{
    XiaDefaults *defaults = xiaGetDefaultFromDetChan(fDetector->detChan);

    if (defaults) {
        XiaDaqEntry* entry = defaults->entry;

        while (entry) {
            int status = XIA_SUCCESS;

            AcquisitionValue* acq = psl__GetAcquisition(fDetector, entry->name);

            if (acq) {
                status = psl__SetAcqValue(acq, entry->data);
                if (status != XIA_SUCCESS) {
                    pslLog(PSL_LOG_ERROR, status,
                           "Unable to convert the default value: %s", entry->name);
                    return status;
                }
            }

            entry = entry->next;
        }
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__SetDetectorTypeValue(int detChan, Detector *det)
{
    int status = XIA_SUCCESS;
    UNUSED(det);
    UNUSED(detChan);

    return status;
}

PSL_STATIC void psl__CheckConnected(FalconXNDetector *fDetector)
{
        UNUSED(fDetector);
}

PSL_STATIC int psl__ModuleLock(Module* module)
{
    int status = XIA_SUCCESS;

    FalconXNModule* fModule = module->pslData;

    status = handel_md_mutex_lock(&fModule->lock);
    if (status != 0) {
        status = XIA_THREAD_ERROR;
        pslLog(PSL_LOG_ERROR, status,
               "Cannot lock module: %s", module->alias);
    }

    return status;
}

PSL_STATIC int psl__ModuleUnlock(Module* module)
{
    int status = XIA_SUCCESS;

    FalconXNModule* fModule = module->pslData;

    status = handel_md_mutex_unlock(&fModule->lock);
    if (status != 0) {
        status = XIA_THREAD_ERROR;
        pslLog(PSL_LOG_ERROR, status,
               "Cannot unlock module: %s", module->alias);
    }

    return status;
}

PSL_STATIC int psl__DetectorChannel(Detector* detector)
{
    FalconXNDetector* fDetector = detector->pslData;
    return fDetector->modDetChan;
}

PSL_STATIC int psl__DetectorLock(Detector* detector)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    status = handel_md_mutex_lock(&fDetector->lock);
    if (status != 0) {
        status = XIA_THREAD_ERROR;
        pslLog(PSL_LOG_ERROR, status,
               "Cannot lock detector: %s", detector->alias);
    }

    return status;
}

PSL_STATIC int psl__DetectorUnlock(Detector* detector)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    status = handel_md_mutex_unlock(&fDetector->lock);
    if (status != 0) {
        status = XIA_THREAD_ERROR;
        pslLog(PSL_LOG_ERROR, status,
               "Cannot unlock detector: %s", detector->alias);
    }

    return status;
}

PSL_STATIC int psl__DetectorWait(Detector* detector, unsigned int timeout)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    pslLog(PSL_LOG_DEBUG,
           "Detector %d waiting, timeout=%u",
           psl__DetectorChannel(detector), timeout);

    status = handel_md_event_wait(&fDetector->asyncEvent, timeout);
    if (status != 0) {
        if (status == THREADING_TIMEOUT) {
            pslLog(PSL_LOG_DEBUG, "Detector %d timeout",
                   psl__DetectorChannel(detector));
            status = XIA_TIMEOUT;
        }
        else {
            int ee = status;
            status = XIA_THREAD_ERROR;
            pslLog(PSL_LOG_ERROR, status,
                   "Detector wait failed: %s: %d", detector->alias, ee);
        }
    }

    pslLog(PSL_LOG_DEBUG,
           "Detector %d woken: %d", psl__DetectorChannel(detector), status);

    return status;
}

PSL_STATIC int psl__DetectorSignal(Detector* detector)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    pslLog(PSL_LOG_DEBUG,
           "Detector %d signalled", psl__DetectorChannel(detector));

    status = handel_md_event_signal(&fDetector->asyncEvent);
    if (status != 0) {
        int ee = status;
        status = XIA_THREAD_ERROR;
        pslLog(PSL_LOG_ERROR, status,
               "Detector signal failed: %s: %d", detector->alias, ee);
    }

    return status;
}

PSL_STATIC void psl__FlushResponse(Sinc_Response* resp)
{
    resp->channel = -1;
    resp->type = -1;
    resp->response = NULL;
}

PSL_STATIC void psl__FreeResponse(Sinc_Response* resp)
{
    if (resp->response != NULL) {
        switch (resp->type) {
            case SI_TORO__SINC__MESSAGE_TYPE__SUCCESS_RESPONSE:
                si_toro__sinc__success_response__free_unpacked(resp->response,
                                                               NULL);
                break;

            case SI_TORO__SINC__MESSAGE_TYPE__GET_PARAM_RESPONSE:
                si_toro__sinc__get_param_response__free_unpacked(resp->response,
                                                                 NULL);
                break;

            case SI_TORO__SINC__MESSAGE_TYPE__GET_CALIBRATION_RESPONSE:
                break;

            case SI_TORO__SINC__MESSAGE_TYPE__CALCULATE_DC_OFFSET_RESPONSE:
                si_toro__sinc__calculate_dc_offset_response__free_unpacked(resp->response,
                                                                           NULL);
                break;

            case SI_TORO__SINC__MESSAGE_TYPE__LIST_PARAM_DETAILS_RESPONSE:
                si_toro__sinc__set_calibration_command__free_unpacked(resp->response,
                                                                      NULL);
                break;

            case SI_TORO__SINC__MESSAGE_TYPE__PARAM_UPDATED_RESPONSE:
                si_toro__sinc__param_updated_response__free_unpacked(resp->response,
                                                                     NULL);
                break;

            case SI_TORO__SINC__MESSAGE_TYPE__SOFTWARE_UPDATE_COMPLETE_RESPONSE:
                si_toro__sinc__software_update_complete_response__free_unpacked(resp->response,
                                                                                NULL);
                break;

            case SI_TORO__SINC__MESSAGE_TYPE__CHECK_PARAM_CONSISTENCY_RESPONSE:
                si_toro__sinc__check_param_consistency_command__free_unpacked(resp->response,
                                                                              NULL);
                break;

            default:
                pslLog(PSL_LOG_ERROR, XIA_INVALID_VALUE,
                       "Invalid message type for response free: %d", resp->type);
                break;
        }

        psl__FlushResponse(resp);
    }
}

PSL_STATIC int psl__CheckSuccessResponse(Module* module)
{
    int status = XIA_SUCCESS;

    Sinc_Response response;

    SiToro__Sinc__SuccessResponse* resp = NULL;

    response.channel = -1;
    response.type = SI_TORO__SINC__MESSAGE_TYPE__SUCCESS_RESPONSE;

    status = psl__ModuleTransactionReceive(module, &response);
    if (status != XIA_SUCCESS) {
        return status;
    }

    resp = response.response;

    if (resp->has_errorcode) {
        status = XIA_FN_BASE_CODE + resp->errorcode;
        if (resp->message != NULL) {
            pslLog(PSL_LOG_ERROR, status,
                   "(%d) %s", resp->errorcode, resp->message);
        }
        else {
            pslLog(PSL_LOG_ERROR, status,
                   "(%d) No error message", resp->errorcode);
        }
    }

    psl__FreeResponse(&response);

    return status;
}

PSL_STATIC int psl__MonitorChannel(Module* module, Detector* detector)
{
    int status;

    uint8_t    pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);

    UNUSED(detector);

    FalconXNModule* fModule = module->pslData;

    int channels[FALCONXN_MAX_CHANNELS];
    int channel;
    int mchannel = 0;

    for (channel = 0; channel < FALCONXN_MAX_CHANNELS; ++channel) {
        if (fModule->channelActive[channel]) {
            channels[mchannel++] = channel;
        }
    }

    SincEncodeMonitorChannels(&packet, &channels[0], mchannel);

    status = psl__ModuleTransactionSend(module, &packet);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error setting channel monitor");
        return status;
    }

    status = psl__CheckSuccessResponse(module);

    psl__ModuleTransactionEnd(module);

    return status;
}

/* Gets the channel.state from Sinc and updates the detector channel state. */
PSL_STATIC int psl__RefreshChannelState(Module* module, Detector* detector)
{
    int status;

    SiToro__Sinc__GetParamResponse* resp = NULL;
    SiToro__Sinc__KeyValue*         kv;

    status = psl__GetParam(module, psl__DetectorChannel(detector),
                           "channel.state", &resp);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the channel state");
        return status;
    }

    kv = resp->results[0];

    if (kv->has_paramtype && kv->optionval)
        pslLog(PSL_LOG_DEBUG, "Refresh channel state: %s", kv->optionval);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status, "Unable to get the detector lock");
        return status;
    }

    status = psl__UpdateChannelState(kv, detector);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status, "Updating channel state");
    }

    psl__DetectorUnlock(detector);

    si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

    return status;
}

/*
 * Sends a command to stop any form of data acquisition on the
 * channel. @a modChan is a module channel (SINC channel) or -1 for
 * all channels in the module.
 */
PSL_STATIC int psl__StopDataAcquisition(Module* module, int modChan, bool skipChar)
{
    int status;

    uint8_t    pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);

    pslLog(PSL_LOG_DEBUG, "Stopping data acquisition %s:%d", module->alias, modChan);

    SincEncodeStop(&packet, modChan, skipChar);

    status = psl__ModuleTransactionSend(module, &packet);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error stopping data acquisition %s:%d", module->alias, modChan);
        return status;
    }

    status = psl__CheckSuccessResponse(module);

    psl__ModuleTransactionEnd(module);

    return status;
}

/*
 * Prints a SINC param value to a string, regardless of value type.
 */
PSL_STATIC void psl__SPrintKV(char *s, size_t max, SiToro__Sinc__KeyValue* kv)
{
    if (kv->has_intval)
        snprintf(s, max, "%" PRId64, kv->intval);
    else if (kv->has_boolval)
        snprintf(s, max, "%d", kv->boolval);
    else if (kv->has_floatval)
        snprintf(s, max, "%.3f", kv->floatval);
    else if (kv->optionval)
        snprintf(s, max, "%s", kv->optionval);
    else if (kv->strval)
        snprintf(s, max, "%s", kv->strval);
    else
        snprintf(s, max, "???");
}

PSL_STATIC int psl__GetParam(Module*                          module,
                             int                              channel,
                             const char*                      name,
                             SiToro__Sinc__GetParamResponse** resp)
{
    int status = XIA_SUCCESS;

    uint8_t    pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);
    Sinc_Response response;

    char logValue[MAX_PARAM_STR_LEN];

    *resp = NULL;

    SincEncodeGetParam(&packet, channel, (char*) name);

    status = psl__ModuleTransactionSend(module, &packet);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error requesting the parameter");
        return status;
    }

    response.channel = channel;
    response.type = SI_TORO__SINC__MESSAGE_TYPE__GET_PARAM_RESPONSE;

    status = psl__ModuleTransactionReceive(module, &response);

    if (status == XIA_SUCCESS) {
        *resp = response.response;

        psl__SPrintKV(logValue, sizeof(logValue) / sizeof(logValue[0]),
                      (*resp)->results[0]);
        pslLog(PSL_LOG_INFO, "Param read: %s = %s", name, logValue);
    }
    else {
        pslLog(PSL_LOG_ERROR, status,
               "Error receiving parameter");
    }

    psl__ModuleTransactionEnd(module);

    return status;
}

PSL_STATIC int psl__SetParam(Module*                 module,
                             Detector*               detector,
                             SiToro__Sinc__KeyValue* param)
{
    int status;

    uint8_t    pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);

    char logValue[MAX_PARAM_STR_LEN];

    psl__SPrintKV(logValue, sizeof(logValue) / sizeof(logValue[0]),
                  param);
    pslLog(PSL_LOG_DEBUG, "Param write: %s = %s", param->key, logValue);

    SincEncodeSetParam(&packet, psl__DetectorChannel(detector), param);

    status = psl__ModuleTransactionSend(module, &packet);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error setting a parameter");
        return status;
    }

    status = psl__CheckSuccessResponse(module);

    psl__ModuleTransactionEnd(module);

    return status;
}

/*
 * Wraps psl__GetParam to manage freeing the sinc packet. It's more
 * convenient for numeric types since callers can declare a
 * psl__SincParamValue on the stack and pass a pointer without having
 * to manage memory.
 *
 * For strings, val.str.str must point to caller-allocated memory and
 * val.str.len should be set to the maximum length.
 *
 * @a channel: module channel
 * @a paramType: SI_TORO__SINC__KEY_VALUE__PARAM_TYPE*
 */
PSL_STATIC int psl__GetParamValue(Module* module, int channel,
                                  const char* name,
                                  int paramType, psl__SincParamValue* val)
{
    int status;

    SiToro__Sinc__GetParamResponse* resp = NULL;
    SiToro__Sinc__KeyValue*         kv;

    status = psl__GetParam(module, channel, name, &resp);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get %s", name);
        return status;
    }

    kv = resp->results[0];

    if (kv->has_paramtype && (kv->paramtype == paramType)) {
        if (paramType == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__STRING_TYPE) {
            strncpy(val->str.str, kv->strval, val->str.len);
        } else if (paramType == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE) {
            ASSERT(kv->has_intval);
            val->intval = kv->intval;
        } else if (paramType == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__FLOAT_TYPE) {
            ASSERT(kv->has_floatval);
            val->floatval = kv->floatval;
        } else if (paramType == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__BOOL_TYPE) {
            ASSERT(kv->has_boolval);
            val->boolval = kv->boolval ? TRUE_ : FALSE_;
        } else {
            status = XIA_BAD_VALUE;
        }
    } else {
        status = XIA_BAD_VALUE;
    }

    si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

    return status;
}

/*
 * Perform the specified gain operation to the hardware.
*
*/
PSL_STATIC int psl__GainOperation(int detChan, const char *name, void *value,
                                  Detector *det, int modChan, Module *m, XiaDefaults *defs)
{
  int status;
  double* scaleFactor = (double*) value;

  ASSERT(name  != NULL);
  ASSERT(value != NULL);
  ASSERT(defs  != NULL);
  ASSERT(det   != NULL);
  ASSERT(m     != NULL);

  if (STREQ(name, "calibrate")) {
    status = psl__GainCalibrate(detChan, det, modChan, m, defs, scaleFactor);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error doing gain operation '%s' for detChan %d", name, detChan);
    }

    return status;
  }

  pslLog(PSL_LOG_ERROR, XIA_BAD_NAME,
         "Unknown gain operation '%s' for detChan %d", name, detChan);
  return XIA_BAD_NAME;
}

PSL_STATIC int psl__GetADCTraceLength(Module* module, Detector* detector,
                                      int64_t* length)
{
    int status;
    psl__SincParamValue sincVal;

    status = psl__GetParamValue(module,  psl__DetectorChannel(detector),
                                "oscilloscope.samples",
                                SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE,
                                &sincVal);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the oscilloscope sample count");
        return status;
    }

    *length = sincVal.intval;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__SetADCTraceLength(Module* module, Detector* detector,
                                      int64_t length)
{
    int status;
    SiToro__Sinc__KeyValue kv;

    if (length > FALCONXN_MAX_ADC_SAMPLES) {
        pslLog(PSL_LOG_WARNING, "%" PRIi64 " is out of range for adc_trace_length."
               " Coercing to %d.",
               length, FALCONXN_MAX_ADC_SAMPLES);
        length = FALCONXN_MAX_ADC_SAMPLES;
    }

    si_toro__sinc__key_value__init(&kv);
    kv.key = (char*) "oscilloscope.samples";
    kv.has_intval = TRUE_;
    kv.intval = length;

    status = psl__SetParam(module, detector, &kv);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to set the oscilloscope sample count");
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__GetADCTrace(Module* module, Detector* detector, void* buffer)
{
    int status;

    uint8_t    pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);

    int32_t* in;
    unsigned int* out;
    int s;

    FalconXNDetector* fDetector = detector->pslData;

    SiToro__Sinc__KeyValue kv;

    pslLog(PSL_LOG_INFO,
           "ADC trace channel %d", fDetector->modDetChan);

    si_toro__sinc__key_value__init(&kv);
    kv.key = (char*) "oscilloscope.runContinuously";
    kv.has_boolval = TRUE_;
    kv.boolval = FALSE_;

    status = psl__SetParam(module, detector, &kv);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to set the oscilloscope run mode");
            return status;
    }

    SincEncodeStartOscilloscope(&packet, psl__DetectorChannel(detector));

    status = psl__ModuleTransactionSend(module, &packet);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error starting oscilloscope mode");
        return status;
    }

    status = psl__CheckSuccessResponse(module);

    psl__ModuleTransactionEnd(module);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Starting oscilloscope failed");
        return status;
    }

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS)
        return status;

    /*
     * Wait for the ready state.
     */
    fDetector->asyncReady = TRUE_;

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS)
        return status;

    status = psl__DetectorWait(detector, FALCONXN_ADC_TRACE_TIMEOUT * 1000);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Oscilloscope data error or timeout");
        return status;
    }

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS)
        return status;

    in = fDetector->adcTrace.intData;
    out = ((unsigned int*) buffer);
    for (s = 0; s < fDetector->adcTrace.len; ++s) {
        /* Convert signed values into our unsigned range. adcTrace
         * minRange/maxRange are typically -0x10000/2 - 1 to 0x10000
         */
      *out++ =
        (unsigned int) (*in++) - (unsigned int) fDetector->adcTrace.minRange;
    }

    free(fDetector->adcTrace.data);
    fDetector->adcTrace.data = NULL;

    free(fDetector->adcTrace.intData);
    fDetector->adcTrace.intData = NULL;

    fDetector->adcTrace.len = 0;

    status = psl__DetectorUnlock(detector);

    return status;
}

PSL_STATIC int psl__GetCalibration(Module* module, Detector* detector)
{
    int status = XIA_SUCCESS;

    uint8_t    pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);

    Sinc_Response response;

    SincEncodeGetCalibration(&packet, psl__DetectorChannel(detector));

    status = psl__ModuleTransactionSend(module, &packet);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error requesting detector characterisation data");
        return status;
    }

    /*
     * The 'resp'onse is NULL. The decoder loaded the data into the detector.
     */
    response.channel = psl__DetectorChannel(detector);
    response.type = SI_TORO__SINC__MESSAGE_TYPE__GET_CALIBRATION_RESPONSE;

    status = psl__ModuleTransactionReceive(module, &response);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Receiving the response");
    }

    psl__ModuleTransactionEnd(module);

    return status;
}

PSL_STATIC int psl__SetCalibration(Module* module, Detector* detector)
{
    int status = XIA_SUCCESS;

    uint8_t    pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);

    FalconXNDetector* fDetector = detector->pslData;

    SincEncodeSetCalibration(&packet,
                             fDetector->modDetChan,
                             &fDetector->calibData,
                             &fDetector->calibExample,
                             &fDetector->calibModel,
                             &fDetector->calibFinal);

    status = psl__ModuleTransactionSend(module, &packet);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error setting the detector characterisation");
        return status;
    }

    status = psl__CheckSuccessResponse(module);

    psl__ModuleTransactionEnd(module);

    return XIA_SUCCESS;
}

/*
 * Set the specified acquisition value. Values are always of
 * type double.
 */
PSL_STATIC int psl__SetAcquisitionValues(int        detChan,
                                         Detector*  detector,
                                         Module*    module,
                                         const char *name,
                                         void       *value)
{
    FalconXNDetector* fDetector = NULL;

    double dvalue = *((double*) value);

    AcquisitionValue* acq;

    ASSERT(detector);
    ASSERT(detector->pslData);
    ASSERT(module);
    ASSERT(name);
    ASSERT(value);

    fDetector = detector->pslData;

    pslLog(PSL_LOG_DEBUG,
           "%s (%d/%u): %s -> %0.3f.",
           module->alias, fDetector->modDetChan, detChan, name, dvalue);

    acq = psl__GetAcquisition(fDetector, name);

    if (acq) {
        int status;

        XiaDefaults *defaults;

        if ((acq->flags & PSL_ACQ_READ_ONLY) != 0) {
            status = XIA_NO_MODIFY;
            pslLog(PSL_LOG_ERROR, status,
                   "Attribute is read-only: %s", name);
            return status;
        }

        psl__CheckConnected(fDetector);

        defaults = xiaGetDefaultFromDetChan(detChan);

        /* Validate the value and send it to the board */
        status = acq->handler(module, detector, fDetector->modDetChan,
                              fDetector, defaults, acq, name, &dvalue, FALSE_);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error writing in acquisition value handler: %s", name);
            return status;
        }

        /* Sync the new (possibly coerced) value into the PSL acq copy */
        status = psl__SetAcqValue(acq, dvalue);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error syncing acq value for acquisition value handler: %s",
                   name);
            return status;
        }

        /* Sync the new value to the Handel defaults, so save system sees it */
        status = psl__UpdateDefault(defaults, name, acq);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error updating default for acquisition value handler: %s",
                   name);
            return status;
        }

        return XIA_SUCCESS;
    }

    pslLog(PSL_LOG_ERROR, XIA_UNKNOWN_VALUE,
           "Unknown acquisition value '%s' for detChan %d.", name, detChan);
    return XIA_UNKNOWN_VALUE;
}


/*
 * Retrieve the current value of the requested acquisition
 *  value as a double.
 */
PSL_STATIC int psl__GetAcquisitionValues(int        detChan,
                                         Detector*  detector,
                                         Module*     module,
                                         const char* name,
                                         void*       value)
{
    int status;
    XiaDefaults *defaults = NULL;
    FalconXNDetector* fDetector = NULL;
    AcquisitionValue* acq;

    double dvalue = 0;

    ASSERT(detector);
    ASSERT(module);
    ASSERT(name);
    ASSERT(value);

    defaults = xiaGetDefaultFromDetChan(detChan);

    if (defaults == NULL) {
        xiaLog(XIA_LOG_ERROR, XIA_INCOMPLETE_DEFAULTS, "psl__GetAcquisitionValues",
               "Unable to get the defaults for detChan %d.", detChan);
        return XIA_INCOMPLETE_DEFAULTS;
    }

    fDetector = detector->pslData;

    if (!fDetector) {
        status = XIA_INVALID_DETCHAN;
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the value of '%s' for detChan %d because the channel is not set up.",
               name, detChan);
        return status;
    }

    acq = psl__GetAcquisition(fDetector, name);

    if (!acq) {
        status = XIA_NOT_FOUND;
        xiaLog(XIA_LOG_ERROR, status, "psl__GetAcquisitionValues",
               "Unable to get the ACQ value '%s' for detChan %d.", name, detChan);
        return status;
    }

    /* Check the Handel default if we expect there is one.
     *
     * TODO Not used in falconxn implementation, considering deleting
     * This could potentially retrieve the previous default value. The getter
     * can be left blank if a refresh from the device is not needed.
     */
    if (PSL_ACQ_FLAG_SET(acq, PSL_ACQ_HAS_DEFAULT)) {
        status = pslGetDefault(name, value, defaults);

        if ((status != XIA_SUCCESS) && (status != XIA_NOT_FOUND)) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the value of '%s' for detChan %d.", name, detChan);
            return status;
        }
    }

    psl__CheckConnected(fDetector);

    /* Get the value from the board */
    status = acq->handler(module, detector, fDetector->modDetChan,
                          fDetector, defaults, acq, name, &dvalue, TRUE_);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error reading in acquisition value handler: %s",
               acq->name);
        return status;
    }

    *((double*) value) = dvalue;

    /* Sync the new value to the Handel defaults, so save system sees
     * values that we refresh that may have been updated by the box and
     * not the user (e.g. during characterization).
     */
    status = psl__UpdateDefault(defaults, name, acq);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error updating default for acquisition value handler: %s",
               acq->name);
        return status;
    }

    return XIA_SUCCESS;
}

/* Get or set the gain, between 1 and 16 inclusive.
 */
ACQ_HANDLER_DECL(analog_gain)
{
    /* It was determined that we want to keep SiToro's dacgain value between
     * 10% and 90% of the 12bit range with logarithmic growth within that range.
     */

    int status;

    UNUSED(defaults);
    UNUSED(fDetector);


    ACQ_HANDLER_LOG(gain);

    if (read) {
        SiToro__Sinc__GetParamResponse* resp = NULL;
        SiToro__Sinc__KeyValue*         kv;

        status = psl__GetParam(module, channel, "afe.dacGain", &resp);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the DAC gain");
            return status;
        }

        kv = resp->results[0];

        if (kv->has_paramtype &&
            (kv->paramtype == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__FLOAT_TYPE)) {
            acq->value.ref.f = pow(16.0, (kv->floatval - 409.6) / (8.0 * 409.6));
            *value = acq->value.ref.f;
        } else {
            status = XIA_BAD_VALUE;
        }

        si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "DAC gain response");
            return status;
        }
    }
    else {
        SiToro__Sinc__KeyValue kv;

        if (*value < ADC_GAIN_MIN || ADC_GAIN_MAX < *value) {
            pslLog(PSL_LOG_ERROR, XIA_DAC_GAIN_OOR,
                   "DAC gain value of %0.2f is outside acceptable range of [1,16]", *value);
            return XIA_DAC_GAIN_OOR;
        }

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "afe.dacGain";
        kv.has_floatval = TRUE_;

        /* Equivalent to the formula 'Gain = a + 8a * log16(dB)'
         * where 'a' is one tenth of the range 4096 and dB is between [1,16]
         */
        kv.floatval = 409.6 + 8.0 * 409.6 * log10(*value) / log10(16.0);

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the DAC gain");
            return status;
        }

        /* Apply the correct rounding to the value to be sent back to the user. */
        *value = pow(16.0, (kv.floatval - 409.6) / (8.0 * 409.6));
    }

    return XIA_SUCCESS;
}

/*
 * Get or set analog offset, between -2048 and 2047 inclusive.
 */
ACQ_HANDLER_DECL(analog_offset)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(analog_offset);

    if (read) {
        SiToro__Sinc__GetParamResponse* resp = NULL;
        SiToro__Sinc__KeyValue*         kv;

        status = psl__GetParam(module, channel, "afe.dacOffset", &resp);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the DAC offset");
            return status;
        }

        kv = resp->results[0];

        if (kv->has_floatval) {
            acq->value.ref.f = kv->floatval + DAC_OFFSET_MIN;
            *value = acq->value.ref.f;
        } else {
            status = XIA_BAD_VALUE;
        }

        si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "DAC gain response");
            return status;
        }
    }
    else {
        SiToro__Sinc__KeyValue kv;

        if (*value < DAC_OFFSET_MIN || DAC_OFFSET_MAX < *value) {
            pslLog(PSL_LOG_ERROR, XIA_DAC_GAIN_OOR,
                "DAC gain value of %0.3f is outside acceptable range of [1,16]",
                *value);
            return XIA_DAC_GAIN_OOR;
        }

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "afe.dacOffset";
        kv.has_floatval = TRUE_;
        kv.floatval = *value - DAC_OFFSET_MIN;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the DAC offset");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(detector_polarity)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);


    ACQ_HANDLER_LOG(detector_polarity);

    if (read) {
        SiToro__Sinc__GetParamResponse* resp = NULL;
        SiToro__Sinc__KeyValue*         kv;

        status = psl__GetParam(module, channel, "afe.invert", &resp);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the detector polarity");
            return status;
        }

        kv = resp->results[0];

        if (kv->has_boolval) {
           /* inverted bool value between Handel and SINC semantics */
            acq->value.ref.b = !PSL_BOOL_OF_KV(kv);
            *value = PSL_ACQ_GET_BOOL(acq);
        } else {
            status = XIA_BAD_VALUE;
        }

        si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "DAC gain response");
            return status;
        }
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "afe.invert";
        kv.has_boolval = TRUE_;
         /* inverted bool value between Handel and SINC semantics */
        kv.boolval = *value == 0.0 ? TRUE_ : FALSE_;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the detector polarity");
            return status;
        }

        /* Sync the user's value to the detector struct, which is the
         * official record on startup (as effected by the sync routine).
         */
        int detPhysChannel = xiaGetModDetectorChan(channel);
        detector->polarity[detPhysChannel] = (unsigned short) *value;
    }

    return XIA_SUCCESS;
}

ACQ_SYNC_DECL(detector_polarity)
{
    int status;

    double polarity = 0.0;

    UNUSED(module);

    polarity = detector->polarity[channel];

    ACQ_SYNC_LOG(detector_polarity, polarity);

    status = pslSetDefault("detector_polarity", (void *)&polarity, defaults);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error synchronizing detector_polarity for detector %d", detChan);
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(termination)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);


    ACQ_HANDLER_LOG(termination);

    if (read) {
        SiToro__Sinc__GetParamResponse* resp = NULL;
        SiToro__Sinc__KeyValue*         kv;

        status = psl__GetParam(module, channel, "afe.termination", &resp);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the termination");
            return status;
        }

        kv = resp->results[0];

        if (kv->has_paramtype &&
            (kv->paramtype == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__OPTION_TYPE)) {
            if (STREQ(kv->optionval, "1kohm"))
                acq->value.ref.i = 0;
            else if (STREQ(kv->optionval, "50ohm"))
                acq->value.ref.i = 1;
            else {
                status = XIA_BAD_VALUE;
            }
            if (status == XIA_SUCCESS)
                *value = (double) acq->value.ref.i;
        } else {
            status = XIA_BAD_VALUE;
        }

        si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "termination response");
            return status;
        }
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "afe.termination";
        if (*value == 0.0)
          kv.optionval = (char*) "1kohm";
        else if (*value == 1.0)
          kv.optionval = (char*) "50ohm";
        else {
            status = XIA_BAD_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "invalid termination value");
            return status;
        }

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the termination");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(attenuation)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);


    ACQ_HANDLER_LOG(attenuation);

    if (read) {
        SiToro__Sinc__GetParamResponse* resp = NULL;
        SiToro__Sinc__KeyValue*         kv;

        status = psl__GetParam(module, channel, "afe.attn", &resp);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the attenuation");
            return status;
        }

        kv = resp->results[0];

        if (kv->has_paramtype &&
            (kv->paramtype == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__OPTION_TYPE)) {
            if (STREQ(kv->optionval, "0dB"))
                acq->value.ref.i = 0;
            else if (STREQ(kv->optionval, "-6dB"))
                acq->value.ref.i = 1;
            else if (STREQ(kv->optionval, "ground"))
                acq->value.ref.i = 2;
            else {
                status = XIA_BAD_VALUE;
            }
            if (status == XIA_SUCCESS)
                *value = (double) acq->value.ref.i;
        } else {
            status = XIA_BAD_VALUE;
        }

        si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "attenuation response");
            return status;
        }
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "afe.attn";
        if (*value == 0.0)
          kv.optionval = (char*) "0dB";
        else if (*value == 1.0)
          kv.optionval = (char*) "-6dB";
        else if (*value == 2.0)
          kv.optionval = (char*) "ground";
        else {
            status = XIA_BAD_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "invalid attenuation value");
            return status;
        }

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the attenuation");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(coupling)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);


    ACQ_HANDLER_LOG(coupling);

    if (read) {
        SiToro__Sinc__GetParamResponse* resp = NULL;
        SiToro__Sinc__KeyValue*         kv;

        status = psl__GetParam(module, channel, "afe.coupling", &resp);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the coupling");
            return status;
        }

        kv = resp->results[0];

        if (kv->has_paramtype &&
            (kv->paramtype == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__OPTION_TYPE)) {
            if (STREQ(kv->optionval, "ac"))
                acq->value.ref.i = 0;
            else if (STREQ(kv->optionval, "dc"))
                acq->value.ref.i = 1;
            else {
                status = XIA_BAD_VALUE;
            }
            if (status == XIA_SUCCESS)
                *value = (double) acq->value.ref.i;
        } else {
            status = XIA_BAD_VALUE;
        }

        si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "coupling response");
            return status;
        }
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "afe.coupling";
        if (*value == 0.0)
          kv.optionval = (char*) "ac";
        else if (*value == 1.0)
          kv.optionval = (char*) "dc";
        else {
            status = XIA_BAD_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "invalid coupling value");
            return status;
        }

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the coupling");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(decay_time)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);


    ACQ_HANDLER_LOG(decay_time);

    if (read) {
        SiToro__Sinc__GetParamResponse* resp = NULL;
        SiToro__Sinc__KeyValue*         kv;

        status = psl__GetParam(module, channel, "afe.decayTime", &resp);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the decay time");
            return status;
        }

        kv = resp->results[0];

        if (kv->has_paramtype &&
            (kv->paramtype == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__OPTION_TYPE)) {
            if (STREQ(kv->optionval, "long"))
                acq->value.ref.i = (int64_t)XIA_DECAY_LONG;
            else if (STREQ(kv->optionval, "medium"))
                acq->value.ref.i = (int64_t)XIA_DECAY_MEDIUM;
            else if (STREQ(kv->optionval, "short"))
                acq->value.ref.i = (int64_t)XIA_DECAY_SHORT;
            else if (STREQ(kv->optionval, "very-short"))
                acq->value.ref.i = (int64_t)XIA_DECAY_VERY_SHORT;
            else {
                status = XIA_BAD_VALUE;
            }
            if (status == XIA_SUCCESS)
                *value = (double) acq->value.ref.i;
        } else {
            status = XIA_BAD_VALUE;
        }

        si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "decay time response");
            return status;
        }
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "afe.decayTime";
        if (*value == XIA_DECAY_LONG)
          kv.optionval = (char*) "long";
        else if (*value == XIA_DECAY_MEDIUM)
          kv.optionval = (char*) "medium";
        else if (*value == XIA_DECAY_SHORT)
          kv.optionval = (char*) "short";
        else if (*value == XIA_DECAY_VERY_SHORT)
          kv.optionval = (char*) "very-short";
        else {
            status = XIA_BAD_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "invalid decay time value");
            return status;
        }

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the attenuation");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(dc_offset)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(dc_offset);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel, "baseline.dcOffset",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__FLOAT_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the parameter for DC offset");
            return status;
        }

        acq->value.ref.f = *value = sincVal.floatval;
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "baseline.dcOffset";
        kv.has_floatval = TRUE_;
        kv.floatval = *value;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the parameter for DC offset");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(reset_blanking_enable)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(reset_blanking_enable);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel, "blanking.enable",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__BOOL_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the parameter for reset blanking enable");
            return status;
        }

        acq->value.ref.b = sincVal.boolval;
        *value = PSL_ACQ_GET_BOOL(acq);
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "blanking.enable";
        kv.has_boolval = TRUE_;
        kv.boolval = *value != 0;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the parameter for reset blanking enable");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(reset_blanking_threshold)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(reset_blanking_threshold);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel, "blanking.threshold",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__FLOAT_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the parameter for reset blanking threshold");
            return status;
        }

        acq->value.ref.f = *value = sincVal.floatval;
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "blanking.threshold";
        kv.has_floatval = TRUE_;
        kv.floatval = *value;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the parameter for reset blanking threshold");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(reset_blanking_presamples)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(reset_blanking_presamples);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel, "blanking.preSamples",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get parameter for reset blanking pre-samples");
            return status;
        }

        acq->value.ref.i = sincVal.intval;
        *value = (double) acq->value.ref.i;
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "blanking.preSamples";
        kv.has_intval = TRUE_;
        kv.intval = (int64_t)*value;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set parameter for reset blanking pre-samples");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(reset_blanking_postsamples)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(reset_blanking_postsamples);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel, "blanking.postSamples",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get parameter for reset blanking post-samples");
            return status;
        }

        acq->value.ref.i = sincVal.intval;
        *value = (double) acq->value.ref.i;
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "blanking.postSamples";
        kv.has_intval = TRUE_;
        kv.intval = (int64_t)*value;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set parameter for reset blanking post-samples");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(detection_threshold)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(detection_threshold);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel, "pulse.detectionThreshold",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__FLOAT_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the parameter for pulse detection threshold");
            return status;
        }

        acq->value.ref.f = *value = sincVal.floatval;
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "pulse.detectionThreshold";
        kv.has_floatval = TRUE_;
        kv.floatval = *value;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the parameter for pulse detection threshold");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(min_pulse_pair_separation)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(min_pulse_pair_separation);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel, "pulse.minPulsePairSeparation",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get parameter for minimum pulse pair separation");
            return status;
        }

        acq->value.ref.i = sincVal.intval;
        *value = (double) acq->value.ref.i;
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "pulse.minPulsePairSeparation";
        kv.has_intval = TRUE_;
        kv.intval = (int64_t)*value;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set parameter for minimum pulse pair separation");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(detection_filter)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);


    ACQ_HANDLER_LOG(detection_filter);

    if (read) {
        SiToro__Sinc__GetParamResponse* resp = NULL;
        SiToro__Sinc__KeyValue*         kv;

        status = psl__GetParam(module, channel, "pulse.sourceType", &resp);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status, "Unable to get the source type");
            return status;
        }

        kv = resp->results[0];

        if (kv->has_paramtype &&
            (kv->paramtype == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__OPTION_TYPE)) {
            if (STREQ(kv->optionval, "lowEnergy"))
                acq->value.ref.i = XIA_FILTER_LOW_ENERGY;
            else if (STREQ(kv->optionval, "lowRate"))
                acq->value.ref.i = XIA_FILTER_LOW_RATE;
            else if (STREQ(kv->optionval, "midRate"))
                acq->value.ref.i = XIA_FILTER_MID_RATE;
            else if (STREQ(kv->optionval, "highRate"))
                acq->value.ref.i = XIA_FILTER_HIGH_RATE;
            else if (STREQ(kv->optionval, "maxThroughput"))
                acq->value.ref.i = XIA_FILTER_MAX_THROUGHPUT;
            else {
                status = XIA_BAD_VALUE;
            }
            if (status == XIA_SUCCESS)
                *value = (double) acq->value.ref.i;
        } else {
            status = XIA_BAD_VALUE;
        }

        si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "source type response");
            return status;
        }
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "pulse.sourceType";
        if (*value == XIA_FILTER_LOW_ENERGY)
            kv.optionval = (char*) "lowEnergy";
        else if (*value == XIA_FILTER_LOW_RATE)
            kv.optionval = (char*) "lowRate";
        else if (*value == XIA_FILTER_MID_RATE)
            kv.optionval = (char*) "midRate";
        else if (*value == XIA_FILTER_HIGH_RATE)
            kv.optionval = (char*) "highRate";
        else if (*value == XIA_FILTER_MAX_THROUGHPUT)
            kv.optionval = (char*) "maxThroughput";
        else {
            status = XIA_BAD_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "invalid detection filter value");
            return status;
        }

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the source type");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(clock_speed)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);
    UNUSED(detector);

    ACQ_HANDLER_LOG(clock_speed);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel, "afe.sampleRate",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status, "Unable to get the sample rate");
            return status;
        }

        acq->value.ref.i = sincVal.intval;
        *value = (double) acq->value.ref.i;
    }
    else {
        status = XIA_READ_ONLY;
        pslLog(PSL_LOG_ERROR, status,
               "Unable to set the clock speed, read only");
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(mapping_mode)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);
    UNUSED(detector);
    UNUSED(channel);
    UNUSED(module);

    ACQ_HANDLER_LOG(mapping_mode);

    if (read) {
        *value = (double) acq->value.ref.i;
    }
    else {
        /*
         * See PSL__CONVERT_TO.
         */
        status = psl__ConvertTo_MM_Mode(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid mapping_mode: %f", *value);
            return status;
        }
    }

    return XIA_SUCCESS;
}

/* This acquisition value only caches the value. The set is performed
 * on run start because a single SINC param is shared by preset_type
 * and pixel_advance_mode.
 */
ACQ_HANDLER_DECL(preset_type)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);
    UNUSED(acq);
    UNUSED(detector);

    ACQ_HANDLER_LOG(preset_type);

    if (read) {
        *value = (double) acq->value.ref.i;
    }
    else {
        if (*value == XIA_PRESET_NONE)
            ;
        else if (*value == (int)XIA_PRESET_FIXED_REAL)
            ;
        else if (*value == (int)XIA_PRESET_FIXED_TRIGGERS)
            ;
        else if (*value == (int)XIA_PRESET_FIXED_EVENTS)
            ;
        else {
            status = XIA_BAD_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "invalid histogram mode value");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(number_mca_channels)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(number_mca_channels);

    if (read) {
        psl__SincParamValue sincVal;
        int64_t lowIndex, highIndex;

        status = psl__GetParamValue(module, channel, "histogram.binSubRegion.highIndex",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram region high index");
            return status;
        }

        highIndex = sincVal.intval;

        status = psl__GetParamValue(module, channel, "histogram.binSubRegion.lowIndex",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram region low index");
            return status;
        }

        lowIndex = sincVal.intval;

        acq->value.ref.i = highIndex - lowIndex + 1;
        *value = (double) acq->value.ref.i;
    }
    else {
        if (*value > MAX_MCA_CHANNELS || *value < MIN_MCA_CHANNELS) {
            status = XIA_BAD_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Number MCA channels %0.3f is out of range [%0.3f,%0.3f]",
                   *value, MIN_MCA_CHANNELS, MAX_MCA_CHANNELS);
            return status;
        }

        acq->value.ref.i = (int64_t) *value;

        status = psl__SyncNumberMCAChannels(module, detector);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to sync bin sub region to set number_mca_channels");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(mca_spectrum_accepted)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(mca_spectrum_accepted);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel,
                                    "histogram.spectrumSelect.accepted",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__BOOL_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram spectrum select accepted");
            return status;
        }

        acq->value.ref.b = sincVal.boolval;
        *value = PSL_ACQ_GET_BOOL(acq);
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "histogram.spectrumSelect.accepted";
        kv.has_boolval = TRUE_;
        kv.boolval = *value != 0;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the histogram select accepted");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(mca_spectrum_rejected)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(mca_spectrum_rejected);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel,
                                    "histogram.spectrumSelect.rejected",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__BOOL_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram spectrum select rejected");
            return status;
        }

        acq->value.ref.b = sincVal.boolval;
        *value = PSL_ACQ_GET_BOOL(acq);
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "histogram.spectrumSelect.rejected";
        kv.has_boolval = TRUE_;
        kv.boolval = *value != 0;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the histogram select rejected");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(mca_start_channel)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(mca_start_channel);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel,
                                    "histogram.binSubRegion.lowIndex",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram bin subregion lower index");
            return status;
        }

        acq->value.ref.i = sincVal.intval;
        *value = (double) acq->value.ref.i;
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "histogram.binSubRegion.lowIndex";
        kv.has_intval = TRUE_;
        kv.intval = (int64_t)*value;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the histogram bin subregion lower index");
            return status;
        }

        acq->value.ref.i = (int64_t)*value;

        status = psl__SyncNumberMCAChannels(module, detector);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to sync bin sub region for setting mca_start_channel");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(mca_refresh)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(mca_refresh);

    if (read) {
        /* Return the cached value; the value on the box may be garbage for mm1. */
        *value = acq->value.ref.f;
    }
    else {
        /* Set to the box for validation. */
        status = psl__SetMCARefresh(module, detector, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the histogram refresh period");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(preset_value)
{
    int status;
    AcquisitionValue *preset_type;
    char *param;
    boolean_t useIntVal = TRUE_;

    UNUSED(defaults);
    UNUSED(acq);

    ACQ_HANDLER_LOG(preset_value);

    preset_type = psl__GetAcquisition(fDetector, "preset_type");
    ASSERT(preset_type);

    pslLog(PSL_LOG_DEBUG, "%s:%d preset type:%d",
           module->alias, channel, (int) preset_type->value.ref.i);

    switch (preset_type->value.ref.i) {
    case (int)XIA_PRESET_NONE:
    case (int)XIA_PRESET_FIXED_REAL:
        param = (char*) "histogram.fixedTime.duration";
        useIntVal = FALSE_;
        break;
    case (int)XIA_PRESET_FIXED_TRIGGERS:
        param = (char*) "histogram.fixedInputCount.count";
        break;
    case (int)XIA_PRESET_FIXED_EVENTS:
        param = (char*) "histogram.fixedOutputCount.count";
        break;
    default:
        status = XIA_BAD_VALUE;
        pslLog(PSL_LOG_ERROR, status,
               "invalid histogram mode value");
        return status;
    }

    if (read) {
        SiToro__Sinc__GetParamResponse* resp = NULL;
        SiToro__Sinc__KeyValue*         kv;

        status = psl__GetParam(module, channel, param, &resp);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get %s for preset_value", param);
            return status;
        }

        kv = resp->results[0];

        if (kv->has_floatval) {
            ASSERT(!useIntVal);
            *value = kv->floatval;
        }
        else if (kv->has_intval) {
            ASSERT(useIntVal);
            *value = (double) kv->intval;
        }
        else {
            status = XIA_BAD_VALUE;
        }

        si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status, "%s response", param);
            return status;
        }
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = param;

        if (useIntVal) {
            kv.has_intval = TRUE_;
            kv.intval = (int64_t) *value;
        }
        else {
            kv.has_floatval = TRUE_;
            kv.floatval = *value;
        }

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the histogram fixed time duration");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(scale_factor)
{
    int status;

    UNUSED(defaults);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(scale_factor);

    if (read) {
        *value = acq->value.ref.f;
    }
    else {
        SiToro__Sinc__KeyValue kv;

        /* scale_factor = pulse_scale_factor * coarse_bin_scale */

        if (*value > SCALE_FACTOR_MAX || *value < SCALE_FACTOR_MIN) {
            pslLog(PSL_LOG_ERROR, XIA_BAD_VALUE,
                   "Requested scale_factor %f is out of range [%f,%f] for channel %s:%d",
                   *value, SCALE_FACTOR_MIN, SCALE_FACTOR_MAX, module->alias, channel);
            return XIA_BAD_VALUE;
        }

        /* Find the nearest power of 2 value for coarse bin scale. */
        double cbs = ROUND(log2(*value));
        cbs = fmin(7, cbs);
        cbs = fmax(1, cbs);
        cbs = pow(2, cbs);

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "histogram.coarseBinScaling";
        kv.has_intval = TRUE_;
        kv.intval = (int64_t)cbs;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set histogram.coarseBinScaling for scale_factor");
            return status;
        }

        /* "Fine trim" using the pulse scale factor */
        double psf = *value / cbs;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "pulse.scaleFactor";
        kv.has_floatval = TRUE_;
        kv.floatval = psf;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set pulse.scaleFactor for scale_factor");
            return status;
        }

        *value = psf * cbs;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(mca_bin_width)
{
    UNUSED(defaults);
    UNUSED(fDetector);
    UNUSED(detector);

    ACQ_HANDLER_LOG(mca_bin_width);

    if (read) {
        *value = acq->value.ref.f;
    }
    else {
        acq->value.ref.f = *value;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(sca_trigger_mode)
{
    int status = XIA_SUCCESS;

    UNUSED(defaults);
    UNUSED(detector);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(sca_trigger_mode);

    if (read) {
        SiToro__Sinc__GetParamResponse* resp = NULL;
        SiToro__Sinc__KeyValue*         kv;

        status = psl__GetParam(module, channel,
                               "instrument.sca.generationTrigger", &resp);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the sca generationTrigger value");
            return status;
        }

        kv = resp->results[0];

        if (kv->has_paramtype &&
            (kv->paramtype == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__OPTION_TYPE)) {
            if (STREQ(kv->optionval, "off"))
                acq->value.ref.i = SCA_TRIGGER_OFF;
            else if (STREQ(kv->optionval, "whenHigh"))
                acq->value.ref.i = SCA_TRIGGER_HIGH;
            else if (STREQ(kv->optionval, "whenLow"))
                acq->value.ref.i = SCA_TRIGGER_LOW;
            else if (STREQ(kv->optionval, "always"))
                acq->value.ref.i = SCA_TRIGGER_ALWAYS;
            else {
                status = XIA_BAD_VALUE;
            }
            if (status == XIA_SUCCESS)
                *value = (double) acq->value.ref.i;
        } else {
            status = XIA_BAD_VALUE;
        }

        si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                  "Unable to parse sca generationTrigger response");
            return status;
        }
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "instrument.sca.generationTrigger";
        if (*value == SCA_TRIGGER_OFF)
            kv.optionval = (char*) "off";
        else if (*value == SCA_TRIGGER_HIGH)
            kv.optionval = (char*) "whenHigh";
        else if (*value == SCA_TRIGGER_LOW)
            kv.optionval = (char*) "whenLow";
        else if (*value == SCA_TRIGGER_ALWAYS)
            kv.optionval = (char*) "always";
        else {
            status = XIA_BAD_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "invalid sca generationTrigger value");
            return status;
        }

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                  "Unable to set the sca generationTrigger");
            return status;
        }
    }

    return status;
}

ACQ_HANDLER_DECL(sca_pulse_duration)
{
    int status = XIA_SUCCESS;

    UNUSED(detector);
    UNUSED(fDetector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(sca_pulse_duration);

    if (read) {
        psl__SincParamValue sincVal;

        status = psl__GetParamValue(module, channel,
                                    "instrument.sca.pulseDuration",
                                    SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE,
                                    &sincVal);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get parameter instrument.sca.pulseDuration");
            return status;
        }

        acq->value.ref.i = sincVal.intval;
        *value = (double) acq->value.ref.i;
    }
    else {
        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key = (char*) "instrument.sca.pulseDuration";
        kv.has_intval = TRUE_;
        kv.intval = (int64_t)*value;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set parameter instrument.sca.pulseDuration");
            return status;
        }
    }

    return status;
}

ACQ_HANDLER_DECL(number_of_scas)
{
    int status = XIA_SUCCESS;

    int max_number_of_scas = 0;
    int number_of_scas = (int)*value;

    UNUSED(detector);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(number_of_scas);

    if (read) {
      *value = (double) acq->value.ref.i;
    }
    else {
      psl__GetMaxNumberSca(channel, module, &max_number_of_scas);

      if (number_of_scas > max_number_of_scas) {
          status = XIA_INVALID_VALUE;
          pslLog(PSL_LOG_ERROR, status,
                 "number of sca %d greater than maximum allowed (%d).",
                 number_of_scas, max_number_of_scas);
          return status;
      }

      status = pslSetNumberSCAs(module, defaults, channel, number_of_scas);

      if (status != XIA_SUCCESS) {
          pslLog(PSL_LOG_ERROR, status, "Unable to set the number of sca");
          return status;
      }

      acq->value.ref.i = number_of_scas;
    }

    return status;
}

ACQ_HANDLER_DECL(sca)
{
    int status;

    unsigned short scaNum = 0;

    char limit[9];

    AcquisitionValue * number_of_scas;

    UNUSED(defaults);
    UNUSED(channel);
    UNUSED(detector);
    UNUSED(acq);

    ACQ_HANDLER_LOG(sca);

    ASSERT(STRNEQ(name, "sca"));

    sscanf(name, "sca%hu_%s", &scaNum, limit);

    /* If an unexpected acq name is requested treat it as a not found error */
    if (!(STREQ(limit, "lo") || STREQ(limit, "hi"))) {
          pslLog(PSL_LOG_ERROR, XIA_NOT_FOUND, "Unexpected acqusition name "
            "string '%s'", name);
      return XIA_NOT_FOUND;
    }

    number_of_scas = psl__GetAcquisition(fDetector, "number_of_scas");
    ASSERT(number_of_scas);

    if (scaNum >= number_of_scas->value.ref.i) {
      pslLog(PSL_LOG_ERROR, XIA_SCA_OOR, "Requested SCA number '%" PRIu16 "' is larger"
            " than the number of SCAs (%" PRIu64 ") for channel %u",
              scaNum, number_of_scas->value.ref.i, channel);
      return XIA_SCA_OOR;
    }

    if (read) {
        /* Since we can't put it all in the acq storage, the sca limit values
         * rely on the defaults data structure */
        status = pslGetDefault(name, value, defaults);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,"Unable to get the value of '%s' for "
                   "detChan %d.", name, channel);
            return status;
        }
    }
    else {
        char keyname[24];

        /* NOTE that the sinc indexing is 1-based */
        snprintf(keyname, sizeof(keyname),
                 "sca.region_%02" PRIu16 ".%s", scaNum + 1,
                 STREQ(limit, "lo") ? "startBin" : "endBin");

        SiToro__Sinc__KeyValue kv;

        si_toro__sinc__key_value__init(&kv);
        kv.key =  keyname;
        kv.has_intval = TRUE_;
        kv.intval = (int64_t)*value;

        status = psl__SetParam(module, detector, &kv);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status, "Unable to set the SCA limit");
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(num_map_pixels)
{
    int status = XIA_SUCCESS;

    UNUSED(detector);
    UNUSED(fDetector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(num_map_pixels);

    if (read) {
        *value = (double) acq->value.ref.i;
    }
    else {
        /* The upper limit is constrained only by downstream mm code and
         * the buffer/pixel format. 2^32.
         */
        int64_t max = 1ull << 32;
        if ((*value < 0) || (*value > max))
            return XIA_ACQ_OOR;
        acq->value.ref.i = (int64_t) *value;
    }

    return status;
}

ACQ_HANDLER_DECL(num_map_pixels_per_buffer)
{
    int status = XIA_SUCCESS;

    UNUSED(detector);
    UNUSED(fDetector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(num_map_pixels_per_buffer);

    if (read) {
        *value = (double) acq->value.ref.i;
    }
    else {
        /* Allow special values -1.0 or 0.0 for XMAP compatibility. Both
         * mean: max possible.
         */
        if ((*value == 0.0) || (*value == -1.0))
            *value = 0.0;
        else if ((*value > 0.0) && (*value <= XMAP_MAX_PIXELS_PER_BUFFER))
            ;
        else if (*value > XMAP_MAX_PIXELS_PER_BUFFER) /* Truncate as XMAP DSP */
            *value = XMAP_MAX_PIXELS_PER_BUFFER;
        else {
            status = XIA_ACQ_OOR;
        }
    }

    return status;
}

/* This acquisition value only caches the value. The set is performed
 * on run start because a single SINC param is shared by preset_type
 * and pixel_advance_mode.
 */
ACQ_HANDLER_DECL(pixel_advance_mode)
{
    int status = XIA_SUCCESS;

    UNUSED(detector);
    UNUSED(fDetector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(pixel_advance_mode);

    if (read) {
        *value = (double) acq->value.ref.i;
    }
    else {
        if ((*value < 0) || (*value > XIA_MAPPING_CTL_GATE))
            return XIA_UNKNOWN_PT_CTL;
        acq->value.ref.i = (int64_t) *value;
    }

    return status;
}

ACQ_HANDLER_DECL(input_logic_polarity)
{
    int status = XIA_SUCCESS;

    UNUSED(defaults);
    UNUSED(detector);
    UNUSED(fDetector);

    ACQ_HANDLER_LOG(input_logic_polarity);

    if (read) {
        *value = (double) acq->value.ref.i;
    }
    else {
        if (*value != (double) XIA_GATE_COLLECT_HI &&
            *value != (double) XIA_GATE_COLLECT_LO)
            return XIA_ACQ_OOR;

        acq->value.ref.i = (int64_t) *value;
    }

    return status;
}

/* The set is performed on run start because a single SINC param is
 * shared by input_logic_polarity and gate_ignore.
 */
ACQ_HANDLER_DECL(gate_ignore)
{
    int status = XIA_SUCCESS;

    UNUSED(detector);
    UNUSED(fDetector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(gate_ignore);

    if (read) {
        *value = (double) acq->value.ref.i;
    }
    else {
        if ((*value != 0.0) && (*value != 1.0))
            return XIA_TYPEVAL_OOR;
        acq->value.ref.i = (int64_t) *value;
    }

    return status;
}

ACQ_HANDLER_DECL(sync_count)
{
    int status = XIA_SUCCESS;

    UNUSED(detector);
    UNUSED(fDetector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(sync_count);

    if (read) {
        *value = (double) acq->value.ref.i;
    }
    else {
        acq->value.ref.i = (int64_t) *value;
    }

    return status;
}

#if 0
PSL_STATIC int psl__UpdateGain(Detector* detector, FalconXNDetector* fDetector,
                               XiaDefaults* defaults, boolean_t gain, boolean_t scaling)
{
    int status = XIA_SUCCESS;

    UNUSED(fDetector);
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(gain);
    UNUSED(scaling);

    return status;
}

PSL_STATIC int psl__CalculateGain(FalconXNDetector* fDetector, double* gaindac,
                                  double* gaincoarse, double* scalefactor)
{
    int status = XIA_SUCCESS;

    UNUSED(fDetector);
    UNUSED(gaindac);
    UNUSED(gaincoarse);
    UNUSED(scalefactor);


    return status;
}
#endif

PSL_STATIC int psl__GainCalibrate(int detChan, Detector *det, int modChan,
                                  Module *m, XiaDefaults *def, double *delta)
{
    int status = XIA_SUCCESS;
    AcquisitionValue *scale_factor;
    double newScale;
    FalconXNDetector *fDetector;

    UNUSED(det);
    UNUSED(detChan);
    UNUSED(modChan);
    UNUSED(m);
    UNUSED(def);
    UNUSED(delta);

    fDetector = det->pslData;

    scale_factor = psl__GetAcquisition(fDetector, "scale_factor");
    ASSERT(scale_factor);

    pslLog(PSL_LOG_DEBUG, "Scaling scale_factor %f by gain delta %f",
           scale_factor->value.ref.f, *delta);

    newScale = *delta * scale_factor->value.ref.f;

    status = psl__SetAcquisitionValues(detChan, det, m,
                                       "scale_factor", &newScale);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Setting scale factor for gain calibration for %s:%d",
               m->alias, modChan);
        return status;
    }

    return status;
}

PSL_STATIC int psl__StartHistogram(Module* module, Detector* detector, int channel)
{
    int status;

    uint8_t    pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);

    boolean_t state_Is_ChannelHistogram = FALSE_;

    FalconXNDetector* fDetector;

    pslLog(PSL_LOG_DEBUG, "Starting Histograms on channel %s:%d",
           detector->alias, channel);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        return status;
    }

    fDetector = detector->pslData;

    if (fDetector->channelState == ChannelHistogram) {
        state_Is_ChannelHistogram = TRUE_;
    }

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS) {
        return status;
    }

    if (!state_Is_ChannelHistogram) {
        SincEncodeStartHistogram(&packet, channel);

        status = psl__ModuleTransactionSend(module, &packet);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error starting histogram transfer");
            return status;
        }

        status = psl__CheckSuccessResponse(module);

        psl__ModuleTransactionEnd(module);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to start the run for channel %s:%d",
                   detector->alias, channel);
            return status;
        }

        status = psl__DetectorLock(detector);
        if (status != XIA_SUCCESS) {
            return status;
        }

        /*
         * Wait for the histo state.
         */
        if (fDetector->channelState == ChannelHistogram) {
            state_Is_ChannelHistogram = TRUE_;
        } else {
            fDetector->asyncReady = TRUE_;
        }

        status = psl__DetectorUnlock(detector);
        if (status != XIA_SUCCESS) {
            return status;
        }

        if (!state_Is_ChannelHistogram) {
            status = psl__DetectorWait(detector,
                                       FALCONXN_CHANNEL_STATE_TIMEOUT * 1000);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Start run data error or timeout for channel %s:%d",
                       detector->alias, channel);
                return status;
            }
        }
    }

    return status;
}

PSL_STATIC int psl__StopHistogram(Module* module, Detector* detector, int channel)
{
    int status;

    FalconXNDetector* fDetector;

    boolean_t state_Is_ChannelReady = FALSE_;

    pslLog(PSL_LOG_DEBUG, "Stopping Histograms on channel %s:%d",
           detector->alias, channel);

    /*
     * Always send. Do not check the local state.
     */
    status = psl__StopDataAcquisition(module, channel, false);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to stop histogram transfer: %s:%d", module->alias, channel);
        return status;
    }

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        return status;
    }

    /*
     * Wait for the ready state.
     */

    fDetector = detector->pslData;

    if (fDetector->channelState == ChannelReady) {
        state_Is_ChannelReady = TRUE_;
    } else {
        fDetector->asyncReady = TRUE_;
    }

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS) {
        return status;
    }

    if (!state_Is_ChannelReady) {
        status = psl__DetectorWait(detector,
                                   FALCONXN_CHANNEL_STATE_TIMEOUT * 1000);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Stop run data error or timeout");
            return status;
        }
    }

    return status;
}

PSL_STATIC int psl__Stop_MappingMode_0(Module* module, Detector* detector)
{
    int status = XIA_SUCCESS;
    int cstatus = XIA_SUCCESS;

    int channel;

    UNUSED(detector);

    for (channel = 0; channel < (int) module->number_of_channels; channel++) {

        Detector* chanDetector;
        FalconXNDetector* fDetector;

        /*
         * Latch the first error we see and return that. Continue and
         * attempt to stop all channels.
         */
        if ((status == XIA_SUCCESS) && (cstatus != XIA_SUCCESS))
            status = cstatus;

        chanDetector = psl__FindDetector(module, channel);
        if (chanDetector == NULL) {
            cstatus = XIA_INVALID_DETCHAN;
            pslLog(PSL_LOG_ERROR, status,
                   "Cannot find channel %s:%d", module->alias, channel);
            continue;
        }

        fDetector = chanDetector->pslData;

        cstatus = psl__StopHistogram(module, chanDetector, channel);
    }

    if ((status == XIA_SUCCESS) && (cstatus != XIA_SUCCESS))
        status = cstatus;

    return status;
}

PSL_STATIC int psl__Start_MappingMode_0(unsigned short resume,
                                        Module*        module,
                                        Detector*      detector)
{
    int status = XIA_SUCCESS;
    int channel;

    UNUSED(resume);
    UNUSED(detector);

    /*
     * Start the run one channel at a time for all channels in the module for
     * Handel multi-channel device compatibility.
     *
     * Return on any error stopping all channels.
     */
    for (channel = 0; channel < (int) module->number_of_channels; channel++) {

        Detector*         chanDetector;
        FalconXNDetector* fDetector;
        uint32_t          number_stats;

        AcquisitionValue* number_mca_channels;

        chanDetector = psl__FindDetector(module, channel);
        if (chanDetector == NULL) {
            status = XIA_INVALID_DETCHAN;
            pslLog(PSL_LOG_ERROR, status,
                   "Cannot find channel %s:%d", module->alias, channel);
            psl__Stop_MappingMode_0(module, detector);
            return status;
        }

        /* Translate preset_type to SINC's histogram mode */
        status = psl__SyncPresetType(module, chanDetector);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error syncing the preset type for starting mm0: %s:%d",
                   module->alias, channel);
            psl__Stop_MappingMode_0(module, chanDetector);
            return status;
        }

        /*
         * Set the refresh to the configured value since mm1 could have
         * set it to a large value.
         */
        status = psl__SyncMCARefresh(module, chanDetector);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error syncing mca_refresh for starting mm0: %s:%d",
                   module->alias, channel);
            psl__Stop_MappingMode_0(module, chanDetector);
            return status;
        }

        fDetector = chanDetector->pslData;

        number_mca_channels = psl__GetAcquisition(fDetector,
                                                  "number_mca_channels");

        ASSERT(number_mca_channels);
        size_t sincstats_size = sizeof(SincHistogramCountStats);

        if ((sincstats_size % sizeof(uint32_t)) != 0) {
            status = XIA_INVALID_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "SINC stats size is not 32bit aligned: %s:%d",
                   module->alias, channel);
            psl__Stop_MappingMode_0(module, detector);
            return status;
        }

        number_stats = sizeof(SincHistogramCountStats) / sizeof(uint32_t);

        status = psl__DetectorLock(chanDetector);
        if (status != XIA_SUCCESS)
            return status;

        /*
         * Close the last mapping mode control.
         */
        status = psl__MappingModeControl_CloseAny(&fDetector->mmc);
        if (status != XIA_SUCCESS) {
            psl__DetectorUnlock(chanDetector);
            pslLog(PSL_LOG_ERROR, status,
                   "Error closing the last mapping mode control");
            psl__Stop_MappingMode_0(module, detector);
            return status;
        }

        status = psl__MappingModeControl_OpenMM0(&fDetector->mmc,
                                                 (uint16_t)number_mca_channels->value.ref.i,
                                                 number_stats);

        if (status != XIA_SUCCESS) {
            psl__DetectorUnlock(chanDetector);
            pslLog(PSL_LOG_ERROR, status,
                   "Error opening the mapping mode control");
            psl__Stop_MappingMode_0(module, detector);
            return status;
        }

        status = psl__DetectorUnlock(chanDetector);
        if (status != XIA_SUCCESS)
            return status;

        status = psl__StartHistogram(module, chanDetector, channel);
        if (status != XIA_SUCCESS) {
            psl__Stop_MappingMode_0(module, detector);
            return status;
        }
    }

    return XIA_SUCCESS;
}

/*
 * Mapping Mode 1: Full Spectrum Mapping.
 */
PSL_STATIC int psl__Stop_MappingMode_1(Module* module, Detector* detector)
{
    int status = XIA_SUCCESS;
    int cstatus = XIA_SUCCESS;

    int channel;

    UNUSED(detector);

    for (channel = 0; channel < (int) module->number_of_channels; channel++) {
        Detector* chanDetector;
        FalconXNDetector* fDetector;

        /*
         * Latch the first error we see and return that. Continue and
         * attempt to stop all channels.
         */
        if ((status == XIA_SUCCESS) && (cstatus != XIA_SUCCESS))
            status = cstatus;

        chanDetector = psl__FindDetector(module, channel);
        if (chanDetector == NULL) {
            cstatus = XIA_INVALID_DETCHAN;
            pslLog(PSL_LOG_ERROR, status,
                   "Cannot find channel %s:%d", module->alias, channel);
            continue;
        }

        fDetector = chanDetector->pslData;

        cstatus = psl__StopHistogram(module, chanDetector, channel);
    }

    if ((status == XIA_SUCCESS) && (cstatus != XIA_SUCCESS))
        status = cstatus;

    return status;
}

PSL_STATIC int psl__Start_MappingMode_1(unsigned short resume,
                                        Module* module, Detector* detector)
{
    int status = XIA_SUCCESS;
     int channel;

    UNUSED(resume);
    UNUSED(detector);

    /*
     * Start the run one channel at a time for all channels in the module for
     * Handel multi-channel device compatibility.
     *
     * Return on any error stopping all channels.
     */
    for (channel = 0; channel < (int) module->number_of_channels; channel++) {

        Detector*         chanDetector;
        FalconXNDetector* fDetector;

        AcquisitionValue* number_mca_channels;
        AcquisitionValue* num_map_pixels;
        AcquisitionValue* num_map_pixels_per_buffer;
        AcquisitionValue* pixel_advance_mode;

        chanDetector = psl__FindDetector(module, channel);
        if (chanDetector == NULL) {
            status = XIA_INVALID_DETCHAN;
            pslLog(PSL_LOG_ERROR, status,
                   "Cannot find channel %s:%d", module->alias, channel);
            psl__Stop_MappingMode_1(module, detector);
            return status;
        }

        /*
         * Translate pixel_advance_mode to the SINC histogram mode to
         * control data collection.
         */
        status = psl__SyncPixelAdvanceMode(module, chanDetector);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error syncing the pixel advance mode for starting mm1: %s:%d",
                   module->alias, channel);
            psl__Stop_MappingMode_1(module, chanDetector);
            return status;
        }

        fDetector = chanDetector->pslData;

        number_mca_channels = psl__GetAcquisition(fDetector,
                                                  "number_mca_channels");
        num_map_pixels = psl__GetAcquisition(fDetector,
                                             "num_map_pixels");
        num_map_pixels_per_buffer = psl__GetAcquisition(fDetector,
                                                        "num_map_pixels_per_buffer");
        pixel_advance_mode = psl__GetAcquisition(fDetector, "pixel_advance_mode");

        ASSERT(number_mca_channels);
        ASSERT(num_map_pixels);
        ASSERT(num_map_pixels_per_buffer);
        ASSERT(pixel_advance_mode);

        /*
         * Set a large histogram refresh period for GATE/SYNC advance
         * modes so we don't get periodic histogram responses. This
         * enables the receive loop to assume all histograms are for
         * GATE transitions.
         */
        if (pixel_advance_mode->value.ref.i == XIA_MAPPING_CTL_USER)
            status = psl__SyncMCARefresh(module, chanDetector);
        else
            status = psl__SetMCARefresh(module, chanDetector, 999);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error syncing mca_refresh for starting mm1: %s:%d",
                   module->alias, channel);
            psl__Stop_MappingMode_1(module, chanDetector);
            return status;
        }

        /*
         * Translate input_logic_polarity/gate_ignore to the SINC gate
         * collection mode.
         */
        if (pixel_advance_mode->value.ref.i == XIA_MAPPING_CTL_GATE) {
            status = psl__SyncGateCollectionMode(module, chanDetector);

            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Error syncing the gate collection mode for starting mm1: %s:%d",
                       module->alias, channel);
                psl__Stop_MappingMode_1(module, chanDetector);
                return status;
            }
        }

        status = psl__DetectorLock(chanDetector);
        if (status != XIA_SUCCESS)
            return status;

        /*
         * Close the last mapping mode control.
         */
        status = psl__MappingModeControl_CloseAny(&fDetector->mmc);
        if (status != XIA_SUCCESS) {
            psl__DetectorUnlock(chanDetector);
            pslLog(PSL_LOG_ERROR, status,
                   "Error closing the last mapping mode control");
            psl__Stop_MappingMode_0(module, detector);
            return status;
        }

        status = psl__MappingModeControl_OpenMM1(&fDetector->mmc,
                                                 fDetector->detChan,
                                                 FALSE_,
                                                 fDetector->runNumber,
                                                 num_map_pixels->value.ref.i,
                                                 (uint16_t)number_mca_channels->value.ref.i,
                                                 num_map_pixels_per_buffer->value.ref.i);

        if (status != XIA_SUCCESS) {
            psl__DetectorUnlock(chanDetector);
            pslLog(PSL_LOG_ERROR, status,
                   "Error opening the mapping mode control");
            psl__Stop_MappingMode_1(module, detector);
            return status;
        }

        /*
         * Flag to disable implicit pixel marker rewind for GATE or
         * SYNC advance, assuming we only get transitional spectra.
         */
        if (pixel_advance_mode->value.ref.i != XIA_MAPPING_CTL_USER) {
            MMC1_Data*  mm1;
            mm1 = psl__MappingModeControl_MM1Data(&fDetector->mmc);
            mm1->pixelAdvanceCounter = -1;
        }

        status = psl__DetectorUnlock(chanDetector);
        if (status != XIA_SUCCESS)
            return status;

        status = psl__StartHistogram(module, chanDetector, channel);
        if (status != XIA_SUCCESS) {
            psl__Stop_MappingMode_1(module, detector);
            return status;
        }
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__StartRun(int detChan, unsigned short resume, XiaDefaults *defs,
                             Detector *detector, Module *module)
{
    int status = XIA_SUCCESS;

    UNUSED(defs);

    FalconXNDetector* fDetector = detector->pslData;

    AcquisitionValue* mapping_mode;

    xiaPSLBadArgs(module, detector, "psl__StartRun");

    mapping_mode = psl__GetAcquisition(fDetector, "mapping_mode");

    ASSERT(mapping_mode);

    pslLog(PSL_LOG_DEBUG, "Detector:%d Mapping Mode:%d",
           detChan, (int) mapping_mode->value.ref.i);

    switch (mapping_mode->value.ref.i)
    {
        case 0:
            status = psl__Start_MappingMode_0(resume, module, detector);
            break;
        case 1:
            status = psl__Start_MappingMode_1(resume, module, detector);
            break;
        default:
            status = XIA_INVALID_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid mapping_mode: %d", (int) mapping_mode->value.ref.i);
            return status;
    }

    if (status == XIA_SUCCESS)
        ++fDetector->runNumber;

    return status;
}

PSL_STATIC int psl__StopRun(int detChan, Detector *detector, Module *module)
{
    int status = XIA_SUCCESS;

    AcquisitionValue* mapping_mode;

    FalconXNDetector* fDetector = detector->pslData;

    xiaPSLBadArgs(module, detector, "psl__StopRun");

    mapping_mode = psl__GetAcquisition(fDetector, "mapping_mode");

    ASSERT(mapping_mode);

    pslLog(PSL_LOG_DEBUG, "Detector:%d Mapping Mode:%d",
           detChan, (int) mapping_mode->value.ref.i);

    switch (mapping_mode->value.ref.i)
    {
        case 0:
            return psl__Stop_MappingMode_0(module, detector);
        case 1:
            return psl__Stop_MappingMode_1(module, detector);
        default:
            status = XIA_INVALID_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid mapping_mode: %d", (int) mapping_mode->value.ref.i);
            return status;
    }
}

/*
 * True if the detector's current or last mapping mode control matches
 * the given mode and the current state is running or ready. That
 * means it is valid to read the mapping data for that mode.
 */
PSL_STATIC bool psl__RunningOrReady(FalconXNDetector* fDetector, int mode)
{
    return (fDetector->channelState == ChannelHistogram ||
            fDetector->channelState == ChannelReady) &&
        psl__MappingModeControl_IsMode(&fDetector->mmc, mode);
}

PSL_STATIC bool psl__mm1_RunningOrReady(FalconXNDetector* fDetector)
{
    return psl__RunningOrReady(fDetector, MAPPING_MODE_MCA_FSM);
}

PSL_STATIC int psl__mm0_mca_length(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    FalconXNDetector* fDetector = detector->pslData;

    AcquisitionValue* number_mca_channels;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(value);
    UNUSED(module);

    number_mca_channels = psl__GetAcquisition(fDetector, "number_mca_channels");
    ASSERT(number_mca_channels);

    /* Must be in range because we validate parameters in the setters. */
    ASSERT(0 < number_mca_channels->value.ref.i &&
           number_mca_channels->value.ref.i < ULONG_MAX);

    *((unsigned long*) value) = (unsigned long) number_mca_channels->value.ref.i;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__mm0_mca(int detChan,
                            Detector* detector, Module* module,
                            const char *name, void *value)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    AcquisitionValue* mca_spectrum_accepted;
    AcquisitionValue* mca_spectrum_rejected;

    uint32_t* buffer = value;

    MMC0_Data* mm0;

    size_t size;

    SincHistogramCountStats stats;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(module);

    mca_spectrum_accepted = psl__GetAcquisition(fDetector, "mca_spectrum_accepted");
    mca_spectrum_rejected = psl__GetAcquisition(fDetector, "mca_spectrum_rejected");

    ASSERT(mca_spectrum_accepted);
    ASSERT(mca_spectrum_rejected);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    if (!psl__MappingModeControl_IsMode(&fDetector->mmc, MAPPING_MODE_MCA)) {
        psl__DetectorUnlock(detector);
        status = XIA_ILLEGAL_OPERATION;
        pslLog(PSL_LOG_ERROR, status,
               "Wrong mode for data request: %s", detector->alias);
        return status;
    }

    mm0 = psl__MappingModeControl_MM0Data(&fDetector->mmc);

    if (psl__MappingModeBuffers_Active_Level(&mm0->buffers) == 0) {
        psl__DetectorUnlock(detector);
        status = XIA_NO_SPECTRUM;
        pslLog(PSL_LOG_ERROR, status,
               "No spectrum yet: %s", detector->alias);
        return status;
    }

    psl__MappingModeBuffers_Active_Reset(&mm0->buffers);

    if (mca_spectrum_accepted->value.ref.b) {
        size = mm0->numMCAChannels;
        status = psl__MappingModeBuffers_CopyOut(&mm0->buffers,
                                                 buffer,
                                                 &size);
        if (status != XIA_SUCCESS)
            pslLog(PSL_LOG_ERROR, status,
                   "Copy out of active data: %s", detector->alias);
        if (size != mm0->numMCAChannels)
            pslLog(PSL_LOG_ERROR, status,
                   "Copy out of active data has bad length: %s",
                   detector->alias);
        buffer += mm0->numMCAChannels;
    }

    if (mca_spectrum_rejected->value.ref.b) {
        size = mm0->numMCAChannels;
        status = psl__MappingModeBuffers_CopyOut(&mm0->buffers,
                                                 buffer,
                                                 &size);
        if (status != XIA_SUCCESS)
            pslLog(PSL_LOG_ERROR, status,
                   "Copy out of rejected data: %s", detector->alias);
        if (size != mm0->numMCAChannels)
            pslLog(PSL_LOG_ERROR, status,
                   "Copy out of rejected data has bad length: %s",
                   detector->alias);
        buffer += mm0->numMCAChannels;
    }

    size = mm0->numStats;
    status = psl__MappingModeBuffers_CopyOut(&mm0->buffers,
                                             &stats,
                                             &size);
    if (status != XIA_SUCCESS)
        pslLog(PSL_LOG_ERROR, status,
               "Copy out of stats data: %s", detector->alias);
    else if (size != mm0->numStats)
        pslLog(PSL_LOG_ERROR, status,
               "Copy out of stats has bad length: %s",
               detector->alias);

    falconXNSetDetectorStats(fDetector, &stats);

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to unlock the detector: %s", detector->alias);
    }

    return status;
}

PSL_STATIC int psl__mm0_baseline_length(int detChan,
                                        Detector* detector, Module* module,
                                        const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(name);
    UNUSED(value);
    UNUSED(module);

    return XIA_UNIMPLEMENTED;
}

PSL_STATIC int psl__mm0_runtime(int detChan,
                                Detector* detector, Module* module,
                                const char *name, void *value)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(module);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    *((double*) value) = fDetector->stats[FALCONXN_STATS_TIME_ELAPSED];

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to unlock the detector: %s", detector->alias);
    }

    return status;
}

PSL_STATIC int psl__mm0_realtime(int detChan,
                                 Detector* detector, Module* module,
                                 const char *name, void *value)
{
    return psl__mm0_runtime(detChan, detector, module, name, value);
}

PSL_STATIC int psl__mm0_trigger_livetime(int detChan,
                                         Detector* detector, Module* module,
                                         const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(name);
    UNUSED(value);
    UNUSED(module);

    return XIA_UNIMPLEMENTED;
}

PSL_STATIC int psl__mm0_livetime(int detChan,
                                 Detector* detector, Module* module,
                                 const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(name);
    UNUSED(value);
    UNUSED(module);

    return XIA_UNIMPLEMENTED;
}

PSL_STATIC int psl__mm0_input_count_rate(int detChan,
                                         Detector* detector, Module* module,
                                         const char *name, void *value)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(module);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    *((double*) value) = fDetector->stats[FALCONXN_STATS_INPUT_COUNT_RATE];

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to unlock the detector: %s", detector->alias);
    }

    return status;
}

PSL_STATIC int psl__mm0_output_count_rate(int detChan,
                                          Detector* detector, Module* module,
                                          const char *name, void *value)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(module);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    *((double*) value) = fDetector->stats[FALCONXN_STATS_OUTPUT_COUNT_RATE];

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to unlock the detector: %s", detector->alias);
    }

    return status;
}

PSL_STATIC int psl__mm0_run_active(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    int status = XIA_SUCCESS;
    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(module);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    if ((fDetector->channelState == ChannelHistogram) &&
        psl__MappingModeControl_IsMode(&fDetector->mmc, MAPPING_MODE_MCA))
        *((unsigned long*) value) = 1;
    else
        *((unsigned long*) value) = 0;

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to unlock the detector: %s", detector->alias);
    }

    return status;
}

PSL_STATIC int psl__mm0_module_statistics_2(int detChan,
                                            Detector* detector, Module* module,
                                            const char *name, void *value)
{
    double* stats     = value;
    int     channel;
    int     status;

    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(name);

    /* Read out all stats for the module per the module_statistics_2 spec. */
    for (channel = 0; channel < (int) module->number_of_channels; channel++) {
        int       i, j;
        Detector* chanDetector;

        i = channel * XIA_NUM_MODULE_STATISTICS;
        for (j = 0; j < XIA_NUM_MODULE_STATISTICS; ++j)
            stats[i + j] = 0;

        chanDetector = psl__FindDetector(module, channel);
        if (chanDetector == NULL) {
            status = XIA_INVALID_DETCHAN;
            pslLog(PSL_LOG_ERROR, status,
                   "Cannot find channel detector %s:%d", module->alias, channel);
            return status;
        }

        FalconXNDetector* fDetector = chanDetector->pslData;

        status = psl__DetectorLock(chanDetector);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to lock the detector: %s", chanDetector->alias);
            return status;
        }

        stats[i + 0] = fDetector->stats[FALCONXN_STATS_TIME_ELAPSED];
        stats[i + 1] = fDetector->stats[FALCONXN_STATS_TRIGGER_LIVETIME];
        /* 2 - reserved */
        stats[i + 3] = fDetector->stats[FALCONXN_STATS_TRIGGERS];
        stats[i + 4] = fDetector->stats[FALCONXN_STATS_MCA_EVENTS];
        stats[i + 5] = fDetector->stats[FALCONXN_STATS_INPUT_COUNT_RATE];
        stats[i + 6] = fDetector->stats[FALCONXN_STATS_OUTPUT_COUNT_RATE];
        /* 7 - reserved */
        /* 8 - reserved */

        status = psl__DetectorUnlock(chanDetector);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to unlock the detector: %s", chanDetector->alias);
        }
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__mm0_module_mca(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(name);
    UNUSED(value);
    UNUSED(module);

    return XIA_UNIMPLEMENTED;
}

PSL_STATIC int psl__mm0_mca_events(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(name);
    UNUSED(value);
    UNUSED(module);

    return XIA_UNIMPLEMENTED;
}

PSL_STATIC int psl__mm0_total_output_events(int detChan,
                                            Detector* detector, Module* module,
                                            const char *name, void *value)
{
    int status = XIA_SUCCESS;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(module);

#if 0
    SiToro_Result siResult;

    SiToroDetector* fDetector = detector->pslData;

    uint64_t pulses = 0;

    siResult = siToro_detector_getHistogramPulsesDetected(fDetector->detector,
                                                          &pulses);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get histogram pulses detected");
        return status;
    }

    *((unsigned long*) value) = pulses;
#else
    UNUSED(detector);
    UNUSED(value);
#endif
    return status;
}

PSL_STATIC int psl__mm0_max_sca_length(int detChan,
                                       Detector* detector, Module* module,
                                       const char *name, void *value)
{
    int status;
    int intval = 0;

    UNUSED(detector);
    UNUSED(name);

    status = psl__GetMaxNumberSca(detChan, module, &intval);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status, "Unable to get the max number of SCA.");
        return status;
    }

    *((unsigned short *)value) = (unsigned short)intval;
    return XIA_SUCCESS;
}

PSL_STATIC int psl__GetMaxNumberSca(int detChan, Module* module, int *value)
{
    int status;

    psl__SincParamValue sincVal;

    status = psl__GetParamValue(module, detChan, "sca.numRegions",
               SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE, &sincVal);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the maximum number of SCA regions");
        return status;
    }

    *value = (int)sincVal.intval;
    return XIA_SUCCESS;
}

PSL_STATIC int psl__SetDigitalConf(int detChan, Detector* detector, Module* module)
{
    int status;

    SiToro__Sinc__KeyValue kv;

    pslLog(PSL_LOG_INFO, "Set digital I/O pins configuration for channel %d",
           detChan);

    si_toro__sinc__key_value__init(&kv);
    kv.key = (char*) "instrument.digital.config";
    kv.optionval = (char*) "8in-24out";

    status = psl__SetParam(module, detector, &kv);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status, "Unable to set instrument.digital.config");
            return status;
    }


    return XIA_SUCCESS;
}

/*
 * MCA mapping mca_length. The run data member is documented on the mm0 routine.
 */
PSL_STATIC int psl__mm1_mca_length(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    /* Same as mm0. */
    return psl__mm0_mca_length(detChan, detector, module, name, value);
}

/*
 * MCA mapping run_active. The run data member is documented on the mm0 routine.
 */
PSL_STATIC int psl__mm1_run_active(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    int status = XIA_SUCCESS;
    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(module);

    *((unsigned long*) value) = 0;

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    MMC1_Data*  mm1;
    MM_Buffers* mmb;

    mm1 = psl__MappingModeControl_MM1Data(&fDetector->mmc);
    mmb = &mm1->buffers;


    if ((fDetector->channelState == ChannelHistogram) &&
        psl__MappingModeControl_IsMode(&fDetector->mmc, MAPPING_MODE_MCA_FSM)) {

        /*
         * If we have received all the pixels we will need, that is the signal
         * to say the run is no longer active.
         */
        if (psl__MappingModeBuffers_PixelsReceived(mmb)) {
            pslLog(PSL_LOG_INFO,
                   "Pixel count reached: %s", detector->alias);
        }
        else {
            *((unsigned long*) value) = 1;
        }
    }

    pslLog(PSL_LOG_INFO, "Active state %d: %s",
           detChan, *((int*) value) ? "ACTIVE" : "ready");

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to unlock the detector: %s", detector->alias);
    }

    return status;
}

PSL_STATIC int psl__mm1_buffer_full_a(int detChan,
                                      Detector* detector, Module* module,
                                      const char *name, void *value)
{
    int status = XIA_SUCCESS;
    int sstatus;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    FalconXNDetector* fDetector = detector->pslData;

    *((int*) value) = 0;

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    if (psl__mm1_RunningOrReady(fDetector)) {
        MMC1_Data* mm1 = psl__MappingModeControl_MM1Data(&fDetector->mmc);
        if (psl__MappingModeBuffers_A_Full(&mm1->buffers))
            *((int*) value) = 1;
    } else {
        status = XIA_NOT_ACTIVE;
        pslLog(PSL_LOG_ERROR, status,
               "Not running or not MM1 mode: %s", detector->alias);
    }

    sstatus = psl__DetectorUnlock(detector);
    if (sstatus != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, sstatus,
               "Unable to unlock the detector: %s", detector->alias);
        if (status == XIA_SUCCESS)
            status = sstatus;
    }

    return status;
}

PSL_STATIC int psl__mm1_buffer_full_b(int detChan,
                                      Detector* detector, Module* module,
                                      const char *name, void *value)
{
    int status = XIA_SUCCESS;
    int sstatus;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    FalconXNDetector* fDetector = detector->pslData;

    *((int*) value) = 0;

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    if (psl__mm1_RunningOrReady(fDetector)) {
        MMC1_Data* mm1 = psl__MappingModeControl_MM1Data(&fDetector->mmc);
        if (psl__MappingModeBuffers_B_Full(&mm1->buffers))
            *((int*) value) = 1;
    } else {
        status = XIA_NOT_ACTIVE;
        pslLog(PSL_LOG_ERROR, status,
               "Not running or not MM1 mode: %s", detector->alias);
    }

    sstatus = psl__DetectorUnlock(detector);
    if (sstatus != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, sstatus,
               "Unable to unlock the detector: %s", detector->alias);
        if (status == XIA_SUCCESS)
            status = sstatus;
    }

    return status;
}

PSL_STATIC int psl__mm1_buffer_len(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    AcquisitionValue* number_mca_channels;
    AcquisitionValue* num_map_pixels_per_buffer;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    *((int*) value) = 0;

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    number_mca_channels = psl__GetAcquisition(fDetector,
                                              "number_mca_channels");
    num_map_pixels_per_buffer = psl__GetAcquisition(fDetector,
                                                    "num_map_pixels_per_buffer");

    ASSERT(number_mca_channels);
    ASSERT(num_map_pixels_per_buffer);

    *((unsigned long*) value)
        = (unsigned long) psl__MappingModeControl_MM1BufferSize(
                              (uint16_t)number_mca_channels->value.ref.i,
                              num_map_pixels_per_buffer->value.ref.i);

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to unlock the detector: %s", detector->alias);
    }

    return status;
}

PSL_STATIC int psl__mm1_buffer_done(int detChan,
                                    Detector* detector, Module* module,
                                    const char *name, void *value)
{
    int status = XIA_SUCCESS;
    int sstatus;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    if ((fDetector->channelState == ChannelHistogram) &&
        psl__MappingModeControl_IsMode(&fDetector->mmc, MAPPING_MODE_MCA_FSM)) {
        MMC1_Data*  mm1 = psl__MappingModeControl_MM1Data(&fDetector->mmc);
        MM_Buffers* mmb = &mm1->buffers;
        const char* selector = (const char*) value;
        char        buffer;
        char        active;
        boolean_t   swapped;

        if ((*selector == 'A') || (*selector == 'a'))
            buffer = 'A';
        else if ((*selector == 'B') || (*selector == 'b'))
            buffer = 'B';
        else
            buffer = '?';

        active = psl__MappingModeBuffers_Active_Label(mmb);

        if (buffer != active) {
            status = XIA_NOT_ACTIVE;
            pslLog(PSL_LOG_ERROR, status,
                   "Buffer %c is not active, cannot signal done on it: %s",
                   buffer, detector->alias);
        } else {
            psl__MappingModeBuffers_Active_SetDone(mmb);
        }

        /*
         * Update the buffers incase Next is full.
         */
        swapped = psl__MappingModeBuffers_Update(mmb);
        if (swapped) {
            pslLog(PSL_LOG_INFO,
                   "A/B buffers swapped: %s", detector->alias);
        }
    } else {
        status = XIA_NOT_ACTIVE;
        pslLog(PSL_LOG_ERROR, status,
               "Not running or not MM1 mode: %s", detector->alias);
    }

    sstatus = psl__DetectorUnlock(detector);
    if (sstatus != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, sstatus,
               "Unable to unlock the detector: %s", detector->alias);
        if (status == XIA_SUCCESS)
            status = sstatus;
    }

    return status;
}

PSL_STATIC int psl__mm1_buffer_a(int detChan,
                                 Detector* detector, Module* module,
                                 const char *name, void *value)
{
    int status = XIA_SUCCESS;
    int sstatus;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    if (psl__mm1_RunningOrReady(fDetector)) {
        MMC1_Data* mm1 = psl__MappingModeControl_MM1Data(&fDetector->mmc);
        MM_Buffers* mmb = &mm1->buffers;

        if (psl__MappingModeBuffers_A_Active(mmb)) {
            size_t size = 0;
            status = psl__MappingModeBuffers_CopyOut(mmb, value, &size);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Error coping buffer A data: %s", detector->alias);
            }
        } else {
            status = XIA_NOT_ACTIVE;
            pslLog(PSL_LOG_ERROR, status,
                   "Buffer A is not active, cannot get copy: %s",
                   detector->alias);
        }
    } else {
        status = XIA_NOT_ACTIVE;
        pslLog(PSL_LOG_ERROR, status,
               "Not running or not MM1 mode: %s", detector->alias);
    }

    sstatus = psl__DetectorUnlock(detector);
    if (sstatus != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, sstatus,
               "Unable to unlock the detector: %s", detector->alias);
        if (status == XIA_SUCCESS)
            status = sstatus;
    }

    return status;
}

PSL_STATIC int psl__mm1_buffer_b(int detChan,
                                 Detector* detector, Module* module,
                                 const char *name, void *value)
{
    int status = XIA_SUCCESS;
    int sstatus;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    if (psl__mm1_RunningOrReady(fDetector)) {
        MMC1_Data* mm1 = psl__MappingModeControl_MM1Data(&fDetector->mmc);
        MM_Buffers* mmb = &mm1->buffers;

        if (psl__MappingModeBuffers_B_Active(mmb)) {
            size_t size = 0;
            status = psl__MappingModeBuffers_CopyOut(mmb, value, &size);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Error coping buffer B data: %s", detector->alias);
            }
        } else {
            status = XIA_NOT_ACTIVE;
            pslLog(PSL_LOG_ERROR, status,
                   "Buffer B is not active, cannot get copy: %s",
                   detector->alias);
        }
    } else {
        status = XIA_NOT_ACTIVE;
        pslLog(PSL_LOG_ERROR, status,
               "Not running or not MM1 mode: %s", detector->alias);
    }

    sstatus = psl__DetectorUnlock(detector);
    if (sstatus != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, sstatus,
               "Unable to unlock the detector: %s", detector->alias);
        if (status == XIA_SUCCESS)
            status = sstatus;
    }

    return status;
}

PSL_STATIC int psl__mm1_current_pixel(int detChan,
                                      Detector* detector, Module* module,
                                      const char *name, void *value)
{
    int status = XIA_SUCCESS;
    int sstatus;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    if ((fDetector->channelState == ChannelHistogram) &&
        psl__MappingModeControl_IsMode(&fDetector->mmc, MAPPING_MODE_MCA_FSM)) {
        MMC1_Data* mm1 = psl__MappingModeControl_MM1Data(&fDetector->mmc);
        MM_Buffers* mmb = &mm1->buffers;
        *((unsigned long*) value) = (unsigned long) psl__MappingModeBuffers_Next_PixelTotal(mmb);
    } else {
        status = XIA_NOT_ACTIVE;
        pslLog(PSL_LOG_ERROR, status,
               "Not running or not MM1 mode: %s", detector->alias);
    }

    sstatus = psl__DetectorUnlock(detector);
    if (sstatus != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, sstatus,
               "Unable to unlock the detector: %s", detector->alias);
        if (status == XIA_SUCCESS)
            status = sstatus;
    }

    return status;
}

PSL_STATIC int psl__mm1_buffer_overrun(int detChan,
                                       Detector* detector, Module* module,
                                       const char *name, void *value)
{
    int status = XIA_SUCCESS;
    int sstatus;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    if ((fDetector->channelState == ChannelHistogram) &&
        psl__MappingModeControl_IsMode(&fDetector->mmc, MAPPING_MODE_MCA_FSM)) {
        MMC1_Data*  mm1 = psl__MappingModeControl_MM1Data(&fDetector->mmc);
        MM_Buffers* mmb = &mm1->buffers;
        uint32_t    overruns = psl__MappingModeBuffers_Overruns(mmb);

        if (overruns) {
            pslLog(PSL_LOG_INFO,
                   "Overrun count %d: %s", (int) overruns, detector->alias);
            *((int*) value) = 1;
        } else {
            *((int*) value) = 0;
        }
    } else {
        status = XIA_NOT_ACTIVE;
        pslLog(PSL_LOG_ERROR, status,
               "Not running or not MM1 mode: %s", detector->alias);
    }

    sstatus = psl__DetectorUnlock(detector);
    if (sstatus != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, sstatus,
               "Unable to unlock the detector: %s", detector->alias);
        if (status == XIA_SUCCESS)
            status = sstatus;
    }

    return status;
}

PSL_STATIC int psl__mm1_module_statistics_2(int detChan,
                                            Detector* detector, Module* module,
                                            const char *name, void *value)
{
    /* same as MM0 for now */
    return psl__mm0_module_statistics_2(detChan, detector, module, name, value);
}

PSL_STATIC int psl__mm1_mapping_pixel_next(int detChan,
                                           Detector* detector, Module* module,
                                           const char *name, void *value)
{
    int status = XIA_SUCCESS;
    int sstatus;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);
    UNUSED(value);

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    if ((fDetector->channelState == ChannelHistogram) &&
        psl__MappingModeControl_IsMode(&fDetector->mmc, MAPPING_MODE_MCA_FSM)) {
        MMC1_Data* mm1 = psl__MappingModeControl_MM1Data(&fDetector->mmc);
        /*
         * Always allowed via the board operation call.
         */
        ++mm1->pixelAdvanceCounter;
    } else {
        status = XIA_NOT_ACTIVE;
        pslLog(PSL_LOG_ERROR, status,
               "Not running or not MM1 mode: %s", detector->alias);
    }

    sstatus = psl__DetectorUnlock(detector);
    if (sstatus != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, sstatus,
               "Unable to unlock the detector: %s", detector->alias);
        if (status == XIA_SUCCESS)
            status = sstatus;
    }

    return status;
}

/*
 * Get run data handlers. The order of the handlers must match the
 * order of the labels.
 */
PSL_STATIC const char* getRunDataLabels[] =
{
    "mca_length",
    "mca",
    "baseline_length",
    "runtime",
    "realtime",
    "trigger_livetime",
    "livetime",
    "input_count_rate",
    "output_count_rate",
    "max_sca_length",
    "run_active",
    "buffer_len",
    "buffer_done",
    "buffer_full_a",
    "buffer_full_b",
    "buffer_a",
    "buffer_b",
    "current_pixel",
    "buffer_overrun",
    "module_statistics_2",
    "module_mca",
    "mca_events",
    "total_output_events",
    "list_buffer_len_a",
    "list_buffer_len_b",
    "mapping_pixel_next"
};

#define GET_RUN_DATA_HANDLER_COUNT (sizeof(getRunDataLabels) / sizeof(const char*))

PSL_STATIC DoBoardOperation_FP getRunDataHandlers[MAPPING_MODE_COUNT][GET_RUN_DATA_HANDLER_COUNT] =
{
    {
        psl__mm0_mca_length,
        psl__mm0_mca,
        psl__mm0_baseline_length,
        psl__mm0_runtime,
        psl__mm0_realtime,
        psl__mm0_trigger_livetime,
        psl__mm0_livetime,
        psl__mm0_input_count_rate,
        psl__mm0_output_count_rate,
        psl__mm0_max_sca_length,
        psl__mm0_run_active,
        NULL,   /* psl__mm0_buffer_len */
        NULL,   /* psl__mm0_buffer_done */
        NULL,   /* psl__mm0_buffer_full_a */
        NULL,   /* psl__mm0_buffer_full_b */
        NULL,   /* psl__mm0_buffer_a */
        NULL,   /* psl__mm0_buffer_b */
        NULL,   /* psl__mm0_current_pixel */
        NULL,   /* psl__mm0_buffer_overrun */
        psl__mm0_module_statistics_2,
        psl__mm0_module_mca,
        psl__mm0_mca_events,
        psl__mm0_total_output_events,
        NULL,   /* psl__mm0_list_buffer_len_a */
        NULL,   /* psl__mm0_list_buffer_len_b */
        NULL,   /* psl__mm0_mapping_pixel_next */
    },
    {
        psl__mm1_mca_length,
        NULL,   /* psl__mm1_mca */
        NULL,   /* psl__mm1_baseline_length */
        NULL,   /* psl__mm1_runtime */
        NULL,   /* psl__mm1_realtime */
        NULL,   /* psl__mm1_trigger_livetime */
        NULL,   /* psl__mm1_livetime */
        NULL,   /* psl__mm1_input_count_rate */
        NULL,   /* psl__mm1_output_count_rate */
        psl__mm0_max_sca_length, /* Defer to mm0 routine--this is generic. */
        psl__mm1_run_active,
        psl__mm1_buffer_len,
        psl__mm1_buffer_done,
        psl__mm1_buffer_full_a,
        psl__mm1_buffer_full_b,
        psl__mm1_buffer_a,
        psl__mm1_buffer_b,
        psl__mm1_current_pixel,
        psl__mm1_buffer_overrun,
        psl__mm1_module_statistics_2,
        NULL,   /* psl__mm1_module_mca */
        NULL,   /* psl__mm1_mca_events */
        NULL,   /* psl__mm1_total_output_events */
        NULL,   /* psl__mm1_list_buffer_len_a */
        NULL,   /* psl__mm1_list_buffer_len_b */
        psl__mm1_mapping_pixel_next,
    },
    {
        NULL,   /* psl__mm2_mca_length */
        NULL,   /* psl__mm2_mca */
        NULL,   /* psl__mm2_baseline_length */
        NULL,   /* psl__mm2_runtime */
        NULL,   /* psl__mm2_realtime */
        NULL,   /* psl__mm2_trigger_livetime */
        NULL,   /* psl__mm2_livetime */
        NULL,   /* psl__mm2_input_count_rate */
        NULL,   /* psl__mm2_output_count_rate */
        NULL,   /* psl__mm2_max_sca_length */
        NULL,   /* psl__mm2_run_active */
        NULL,   /* psl__mm2_buffer_len */
        NULL,   /* psl__mm2_buffer_done */
        NULL,   /* psl__mm2_buffer_full_a */
        NULL,   /* psl__mm2_buffer_full_b */
        NULL,   /* psl__mm2_buffer_a */
        NULL,   /* psl__mm2_buffer_b */
        NULL,   /* psl__mm2_current_pixel */
        NULL,   /* psl__mm2_buffer_overrun */
        NULL,   /* psl__mm2_module_statistics_2 */
        NULL,   /* psl__mm2_module_mca */
        NULL,   /* psl__mm2_mca_events */
        NULL,   /* psl__mm2_total_output_events */
        NULL,   /* psl__mm2_list_buffer_len_a */
        NULL,   /* psl__mm2_list_buffer_len_b */
        NULL,   /* psl__mm2_mapping_pixel_next */
    },
};

PSL_STATIC int psl__GetRunData(int detChan, const char *name, void *value,
                               XiaDefaults *defs, Detector *detector, Module *module)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(defs);

    AcquisitionValue* mapping_mode;

    int h;

    xiaPSLBadArgs(module, detector, "psl__GetRunData");

    mapping_mode = psl__GetAcquisition(fDetector, "mapping_mode");

    ASSERT(mapping_mode);

    pslLog(PSL_LOG_DEBUG, "Detector:%d Mapping Mode:%d Name:%s",
           detChan, (int) mapping_mode->value.ref.i, name);

    if (mapping_mode->value.ref.i >= MAPPING_MODE_COUNT) {
        status = XIA_INVALID_VALUE;
        pslLog(PSL_LOG_ERROR, status,
               "Invalid mapping_mode: %d", (int) mapping_mode->value.ref.i);
        return status;
    }

    for (h = 0; h < (int) GET_RUN_DATA_HANDLER_COUNT; ++h) {
        if (STREQ(name, getRunDataLabels[h])) {
            DoBoardOperation_FP handler;
            handler = getRunDataHandlers[mapping_mode->value.ref.i][h];
            if (handler) {
                return handler(detChan, detector, module,
                               name, value);
            }
            break;
        }
    }

    status = XIA_INVALID_VALUE;
    pslLog(PSL_LOG_ERROR, status,
           "Invalid mapping name: %s", name);
    return status;
}

PSL_STATIC int psl__CheckDetCharWaveform(const char* name, SincCalibrationPlot* wave)
{
    int status = XIA_SUCCESS;

    int i;

    for (i = 0; i < wave->len; ++i) {
        if (wave->x[i] != i) {
            status = XIA_FORMAT_ERROR;
            xiaLog(XIA_LOG_ERROR, status, "psl__CheckDetCharWaveform",
                   "%s X waveform data out of range: %d = %f", name, i, wave->x[i]);
            return status;
        }
    }

    for (i = 0; i < wave->len; ++i) {
        if ((wave->y[i] < -100) || (wave->y[i] > 100)) {
            status = XIA_FORMAT_ERROR;
            xiaLog(XIA_LOG_ERROR, status, "psl__CheckDetCharWaveform",
                   "%s Y waveform data out of range: %d = %f", name, i, wave->y[i]);
            return status;
        }
    }

    return status;
}

PSL_STATIC int psl__SpecialRun(int detChan, const char *name, void *info,
                               XiaDefaults *defaults, Detector *detector,
                               Module *module)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(defaults);

    xiaPSLBadArgs(module, detector, "psl__SpecialRun");

    pslLog(PSL_LOG_DEBUG,
           "%s (%d/%d): %s",
           module->alias, fDetector->modDetChan, detChan, name);

    if (STREQ(name, "adc_trace")) {
        double* value = info;
        if (*value <= 0) {
            pslLog(PSL_LOG_WARNING, "%f is out of range for adc_trace_length. Coercing to %d.",
                   *value, 0x2000);
            *value = 0x2000;
        }
        status = psl__SetADCTraceLength(module, detector, (int64_t)*value);
    }
    else if (STREQ(name, "detc-start")) {
        status = psl__DetCharacterizeStart(detChan, detector, module);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Start characterization special run failed");
            return status;
        }

        status = psl__DetectorLock(detector);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to lock the detector: %s", detector->alias);
            return status;
        }

        /*
         * Set the state explicitly here so give the right answer if the user
         * immediately requests special run data detc-running. Alternatively we
         * could wait here and signal when the channel state changes.
         */
        fDetector->channelState = ChannelCharacterizing;
        fDetector->calibPercentage = 0.0;
        falconXNClearDetectorCalibrationData(fDetector);
        strcpy(fDetector->calibStage, "Starting");

        status = psl__DetectorUnlock(detector);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to unlock the detector: %s", detector->alias);
        }
    }
    else if (STREQ(name, "detc-stop")) {
        /*
         * Cancel by using the generic stop API. skip=true means if it
         * is in the optimization phase we just skip and keep the
         * results gathered to that point. In other words,
         * characterization may succeed even though we are stopping
         * it.
         */
        status = psl__StopDataAcquisition(module, fDetector->modDetChan, true);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to stop detector characterization");
            return status;
        }
    }
    else {
        status = XIA_BAD_SPECIAL;
        pslLog(PSL_LOG_ERROR, status,
               "Invalid name: %s", name);
    }

    return status;
}

PSL_STATIC int psl__GetSpecialRunData(int detChan, const char *name, void *value,
                                      XiaDefaults *defaults,
                                      Detector *detector, Module *module)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    UNUSED(defaults);

    xiaPSLBadArgs(module, detector, "psl__GetSpecialRunData");

    pslLog(PSL_LOG_DEBUG,
           "Detector %s (%d): %s", detector->alias, detChan, name);

    if (STREQ(name, "adc_trace")) {
        status = psl__GetADCTrace(module, detector, value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get ADC trace data");
        }
        return status;
    }
    else if (STREQ(name, "adc_trace_length")) {
        int64_t length;
        status = psl__GetADCTraceLength(module, detector, &length);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get ADC trace length");
        }
        *((unsigned long *)value) = (unsigned long)length;
        return status;
    }

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS)
        return status;

    if (STREQ(name, "detc-progress-text-size")) {
            pslLog(PSL_LOG_INFO,
                   "Progress text size: %zu", sizeof(fDetector->calibStage));
            *((int*) value) = sizeof(fDetector->calibStage);
    }
    else if (STREQ(name, "detc-running")) {
        pslLog(PSL_LOG_INFO,
               "Running: %s",
               fDetector->channelState == ChannelCharacterizing ? "yes" : "no");
        *((int*) value) = fDetector->channelState == ChannelCharacterizing ? 1 : 0;
    }
    else if (STREQ(name, "detc-successful")) {
        /*
         * This is never returned in the protocol so just assume it is ok
         * until it is fixed.
         *
         * HACK: additionally, getting detc-successful is the only place we refresh
         * the calibration data after the process completes, so this has to be called
         * before retrieving the pulses. case 12292
         */
        int *success = (int*) value;

        pslLog(PSL_LOG_INFO,
               "Successful: %s",
               fDetector->channelState == ChannelCharacterizing ? "no" : "yes");
        *success = fDetector->channelState == ChannelCharacterizing ? 0 : 1;

        status = psl__DetectorUnlock(detector);

        if (status == XIA_SUCCESS) {
            if (*success) {
                status = psl__GetCalibration(module, detector);
                if (status != XIA_SUCCESS) {
                    pslLog(PSL_LOG_ERROR, status,
                           "Cannot get calibration data");
                }
            }
        }

        return status;
    }
    else if (STREQ(name, "detc-percentage")) {
        pslLog(PSL_LOG_INFO,
               "Percentage: %3.0f", fDetector->calibPercentage);
        ASSERT(fDetector->calibPercentage < INT_MAX);
        *((int*) value) = (int) fDetector->calibPercentage;
    }
    else if (STREQ(name, "detc-progress-text")) {
        pslLog(PSL_LOG_INFO,
               "Stage: %s", fDetector->calibStage);
        strncpy(value, fDetector->calibStage, sizeof(fDetector->calibStage) - 1);
    }
    else if (STREQ(name, "detc-string-size")) {
        *((int*) value) = fDetector->calibData.len;
    }
    else if (STREQ(name, "detc-string")) {
        if (fDetector->calibData.len)
            memcpy(value, fDetector->calibData.data,
                   (size_t) fDetector->calibData.len);
    }
    else if (STREQ(name, "detc-example-pulse-size")) {
        *((int*) value) = fDetector->calibExample.len;
    }
    else if (STREQ(name, "detc-example-pulse-x")) {
        if (fDetector->calibExample.len)
            memcpy(value, fDetector->calibExample.x,
                   sizeof(double) * (size_t) fDetector->calibExample.len);
    }
    else if (STREQ(name, "detc-example-pulse-y")) {
        if (fDetector->calibExample.len)
            memcpy(value, fDetector->calibExample.y,
                   sizeof(double) * (size_t) fDetector->calibExample.len);
    }
    else if (STREQ(name, "detc-model-pulse-size")) {
        *((int*) value) = fDetector->calibModel.len;
    }
    else if (STREQ(name, "detc-model-pulse-x")) {
        if (fDetector->calibModel.len)
            memcpy(value, fDetector->calibModel.x,
                   sizeof(double) * (size_t) fDetector->calibModel.len);
    }
    else if (STREQ(name, "detc-model-pulse-y")) {
        if (fDetector->calibModel.len)
            memcpy(value, fDetector->calibModel.y,
                   sizeof(double) * (size_t) fDetector->calibModel.len);
    }
    else if (STREQ(name, "detc-final-pulse-size")) {
        *((int*) value) = fDetector->calibFinal.len;
    }
    else if (STREQ(name, "detc-final-pulse-x")) {
        if (fDetector->calibFinal.len)
            memcpy(value, fDetector->calibFinal.x,
                   sizeof(double) * (size_t) fDetector->calibFinal.len);
    }
    else if (STREQ(name, "detc-final-pulse-y")) {
        if (fDetector->calibFinal.len)
            memcpy(value, fDetector->calibFinal.y,
                   sizeof(double) * (size_t) fDetector->calibFinal.len);
    }
    else {
        status = XIA_BAD_NAME;
        pslLog(PSL_LOG_ERROR, status,
               "Invalid name: %s", name);
    }

    status = psl__DetectorUnlock(detector);

    return status;
}

PSL_STATIC int psl__IniWrite(FILE* fp, const char* section, const char* path,
                             void *value, const int index, Module *module)
{
    UNUSED(fp);
    UNUSED(path);

    if (STREQ(section, "detector")) {
        int status;

        char item[MAXITEM_LEN];
        char firmware[MAXITEM_LEN];
        char filename[MAXITEM_LEN];

        Detector* detector = value;

        /*
         * Check a firmware set is present for this channel. It must exist
         * before running a detector characterization.
         */
        sprintf(item, "firmware_set_chan%d", index);

        status = xiaGetModuleItem(module->alias, item, firmware);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error getting the firmware from the module: %s", item);
            return status;
        }

        status = xiaGetFirmwareItem(firmware, 0, "filename", filename);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error getting the filename from the firmware set alias: %s",
                   firmware);
            return status;
        }

        /*
         * Do not abort the INI write because it can become corrupted.
         */
        psl__UnloadDetCharacterization(module, detector, filename);
    }

    return XIA_SUCCESS;
}

 PSL_STATIC int psl__ModuleTransactionSend(Module* module, SincBuffer* packet)
{
    int status;

    FalconXNModule* fModule = module->pslData;

    pslLog(PSL_LOG_INFO, "SINC Send");

    status = handel_md_mutex_lock(&fModule->sendLock);
    if (status != 0) {
        int me = status;
        status = XIA_THREAD_ERROR;
        pslLog(PSL_LOG_ERROR, status,
               "Module send mutex lock failed: %d", me);
        return status;
    }

    /*
     * Send will clear the packet buffer. No need to clear.
     */
    status = SincSend(&fModule->sinc, packet);
    if (status == false) {
        status = falconXNSincResultToHandel(SincWriteErrorCode(&fModule->sinc),
                                            SincWriteErrorMessage(&fModule->sinc));
        handel_md_mutex_unlock(&fModule->sendLock);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to send to FalconXN connection: %s:%d",
               fModule->hostAddress, fModule->portBase);
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__ModuleTransactionReceive(Module* module, Sinc_Response* response)
{
    int sstatus = XIA_SUCCESS;

    boolean_t waiting = TRUE_;

    response->response = NULL;

    while (waiting) {
        int status;

        FalconXNModule* fModule = module->pslData;

        boolean_t matching = TRUE_;

        /*
         * The sender waits here for the response.
         */
        status = handel_md_event_wait(&fModule->sendEvent, 0);
        if (status != 0) {
            int me = status;
            status = XIA_THREAD_ERROR;
            handel_md_mutex_unlock(&fModule->sendLock);
            pslLog(PSL_LOG_ERROR, status,
                   "Module send event wait failed: %d", me);
            return status;
        }

        status = psl__ModuleLock(module);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Module lock failed");
            return status;
        }

        sstatus = fModule->sendStatus;
        if (sstatus != XIA_SUCCESS) {
            psl__FlushResponse(response);
            waiting = FALSE_;
        } else {
            if ((response->channel > 0) &&
                (fModule->response.channel > 0) &&
                (response->channel != fModule->response.channel)) {
                matching = FALSE_;
            }

            if (matching &&
                (response->type > 0) &&
                (response->type != fModule->response.type)) {
                matching = FALSE_;
            }

            if (matching) {
                *response = fModule->response;
                psl__FlushResponse(&fModule->response);
                waiting = FALSE_;
            } else {
                pslLog(PSL_LOG_ERROR, XIA_PROTOCOL_ERROR,
                       "Invalid response: { %d %d %p }",
                       fModule->response.channel,
                       fModule->response.type,
                       fModule->response.response);
                psl__FreeResponse(&fModule->response);
            }
        }

        status = psl__ModuleUnlock(module);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Module unlock failed");
            return status;
        }
    }

    return sstatus;
}

PSL_STATIC int psl__ModuleTransactionEnd(Module* module)
{
    int status;

    FalconXNModule* fModule = module->pslData;

    status = handel_md_mutex_unlock(&fModule->sendLock);
    if (status != 0) {
        int me = status;
        status = XIA_THREAD_ERROR;
        pslLog(PSL_LOG_ERROR, status,
               "Module send mutex unlock failed: %d", me);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Get the Detector struct for a SINC channel within a module. The
 * SINC channel number is the same as the module channel.
 */
PSL_STATIC Detector* psl__FindDetector(Module* module, int channel)
{
    ASSERT(channel < (int)module->number_of_channels);

    if (channel >= (int)module->number_of_channels)
        return NULL;

    return xiaFindDetector(module->detector[channel]);
}

PSL_STATIC int psl__ModuleResponse(Module* module, int channel, int type, void* resp)
{
    int status;

    FalconXNModule* fModule = module->pslData;

    Sinc_Response sresp = { channel, type, resp };

    status = psl__ModuleLock(module);
    if (status != 0) {
        psl__FreeResponse(&sresp);
        return status;
    }

    pslLog(PSL_LOG_INFO,
           "SET channel=%d type=%d response=%p", channel, type, resp);

    if (fModule->response.response != NULL) {
        pslLog(PSL_LOG_INFO,
               "Module response not empty: { %d %d %p }",
               fModule->response.channel,
               fModule->response.type,
               fModule->response.response);
        psl__FreeResponse(&fModule->response);
    }

    fModule->response = sresp;
    fModule->sendStatus = status;

    pslLog(PSL_LOG_INFO,
           "Set response: { %d, %d, %p }",
           fModule->response.channel,
           fModule->response.type,
           fModule->response.response);

    status = psl__ModuleUnlock(module);
    if (status != 0) {
        psl__FreeResponse(&sresp);
        return status;
    }

    status = handel_md_event_signal(&fModule->sendEvent);
    if (status != 0) {
        pslLog(PSL_LOG_ERROR, status,
               "Cannot signal requestor: %d", channel);
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__ModuleStatusResponse(Module* module, int mstatus)
{
    int status;

    FalconXNModule* fModule = module->pslData;

    status = psl__ModuleLock(module);
    if (status != 0) {
        return status;
    }

    fModule->sendStatus = mstatus;

    status = psl__ModuleUnlock(module);
    if (status != 0) {
        return status;
    }

    status = handel_md_event_signal(&fModule->sendEvent);
    if (status != 0) {
        pslLog(PSL_LOG_ERROR, status,
               "Cannot signal requestor");
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__ReceiveHistogram_MM0(Detector*                detector,
                                         MM_Control*              mmc,
                                         SincHistogram*           accepted,
                                         SincHistogram*           rejected,
                                         SincHistogramCountStats* stats)
{
    int status = XIA_SUCCESS;
    int sstatus;

     MMC0_Data* mm0;

     mm0 = psl__MappingModeControl_MM0Data(mmc);

     psl__MappingModeBuffers_Next_Clear(&mm0->buffers);

     if (accepted->len) {
         if (mm0->numMCAChannels != (uint32_t) accepted->len) {
             pslLog(PSL_LOG_ERROR, XIA_INVALID_VALUE,
                    "Invalid accepted length (mca_channels=%d,accepted=%d): %s",
                    mm0->numMCAChannels, accepted->len, detector->alias);
         } else {
             status = psl__MappingModeBuffers_CopyIn(&mm0->buffers,
                                                     accepted->data,
                                                     (size_t) accepted->len);
             if (status != XIA_SUCCESS) {
                 pslLog(PSL_LOG_ERROR, status,
                        "Error coping in accepted data: %s", detector->alias);
             }
         }
     }

     if (rejected->len) {
         if (mm0->numMCAChannels != (uint32_t) rejected->len) {
             pslLog(PSL_LOG_ERROR, XIA_INVALID_VALUE,
                    "Invalid rejected length (mca_channels=%d,rejected=%d): %s",
                    mm0->numMCAChannels, rejected->len, detector->alias);
         } else {
             sstatus = psl__MappingModeBuffers_CopyIn(&mm0->buffers,
                                                      rejected->data,
                                                      (size_t) rejected->len);
             if (sstatus != XIA_SUCCESS) {
                 pslLog(PSL_LOG_ERROR, sstatus,
                        "Error coping in rejected data: %s", detector->alias);
             }
             if ((status == XIA_SUCCESS) && (sstatus != XIA_SUCCESS))
                 status = sstatus;
         }
     }

     /*
      * Copy in the FalconXN stats we received. It is better to buffer 32bit
      * values than doubles. We buffer with the histogram to make sure the
      * stats and the histogram match.
      */
     sstatus = psl__MappingModeBuffers_CopyIn(&mm0->buffers,
                                              stats,
                                              mm0->numStats);
     if (sstatus != XIA_SUCCESS) {
         pslLog(PSL_LOG_ERROR, sstatus,
                "Error coping in stats: %s", detector->alias);
     }

     if ((status == XIA_SUCCESS) && (sstatus != XIA_SUCCESS))
         status = sstatus;


     psl__MappingModeBuffers_Toggle(&mm0->buffers);

     return status;
}

PSL_STATIC int psl__ReceiveHistogram_MM1(Module*                  module,
                                         Detector*                detector,
                                         int                      channel,
                                         MM_Control*              mmc,
                                         SincHistogram*           accepted,
                                         SincHistogramCountStats* stats)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector;

    MMC1_Data*  mm1;
    MM_Buffers* mmb;

    boolean_t swapped;

    MM_Pixel_Stats pstats;

    UNUSED(module);
    UNUSED(channel);

    if (accepted->len == 0) {
        status = XIA_INVALID_VALUE;
        pslLog(PSL_LOG_ERROR, status,
               "Accepted length is 0: %s", detector->alias);
        return status;
    }

    fDetector = detector->pslData;

    falconXNSetDetectorStats(fDetector, stats);

    mm1 = psl__MappingModeControl_MM1Data(mmc);
    mmb = &mm1->buffers;

    /*
     * We need to channels we expect to match the number received or the buffer
     * sizing does not match and we could corrupt memory.
     */
    if (mm1->numMCAChannels != (uint32_t) accepted->len) {
        status = XIA_INVALID_VALUE;
        pslLog(PSL_LOG_ERROR, status,
               "Accepted length is does not match MCA channels: %s",
               detector->alias);
        return status;
    }

    /*
     * See if we have received all the pixels we will need.
     */
    if (psl__MappingModeBuffers_PixelsReceived(mmb)) {
        pslLog(PSL_LOG_INFO,
               "Pixel count reached: %s", detector->alias);
      return status;
    }

    /*
     * Update before we write the pixel in case the user has just finished
     * reading the Active buffer and so it is flagged as done. It might have
     * not been done when the last pixel was written in.
     */
    swapped = psl__MappingModeBuffers_Update(mmb);
    if (swapped) {
        pslLog(PSL_LOG_INFO,
               "A/B buffers swapped: %s", detector->alias);
    }

    /*
     * Are the buffers full? Increment the overflow counter. This is used to
     * signal the user if they call the buffer_overrun call.
     */
    if (psl__MappingModeBuffers_Next_Full(mmb)) {
        psl__MappingModeBuffers_Overrun(mmb);
        status = XIA_INTERNAL_BUFFER_OVERRUN;
        pslLog(PSL_LOG_ERROR, status,
               "Overfow, next buffer is full: %s", detector->alias);
        return status;
    }

    pslLog(PSL_LOG_DEBUG,
           "Next:%c pixels=%d bufferPixel=%d level=%d size=%d: %s",
           psl__MappingModeBuffers_Next_Label(mmb),
           (int) psl__MappingModeBuffers_Next_PixelTotal(mmb),
           (int) psl__MappingModeBuffers_Next_Pixels(mmb),
           (int) psl__MappingModeBuffers_Next_Level(mmb),
           (int) psl__MappingModeBuffers_Size(mmb),
           detector->alias);

    /*
     * If the Next's level is 0 the buffer does not have an XMAP header. Add
     * it. We always write a pixel into a new buffer.
     */
    if (psl__MappingModeBuffers_Next_Level(mmb) == 0) {
        status = psl__XMAP_WriteBufferHeader_MM1(mm1);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error adding an XMAP buffer header: %s", detector->alias);
            return status;
        }
    } else {
        /*
         * If the user has indicated a next pixel rewind the pixel and
         * overwrite it with this histogram. In GATE or SYNC advance
         * mode, this should be -1 to disable rewind; we assume all
         * received histograms correspond to transitions indicating a
         * new pixel.
         */
        if (mm1->pixelAdvanceCounter == 0) {
            psl__MappingModeBuffers_Pixel_Dec(&mm1->buffers);
            psl__MappingModeBuffers_Pixel_RewindToMarker(&mm1->buffers);
        }
    }

    if (mm1->pixelAdvanceCounter > 0)
        --mm1->pixelAdvanceCounter;

    /*
     * Scale times from seconds to standard format ticks.
     */
    pstats.realtime = (uint32_t) (fDetector->stats[FALCONXN_STATS_TIME_ELAPSED] /
                                  XMAP_MAPPING_TICKS);
    pstats.livetime = (uint32_t) (fDetector->stats[FALCONXN_STATS_TRIGGER_LIVETIME] /
                                  XMAP_MAPPING_TICKS);

    pstats.triggers =
        (uint32_t) fDetector->stats[FALCONXN_STATS_TRIGGERS];
    pstats.output_events =
        (uint32_t) fDetector->stats[FALCONXN_STATS_PULSES_ACCEPTED];

    /*
     * Set the buffer marker to the current level, add the XMAP pixel header,
     * increment the pixel counters, then copy in the histogram.
     */
    psl__MappingModeBuffers_Pixel_SetMarker(&mm1->buffers);

    status = psl__XMAP_WritePixelHeader_MM1(mm1, &pstats);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error adding an XMAP pixel header: %s", detector->alias);
        return status;
    }

    psl__MappingModeBuffers_Pixel_Inc(&mm1->buffers);

    status = psl__MappingModeBuffers_CopyIn(mmb,
                                            accepted->data,
                                            (size_t) accepted->len);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error copying in accepted data: %s", detector->alias);
    }

    status = psl__XMAP_UpdateBufferHeader_MM1(mm1);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error updating buffer header: %s", detector->alias);
    }

    /*
     * Update again so the data any data is waiting for the user to read from
     * the Active buffer.
     */
    swapped = psl__MappingModeBuffers_Update(mmb);
    if (swapped) {
        pslLog(PSL_LOG_INFO,
               "A/B buffers swapped: %s", detector->alias);
    }

    /*
     * See if we have received all the pixels we will need. If so we
     * will not process any more histograms and the next run_active
     * check will return false. It is up to the user to stop the run
     * per Handel convention.
     */
    if (psl__MappingModeBuffers_PixelsReceived(mmb)) {
        pslLog(PSL_LOG_INFO,
               "Pixel count reached: %s", detector->alias);
    }

    return status;
}

PSL_STATIC int psl__ReceiveHistogramData(Module* module, SincBuffer* packet)
{
    int status;

    FalconXNModule* fModule = module->pslData;

    Detector* detector;
    FalconXNDetector* fDetector;

    int channel = -1;

    SincHistogram accepted;
    SincHistogram rejected;
    SincHistogramCountStats stats;

    SincError se;

    MM_Control* mmc;

    memset(&accepted, 0, sizeof(accepted));
    memset(&rejected, 0, sizeof(rejected));
    memset(&stats, 0, sizeof(stats));

    status = SincDecodeHistogramDataResponse(&se,
                                             packet,
                                             &channel,
                                             &accepted,
                                             &rejected,
                                             &stats);
    if (status != true) {
        status = falconXNSincErrorToHandel(&se);
        pslLog(PSL_LOG_ERROR, status,
               "Decode from FalconXN connection failed: %s:%d",
               fModule->hostAddress, fModule->portBase);
        return status;
    }

    detector = psl__FindDetector(module, channel);
    if (detector == NULL) {
        free(accepted.data);
        free(rejected.data);
        status = XIA_INVALID_DETCHAN;
        pslLog(PSL_LOG_ERROR, status,
               "Cannot find channel detector: %d", channel);
        return status;
    }

    pslLog(PSL_LOG_DEBUG,
           "Histo Id:%" PRIu64 " elapsed=%0.3f accepted=%" PRIu64 " icr=%0.3f ocr=%0.3f deadtime=%0.3f gate=%d: %s",
           stats.dataSetId,
           stats.timeElapsed,
           stats.pulsesAccepted,
           stats.inputCountRate,
           stats.outputCountRate,
           stats.deadTime,
           stats.gateState,
           detector->alias);

    /* If a previous process was aborted during a run, sometimes we get an extra
     * histogram data response on startup.
     */
    fDetector = detector->pslData;
    if (fDetector == NULL) {
        free(accepted.data);
        free(rejected.data);
        status = XIA_INVALID_DETCHAN;
        pslLog(PSL_LOG_ERROR, status,
               "Detector data uninitialized for histogram data channel: %d", channel);
        return status;
    }

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        free(accepted.data);
        free(rejected.data);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to lock the detector: %s", detector->alias);
        return status;
    }

    mmc = &fDetector->mmc;

    switch (psl__MappingModeControl_Mode(mmc))
    {
        case MAPPING_MODE_NIL:
            break;

        case MAPPING_MODE_MCA:
            status = psl__ReceiveHistogram_MM0(detector,
                                               mmc,
                                               &accepted,
                                               &rejected,
                                               &stats);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Error in MM0 histogram receiver: %s", detector->alias);
            }
            break;

        case MAPPING_MODE_MCA_FSM:
            status = psl__ReceiveHistogram_MM1(module,
                                               detector,
                                               channel,
                                               mmc,
                                               &accepted,
                                               &stats);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Error in MM1 histogram receiver: %s", detector->alias);
            }
            break;

        case MAPPING_MODE_SCA:
        case MAPPINGMODE_LIST:
        case MAPPING_MODE_COUNT:
        default:
            pslLog(PSL_LOG_ERROR, XIA_INVALID_VALUE,
                   "Invalid mapping mode (%d): %s",
                   psl__MappingModeControl_Mode(mmc), detector->alias);
            break;
    }

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to unlock the detector: %s", detector->alias);
        /* drop through to free the memory */
    }

    free(accepted.data);
    free(rejected.data);

    return XIA_SUCCESS;
}

PSL_STATIC int psl__ReceiveListModeData(Module* module, SincBuffer* packet)
{
    UNUSED(module);
    UNUSED(packet);

    pslLog(PSL_LOG_INFO, "No decoder");
    return XIA_SUCCESS;
}

PSL_STATIC int psl__ReceiveOscilloscopeData(Module* module, SincBuffer* packet)
{
    int status;

    FalconXNModule* fModule = module->pslData;

    Detector* detector;
    FalconXNDetector* fDetector;

    int channel = -1;

    SincOscPlot raw;

    SincError se;

    status = SincDecodeOscilloscopeDataResponse(&se,
                                                packet,
                                                &channel,
                                                NULL,
                                                NULL,
                                                &raw);
    if (status != true) {
        status = falconXNSincErrorToHandel(&se);
        pslLog(PSL_LOG_ERROR, status,
               "Decode from FalconXN connection failed: %s:%d",
               fModule->hostAddress, fModule->portBase);
        return status;
    }

    detector = psl__FindDetector(module, channel);
    if (detector == NULL) {
        free(raw.data);
        free(raw.intData);
        status = XIA_INVALID_DETCHAN;
        pslLog(PSL_LOG_ERROR, status,
               "Cannot find channel detector: %d", channel);
        return status;
    }

    fDetector = detector->pslData;

    /* TODO: Handle this properly or return an error. This is happening with
     * multi-channel systems during startup. When clearing the state of one
     * channel (psl__StopDataAcquisition), we see responses for channels we
     * haven't set up yet. It is unclear if this is something we need to
     * handle or a bug in the protocol. case 12291
     */
    if (fDetector == NULL) {
        pslLog(PSL_LOG_WARNING,
               "Received scope data for unintialized detector %d", channel);
        return XIA_SUCCESS;
    }

    status = psl__DetectorLock(detector);

    fDetector->adcTrace = raw;

    fDetector->asyncStatus = XIA_SUCCESS;

    psl__DetectorUnlock(detector);

    return status;
}

PSL_STATIC int psl__ReceiveCalibrationProgress(Module* module, SincBuffer* packet)
{
    int status;

    int channel = -1;

    Detector* detector;
    FalconXNDetector* fDetector;

    SiToro__Sinc__CalibrationProgressResponse* resp;

    SincError se;

    status = SincDecodeCalibrationProgressResponse(&se,
                                                   packet,
                                                   &resp,
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   &channel);
    if (status != true) {
        if (resp != NULL)
            si_toro__sinc__calibration_progress_response__free_unpacked(resp, NULL);
        status = falconXNSincErrorToHandel(&se);
        pslLog(PSL_LOG_ERROR, status,
               "Decode calibration progress response failed %s:%d",
               module->alias, channel);
        return status;
    }

    detector = psl__FindDetector(module, channel);
    if (detector == NULL) {
        si_toro__sinc__calibration_progress_response__free_unpacked(resp, NULL);
        status = XIA_INVALID_DETCHAN;
        pslLog(PSL_LOG_ERROR, status,
               "Cannot find channel detector: %d", channel);
        return status;
    }

    status = psl__DetectorLock(detector);
    if (status != 0) {
        si_toro__sinc__calibration_progress_response__free_unpacked(resp, NULL);
        return status;
    }

    fDetector = detector->pslData;

    if (resp->has_progress && (resp->progress != 0.0)) {
        fDetector->calibPercentage = resp->progress;
    }
    if (resp->stage != NULL) {
        strncpy(fDetector->calibStage,
                resp->stage,
                sizeof(fDetector->calibStage) - 1);
    }
    if (resp->has_complete && resp->complete) {
        pslLog(PSL_LOG_INFO, "Characterization completed [%d]", channel);
    }

    si_toro__sinc__calibration_progress_response__free_unpacked(resp, NULL);

    status = psl__DetectorUnlock(detector);

    return status;
}

PSL_STATIC int psl__ReceiveAsynchronousError(Module* module, SincBuffer* packet)
{

    int status;

    int channel = -1;

    SincError se;

    UNUSED(module);
    UNUSED(packet);

    /*
     * We don't need to pass a resp because it's only a success response,
     * which gets unpacked into se.
     */
    status = SincDecodeAsynchronousErrorResponse(&se,
                                                 packet,
                                                 NULL,
                                                 &channel);
    if (status != true) {
        pslLog(PSL_LOG_ERROR, status, "SINC asynchronous error, channel = %d", channel);
        status = falconXNSincErrorToHandel(&se);
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__ReceiveSuccess(Module* module, SincBuffer* packet)
{
    int status;

    FalconXNModule* fModule = module->pslData;

    int channel = -1;

    SiToro__Sinc__SuccessResponse* resp;

    SincError se;

    status = SincDecodeSuccessResponse(&se,
                                       packet,
                                       &resp,
                                       &channel);
    if (status != true) {
        status = falconXNSincErrorToHandel(&se);
        psl__ModuleStatusResponse(module, status);
        pslLog(PSL_LOG_ERROR, status,
               "Decode from FalconXN connection failed: %s:%d",
               fModule->hostAddress, fModule->portBase);
        return status;
    }

    return psl__ModuleResponse(module, channel,
                               SI_TORO__SINC__MESSAGE_TYPE__SUCCESS_RESPONSE,
                               resp);
}

PSL_STATIC int psl__ReceiveGetParam(Module* module, SincBuffer* packet)
{
    int status;

    FalconXNModule* fModule = module->pslData;

    int channel = -1;
    SiToro__Sinc__GetParamResponse *resp;

    SincError se;

    status = SincDecodeGetParamResponse(&se,
                                        packet,
                                        &resp,
                                        &channel);
    if (status != true) {
        status = falconXNSincErrorToHandel(&se);
        psl__ModuleStatusResponse(module, status);
        pslLog(PSL_LOG_ERROR, status,
               "Decode from FalconXN connection failed: %s:%d",
               fModule->hostAddress, fModule->portBase);
        return status;
    }

    if (resp->n_results != 1) {
        status = XIA_INVALID_VALUE;
        psl__ModuleStatusResponse(module, status);
        pslLog(PSL_LOG_ERROR, status,
               "Too many results from FalconXN connection: %s:%d",
               fModule->hostAddress, fModule->portBase);
        return status;
    }

    return psl__ModuleResponse(module, channel,
                               SI_TORO__SINC__MESSAGE_TYPE__GET_PARAM_RESPONSE,
                               resp);
}

PSL_STATIC int psl__ReceiveGetCalibration(Module* module, SincBuffer* packet)
{
    int status;

    Detector* detector;
    FalconXNDetector* fDetector;

    int channel = -1;

    SincCalibrationData data;
    SincCalibrationPlot example;
    SincCalibrationPlot model;
    SincCalibrationPlot final;

    SincError se;

    status = SincDecodeGetCalibrationResponse(&se,
                                              packet,
                                              NULL,
                                              &channel,
                                              &data,
                                              &example,
                                              &model,
                                              &final);
    if (status != true) {
        status = falconXNSincErrorToHandel(&se);
        psl__ModuleStatusResponse(module, status);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get calibration data for channel %d", channel);
        return status;
    }

    pslLog(PSL_LOG_DEBUG,
           "Got det-char response for channel %d", channel);

    detector = psl__FindDetector(module, channel);
    if (detector == NULL) {
        free(data.data);
        falconXNClearCalibrationData(&example);
        falconXNClearCalibrationData(&model);
        falconXNClearCalibrationData(&final);
        status = XIA_INVALID_DETCHAN;
        psl__ModuleStatusResponse(module, status);
        pslLog(PSL_LOG_ERROR, status,
               "Cannot find channel detector: %d", channel);
        return status;
    }

    fDetector = detector->pslData;

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        free(data.data);
        falconXNClearCalibrationData(&example);
        falconXNClearCalibrationData(&model);
        falconXNClearCalibrationData(&final);
        psl__ModuleStatusResponse(module, status);
        return status;
    }

    fDetector->calibData = data;
    fDetector->calibExample = example;
    fDetector->calibModel = model;
    fDetector->calibFinal = final;

    status = psl__DetectorUnlock(detector);
    if (status != XIA_SUCCESS) {
        psl__ModuleStatusResponse(module, status);
        return status;
    }

    return psl__ModuleResponse(module, channel,
                               SI_TORO__SINC__MESSAGE_TYPE__GET_CALIBRATION_RESPONSE,
                               NULL);
}

PSL_STATIC int psl__ReceiveCalculateDCOffset(Module* module, SincBuffer* packet)
{
    int status;

    FalconXNModule* fModule = module->pslData;

    int channel = -1;

    SiToro__Sinc__CalculateDcOffsetResponse *resp;

    SincError se;

    status = SincDecodeCalculateDCOffsetResponse(&se,
                                                 packet,
                                                 &resp,
                                                 NULL,
                                                 &channel);
    if (status != true) {
        status = falconXNSincErrorToHandel(&se);
        psl__ModuleStatusResponse(module, status);
        pslLog(PSL_LOG_ERROR, status,
               "Decode from FalconXN connection failed: %s:%d",
               fModule->hostAddress, fModule->portBase);
        return status;
    }

    return psl__ModuleResponse(module, channel,
                               SI_TORO__SINC__MESSAGE_TYPE__CALCULATE_DC_OFFSET_RESPONSE,
                               resp);
}

PSL_STATIC int psl__ReceiveListParamDetails(Module* module, SincBuffer* packet)
{
    UNUSED(module);
    UNUSED(packet);

    pslLog(PSL_LOG_INFO, "No decoder");
    return XIA_SUCCESS;
}

PSL_STATIC int psl__ReceiveParamUpdated(Module* module, SincBuffer* packet)
{
    int status;

    FalconXNModule* fModule = module->pslData;

    int channel = -1;

    Detector* detector;
    FalconXNDetector* fDetector;

    SiToro__Sinc__ParamUpdatedResponse* resp;
    size_t param;

    SincError se;

    status = SincDecodeParamUpdatedResponse(&se,
                                            packet,
                                            &resp,
                                            &channel);
    if (status != true) {
        status = falconXNSincErrorToHandel(&se);
        psl__ModuleStatusResponse(module, status);
        pslLog(PSL_LOG_ERROR, status,
               "Decode from FalconXN connection failed: %s:%d",
               fModule->hostAddress, fModule->portBase);
        return status;
    }

    detector = psl__FindDetector(module, channel);
    if (detector == NULL) {
        si_toro__sinc__param_updated_response__free_unpacked(resp, NULL);
        status = XIA_INVALID_DETCHAN;
        psl__ModuleStatusResponse(module, status);
        pslLog(PSL_LOG_ERROR, status,
               "Cannot find channel detector: %d", channel);
        return status;
    }

    fDetector = detector->pslData;

    if (fDetector == NULL) {
        return XIA_SUCCESS;
    }

    status = psl__DetectorLock(detector);
    if (status != XIA_SUCCESS) {
        si_toro__sinc__param_updated_response__free_unpacked(resp, NULL);
        psl__ModuleStatusResponse(module, status);
        return status;
    }

    for (param = 0; param < resp->n_params; ++param) {
        SiToro__Sinc__KeyValue* kv;
        char logValue[MAX_PARAM_STR_LEN];

        kv = resp->params[param];

        psl__SPrintKV(logValue, sizeof(logValue) / sizeof(logValue[0]), kv);

        pslLog(PSL_LOG_DEBUG,
               "Channel %d param: %d/%d: %s (%c%c%c%c%c%c%c) = %s",
               channel, (int) param, (int) resp->n_params, kv->key,
               kv->has_channelid ? 'c' : '-',
               kv->has_intval ? 'i' : '-',
               kv->has_floatval ? 'f' : '-',
               kv->has_boolval ? 'b' : '-',
               kv->has_paramtype ? 'p' : '-',
               kv->strval != NULL ? 's' : '-',
               kv->optionval != NULL ? 'o' : '-',
               logValue);

        /*
         * We ignore some lock/unlock results to keep processing all params.
         */
        if (strcmp(kv->key, "channel.state") == 0) {
            status = psl__UpdateChannelState(kv, detector);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status, "Processing channel.state");
            }
        }
    }

    status = psl__DetectorUnlock(detector);

    si_toro__sinc__param_updated_response__free_unpacked(resp, NULL);

    return status;
}

/* Parses a channel.state value from Sinc and updates the detector channel state. */
PSL_STATIC int psl__UpdateChannelState(SiToro__Sinc__KeyValue* kv, Detector* detector)
{
    int status;

    FalconXNDetector* fDetector = detector->pslData;

    if (kv->optionval) {
        if (strcmp(kv->optionval, "ready") == 0) {
            fDetector->channelState = ChannelReady;

            if (fDetector->asyncReady) {
                fDetector->asyncReady = FALSE_;
                status = psl__DetectorSignal(detector);
                if (status != XIA_SUCCESS) {
                    pslLog(PSL_LOG_ERROR, status,
                           "Detector event signal error");
                }

                return status;
            }
        }
        else if (strcmp(kv->optionval, "error") == 0) {
            fDetector->channelState = ChannelError;
            pslLog(PSL_LOG_WARNING, "Detector %s is in the error state",
                   detector->alias);
        }
        else if (strcmp(kv->optionval, "osc") == 0) {
            fDetector->channelState = ChannelADC;
        }
        else if (strcmp(kv->optionval, "histo") == 0) {
            fDetector->channelState = ChannelHistogram;

            if (fDetector->asyncReady) {
                fDetector->asyncReady = FALSE_;
                status = psl__DetectorSignal(detector);
                if (status != XIA_SUCCESS) {
                    pslLog(PSL_LOG_ERROR, status,
                           "Detector event signal error");
                }

                return status;
            }
        }
        else if (strcmp(kv->optionval, "listMode") == 0) {
            fDetector->channelState = ChannelListMode;
        }
        else if (strcmp(kv->optionval, "calibrate") == 0) {
            /* special run should set it first for now */
            //ASSERT(fDetector->channelState == ChannelCharacterizing);
            fDetector->channelState = ChannelCharacterizing;
        }
        else if (strcmp(kv->optionval, "dcOffset") == 0) {
            pslLog(PSL_LOG_WARNING, "TODO: handle channel.state = dcOffset");
        }
        else {
            pslLog(PSL_LOG_WARNING, "Unexpected channel.state: %s", kv->optionval);
        }
    }
    else {
        status = XIA_BAD_VALUE;
        pslLog(PSL_LOG_ERROR, status, "Channel.state value bad: %s, %s",
               kv->has_paramtype ? "has-paramtype" : "no-paramtype",
               kv->optionval);

    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__ReceiveSoftwareUpdateComplete(Module* module, SincBuffer* packet)
{
    /* Software update support. Not used in Handel. */
    UNUSED(module);
    UNUSED(packet);

    return XIA_SUCCESS;
}

PSL_STATIC int psl__ReceiveCheckParamConsistency(Module* module, SincBuffer* packet)
{
    int status;

    FalconXNModule* fModule = module->pslData;

    int channel = -1;

    SiToro__Sinc__CheckParamConsistencyResponse *resp;

    SincError se;

    status = SincDecodeCheckParamConsistencyResponse(&se,
                                                     packet,
                                                     &resp,
                                                     &channel);
    if (status != true) {
        status = falconXNSincErrorToHandel(&se);
        psl__ModuleStatusResponse(module, status);
        pslLog(PSL_LOG_ERROR, status,
               "Decode from FalconXN connection failed: %s:%d",
               fModule->hostAddress, fModule->portBase);
        return status;
    }

    return psl__ModuleResponse(module, channel,
                               SI_TORO__SINC__MESSAGE_TYPE__CHECK_PARAM_CONSISTENCY_RESPONSE,
                               resp);
}

PSL_STATIC int psl__ModuleReceiveProcessor(Module*                   module,
                                           SiToro__Sinc__MessageType msgType,
                                           SincBuffer*               packet)
{
    FalconXNModule* fModule = module->pslData;
    int             status = XIA_SUCCESS;

    pslLog(PSL_LOG_DEBUG,
           "SINC Receive: %d", msgType);

    switch (msgType) {
        /*
         * Async responses.
         */
        case SI_TORO__SINC__MESSAGE_TYPE__HISTOGRAM_DATA_RESPONSE:
            status = psl__ReceiveHistogramData(module, packet);
            break;

        case SI_TORO__SINC__MESSAGE_TYPE__LIST_MODE_DATA_RESPONSE:
            status = psl__ReceiveListModeData(module, packet);
            break;

        case SI_TORO__SINC__MESSAGE_TYPE__OSCILLOSCOPE_DATA_RESPONSE:
            status = psl__ReceiveOscilloscopeData(module, packet);
            break;

        case SI_TORO__SINC__MESSAGE_TYPE__CALIBRATION_PROGRESS_RESPONSE:
            status = psl__ReceiveCalibrationProgress(module, packet);
            break;

        /*
         * Internal errors.
         */
        case SI_TORO__SINC__MESSAGE_TYPE__ASYNCHRONOUS_ERROR_RESPONSE:
            status = psl__ReceiveAsynchronousError(module, packet);
            break;

        /*
         * Command responses.
         */
        case SI_TORO__SINC__MESSAGE_TYPE__SUCCESS_RESPONSE:
            status = psl__ReceiveSuccess(module, packet);
            break;

        case SI_TORO__SINC__MESSAGE_TYPE__GET_PARAM_RESPONSE:
            status = psl__ReceiveGetParam(module, packet);
            break;

        case SI_TORO__SINC__MESSAGE_TYPE__GET_CALIBRATION_RESPONSE:
            status = psl__ReceiveGetCalibration(module, packet);
            break;

        case SI_TORO__SINC__MESSAGE_TYPE__CALCULATE_DC_OFFSET_RESPONSE:
            status = psl__ReceiveCalculateDCOffset(module, packet);
            break;

        case SI_TORO__SINC__MESSAGE_TYPE__LIST_PARAM_DETAILS_RESPONSE:
            status = psl__ReceiveListParamDetails(module, packet);
            break;

        case SI_TORO__SINC__MESSAGE_TYPE__PARAM_UPDATED_RESPONSE:
            status = psl__ReceiveParamUpdated(module, packet);
            break;

        case SI_TORO__SINC__MESSAGE_TYPE__SOFTWARE_UPDATE_COMPLETE_RESPONSE:
            status = psl__ReceiveSoftwareUpdateComplete(module, packet);
            break;

        case SI_TORO__SINC__MESSAGE_TYPE__CHECK_PARAM_CONSISTENCY_RESPONSE:
            status = psl__ReceiveCheckParamConsistency(module, packet);
            break;

        case SI_TORO__SINC__MESSAGE_TYPE__NO_MESSAGE_TYPE:
        case SI_TORO__SINC__MESSAGE_TYPE__PING_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__GET_PARAM_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__SET_PARAM_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__START_HISTOGRAM_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__START_LIST_MODE_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__START_OSCILLOSCOPE_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__STOP_DATA_ACQUISITION_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__RESET_SPATIAL_SYSTEM_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__START_CALIBRATION_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__GET_CALIBRATION_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__SET_CALIBRATION_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__CALCULATE_DC_OFFSET_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__CLEAR_HISTOGRAM_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__LIST_PARAM_DETAILS_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__START_FFT_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__RESTART_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__SOFTWARE_UPDATE_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__SAVE_CONFIGURATION_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__MONITOR_CHANNELS_COMMAND:
        case _SI_TORO__SINC__MESSAGE_TYPE_IS_INT_SIZE:
        case SI_TORO__SINC__MESSAGE_TYPE__PROBE_DATAGRAM_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__PROBE_DATAGRAM_RESPONSE:
        case SI_TORO__SINC__MESSAGE_TYPE__HISTOGRAM_DATAGRAM_RESPONSE:
        case SI_TORO__SINC__MESSAGE_TYPE__DOWNLOAD_CRASH_DUMP_COMMAND:
        case SI_TORO__SINC__MESSAGE_TYPE__DOWNLOAD_CRASH_DUMP_RESPONSE:
        default:
            pslLog(PSL_LOG_INFO,
                   "Invalid message type for FalconXN connection: %s:%d: %d",
                   fModule->hostAddress, fModule->portBase, msgType);
            break;
    }

    return status;
}

/* Suppress while(0) warnings in protobuf code transitively included through sinc. */
#ifdef _WIN32
#define PSL_SINC_BUFFER_CLEAR(x) __pragma( warning(push) )    \
    __pragma( warning(disable:4127) ) \
    SINC_BUFFER_CLEAR(x) \
    __pragma( warning(pop) )
#else
#define PSL_SINC_BUFFER_CLEAR(x) SINC_BUFFER_CLEAR(x)
#endif

PSL_STATIC void psl__ModuleReceiver(void* arg)
{
    Module*         module = (Module*) arg;
    FalconXNModule* fModule = module->pslData;
    int             r = 0;

    pslLog(PSL_LOG_DEBUG,
           "Receiver thread starting: %s", module->alias);

    r = handel_md_mutex_lock(&fModule->lock);
    if (r == 0)
    {
        fModule->receiverRunning = TRUE_;

        while (fModule->receiverActive)
        {
            SiToro__Sinc__MessageType msgType;
            int                       status;
            uint8_t                   receiveBufferData[4096];
            SincBuffer                sb = SINC_BUFFER_INIT(receiveBufferData);
            boolean_t                 timeout = FALSE_;

            /*
             * The receive message in the Sinc API is thread safe in respect to
             * the send path so we can unlock the module mutex. We hold the
             * mutex while decoding the received data.
             */
            r = handel_md_mutex_unlock(&fModule->lock);
            if (r != 0)
                break;

            status = SincReadMessage(&fModule->sinc,
                                     100,
                                     &sb,
                                     &msgType);

            r = handel_md_mutex_lock(&fModule->lock);
            if (r != 0)
                break;

            if (status != true) {
                int sincErrCode = SincReadErrorCode(&fModule->sinc);
                if (sincErrCode != SI_TORO__SINC__ERROR_CODE__TIMEOUT) {
                    status = falconXNSincResultToHandel(sincErrCode,
                                                        SincReadErrorMessage(&fModule->sinc));
                    pslLog(PSL_LOG_ERROR, status,
                           "Read message failed for FalconXN connection: %s:%d",
                           fModule->hostAddress, fModule->portBase);
                    continue;
                }
                timeout = TRUE_;
            }

            if (!timeout) {
                status = psl__ModuleReceiveProcessor(module,
                                                     msgType,
                                                     &sb);

                /* We have to clear SINC buffers after reading. They
                 * do it for sends, so this is likely the only
                 * place we need it.
                 */
                PSL_SINC_BUFFER_CLEAR(&sb);

                if (status != XIA_SUCCESS) {
                    continue;
                }
            }
        }

        fModule->receiverRunning = FALSE_;
    }

    pslLog(PSL_LOG_DEBUG,
           "Receiver thread stopping: %s: %d", module->alias, r);

    handel_md_mutex_unlock(&fModule->lock);
}

PSL_STATIC int psl__ModuleReceiverStop(const char* alias, FalconXNModule* fModule)
{
    int status;

    int period;

    handel_md_mutex_lock(&fModule->lock);
    fModule->receiverActive = FALSE_;
    handel_md_mutex_unlock(&fModule->lock);

    status = handel_md_event_signal(&fModule->receiverEvent);
    if (status != 0) {
        pslLog(PSL_LOG_ERROR, XIA_THREAD_ERROR,
               "Receiver thread signal failed for %s: %d",
               alias, status);
    }

    period = 2000;

    handel_md_mutex_lock(&fModule->lock);

    while (period > 0) {
        if (!fModule->receiverRunning)
            break;
        handel_md_mutex_unlock(&fModule->lock);
        handel_md_thread_sleep(50);
        period -= 50;
        handel_md_mutex_lock(&fModule->lock);
    }

    handel_md_mutex_unlock(&fModule->lock);

    if (period <= 0) {
        pslLog(PSL_LOG_ERROR, XIA_THREAD_ERROR,
               "Receiver thread stop failed for %s", alias);
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__SetupModule(Module *module)
{
    int status;

    FalconXNModule* fModule = NULL;
    char            item[MAXITEM_LEN];
    int             value;
    int             period;

    pslLog(PSL_LOG_DEBUG, "Module %s", module->alias);

    ASSERT(module);
    ASSERT(module->pslData == NULL);

    fModule = handel_md_alloc(sizeof(FalconXNModule));
    if (fModule == NULL) {
        pslLog(PSL_LOG_ERROR, XIA_NOMEM,
               "No memory for SiToro module PSL data");
        return XIA_NOMEM;
    }

    memset(fModule, 0, sizeof(*fModule));

    /*
     * The module level set up need to change once we move to a single
     * connection for the module the detectors share. This will allow us to
     * determine the number of detectors.
     */
    status = xiaGetModuleItem(module->alias, "inet_address", item);
    if (status != XIA_SUCCESS) {
        handel_md_free(fModule);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting the INET address from the module:");
        return status;
    }

    strcpy(fModule->hostAddress, item);

    status = xiaGetModuleItem(module->alias, "inet_port", &value);
    if (status != XIA_SUCCESS) {
        handel_md_free(fModule);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting the INET port from the module:");
        return status;
    }

    fModule->portBase = value;

    status = xiaGetModuleItem(module->alias, "inet_timeout", &value);
    if (status != XIA_SUCCESS) {
        handel_md_free(fModule);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting the INET timeout from the module:");
        return status;
    }

    fModule->timeout = value;

    SincInit(&fModule->sinc);
    SincSetTimeout(&fModule->sinc, fModule->timeout);

    status = SincConnect(&fModule->sinc,
                         fModule->hostAddress,
                         fModule->portBase);
    if (status == false) {
        status = falconXNSincResultToHandel(SincCurrentErrorCode(&fModule->sinc),
                                            SincCurrentErrorMessage(&fModule->sinc));
        pslLog(PSL_LOG_ERROR, status,
               "Unable to open the FalconXN connection: %s:%d",
               fModule->hostAddress, fModule->portBase);
        handel_md_free(fModule);
        return status;
    }

    status = SincPing(&fModule->sinc, 0);
    if (!status) {
        status = falconXNSincResultToHandel(SincCurrentErrorCode(&fModule->sinc),
                                            SincCurrentErrorMessage(&fModule->sinc));
        SincDisconnect(&fModule->sinc);
        pslLog(PSL_LOG_ERROR, status,
               "Detector ping failed: %s:%d",
               fModule->hostAddress, fModule->portBase);
        handel_md_free(fModule);
        return status;
    }

    module->pslData = fModule;

    status = handel_md_mutex_create(&fModule->lock);
    if (status != 0) {
        int me = status;
        status = XIA_THREAD_ERROR;
        SincDisconnect(&fModule->sinc);
        handel_md_free(fModule);
        module->pslData = NULL;
        pslLog(PSL_LOG_ERROR, status,
               "Module mutex create failed for %s: %d",
               module->alias, me);
        return status;
    }

    status = handel_md_event_create(&fModule->receiverEvent);
    if (status != 0) {
        int me = status;
        status = XIA_THREAD_ERROR;
        SincDisconnect(&fModule->sinc);
        handel_md_mutex_destroy(&fModule->lock);
        handel_md_free(fModule);
        module->pslData = NULL;
        pslLog(PSL_LOG_ERROR, status,
               "Module event create failed for %s: %d",
               module->alias, me);
        return status;
    }

    fModule->receiver.name = "Module.receiver";
    fModule->receiver.priority = 10;
    fModule->receiver.stackSize = 128 * 1024;
    fModule->receiver.attributes = 0;
    fModule->receiver.realtime = FALSE_;
    fModule->receiver.entryPoint = psl__ModuleReceiver;
    fModule->receiver.argument = module;

    fModule->receiverActive = TRUE_;

    status = handel_md_thread_create(&fModule->receiver);
    if (status != 0) {
        int te = status;
        status = XIA_THREAD_ERROR;
        SincDisconnect(&fModule->sinc);
        handel_md_event_destroy(&fModule->receiverEvent);
        handel_md_mutex_destroy(&fModule->lock);
        handel_md_free(fModule);
        module->pslData = NULL;
        pslLog(PSL_LOG_ERROR, status,
               "Receive thread create failed for %s: %d",
               module->alias, te);
        return status;
    }

    /*
     * Wait 2 seconds for the thread to say it is running.
     */
    period = 2000;

    handel_md_mutex_lock(&fModule->lock);

    while (period > 0) {
        if (fModule->receiverRunning)
            break;
        handel_md_mutex_unlock(&fModule->lock);
        handel_md_thread_sleep(50);
        period -= 50;
        handel_md_mutex_lock(&fModule->lock);
    }

    handel_md_mutex_unlock(&fModule->lock);

    if (period <= 0) {
        status = XIA_THREAD_ERROR;
        handel_md_thread_destroy(&fModule->receiver);
        handel_md_event_destroy(&fModule->receiverEvent);
        handel_md_mutex_destroy(&fModule->lock);
        SincDisconnect(&fModule->sinc);
        handel_md_free(fModule);
        module->pslData = NULL;
        pslLog(PSL_LOG_ERROR, status,
               "Receive thread start failed for %s", module->alias);
        return status;
    }

    status = handel_md_mutex_create(&fModule->sendLock);
    if (status != 0) {
        psl__ModuleReceiverStop(module->alias, fModule);
        status = XIA_THREAD_ERROR;
        handel_md_thread_destroy(&fModule->receiver);
        handel_md_event_destroy(&fModule->receiverEvent);
        handel_md_mutex_destroy(&fModule->lock);
        SincDisconnect(&fModule->sinc);
        handel_md_free(fModule);
        module->pslData = NULL;
        pslLog(PSL_LOG_ERROR, status,
               "Module send lock create failed for %s", module->alias);
        return status;
    }

    status = handel_md_event_create(&fModule->sendEvent);
    if (status != 0) {
        handel_md_mutex_destroy(&fModule->sendLock);
        psl__ModuleReceiverStop(module->alias, fModule);
        status = XIA_THREAD_ERROR;
        handel_md_thread_destroy(&fModule->receiver);
        handel_md_event_destroy(&fModule->receiverEvent);
        handel_md_mutex_destroy(&fModule->lock);
        SincDisconnect(&fModule->sinc);
        handel_md_free(fModule);
        module->pslData = NULL;
        pslLog(PSL_LOG_ERROR, status,
               "Receive thread start failed for %s", module->alias);
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__EndModule(Module *module)
{
    if (module && module->pslData) {
        FalconXNModule* fModule = fModule = module->pslData;

        pslLog(PSL_LOG_DEBUG, "Module %s", module->alias);

        psl__ModuleReceiverStop(module->alias, fModule);

        if (fModule->sinc.connected) {
            pslLog(PSL_LOG_DEBUG, "Disconnecting %s:%d",
                   fModule->hostAddress, fModule->portBase);
            SincDisconnect(&fModule->sinc);
            SincCleanup(&fModule->sinc);
        }

        handel_md_event_destroy(&fModule->sendEvent);
        handel_md_mutex_destroy(&fModule->sendLock);
        handel_md_thread_destroy(&fModule->receiver);
        handel_md_event_destroy(&fModule->receiverEvent);
        handel_md_mutex_destroy(&fModule->lock);

        handel_md_free(module->pslData);
        module->pslData = NULL;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__SetupDetChan(int detChan, Detector *detector, Module *module)
{
    int status;

    FalconXNModule* fModule = NULL;
    FalconXNDetector* fDetector = NULL;

    int modDetChan;

    int av;

    modDetChan = xiaGetModChan(detChan);

    if (modDetChan == 999) {
        pslLog(PSL_LOG_ERROR, XIA_INVALID_DETCHAN,
               "Unable to get the FalconXN module channel for detector channel: %d",
               detChan);
        return XIA_INVALID_DETCHAN;
    }

    pslLog(PSL_LOG_DEBUG,
           "Set up %s (%d/%d)", module->alias, modDetChan, detChan);

    if (!module) {
        xiaLog(XIA_LOG_ERROR, XIA_BAD_PSL_ARGS, "psl__SetupDetChan",
               "Module is NULL");
        return XIA_BAD_PSL_ARGS;
    }

    if (!module->pslData) {
        xiaLog(XIA_LOG_ERROR, XIA_BAD_PSL_ARGS, "psl__SetupDetChan",
               "Module PSL data is NULL");
        return XIA_BAD_PSL_ARGS;
    }

    if (!detector) {
        xiaLog(XIA_LOG_ERROR, XIA_BAD_PSL_ARGS, "psl__SetupDetChan",
               "Detector is NULL");
        return XIA_BAD_PSL_ARGS;
    }

    ASSERT(detector->pslData == NULL);

    fModule = module->pslData;

    fDetector = handel_md_alloc(sizeof(FalconXNDetector));
    if (fDetector == NULL) {
        pslLog(PSL_LOG_ERROR, XIA_NOMEM,
               "No memory for SiToro detector PSL data");
        return XIA_NOMEM;
    }

    /*
     * <sigh> Need to use memset to clear this struct.
     */
    memset(fDetector, 0, sizeof(*fDetector));

    fDetector->modDetChan = modDetChan;
    fDetector->mmc.mode = MAPPING_MODE_NIL;
    fModule->channelActive[fDetector->modDetChan] = TRUE_;

    status = handel_md_mutex_create(&fDetector->lock);
    if (status != 0) {
        int me = status;
        status = XIA_THREAD_ERROR;
        handel_md_free(fDetector);
        pslLog(PSL_LOG_ERROR, status,
               "Detector mutex create failed for %s: %d",
               detector->alias, me);
        return status;
    }

    status = handel_md_event_create(&fDetector->asyncEvent);
    if (status != 0) {
        int me = status;
        status = XIA_THREAD_ERROR;
        handel_md_mutex_destroy(&fDetector->lock);
        handel_md_free(fDetector);
        pslLog(PSL_LOG_ERROR, status,
               "Detector mutex create failed for %s: %d",
               detector->alias, me);
        return status;
    }

    detector->pslData = fDetector;

    /*
     * Set up the detector's local copy.
     */
    fDetector->numOfAcqValues = SI_DET_NUM_OF_ACQ_VALUES;
    fDetector->acqValues =
        handel_md_alloc((size_t)fDetector->numOfAcqValues * sizeof(AcquisitionValue));
    if (fDetector->acqValues == NULL) {
        handel_md_event_destroy(&fDetector->asyncEvent);
        handel_md_mutex_destroy(&fDetector->lock);
        handel_md_free(fDetector);
        pslLog(PSL_LOG_ERROR, XIA_NOMEM,
               "No memory for detector acquisition values");
        return XIA_NOMEM;
    }

    for (av = 0; av < fDetector->numOfAcqValues; ++av) {
        fDetector->acqValues[av] = DEFAULT_ACQ_VALUES[av];
    }

    /*
     * Set up the ACQ values from the defaults.
     */
    status = psl__ReloadDefaults(fDetector);
    if (status != XIA_SUCCESS) {
        detector->pslData = NULL;
        handel_md_event_destroy(&fDetector->asyncEvent);
        handel_md_mutex_destroy(&fDetector->lock);
        handel_md_free(fDetector->acqValues);
        handel_md_free(fDetector);
        pslLog(PSL_LOG_ERROR, status,
               "Detector channel default load failed: %d", modDetChan);
        return status;
    }

    fDetector->detChan = detChan;

    fDetector->channelState = ChannelDisconnected;

    falconXNClearDetectorStats(fDetector);

    status = psl__RefreshChannelState(module, detector);
    if (status != XIA_SUCCESS) {
        detector->pslData = NULL;
        handel_md_event_destroy(&fDetector->asyncEvent);
        handel_md_mutex_destroy(&fDetector->lock);
        handel_md_free(fDetector->acqValues);
        handel_md_free(fDetector);
        pslLog(PSL_LOG_ERROR, status, "Unable to get channel.state");
        return status;
    }

    if (fDetector->channelState != ChannelReady) {
        pslLog(PSL_LOG_DEBUG,
               "Stopping data acquisition to clear the channel state %d on startup.",
               fDetector->channelState);
        status = psl__StopDataAcquisition(module, fDetector->modDetChan, false);
        if (status != XIA_SUCCESS) {
            detector->pslData = NULL;
            handel_md_event_destroy(&fDetector->asyncEvent);
            handel_md_mutex_destroy(&fDetector->lock);
            handel_md_free(fDetector->acqValues);
            handel_md_free(fDetector);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to stop any running data acquisition modes");
            return status;
        }
    }

    status = psl__MonitorChannel(module, detector);
    if (status != XIA_SUCCESS) {
        detector->pslData = NULL;
        handel_md_event_destroy(&fDetector->asyncEvent);
        handel_md_mutex_destroy(&fDetector->lock);
        handel_md_free(fDetector->acqValues);
        handel_md_free(fDetector);
        pslLog(PSL_LOG_ERROR, status,
                   "Unable to set channel monitoring");
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__EndDetChan(int detChan, Detector *detector, Module *module)
{
    if (!module) {
        pslLog(PSL_LOG_ERROR, XIA_BAD_PSL_ARGS,"Module is NULL");
        return XIA_BAD_PSL_ARGS;
    }

    if (!module->pslData) {
        pslLog(PSL_LOG_ERROR, XIA_BAD_PSL_ARGS,
               "Module PSL data is NULL");
        return XIA_BAD_PSL_ARGS;
    }

    if (!detector) {
        pslLog(PSL_LOG_ERROR, XIA_BAD_PSL_ARGS,
               "Detector is NULL");
        return XIA_BAD_PSL_ARGS;
    }

    pslLog(PSL_LOG_DEBUG,
           "Detector %s (%d)", detector->alias, detChan);

    if (detector->pslData) {
        FalconXNModule* fModule = module->pslData;
        FalconXNDetector* fDetector = detector->pslData;

        psl__ModuleLock(module);

        falconXNClearDetectorCalibrationData(fDetector);

        fModule->channelActive[fDetector->modDetChan] = FALSE_;

        psl__ModuleUnlock(module);

        psl__MonitorChannel(module, detector);

        psl__ModuleLock(module);

        handel_md_event_destroy(&fDetector->asyncEvent);
        handel_md_mutex_destroy(&fDetector->lock);
        handel_md_free(fDetector->acqValues);
        handel_md_free(fDetector);

        detector->pslData = NULL;

        psl__ModuleUnlock(module);
    }

    return XIA_SUCCESS;
}


/*
 * Returns true if the given name is in the removed acquisition values list.
 */
PSL_STATIC bool psl__AcqRemoved(const char *name)
{
    int i;

    for (i = 0; i < sizeof(REMOVED_ACQ_VALUES) / sizeof(REMOVED_ACQ_VALUES[0]); i++) {
        if (STREQ(name, REMOVED_ACQ_VALUES[i]))
            return true;
    }

    return false;
}


PSL_STATIC int psl__UserSetup(int detChan, Detector *detector, Module *module)
{
    int status;

    XiaDefaults *defaults = NULL;

    XiaDaqEntry* entry;

    FalconXNDetector* fDetector = detector->pslData;

    int channel = xiaGetModDetectorChan(detChan);

    int i;

    xiaPSLBadArgs(module, detector, "psl__UserSetup");

    pslLog(PSL_LOG_DEBUG,
           "%s (%d:%d) user set up", module->alias, fDetector->modDetChan, detChan);

    /*
     * Load the detector characterization data if there is any.
     */
    status = psl__LoadDetCharacterization(detector, module);

    if ((status != XIA_SUCCESS) && (status != XIA_NOT_FOUND)) {
        pslLog(PSL_LOG_ERROR, status,
               "Error setting the detector characterization data: %s (%d)",
               detector->alias, detChan);
        return status;
    }

    /*
     * Loop over the ACQ defaults and make sure they are all present. If they
     * are not add them. They should be written to the INI file when saved.
     */
    for (i = 0; i < fDetector->numOfAcqValues; i++) {
        defaults = xiaGetDefaultFromDetChan(detChan);
        entry = defaults->entry;

        while (entry) {
            if (strcmp(entry->name, fDetector->acqValues[i].name) == 0) {
                break;
            }
            entry = entry->next;
        }
        if ((entry == NULL) &&
            ((fDetector->acqValues[i].flags & PSL_ACQ_HAS_DEFAULT) != 0)) {
            status = xiaAddDefaultItem(defaults->alias,
                                       fDetector->acqValues[i].name,
                                       &fDetector->acqValues[i].defaultValue);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Adding default: %s <-r %s",
                       defaults->alias, fDetector->acqValues[i].name);
            }
        }
    }
    /*
     * Some acquisition values require synchronization with another data
     * structure in the program prior to setting the initial acquisition
     * value.
     */
    for (i = 0; i < fDetector->numOfAcqValues; i++) {
        if (fDetector->acqValues[i].sync != NULL) {
            status = fDetector->acqValues[i].sync(detChan, channel,
                                                  module, detector, defaults);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Error synchronizing '%s' for detChan %d (%u)",
                       fDetector->acqValues[i].name, detChan, channel);
                return status;
            }
        }
    }

    defaults = xiaGetDefaultFromDetChan(detChan);

    entry = defaults->entry;

    while (entry) {
        if (strlen(entry->name) > 0) {
            AcquisitionValue* acq = psl__GetAcquisition(fDetector, entry->name);

            if (!acq) {
                if (psl__AcqRemoved(entry->name)) {
                    pslLog(PSL_LOG_WARNING, "ignoring deprecated acquisition value: %s",
                           entry->name);
                }
                else {
                    status = XIA_UNKNOWN_VALUE;
                    pslLog(PSL_LOG_ERROR, status,
                           "invalid entry: %s\n", entry->name);
                    return status;
                }

                entry = entry->next;
                continue;
            }

            /*
             * Ignore the read-only acquisition values.
             */
            if ((acq->flags & PSL_ACQ_READ_ONLY) == 0) {
                status = psl__SetAcquisitionValues(detChan, detector, module,
                                                   entry->name, &(entry->data));

                if (status != XIA_SUCCESS) {
                    pslLog(PSL_LOG_ERROR, status,
                           "Error setting '%s' to %0.3f for detChan %d.",
                           entry->name, entry->data, detChan);
                    return status;
                }
            }
        }
        entry = entry->next;
    }


    /*
     * Set digital pin configuration.
     */
    status = psl__SetDigitalConf(detChan, detector, module);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error setting the detector digital configuration for detChan %d",
               detChan);
        return status;
    }

    pslLog(PSL_LOG_DEBUG,
           "Finished %s (%d:%d) set up", module->alias, fDetector->modDetChan, detChan);

    return XIA_SUCCESS;
}


PSL_STATIC int psl__BoardOperation(int detChan, Detector* detector, Module* module,
                                   const char *name, void *value)
{
    int status;
    int i;

    ASSERT(name);
    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOperation");

    for (i = 0; i < (int) N_ELEMS(boardOps); i++) {
        if (STREQ(name, boardOps[i].name)) {

            status = boardOps[i].fn(detChan, detector, module, name, value);

            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Error doing board operation '%s' for detChan %d",
                       name, detChan);
                return status;
            }

            return XIA_SUCCESS;
        }
    }

    pslLog(PSL_LOG_ERROR, XIA_BAD_NAME,
           "Unknown board operation '%s' for detChan %d", name, detChan);

    return XIA_BAD_NAME;
}


PSL_STATIC int psl__GetDefaultAlias(char *alias, char **names, double *values)
{
    UNUSED(alias);
    UNUSED(names);
    UNUSED(values);

    return XIA_SUCCESS;
}


PSL_STATIC unsigned int psl__GetNumDefaults(void)
{
    return 0;
}


PSL_STATIC boolean_t psl__CanRemoveName(const char *name)
{
    UNUSED(name);

    return FALSE_;
}

PSL_STATIC int psl__DetCharacterizeStart(int detChan, Detector* detector, Module* module)
{
    int status;

    char item[MAXITEM_LEN];
    char firmware[MAXITEM_LEN];

    uint8_t    pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);

    SiToro__Sinc__KeyValue kv;

    /*
     * Check a firmware set is present for this channel. It must exist
     * before running a detector characterization.
     */
    sprintf(item, "firmware_set_chan%d", detChan);

    status = xiaGetModuleItem(module->alias, item, firmware);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error getting the firmware from the module: %s", item);
        return status;
    }

    /*
     * Enable optimization by default. We can always skip it by stopping
     * acquisition during this phase.
     */
    si_toro__sinc__key_value__init(&kv);
    kv.key = (char*) "pulse.calibration.optimize";
    kv.has_boolval = TRUE_;
    kv.boolval = TRUE_;

    status = psl__SetParam(module, detector, &kv);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error setting pulse optimization for starting characterization");
        return status;
    }

    /*
     * Start the detector characterization.
     */
    SincEncodeStartCalibration(&packet, psl__DetectorChannel(detector));

    status = psl__ModuleTransactionSend(module, &packet);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error starting characterization");
        return status;
    }

    status = psl__CheckSuccessResponse(module);

    psl__ModuleTransactionEnd(module);

    return status;
}

PSL_STATIC int psl__WriteDetCharacterizationWave(FILE* dcFile,
                                                 const char *filename,
                                                 const char* name,
                                                 const double* data,
                                                 const int len)
{
    int status = XIA_SUCCESS;

    ssize_t lineLength = 0;
    ssize_t written =  0;
    int i;

    written = fprintf(dcFile, "%s=%d\n", name, len);
    if (written < 0) {
        xia_file_close(dcFile);
        status = XIA_BAD_FILE_WRITE;
        xiaLog(XIA_LOG_ERROR, status, "psl__WriteDetCharacterizationWave",
               "Writing to detector characterization %s size failed: %s: (%d) %s",
               name, filename, errno, strerror(errno));
        return status;
    }

    for (i = 0; i < (len - 1); ++i) {
        written = fprintf(dcFile, "%f,", data[i]);
        if (written < 0) {
            xia_file_close(dcFile);
            status = XIA_BAD_FILE_WRITE;
            xiaLog(XIA_LOG_ERROR, status, "psl__WriteDetCharacterizationWave",
                   "Writing to detector characterization %s failed: %s: (%d) %s",
                   name, filename, errno, strerror(errno));
            return status;
        }

        lineLength += written;
        if (lineLength > 60) {
            written = fprintf(dcFile, "\n");
            if (written < 0) {
                xia_file_close(dcFile);
                status = XIA_BAD_FILE_WRITE;
                xiaLog(XIA_LOG_ERROR, status, "psl__WriteDetCharacterizationWave",
                       "Writing to detector characterization %s failed: %s: (%d) %s",
                       name, filename, errno, strerror(errno));
                return status;
            }
            lineLength = 0;
        }
    }

    written = fprintf(dcFile, "%f\n", data[i]);
    if (written < 0) {
        xia_file_close(dcFile);
        status = XIA_BAD_FILE_WRITE;
        xiaLog(XIA_LOG_ERROR, status, "psl__WriteDetCharacterizationWave",
               "Writing to detector characterization %s failed: %s: (%d) %s",
               name, filename, errno, strerror(errno));
        return status;
    }

    return status;
}

PSL_STATIC int psl__UnloadDetCharacterization(Module* module,
                                              Detector* detector,
                                              const char *filename)
{
    int status = XIA_SUCCESS;

    FalconXNDetector* fDetector = detector->pslData;

    SiToro__Sinc__GetParamResponse* resp = NULL;
    SiToro__Sinc__KeyValue*         kv;

    FILE* dcFile = NULL;

    boolean_t calibrated = FALSE_;

    status = psl__GetParam(module, psl__DetectorChannel(detector),
                           "pulse.calibrated", &resp);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get pulse.calibrated");
        return status;
    }

    kv = resp->results[0];

    if (kv->has_paramtype &&
        (kv->paramtype == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__BOOL_TYPE)) {
        calibrated = kv->boolval ? TRUE_ : FALSE_;
    }

    si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

    if (!calibrated)
        return XIA_SUCCESS;

    status = psl__GetCalibration(module, detector);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Cannot get calibration data");
        return status;
    }

    /*
     * Make sure the data returned from the FalconX is sane. Reject it
     * if it is rubish.
     */
    status = psl__CheckDetCharWaveform("Example", &fDetector->calibExample);
    if (status != XIA_SUCCESS) {
        falconXNClearDetectorCalibrationData(fDetector);
        return XIA_SUCCESS;
    }
    status = psl__CheckDetCharWaveform("Model", &fDetector->calibModel);
    if (status != XIA_SUCCESS) {
        falconXNClearDetectorCalibrationData(fDetector);
        return XIA_SUCCESS;
    }
    status = psl__CheckDetCharWaveform("Final", &fDetector->calibFinal);
    if (status != XIA_SUCCESS) {
        falconXNClearDetectorCalibrationData(fDetector);
        return XIA_SUCCESS;
    }

    xiaLog(XIA_LOG_INFO, "psl__UnloadDetCharacterization",
           "writing detector characterization file: %s", filename);

    dcFile = xia_file_open(filename, "w");

    if (dcFile == NULL) {
        status = XIA_NOT_FOUND;
        xiaLog(XIA_LOG_ERROR, status, "psl__UnloadDetCharacterization",
               "Could not open: %s", filename);
    }
    else {
        ssize_t lineLength = 0;
        ssize_t written =  0;
        int i;

        written = fprintf(dcFile, "data=%d\n", fDetector->calibData.len);
        if (written < 0) {
            xia_file_close(dcFile);
            status = XIA_BAD_FILE_WRITE;
            xiaLog(XIA_LOG_ERROR, status, "psl__UnloadDetCharacterization",
                   "Writing to detector characterization data size failed: %s: (%d) %s",
                   filename, errno, strerror(errno));
            return status;
        }

        for (i = 0; i < (fDetector->calibData.len - 1); ++i) {
            written = fprintf(dcFile, "%02x,", fDetector->calibData.data[i]);
            if (written < 0) {
                xia_file_close(dcFile);
                status = XIA_BAD_FILE_WRITE;
                xiaLog(XIA_LOG_ERROR, status, "psl__UnloadDetCharacterization",
                       "Writing to detector characterization data failed: %s: (%d) %s",
                       filename, errno, strerror(errno));
                return status;
            }

            lineLength += written;
            if (lineLength > 60) {
                written = fprintf(dcFile, "\n");
                if (written < 0) {
                    xia_file_close(dcFile);
                    status = XIA_BAD_FILE_WRITE;
                    xiaLog(XIA_LOG_ERROR, status, "psl__UnloadDetCharacterization",
                           "Writing to detector characterization data failed: %s: (%d) %s",
                           filename, errno, strerror(errno));
                    return status;
                }
                lineLength = 0;
            }
        }

        written = fprintf(dcFile, "%02x\n", fDetector->calibData.data[i]);
        if (written < 0) {
            xia_file_close(dcFile);
            status = XIA_BAD_FILE_WRITE;
            xiaLog(XIA_LOG_ERROR, status, "psl__UnloadDetCharacterization",
                   "Writing to detector characterization data failed: %s: (%d) %s",
                   filename, errno, strerror(errno));
            return status;
        }

        /*
         * Example waveform.
         */
        status = psl__WriteDetCharacterizationWave(dcFile,
                                                   filename,
                                                   "example-x",
                                                   fDetector->calibExample.x,
                                                   fDetector->calibExample.len);
        if (status != XIA_SUCCESS) {
            xia_file_close(dcFile);
            return status;
        }
        status = psl__WriteDetCharacterizationWave(dcFile,
                                                   filename,
                                                   "example-y",
                                                   fDetector->calibExample.y,
                                                   fDetector->calibExample.len);
        if (status != XIA_SUCCESS) {
            xia_file_close(dcFile);
            return status;
        }

        /*
         * Model waveform.
         */
        status = psl__WriteDetCharacterizationWave(dcFile,
                                                   filename,
                                                   "model-x",
                                                   fDetector->calibModel.x,
                                                   fDetector->calibModel.len);
        if (status != XIA_SUCCESS) {
            xia_file_close(dcFile);
            return status;
        }
        status = psl__WriteDetCharacterizationWave(dcFile,
                                                   filename,
                                                   "model-y",
                                                   fDetector->calibModel.y,
                                                   fDetector->calibModel.len);
        if (status != XIA_SUCCESS) {
            xia_file_close(dcFile);
            return status;
        }

        /*
         * Final waveform.
         */
        status = psl__WriteDetCharacterizationWave(dcFile,
                                                   filename,
                                                   "final-x",
                                                   fDetector->calibFinal.x,
                                                   fDetector->calibFinal.len);
        if (status != XIA_SUCCESS) {
            xia_file_close(dcFile);
            return status;
        }
        status = psl__WriteDetCharacterizationWave(dcFile,
                                                   filename,
                                                   "final-y",
                                                   fDetector->calibFinal.y,
                                                   fDetector->calibFinal.len);
        if (status != XIA_SUCCESS) {
            xia_file_close(dcFile);
            return status;
        }

        xia_file_close(dcFile);
    }

    return status;
}

PSL_STATIC int psl__ReadDetCharacterizationWave(FILE* dcFile,
                                                const char *filename,
                                                const char* name,
                                                double** ddata,
                                                int* len,
                                                int* lc)
{
    int status = XIA_SUCCESS;

    double* data;
    char line[XIA_LINE_LEN];
    char* p;
    int i;

    *lc += 1;
    p = handel_md_fgets(line, XIA_LINE_LEN, dcFile);

    if (p == NULL) {
        status = XIA_BAD_FILE_READ;
        xiaLog(XIA_LOG_ERROR, status, "psl__ReadDetCharacterizationWave",
               "Could not read %s length: %s:%d", name, filename, *lc);
        return status;
    }

    if (!STRNEQ(line, name)) {
        status = XIA_BAD_FILE_READ;
        xiaLog(XIA_LOG_ERROR, status, "psl__ReadDetCharacterizationWave",
               "Could not find %s length: %s:%d", name, filename, *lc);
        return status;
    }

    i = atoi(line + strlen(name) + 1);

    if ((*len != 0) && (i != *len)) {
        status = XIA_BAD_FILE_READ;
        xiaLog(XIA_LOG_ERROR, status, "psl__ReadDetCharacterizationWave",
               "Could not match %s length: %s:%d", name, filename, *lc);
        return status;
    }

    *len = i;

    *ddata = data = handel_md_alloc((size_t) *len * sizeof(double));
    if (data == NULL) {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "psl__LoadDetCharacterization",
               "No memory for %s length of %d: %s:%d",
               name, *len, filename, *lc);
        return status;
    }

    p = NULL;
    i = 0;
    while (i < *len) {
        double value;

        if (p == NULL) {
            *lc += 1;
            p = handel_md_fgets(line, XIA_LINE_LEN, dcFile);
            if (p == NULL) {
                status = XIA_BAD_FILE_READ;
                xiaLog(XIA_LOG_ERROR, status, "psl__LoadDetCharacterization",
                       "Could not read %s values: %s:%d", name, filename, *lc);
                return status;
            }
        }

        value = strtod(p, 0);

        data[i] = value;
        ++i;
        p = strchr(p, ',');
        if (p != NULL) {
            ++p;
            if (strlen(p) < 3)
                p = NULL;
        }
    }

    return status;
}

PSL_STATIC int psl__LoadDetCharacterization(Detector *detector, Module *module)
{
    int status;

    FalconXNDetector* fDetector = detector->pslData;

    char item[MAXITEM_LEN];
    char firmware[MAXITEM_LEN];
    char filename[MAXITEM_LEN];

    /*
     * Check a firmware set is present for this channel.
     */
    sprintf(item, "firmware_set_chan%d", psl__DetectorChannel(detector));

    /*
     * If there is no detector characterisation data it just means the
     * detector's SiToro calibration has not been run.
     */

    status = xiaGetModuleItem(module->alias, item, firmware);

    xiaLog(XIA_LOG_INFO, "psl__LoadDetCharacterization",
           "module item %s = %s", item, firmware);

    if ((status == XIA_SUCCESS) && !STREQ(firmware, "null")) {
        status = xiaGetFirmwareItem(firmware, 0, "filename", filename);

        if (status == XIA_SUCCESS) {
            char newFile[MAXFILENAME_LEN];

            FILE* dcFile;

            xiaLog(XIA_LOG_INFO, "psl__LoadDetCharacterization",
                   "read detector characterization: %s", filename);

            dcFile = xiaFindFile(filename, "rb", newFile);

            if (dcFile) {
                char line[XIA_LINE_LEN];
                int lc = 0;
                char* p;
                int i;

                falconXNClearDetectorCalibrationData(fDetector);

                ++lc;
                p = handel_md_fgets(line, XIA_LINE_LEN, dcFile);

                /* TODO: close the file in this series of error checks */

                if (p == NULL) {
                    status = XIA_BAD_FILE_READ;
                    xiaLog(XIA_LOG_ERROR, status, "psl__LoadDetCharacterization",
                           "Could not read data length: %s:%d", filename, lc);
                    return status;
                }

                if (!STRNEQ(line, "data=")) {
                    status = XIA_BAD_FILE_READ;
                    xiaLog(XIA_LOG_ERROR, status, "psl__LoadDetCharacterization",
                           "Could not find data length: %s:%d", filename, lc);
                    return status;
                }

                fDetector->calibData.len = atoi(line + sizeof("data=") - 1);
                fDetector->calibData.data =
                    handel_md_alloc((size_t) fDetector->calibData.len);
                if (fDetector->calibData.data == NULL) {
                    status = XIA_NOMEM;
                    xiaLog(XIA_LOG_ERROR, status, "psl__LoadDetCharacterization",
                           "No memory for data length of %d: %s:%d",
                           fDetector->calibData.len, filename, lc);
                    return status;
                }

                p = NULL;
                i = 0;
                while (i < fDetector->calibData.len) {
                    int value;

                    if (p == NULL) {
                        ++lc;
                        p = handel_md_fgets(line, XIA_LINE_LEN, dcFile);
                        if (p == NULL) {
                            falconXNClearDetectorCalibrationData(fDetector);
                            status = XIA_BAD_FILE_READ;
                            xiaLog(XIA_LOG_ERROR, status, "psl__LoadDetCharacterization",
                                   "Could not read data values: %s:%d", filename, lc);
                            return status;
                        }
                    }

                    value = (int) strtol(p, 0, 16);

                    if (value > 255) {
                        falconXNClearDetectorCalibrationData(fDetector);
                        status = XIA_BAD_FILE_READ;
                        xiaLog(XIA_LOG_ERROR, status, "psl__LoadDetCharacterization",
                               "Could not parse data value: %s:%d", filename, lc);
                        return status;
                    }

                    fDetector->calibData.data[i] = (uint8_t) value;
                    ++i;
                    p += 3;

                    if (strlen(p) < 3)
                        p = NULL;
                }

                /*
                 * Example waveform.
                 */
                status = psl__ReadDetCharacterizationWave(dcFile,
                                                          filename,
                                                          "example-x",
                                                          &fDetector->calibExample.x,
                                                          &fDetector->calibExample.len,
                                                          &lc);
                if (status != XIA_SUCCESS) {
                    falconXNClearDetectorCalibrationData(fDetector);
                    xia_file_close(dcFile);
                    return status;
                }
                status = psl__ReadDetCharacterizationWave(dcFile,
                                                          filename,
                                                          "example-y",
                                                          &fDetector->calibExample.y,
                                                          &fDetector->calibExample.len,
                                                          &lc);
                if (status != XIA_SUCCESS) {
                    falconXNClearDetectorCalibrationData(fDetector);
                    xia_file_close(dcFile);
                    return status;
                }

                /*
                 * Model waveform.
                 */
                status = psl__ReadDetCharacterizationWave(dcFile,
                                                          filename,
                                                          "model-x",
                                                          &fDetector->calibModel.x,
                                                          &fDetector->calibModel.len,
                                                          &lc);
                if (status != XIA_SUCCESS) {
                    falconXNClearDetectorCalibrationData(fDetector);
                    xia_file_close(dcFile);
                    return status;
                }
                status = psl__ReadDetCharacterizationWave(dcFile,
                                                          filename,
                                                          "model-y",
                                                          &fDetector->calibModel.y,
                                                          &fDetector->calibModel.len,
                                                          &lc);
                if (status != XIA_SUCCESS) {
                    falconXNClearDetectorCalibrationData(fDetector);
                    xia_file_close(dcFile);
                    return status;
                }

                /*
                 * Final waveform.
                 */
                status = psl__ReadDetCharacterizationWave(dcFile,
                                                          filename,
                                                          "final-x",
                                                          &fDetector->calibFinal.x,
                                                          &fDetector->calibFinal.len,
                                                          &lc);
                if (status != XIA_SUCCESS) {
                    falconXNClearDetectorCalibrationData(fDetector);
                    xia_file_close(dcFile);
                    return status;
                }
                status = psl__ReadDetCharacterizationWave(dcFile,
                                                          filename,
                                                          "final-y",
                                                          &fDetector->calibFinal.y,
                                                          &fDetector->calibFinal.len,
                                                          &lc);
                if (status != XIA_SUCCESS) {
                    falconXNClearDetectorCalibrationData(fDetector);
                    xia_file_close(dcFile);
                    return status;
                }

                xia_file_close(dcFile);

                status = psl__SetCalibration(module, detector);
                if (status != XIA_SUCCESS) {
                    falconXNClearDetectorCalibrationData(fDetector);
                    xiaLog(XIA_LOG_ERROR, status, "psl__LoadDetCharacterization",
                           "Error setting the detector characterization");
                    return status;
                }

                status = XIA_SUCCESS;
            }
        }
    }

    return status;
}

/*
 * Set the SINC histogram mode from pixel_advance_mode for mapping
 * mode pixel control.
 */
PSL_STATIC int psl__SyncPixelAdvanceMode(Module *module, Detector *detector)
{
    int status;
    char *mode;
    AcquisitionValue *pixel_advance_mode;
    FalconXNDetector *fDetector = detector->pslData;

    pixel_advance_mode = psl__GetAcquisition(fDetector, "pixel_advance_mode");
    ASSERT(pixel_advance_mode);

    switch(pixel_advance_mode->value.ref.i) {
    case (int64_t) XIA_MAPPING_CTL_USER:
        mode = "continuous";
        break;
    case (int64_t) XIA_MAPPING_CTL_GATE:
        mode = "gated";
        break;
    default:
        status = XIA_BAD_VALUE;
        pslLog(PSL_LOG_ERROR, status,
               "invalid pixel_advance_mode value: %" PRIu64,
               pixel_advance_mode->value.ref.i);
        return status;
    }

    status = psl__SetHistogramMode(module, detector, mode);
    return status;
}

/*
 * Set the SINC histogram mode from preset_type for MCA mode preset
 * runs.
 */
PSL_STATIC int psl__SyncPresetType(Module *module, Detector *detector)
{
    int status;
    const char *mode;
    AcquisitionValue *preset_type;
    FalconXNDetector *fDetector = detector->pslData;

    preset_type = psl__GetAcquisition(fDetector, "preset_type");
    ASSERT(preset_type);

    switch(preset_type->value.ref.i) {
    case (int64_t) XIA_PRESET_NONE:
        mode = "continuous";
        break;
    case (int64_t) XIA_PRESET_FIXED_REAL:
        mode = "fixedTime";
        break;
    case (int64_t) XIA_PRESET_FIXED_TRIGGERS:
        mode = "fixedInputCount";
        break;
    case (int64_t) XIA_PRESET_FIXED_EVENTS:
        mode = "fixedOutputCount";
        break;
    default:
        status = XIA_BAD_VALUE;
        pslLog(PSL_LOG_ERROR, status,
               "invalid histogram mode value");
        return status;
    }

    status = psl__SetHistogramMode(module, detector, mode);
    return status;
}

/*
 * Set the SINC histogram mode to a given mode.
 */
PSL_STATIC int psl__SetHistogramMode(Module *module, Detector *detector, const char *mode)
{
    int status;
    SiToro__Sinc__KeyValue kv;

    si_toro__sinc__key_value__init(&kv);
    kv.key = (char*) "histogram.mode";
    kv.optionval = (char*) mode;

    status = psl__SetParam(module, detector, &kv);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to set the histogram mode: %s", mode);
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Set the SINC histogram refresh rate from mca_refresh for an MCA run.
 */
PSL_STATIC int psl__SyncMCARefresh(Module *module, Detector *detector)
{
    int status;
    AcquisitionValue *mca_refresh;
    FalconXNDetector *fDetector = detector->pslData;

    mca_refresh = psl__GetAcquisition(fDetector, "mca_refresh");
    ASSERT(mca_refresh);

    status = psl__SetMCARefresh(module, detector, mca_refresh->value.ref.f);
    return status;
}

/*
 * SINC histogram.refreshRate setter.
 */
PSL_STATIC int psl__SetMCARefresh(Module *module, Detector *detector, double period)
{
    int status;
    SiToro__Sinc__KeyValue kv;

    si_toro__sinc__key_value__init(&kv);
    kv.key = (char*) "histogram.refreshRate";
    kv.has_floatval = TRUE_;
    kv.floatval = period;

    status = psl__SetParam(module, detector, &kv);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to set the histogram refresh rate");
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Set params underlying number_mca_channels in terms of related acqs.
 */
PSL_STATIC int psl__SyncNumberMCAChannels(Module *module, Detector *detector)
{
    AcquisitionValue *mca_start_channel;
    AcquisitionValue *number_mca_channels;

    FalconXNDetector *fDetector;

    int64_t highIndex;

    SiToro__Sinc__KeyValue kv;

    int status;

    fDetector = detector->pslData;

    mca_start_channel = psl__GetAcquisition(fDetector, "mca_start_channel");
    number_mca_channels = psl__GetAcquisition(fDetector, "number_mca_channels");
    ASSERT(mca_start_channel);
    ASSERT(number_mca_channels);

    highIndex = mca_start_channel->value.ref.i + number_mca_channels->value.ref.i - 1;

    si_toro__sinc__key_value__init(&kv);
    kv.key = (char*) "histogram.binSubRegion.highIndex";
    kv.has_intval = TRUE_;
    kv.intval = highIndex;

    status = psl__SetParam(module, detector, &kv);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to set the histogram region high index");
        return status;
    }

    return XIA_SUCCESS;
}

/*
 * Combine XMAP-style gate collection values into one SINC param.
 */
PSL_STATIC int psl__SyncGateCollectionMode(Module *module, Detector *detector)
{
    AcquisitionValue *input_logic_polarity;
    AcquisitionValue *gate_ignore;

    FalconXNDetector *fDetector;

    SiToro__Sinc__KeyValue kv;

    int status;

    fDetector = detector->pslData;

    input_logic_polarity = psl__GetAcquisition(fDetector, "input_logic_polarity");
    gate_ignore = psl__GetAcquisition(fDetector, "gate_ignore");
    ASSERT(input_logic_polarity);
    ASSERT(gate_ignore);

    si_toro__sinc__key_value__init(&kv);
    kv.key = (char*) "gate.statsCollectionMode";

    if (input_logic_polarity->value.ref.i == XIA_GATE_COLLECT_LO) {
        if (gate_ignore->value.ref.i == 1)
            kv.optionval = (char*) "risingEdge";
        else
            kv.optionval = (char*) "whenLow";
    }
    else { /* XIA_GATE_COLLECT_HI */
        if (gate_ignore->value.ref.i == 1)
            kv.optionval = (char*) "fallingEdge";
        else
            kv.optionval = (char*) "whenHigh";
    }

    status = psl__SetParam(module, detector, &kv);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to set the gate collection mode");
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_Apply(int detChan, Detector* detector, Module* module,
                                  const char *name, void *value)
{
    int status = XIA_SUCCESS;

    int channel = xiaGetModDetectorChan(detChan);

    uint8_t    pad[256];
    SincBuffer packet = SINC_BUFFER_INIT(pad);

    Sinc_Response response;
    SiToro__Sinc__CheckParamConsistencyResponse* resp = NULL;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_Apply");

    pslLog(PSL_LOG_INFO, "Checking params consistency for detChan %d", detChan);

    SincEncodeCheckParamConsistency(&packet, channel);

    status = psl__ModuleTransactionSend(module, &packet);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error checking param consistency");
        return status;
    }

    response.channel = -1;
    response.type = SI_TORO__SINC__MESSAGE_TYPE__CHECK_PARAM_CONSISTENCY_RESPONSE;

    status = psl__ModuleTransactionReceive(module, &response);
    if (status != XIA_SUCCESS) {
        return status;
    }

    resp = response.response;

    if (resp->has_healthy && !resp->healthy) {
        status = XIA_BAD_VALUE;
        pslLog(PSL_LOG_ERROR, status, "Params not healthy");
    }

    if (resp->badkey != NULL) {
        status = XIA_BAD_VALUE;
        pslLog(PSL_LOG_ERROR, status, "Bad key: %s", resp->badkey);
    }

    if (resp->message != NULL) {
        status = XIA_BAD_VALUE;
        pslLog(PSL_LOG_ERROR, status, "Check param consistency: %s", resp->message);
    }

	psl__ModuleTransactionEnd(module);

    return XIA_SUCCESS;;
}

PSL_STATIC int psl__BoardOp_BufferDone(int detChan, Detector* detector, Module* module,
                                       const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(name);
    UNUSED(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_BufferDone");

    /*
     * This is handled by the xiaGetRunData call. This lets the API get the
     * required data.
     */
    return xiaGetRunData(detChan, name, value);
}

PSL_STATIC int psl__BoardOp_MappingPixelNext(int detChan, Detector* detector,
                                             Module* module,
                                             const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(name);
    UNUSED(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_MappingPixelNext");

    /*
     * This is handled by the xiaGetRunData call. This lets the API get the
     * required data.
     */
    return xiaGetRunData(detChan, name, value);
}

/* This is a clunky API and perhaps should be split out completely
 * (some elements already have their own board info value) rather than
 * bothering to document and support.
 */
PSL_STATIC int psl__BoardOp_GetBoardInfo(int detChan, Detector* detector, Module* module,
                                         const char *name, void *value)
{
    int status;

    psl__SincParamValue             sincVal;

    char* info = (char*) value;

    size_t i;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetBoardInfo");

    /*
     * The board info is an array of characters with the following fields:
     *
     *   0(32): Product name.
     *  32(8) : Reserved.
     *  40(8) : Protocol version.
     *  48(32): Firmware version.
     *  80(32): Digital board serial number.
     * 112(32): Analog board serial number.
     *
     * Length is 144 bytes.
     */

    memset(info, 0, 144);

    /* product name */
    sincVal.str.len = 32;
    sincVal.str.str = &info[0];
    status = psl__GetParamValue(module, 0, "instrument.productName",
                                SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__STRING_TYPE,
                                &sincVal);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get instrument.productName");
        return status;
    }

    /* protocol version */
    status = psl__GetParamValue(module, 0, "instrument.protocolVersion",
                                SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__INT_TYPE,
                                &sincVal);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get instrument.protocolVersion");
        return status;
    }

    for (i = 0; i < sizeof(int64_t); ++i)
        info[40 + i] =
            (char) (sincVal.intval >> (8 * (sizeof(int64_t) - i - 1)));

    /* firmware */
    sincVal.str.len = 32;
    sincVal.str.str = &info[48];
    status = psl__GetParamValue(module, 0, "instrument.firmwareVersion",
                                SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__STRING_TYPE,
                                &sincVal);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get instrument.firmwareVersion");
        return status;
    }

    /* digital board */
    sincVal.str.len = 32;
    sincVal.str.str = &info[80];
    status = psl__GetParamValue(module, 0, "instrument.digital.serialNumber",
                                SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__STRING_TYPE,
                                &sincVal);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get instrument.digital.serialNumber");
        return status;
    }

    /* analog board */
    sincVal.str.len = 32;
    sincVal.str.str = &info[112];
    status = psl__GetParamValue(module, 0, "instrument.analog.serialNumber",
                                SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__STRING_TYPE,
                                &sincVal);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get instrument.analog.serialNumber");
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetConnected(int detChan, Detector* detector, Module* module,
                                         const char *name, void *value)
{
    int status;

    FalconXNModule* fModule;
    uint8_t         pad[256];
    SincBuffer      packet = SINC_BUFFER_INIT(pad);
    int*            connected = (int*) value;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetConnected");

    fModule = module->pslData;

    *connected = FALSE_;

    SincEncodePing(&packet, false);

    status = psl__ModuleTransactionSend(module, &packet);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Detector ping: %d", detChan);
        return status;
    }

    status = psl__CheckSuccessResponse(module);

    psl__ModuleTransactionEnd(module);

    if (status == XIA_SUCCESS)
        *connected = TRUE_;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetChannelCount(int detChan, Detector* detector, Module* module,
                                            const char *name, void *value)
{
    int status;

    FalconXNModule* fModule;
    int*            channelCount = (int*) value;

    SiToro__Sinc__GetParamResponse* resp = NULL;
    SiToro__Sinc__KeyValue*         kv;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetChannelCount");

    fModule = module->pslData;

    *channelCount = 0;

    status = psl__GetParam(module, 0, "instrument.numChannels", &resp);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the channel count");
        return status;
    }

    kv = resp->results[0];

    if (kv->has_intval) {
        *channelCount = (int)kv->intval;
    } else {
        status = XIA_BAD_VALUE;
    }

    si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Channel count response");
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetSerialNumber(int detChan, Detector* detector, Module* module,
                                            const char *name, void *value)
{
    int status;

    FalconXNModule*                 fModule;
    SiToro__Sinc__GetParamResponse* resp = NULL;
    SiToro__Sinc__KeyValue*         kv;

    char*                           serialNum = (char *)value;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetSerialNumber");

    fModule = module->pslData;

    status = psl__GetParam(module, 0, "instrument.assembly.serialNumber", &resp);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the serial number");
        return status;
    }

    kv = resp->results[0];

    if (kv->has_paramtype &&
        (kv->paramtype == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__STRING_TYPE)) {
        sprintf(serialNum, "%s", kv->strval);
    } else {
        status = XIA_BAD_VALUE;
    }

    si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Serial number response");
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetFirmwareVersion(int detChan, Detector* detector, Module* module,
                                               const char *name, void *value)
{
    int status;

    FalconXNModule*                 fModule;
    SiToro__Sinc__GetParamResponse* resp = NULL;
    SiToro__Sinc__KeyValue*         kv;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetFirmwareVersion");

    fModule = module->pslData;

    status = psl__GetParam(module, 0, "instrument.firmwareVersion", &resp);
    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the firmware version number");
        return status;
    }

    kv = resp->results[0];

    if (kv->has_paramtype &&
        (kv->paramtype == SI_TORO__SINC__KEY_VALUE__PARAM_TYPE__STRING_TYPE)) {
        strcpy(value, kv->strval);

    } else {
        status = XIA_BAD_VALUE;
    }

    si_toro__sinc__get_param_response__free_unpacked(resp, NULL);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Firmware version response");
        return status;
    }

    return XIA_SUCCESS;
}
