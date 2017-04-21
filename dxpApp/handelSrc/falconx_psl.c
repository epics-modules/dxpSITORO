/*
 * Copyright (c) 2012 XIA LLC
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
 *
 * $Id$
 *
 */

/*
 * FalconX Platform Specific Layer
 */

#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "handel_log.h"

#include "psldef.h"
#include "psl_common.h"

#include "xia_handel.h"
#include "xia_system.h"
#include "xia_common.h"
#include "xia_assert.h"

#include "handel_errors.h"

#include "handel_file.h"
#include "xia_file.h"

#include "handel_mapping_modes.h"

#include "falconx_psl.h"

#ifdef _MSC_VER
#pragma warning(once: 4100) /* unreferenced formal parameter */
#pragma warning(once: 4127) /* conditional expression is constant */
#endif

/* Hack to fix the broken SI API. Removed in the new version yet siBool is used. */
#define SIBOOL_FALSE (0)
#define SIBOOL_TRUE  (1)

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
} psl__SiToroListModeStats32;

typedef struct {
    uint8_t  statsType;
    uint64_t samplesDetected;
    uint64_t samplesErased;
    uint64_t pulsesDetected;
    uint64_t pulsesAccepted;
    double   inputCountRate;
    double   outputCountRate;
    double   deadTimePercent;
} psl__SiToroListModeStats;

/*
 * Data formatter structures.
 *
 * All levels are counts of uint32_t and not byte offsets.
 */
#define MMC_BUFFERS (2)

typedef struct
{
    uint32_t low;
    uint32_t high;
} MM_Region;

typedef struct
{
    uint32_t   numOfRegions;
    MM_Region* regions;
} MM_Rois;

/*
 * Binner flags.
 */
#define MM_BINNER_GATE_HIGH      (1 << 0)   /* Gate 1 for trigger. */
/* #define MM_BINNER_GATE_TRIGGER   (1 << 16) */ /* Gate has been triggered. */
/* #define MM_BINNER_STATS_VALID    (1 << 17) */  /* The stats are valid. */

/* #define MM_BINNER_PIXEL_VALID(_b)                                     \
   (((_b)->flags ^ (MM_BINNER_GATE_TRIGGER | MM_BINNER_STATS_VALID)) == 0) */

/*
 * The binner takes the list mode data stream from the SiToro API and converts
 * it to bins. The binner has an input buffer used to get the list mode data.
 */
typedef struct
{
    /* Binning output */
    uint32_t                 flags;         /* State flags. */
    size_t                   numberOfBins;  /* The number of bins. */
    uint64_t*                bins;          /* The bins. */
    uint64_t                 outOfRange;    /* Count of energy levels out of range. */
    uint32_t                 errorBits;     /* Error bits returned from the List API. */
    uint64_t                 timestamp;     /* Current timestamp. */
    psl__SiToroListModeStats stats;         /* Extracted stats. */
    /* Input buffering of data from SiToro */
    uint32_t*                buffer;        /* Output buffer. */
    uint32_t                 bufferSize;    /* The size of the buffer */
    uint32_t                 bufferLevel;   /* The level of data in the buffer. */
} MM_Binner;

/*
 * A buffer is one of 2 output buffers accessed by the Handel user. The buffer
 * is large enough to hold the required number of pixels and any pixel header.
 */
typedef struct
{
    boolean_t full;         /* The buffer is full. */
    uint32_t  pixel;        /* The pixel number. */
    uint32_t  bufferPixel;  /* The pixel number. */
    uint32_t* next;         /* The next value to read. */
    size_t    level;        /* The amount of data in the buffer. */
    uint32_t* buffer;       /* The buffer. */
    size_t    size;         /* uint32_t units, not bytes. */
} MM_Buffer;

typedef struct
{
    int       active;
    MM_Buffer buffer[MMC_BUFFERS];
} MM_Buffers;

typedef struct
{
    uint32_t   numMapPixelsPerBuffer;
    uint32_t   pixels;
    uint32_t   pixelsInBuffer;
    MM_Buffers buffers;
    MM_Binner  bins;
} MMC1_Data;

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

PSL_STATIC int psl__UpdateGain(Detector* detector, SiToroDetector* siDetector,
                               XiaDefaults* defaults, boolean_t gain, boolean_t scaling);
PSL_STATIC int psl__CalculateGain(SiToroDetector* siDetector, double* gaindac,
                                  double* gaincoarse, double* scalefactor);
PSL_STATIC int psl__GainCalibrate(int detChan, Detector *det, int modChan,
                                  Module *m, XiaDefaults *def, double delta);
PSL_STATIC int psl__StartRun(int detChan, unsigned short resume,
                             XiaDefaults *defs, Detector *detector, Module *m);
PSL_STATIC int psl__StopRun(int detChan, Detector *detector, Module *m);
PSL_STATIC int psl__GetRunData(int detChan, const char *name, void *value,
                               XiaDefaults *defs, Detector *detector, Module *m);
PSL_STATIC int psl__SpecialRun(int detChan, const char *name, void *info,
                               XiaDefaults *defaults, Detector *detector, Module *module);
PSL_STATIC int psl__GetSpecialRunData(int detChan, const char *name,
                                      void *value, XiaDefaults *defaults,
                                      Detector *detector, Module *module);

PSL_STATIC boolean_t psl__CanRemoveName(const char *name);

PSL_STATIC int psl__DetCharacterizeStart(int detChan, Detector* detector, Module* module);
PSL_STATIC int psl__UnloadDetCharacterization(Detector* detector, const char *filename);
PSL_STATIC int psl__LoadDetCharacterization(int detChan, Detector *detector, Module *module);

/* Board operations */
PSL_STATIC int psl__BoardOp_Apply(int detChan, Detector* detector, Module* module,
                                  const char *name, void *value);
PSL_STATIC int psl__BoardOp_BufferDone(int detChan, Detector* detector, Module* module,
                                       const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetSiToroAPIVersion(int detChan, Detector* detector, Module* module,
                                                const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetSiToroBuildDate(int detChan, Detector* detector, Module* module,
                                               const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetSiToroDetector(int detChan, Detector* detector, Module* module,
                                              const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetBootLoaderVersion(int detChan, Detector* detector, Module* module,
                                                 const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetCardName(int detChan, Detector* detector, Module* module,
                                        const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetCardChannels(int detChan, Detector* detector, Module* module,
                                            const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetSerialNumber(int detChan, Detector* detector, Module* module,
                                            const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetFPGAVersion(int detChan, Detector* detector, Module* module,
                                           const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetAppId(int detChan, Detector* detector, Module* module,
                                     const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetFPGAId(int detChan, Detector* detector, Module* module,
                                      const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetFPGARunning(int detChan, Detector* detector, Module* module,
                                           const char *name, void *value);
PSL_STATIC int psl__BoardOp_GetConnected(int detBoardOp_Chan, Detector* detector, Module* module,
                                         const char *name, void *value);

/* Acquisition value handler */
#define ACQ_HANDLER_DECL(_n) \
    PSL_STATIC int psl__Acq_ ## _n (Detector*         detector, \
                                    SiToroDetector*   siDetector, \
                                    XiaDefaults*      defaults, \
                                    AcquisitionValue* acq, \
                                    double*           value, \
                                    boolean_t         read)
#define ACQ_HANDLER(_n) psl__Acq_ ## _n
/* #define ACQ_HANDLER_NAME(_n) "psl__Acq_" # _n */
#define ACQ_HANDLER_CALL(_n) ACQ_HANDLER(_n)(detector, siDetector, defaults, acq, value, read)
#define ACQ_HANDLER_LOG(_n) \
    pslLog(PSL_LOG_DEBUG, "%s", read ? "reading": "writing")

ACQ_HANDLER_DECL(analog_offset);
ACQ_HANDLER_DECL(analog_gain);
ACQ_HANDLER_DECL(analog_gain_boost);
ACQ_HANDLER_DECL(invert_input);
ACQ_HANDLER_DECL(detector_polarity);
ACQ_HANDLER_DECL(analog_discharge);
ACQ_HANDLER_DECL(analog_discharge_threshold);
ACQ_HANDLER_DECL(analog_discharge_period);
ACQ_HANDLER_DECL(disable_input);

ACQ_HANDLER_DECL(sample_rate);
ACQ_HANDLER_DECL(dc_offset);
ACQ_HANDLER_DECL(dc_tracking_mode);
ACQ_HANDLER_DECL(operating_mode);
ACQ_HANDLER_DECL(operating_mode_target);
ACQ_HANDLER_DECL(reset_blanking_enable);
ACQ_HANDLER_DECL(reset_blanking_threshold);
ACQ_HANDLER_DECL(reset_blanking_presamples);
ACQ_HANDLER_DECL(reset_blanking_postsamples);
ACQ_HANDLER_DECL(min_pulse_pair_separation);
ACQ_HANDLER_DECL(detection_threshold);
ACQ_HANDLER_DECL(validator_threshold_fixed);
ACQ_HANDLER_DECL(validator_threshold_proport);
ACQ_HANDLER_DECL(pulse_scale_factor);

ACQ_HANDLER_DECL(cal_noise_floor);
ACQ_HANDLER_DECL(cal_min_pulse_amp);
ACQ_HANDLER_DECL(cal_max_pulse_amp);
ACQ_HANDLER_DECL(cal_source_type);
ACQ_HANDLER_DECL(cal_pulses_needed);
ACQ_HANDLER_DECL(cal_filter_cutoff);
ACQ_HANDLER_DECL(cal_est_count_rate);

ACQ_HANDLER_DECL(hist_bin_count);
ACQ_HANDLER_DECL(hist_samples_detected);
ACQ_HANDLER_DECL(hist_samples_erased);
ACQ_HANDLER_DECL(hist_pulses_detected);
ACQ_HANDLER_DECL(hist_pulses_accepted);
ACQ_HANDLER_DECL(hist_pulses_rejected);
ACQ_HANDLER_DECL(hist_input_count_rate);
ACQ_HANDLER_DECL(hist_output_count_rate);
ACQ_HANDLER_DECL(hist_dead_time);

ACQ_HANDLER_DECL(mapping_mode);
ACQ_HANDLER_DECL(preset_type);
ACQ_HANDLER_DECL(preset_value);
ACQ_HANDLER_DECL(preset_baseline);
ACQ_HANDLER_DECL(preset_get_timing);

ACQ_HANDLER_DECL(number_of_scas);
ACQ_HANDLER_DECL(sca);
ACQ_HANDLER_DECL(num_map_pixels_per_buffer);
ACQ_HANDLER_DECL(num_map_pixels);
ACQ_HANDLER_DECL(buffer_check_period);

ACQ_HANDLER_DECL(input_logic_polarity);
ACQ_HANDLER_DECL(gate_ignore);
ACQ_HANDLER_DECL(pixel_advance_mode);

ACQ_HANDLER_DECL(number_mca_channels);

ACQ_HANDLER_DECL(preamp_gain);
ACQ_HANDLER_DECL(dynamic_range);
ACQ_HANDLER_DECL(adc_percent_rule);
ACQ_HANDLER_DECL(calibration_energy);
ACQ_HANDLER_DECL(mca_bin_width);

/* The default acquisition values. */
#define DECL_DEFAULT_ACQ_VALUES(_n, _t, _d, _f) \
    { # _n, (_d), { (_t), { (double) 0 } }, (_f), ACQ_HANDLER(_n) }

static const AcquisitionValue DEFAULT_ACQ_VALUES[SI_DET_NUM_OF_ACQ_VALUES] = {
    /* analog settings */
    DECL_DEFAULT_ACQ_VALUES(analog_offset,               acqInt16,  0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(analog_gain,                 acqUint16, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(analog_gain_boost,           acqBool,   0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(invert_input,                acqBool,   0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(detector_polarity,           acqBool,   0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(analog_discharge,            acqBool,   0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(analog_discharge_threshold,  acqUint16, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(analog_discharge_period,     acqUint16, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(disable_input,               acqBool,   0.0, PSL_ACQ_EMPTY),

    /* Digital detector settings */
    DECL_DEFAULT_ACQ_VALUES(sample_rate,                 acqDouble, 0.0, PSL_ACQ_READ_ONLY),
    DECL_DEFAULT_ACQ_VALUES(dc_offset,                   acqDouble, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(dc_tracking_mode,            acqUint32, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(operating_mode,              acqUint32, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(operating_mode_target,       acqUint32, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(reset_blanking_enable,       acqBool,   0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(reset_blanking_threshold,    acqDouble, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(reset_blanking_presamples,   acqUint16, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(reset_blanking_postsamples,  acqUint16, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(min_pulse_pair_separation,   acqUint32, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(detection_threshold,         acqDouble, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(validator_threshold_fixed,   acqDouble, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(validator_threshold_proport, acqDouble, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(pulse_scale_factor,          acqDouble, 0.0, PSL_ACQ_EMPTY),

    /* Calibration */
    DECL_DEFAULT_ACQ_VALUES(cal_noise_floor,             acqDouble, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(cal_min_pulse_amp,           acqDouble, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(cal_max_pulse_amp,           acqDouble, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(cal_source_type,             acqUint32, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(cal_pulses_needed,           acqUint32, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(cal_filter_cutoff,           acqDouble, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(cal_est_count_rate,          acqDouble, 0.0, PSL_ACQ_READ_ONLY | PSL_ACQ_RUNNING),

    /* Histogram */
    DECL_DEFAULT_ACQ_VALUES(hist_bin_count,              acqUint32, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(hist_samples_detected,       acqUint64, 0.0, PSL_ACQ_READ_ONLY | PSL_ACQ_RUNNING),
    DECL_DEFAULT_ACQ_VALUES(hist_samples_erased,         acqUint64, 0.0, PSL_ACQ_READ_ONLY | PSL_ACQ_RUNNING),
    DECL_DEFAULT_ACQ_VALUES(hist_pulses_detected,        acqUint64, 0.0, PSL_ACQ_READ_ONLY | PSL_ACQ_RUNNING),
    DECL_DEFAULT_ACQ_VALUES(hist_pulses_accepted,        acqUint64, 0.0, PSL_ACQ_READ_ONLY | PSL_ACQ_RUNNING),
    DECL_DEFAULT_ACQ_VALUES(hist_pulses_rejected,        acqUint64, 0.0, PSL_ACQ_READ_ONLY | PSL_ACQ_RUNNING),
    DECL_DEFAULT_ACQ_VALUES(hist_input_count_rate,       acqDouble, 0.0, PSL_ACQ_READ_ONLY | PSL_ACQ_RUNNING),
    DECL_DEFAULT_ACQ_VALUES(hist_output_count_rate,      acqDouble, 0.0, PSL_ACQ_READ_ONLY | PSL_ACQ_RUNNING),
    DECL_DEFAULT_ACQ_VALUES(hist_dead_time,              acqDouble, 0.0, PSL_ACQ_READ_ONLY | PSL_ACQ_RUNNING),

    /* MCA */
    DECL_DEFAULT_ACQ_VALUES(mapping_mode,                acqUint32, 0.0,   PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(preset_type,                 acqUint32, 0.0,   PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(preset_value,                acqUint32, 0.0,   PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(preset_baseline,             acqUint32, 0.0,   PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(preset_get_timing,           acqUint32, 500.0, PSL_ACQ_EMPTY),

    /* SCA */
    DECL_DEFAULT_ACQ_VALUES(number_of_scas,              acqUint32, 0.0,  PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(sca,                         acqString, 0.0,  PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(num_map_pixels_per_buffer,   acqUint32, 64.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(num_map_pixels,              acqUint32, 0.0,  PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(buffer_check_period,         acqUint32, 0.0,  PSL_ACQ_EMPTY),

    /* Gating */
    DECL_DEFAULT_ACQ_VALUES(input_logic_polarity,        acqUint32, 0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(gate_ignore,                 acqBool,   0.0, PSL_ACQ_EMPTY),
    DECL_DEFAULT_ACQ_VALUES(pixel_advance_mode,          acqUint32, 0.0, PSL_ACQ_EMPTY),

    /* Aliases */
    DECL_DEFAULT_ACQ_VALUES(number_mca_channels,         acqUint32, 0.0, PSL_ACQ_EMPTY),

    /* Gain */
    DECL_DEFAULT_ACQ_VALUES(preamp_gain,                 acqDouble, 3.0,  PSL_ACQ_HAS_DEFAULT),
    DECL_DEFAULT_ACQ_VALUES(dynamic_range,               acqDouble, 47.2, PSL_ACQ_HAS_DEFAULT),
    DECL_DEFAULT_ACQ_VALUES(adc_percent_rule,            acqDouble, 5.0,  PSL_ACQ_HAS_DEFAULT),
    DECL_DEFAULT_ACQ_VALUES(calibration_energy,          acqDouble, 5.9,  PSL_ACQ_HAS_DEFAULT),
    DECL_DEFAULT_ACQ_VALUES(mca_bin_width,               acqDouble, 10.0, PSL_ACQ_HAS_DEFAULT),
};

/* These are the allowed board operations for this hardware */
static  BoardOperation boardOps[] =
  {
    { "apply",                psl__BoardOp_Apply },
    { "buffer_done",          psl__BoardOp_BufferDone },

    /* SiToro Specific board operations. */
    { "get_sitoro_api_ver",   psl__BoardOp_GetSiToroAPIVersion },
    { "get_sitoro_builddate", psl__BoardOp_GetSiToroBuildDate },
    { "get_bootloader_ver",   psl__BoardOp_GetBootLoaderVersion },
    { "get_card_name",        psl__BoardOp_GetCardName },
    { "get_card_channels",    psl__BoardOp_GetCardChannels },
    { "get_serial_number",    psl__BoardOp_GetSerialNumber },
    { "get_fpga_version",     psl__BoardOp_GetFPGAVersion },
    { "get_app_id",           psl__BoardOp_GetAppId },
    { "get_fpga_id",          psl__BoardOp_GetFPGAId },
    { "get_fpga_running",     psl__BoardOp_GetFPGARunning },
    { "get_fpga_running",     psl__BoardOp_GetFPGARunning },
    { "get_sitoro_detector",  psl__BoardOp_GetSiToroDetector },
    { "get_connected",        psl__BoardOp_GetConnected }
  };

/* The number of SiToro setup calls. */
static int siToroSetups;

/* The PSL Handlers table. This is exported to Handel. */
static PSLHandlers handlers;

static const struct SiErrorTable {
    SiToro_Result siResult;
    int           handelError;
} errorTable[] = {
    { SiToro_Result_DetectorDisconnected,       XIA_SI_DETECTOR_DISCONNECTED },
    { SiToro_Result_CardNotFound,               XIA_SI_CARD_NOT_FOUND },
    { SiToro_Result_DetectorNotFound,           XIA_SI_DETECTOR_NOT_FOUND },
    { SiToro_Result_AlreadyOpen,                XIA_SI_ALREADY_OPEN },
    { SiToro_Result_HandleInvalid,              XIA_SI_HANDLE_INVALID },
    { SiToro_Result_NotOpen,                    XIA_SI_NOT_OPEN },
    { SiToro_Result_InternalError,              XIA_SI_INTERNAL_ERROR },
    { SiToro_Result_BadValue,                   XIA_SI_BAD_VALUE },
    { SiToro_Result_InvalidCardSoftwareVersion, XIA_SI_INVALID_CARD_SOFTWARE_VERSION },
    { SiToro_Result_FeatureNotImplemented,      XIA_SI_FEATURE_NOT_IMPLEMENTED },
    { SiToro_Result_OperationRunning,           XIA_SI_OPERATION_RUNNING },
    { SiToro_Result_NoEnergyData,               XIA_SI_NO_ENERGY_DATA },
    { SiToro_Result_NoCalibrationData,          XIA_SI_NO_CALIBRATION_DATA },
    { SiToro_Result_NullPointerPassed,          XIA_SI_NULL_POINTER_PASSED },
    { SiToro_Result_InvalidMemoryHandling,      XIA_SI_INVALID_MEMORY_HANDLING },
    { SiToro_Result_InvalidCalibrationString,   XIA_SI_INVALID_CALIBRATION_STRING },
    { SiToro_Result_StaleCalibration,           XIA_SI_STALE_CALIBRATION },
    { SiToro_Result_ConfigChangeNotPermitted,   XIA_SI_CONFIG_CHANGE_NOT_PERMITTED },
    { SiToro_Result_BufferTooSmall,             XIA_SI_BUFFER_TOO_SMALL },
    { SiToro_Result_NotFound,                   XIA_SI_NOT_FOUND },
    { SiToro_Result_TooBig,                     XIA_SI_TOO_BIG },
    { SiToro_Result_TooMany,                    XIA_SI_TOO_MANY },
    { SiToro_Result_CardHasBeenReset,           XIA_SI_CARD_HAS_BEEN_RESET },
    { SiToro_Result_FpgaFailure,                XIA_SI_FPGA_FAILURE },
    { SiToro_Result_InvalidFpgaVersion,         XIA_SI_INVALID_FPGA_VERSION },
    { SiToro_Result_HistogramNotRunning,        XIA_SI_HISTOGRAM_NOT_RUNNING },
    { SiToro_Result_ListModeNotRunning,         XIA_SI_LISTMODE_NOT_RUNNING },
    { SiToro_Result_CalibrationNotRunning,      XIA_SI_CALIBRATION_NOT_RUNNING },
    { SiToro_Result_StartupBaselineFailed,      XIA_SI_STARTUP_BASELINE_FAILED },
    { SiToro_Result_HistogramFpgaBadData,       XIA_SI_HISTOGRAM_FPGA_BAD_DATA },
    { SiToro_Result_GenericError,               XIA_SI_GENERIC_ERROR }
};

PSL_SHARED int falconx_PSLInit(const PSLHandlers** psl);
PSL_SHARED int falconx_PSLInit(const PSLHandlers** psl)
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
    handlers.gainCalibrate = psl__GainCalibrate;
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

/** @brief Handle the SiToro API result.
 */
PSL_STATIC int siToroResultToHandel(SiToro_Result result)
{
    int handelError = XIA_SUCCESS;

    if (result != SiToro_Result_Success) {
        size_t i;
        for (i = 0; i < (sizeof(errorTable) / sizeof(struct SiErrorTable)); ++i) {
            if (result == errorTable[i].siResult) {
                handelError = errorTable[i].handelError;
                pslLog(PSL_LOG_ERROR, handelError,
                       "%d: %s.", (int)result, siToro_getErrorMessage(result));
                return handelError;
            }
        }
        handelError = XIA_SI_BAD_ERROR_CODE;
        pslLog(PSL_LOG_ERROR, handelError, "bad SiToro Error code: %d", (int)result);
    }

    return handelError;
}

/**
 * @brief Get the acquisition value reference given the label.
 */
PSL_STATIC AcquisitionValue* psl__GetAcquisition(SiToroDetector* siDetector,
                                                 const char*     name)
{
    int i;
    for (i = 0; i < SI_DET_NUM_OF_ACQ_VALUES; i++) {
        if (STREQ(siDetector->acqValues[i].name, name)) {
            return &siDetector->acqValues[i];
        }
    }
    return NULL;
}

/** @brief Convert the Handel standard double to the specified type.
 */
#define PSL__CONVERT_TO(_t, _T, _R, _m, _M) \
PSL_STATIC int psl__ConvertTo_ ## _t (AcquisitionValue* acq, \
                                      const double      value) { \
    if (acq->value.type != _T) \
        return XIA_UNKNOWN_VALUE; \
    if ((value < (double) _m) || (value > (double) _M))  \
        return XIA_TYPEVAL_OOR; \
    acq->value.ref._R = (_t) value; \
    return XIA_SUCCESS; \
}

PSL__CONVERT_TO(uint16_t,  acqUint16, u16, 0,         USHRT_MAX)
PSL__CONVERT_TO(int16_t,   acqInt16,  i16, SHRT_MIN,  SHRT_MAX)
PSL__CONVERT_TO(uint32_t,  acqUint32, u32, 0,         ULONG_MAX)
PSL__CONVERT_TO(int32_t,   acqInt32,  i32, LONG_MIN,  LONG_MAX)
PSL__CONVERT_TO(uint64_t,  acqUint64, u64, 0,         ULLONG_MAX)
PSL__CONVERT_TO(int64_t,   acqInt64,  i64, LLONG_MIN, LLONG_MAX)
PSL__CONVERT_TO(boolean_t, acqBool,   b,   0,         1)

/** @brief Convert the local endian format.
 */
PSL_STATIC PSL_INLINE int psl__SetAcqValue(AcquisitionValue*   acq,
                                           const double        value)
{
    int status = XIA_SUCCESS;
    if (acq) {
        switch (acq->value.type)
        {
            case acqDouble:
                acq->value.ref.d = value;
                break;
            case acqUint16:
                status = psl__ConvertTo_uint16_t(acq, value);
                break;
            case acqInt16:
                status = psl__ConvertTo_int16_t(acq, value);
                break;
            case acqUint32:
                status = psl__ConvertTo_uint32_t(acq, value);
                break;
            case acqInt32:
                status = psl__ConvertTo_int32_t(acq, value);
                break;
            case acqUint64:
                status = psl__ConvertTo_uint64_t(acq, value);
                break;
            case acqInt64:
                status = psl__ConvertTo_int64_t(acq, value);
                break;
            case acqBool:
                status = psl__ConvertTo_boolean_t(acq, value);
                break;
            case acqString:
                status = XIA_BAD_TYPE;
                break;
        }
    }
    else {
        status = XIA_BAD_VALUE;
    }
    return status;
}

/**
 * @brief Get the value given an index.
 */
PSL_STATIC int psl__GetValueByIndex(Detector*       detector,
                                    SiToroDetector* siDetector,
                                    int             index,
                                    double*         value)
{
    int status = XIA_SUCCESS;
    boolean_t getValue = TRUE_;

    if ((index < 0) || (index >= SI_DET_NUM_OF_ACQ_VALUES)) {
        status = XIA_UNKNOWN_VALUE;
        pslLog(PSL_LOG_ERROR, status,
               "invalid index: %d\n", index);
        return status;
    }

    if ((siDetector->acqValues[index].flags & PSL_ACQ_RUNNING) && !xiaHandelSystemRunning())
        getValue = FALSE_;

    if (getValue) {
        status = siDetector->acqValues[index].handler(detector,
                                                      siDetector,
                                                      NULL,
                                                      &siDetector->acqValues[index],
                                                      value,
                                                      TRUE_);
    }
    else {
        *value = 0.0;
    }

    if (status != XIA_SUCCESS)
        pslLog(PSL_LOG_ERROR, status,
               "Error reading acquisition value handler: %s",
               siDetector->acqValues[index].name);

    return status;
}

/** @brief Update a defaults.
 */
PSL_STATIC int psl__UpdateDefault(SiToroDetector*   siDetector,
                                  XiaDefaults*      defaults,
                                  AcquisitionValue* acq)
{
    if (!PSL_ACQ_FLAG_SET(acq, PSL_ACQ_READ_ONLY)) {
        int status;

        double value = 0.0;

        switch (acq->value.type)
        {
            case acqDouble:
                value = acq->value.ref.d;
                break;
            case acqUint16:
                value = (double) acq->value.ref.u16;
                break;
            case acqInt16:
                value = (double) acq->value.ref.i16;
                break;
            case acqUint32:
                value = (double) acq->value.ref.u32;
                break;
            case acqInt32:
                value = (double) acq->value.ref.i32;
                break;
            case acqUint64:
                value = (double) acq->value.ref.u64;
                break;
            case acqInt64:
                value = (double) acq->value.ref.i64;
                break;
            case acqBool:
                value = (double) acq->value.ref.b;
                break;
            case acqString:
                value = 0;
                break;
        }

        pslLog(PSL_LOG_INFO, "Name:%s = %0.3f", acq->name, value);

        status = pslSetDefault(acq->name, &value, defaults);

        if (status != XIA_SUCCESS) {
            if (status == XIA_NOT_FOUND) {
                boolean_t adding = TRUE_;
                while (adding) {
                    pslLog(PSL_LOG_DEBUG,
                           "Adding default entry %s to %s", acq->name, siDetector->defaultStr);

                    status = xiaAddDefaultItem(siDetector->defaultStr, acq->name,
                                               &value);
                    if (status == XIA_SUCCESS) {
                        adding = FALSE_;
                    } else {
                        if (status == XIA_NO_ALIAS) {
                            pslLog(PSL_LOG_DEBUG,
                                   "Adding defaults %s", siDetector->defaultStr);

                            status = xiaNewDefault(siDetector->defaultStr);
                            if (status != XIA_SUCCESS) {
                                pslLog(PSL_LOG_ERROR, status,
                                       "Error creating new default alias: %s",
                                       siDetector->defaultStr);
                                return status;
                            }
                        }
                        else {
                            pslLog(PSL_LOG_ERROR, status,
                                   "Error adding  default item to %s: %s",
                                   siDetector->defaultStr, acq->name);
                            return status;
                        }
                    }
                }
            }
            else {
                pslLog(PSL_LOG_ERROR, status,
                       "Error setting default: %s", acq->name);
                return status;
            }
        }

        acq->flags |= PSL_ACQ_HAS_DEFAULT;
    }

    return XIA_SUCCESS;
}

/** @brief Check the defaults and if they have are
*/
PSL_STATIC int psl__ReloadDefaults(SiToroDetector* siDetector)
{
    XiaDefaults *defaults = xiaGetDefaultFromDetChan(siDetector->detChan);

    if (defaults) {
        XiaDaqEntry* entry = defaults->entry;

        ASSERT(entry);

        while (entry) {
            int status = XIA_SUCCESS;

            AcquisitionValue* acq = psl__GetAcquisition(siDetector, entry->name);

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

/** @brief Set the SiToro API.
 */
PSL_STATIC void psl__SetupSiToro(void)
{
    if (siToroSetups++ == 0) {
        /* nothing to do here with the new API */ ;
    }
}

/** @brief End the SiToro API.
 */
PSL_STATIC void psl__EndSiToro(void)
{
    if (--siToroSetups == 0) {
      /* nothing to do here with the new API */ ;
    }
}

PSL_STATIC void psl__CheckConnected(SiToroDetector *siDetector)
{
    siBool isOpen;

    ASSERT(siDetector->detector.detector);
    isOpen = siToro_detector_isOpen(siDetector->detector);
}

/** @brief Set the specified acquisition value. Values are always of
 * type double.
 */
PSL_STATIC int psl__SetAcquisitionValues(int        detChan,
                                         Detector*  detector,
                                         Module*    module,
                                         const char *name,
                                         void       *value)
{
    SiToroDetector* siDetector = NULL;

    double dvalue = *((double*) value);

    AcquisitionValue* acq;

    UNUSED(module);

    ASSERT(detector);
    ASSERT(detector->pslData);
    ASSERT(module);
    ASSERT(name);
    ASSERT(value);

    pslLog(PSL_LOG_DEBUG,
           "%s (%d): %s -> %0.3f.",
           detector->alias, detChan,  name, dvalue);

    siDetector = detector->pslData;

    acq = psl__GetAcquisition(siDetector, name);

    if (acq) {
        int status;

        XiaDefaults *defaults;

        if ((acq->flags & PSL_ACQ_READ_ONLY) != 0) {
            status = XIA_NO_MODIFY;
            pslLog(PSL_LOG_ERROR, status,
                   "Attribute is read-only: %s", name);
            return status;
        }

        psl__CheckConnected(siDetector);

        defaults = xiaGetDefaultFromDetChan(detChan);

        status = acq->handler(detector, siDetector, defaults, acq, &dvalue, FALSE_);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error writing in acquisition value handler: %s",
                   acq->name);
            return status;
        }

        status = psl__UpdateDefault(siDetector, defaults, acq);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Error updating default for acquisition value handler: %s",
                   acq->name);
            return status;
        }

        return XIA_SUCCESS;
    }

    pslLog(PSL_LOG_ERROR, XIA_UNKNOWN_VALUE,
           "Unknown acquisition value '%s' for detChan %d.", name, detChan);
    return XIA_UNKNOWN_VALUE;
}


/** @brief Retrieve the current value of the requested acquisition
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
    SiToroDetector* siDetector = NULL;
    AcquisitionValue* acq;

    double dvalue = 0;

    UNUSED(module);

    ASSERT(detector);
    ASSERT(module);
    ASSERT(name);
    ASSERT(value);

    pslLog(PSL_LOG_DEBUG,
           "Detector %s (%d)", detector->alias, detChan);

    defaults = xiaGetDefaultFromDetChan(detChan);

    if (defaults == NULL) {
        xiaLog(XIA_LOG_ERROR, XIA_INCOMPLETE_DEFAULTS, "psl__GetAcquisitionValues",
               "Unable to get the defaults for detChan %d.", detChan);
        return XIA_INCOMPLETE_DEFAULTS;
    }

    status = pslGetDefault(name, value, defaults);

    if ((status != XIA_SUCCESS) && (status != XIA_NOT_FOUND)) {
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the value of '%s' for detChan %d.", name, detChan);
        return status;
    }

    siDetector = detector->pslData;

    acq = psl__GetAcquisition(siDetector, name);

    if (!acq) {
        status = XIA_NOT_FOUND;
        xiaLog(XIA_LOG_ERROR, status, "psl__GetAcquisitionValues",
               "Unable to get the ACQ value '%s' for detChan %d.", name, detChan);
        return status;
    }

    psl__CheckConnected(siDetector);

    status = acq->handler(detector, siDetector, defaults, acq, &dvalue, TRUE_);

    if (status != XIA_SUCCESS)
    {
        pslLog(PSL_LOG_ERROR, status,
               "Error reading in acquisition value handler: %s",
               acq->name);
        return status;
    }

    *((double*) value) = dvalue;

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(analog_offset)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(analog_offset);

    if (read) {
        int16_t offset = 0;
        siResult = siToro_detector_getAnalogOffset(siDetector->detector, &offset);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the analog offset");
            return status;
        }
        acq->value.ref.i16 = offset;
        *value = (double) acq->value.ref.i16;
    }
    else {
        status = psl__ConvertTo_int16_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the analog offset: %f", *value);
            return status;
        }

        siResult = siToro_detector_setAnalogOffset(siDetector->detector,
                                                   acq->value.ref.i16);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the analog offset: %i",
                   acq->value.ref.i16);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(analog_gain)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(analog_gain);

    if (read) {
        uint16_t gain = 0;
        siResult = siToro_detector_getAnalogGain(siDetector->detector, &gain);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the analog gain");
            return status;
        }
        acq->value.ref.u16 = gain;
        *value = (double) acq->value.ref.u16;
    }
    else {
        status = psl__ConvertTo_uint16_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the analog gain: %f", *value);
            return status;
        }

        siResult = siToro_detector_setAnalogGain(siDetector->detector,
                                                 acq->value.ref.u16);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the analog gain: %u",
                   acq->value.ref.u16);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(analog_gain_boost)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(analog_gain_boost);

    if (read) {
        siBool boost = SIBOOL_FALSE;
        siResult = siToro_detector_getAnalogGainBoost(siDetector->detector, &boost);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the analog gain boost setting");
            return status;
        }
        acq->value.ref.b = boost ? 1 : 0;
        *value = (double) acq->value.ref.b;
    }
    else {
        status = psl__ConvertTo_boolean_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the analog gain boost setting: %f", *value);
            return status;
        }

        siResult = siToro_detector_setAnalogGainBoost(siDetector->detector,
                                                      acq->value.ref.b);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the analog gain boost setting: %d",
                   acq->value.ref.b);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(invert_input)
{
    int status;

    SiToro_Result siResult;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(invert_input);

    if (read) {
        siBool on = SIBOOL_FALSE;
        siResult = siToro_detector_getAnalogInvert(siDetector->detector, &on);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the analog invert setting");
            return status;
        }
        acq->value.ref.b = on ? 1 : 0;
        *value = (double) acq->value.ref.b;
    }
    else {
        status = psl__ConvertTo_boolean_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the analog invert setting: %f", *value);
            return status;
        }

        siResult = siToro_detector_setAnalogInvert(siDetector->detector,
                                                   acq->value.ref.b);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the analog invert setting: %d",
                   acq->value.ref.b);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(detector_polarity)
{
    ACQ_HANDLER_LOG(detector_polarity);
    return ACQ_HANDLER(invert_input)(detector, siDetector, defaults, acq, value, read);
}

/*
 * Note in SiToro 2.5.0 AnalogEnabled is backwards, so our disable_input here
 * does in fact for now map straight to it with no complement.
 */
ACQ_HANDLER_DECL(disable_input)
{
    int status;

    SiToro_Result siResult;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(disable_input);

    if (read) {
        siBool on = SIBOOL_FALSE;
        siResult = siToro_detector_getAnalogEnabled(siDetector->detector, &on);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the analog disable setting");
            return status;
        }
        acq->value.ref.b = on ? 0 : 1;
        *value = (double) acq->value.ref.b;
    }
    else {
        siBool on = SIBOOL_FALSE;
        status = psl__ConvertTo_boolean_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the analog disable setting: %f", *value);
            return status;
        }

        on = acq->value.ref.b ? 0 : 1;
        siResult = siToro_detector_setAnalogEnabled(siDetector->detector, on);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the analog disable setting: %d",
                   acq->value.ref.b);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(analog_discharge)
{
    int status;

    SiToro_Result siResult;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(analog_discharge);

    if (read) {
        siBool on = SIBOOL_FALSE;
        siResult = siToro_detector_getAnalogDischarge(siDetector->detector, &on);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the analog discharge setting");
            return status;
        }
        acq->value.ref.b = on ? 1 : 0;
        *value = (double) acq->value.ref.b;
    }
    else {
        status = psl__ConvertTo_boolean_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the analog discharge setting: %f", *value);
            return status;
        }

        siResult = siToro_detector_setAnalogDischarge(siDetector->detector,
                                                      acq->value.ref.b);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the analog discharge setting: %d",
                   acq->value.ref.b);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(analog_discharge_threshold)
{
    int status;

    SiToro_Result siResult;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(analog_discharge_threshold);

    if (read) {
        uint16_t threshold = 0;
        siResult = siToro_detector_getAnalogDischargeThreshold(siDetector->detector,
                                                               &threshold);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the analog threshold");
            return status;
        }
        acq->value.ref.u16 = threshold;
        *value = (double) acq->value.ref.u16;
    }
    else {
        status = psl__ConvertTo_uint16_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the analog threshold: %f", *value);
            return status;
        }

        siResult = siToro_detector_setAnalogDischargeThreshold(siDetector->detector,
                                                               acq->value.ref.u16);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the analog threshold: %u",
                   acq->value.ref.u16);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(analog_discharge_period)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(analog_discharge_period);

    if (read) {
        uint16_t samples = 0;
        siResult = siToro_detector_getAnalogDischargePeriod(siDetector->detector,
                                                            &samples);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the analog samples");
            return status;
        }
        acq->value.ref.u16 = samples;
        *value = (double) acq->value.ref.u16;
    }
    else {
        status = psl__ConvertTo_uint16_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the analog samples: %f", *value);
            return status;
        }

        siResult = siToro_detector_setAnalogDischargePeriod(siDetector->detector,
                                                            acq->value.ref.u16);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the analog samples: %u",
                   acq->value.ref.u16);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(sample_rate)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(sample_rate);

    if (read) {
        double rateHz = 0;
        siResult = siToro_detector_getSampleRate(siDetector->detector,
                                                 &rateHz);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the sample rate");
            return status;
        }
        acq->value.ref.d = rateHz;
        *value = acq->value.ref.d;
    }
    else {
        status = XIA_READ_ONLY;
        pslLog(PSL_LOG_ERROR, status,
               "Unable to set the sample rate, read only");
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(dc_offset)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(dc_offset);

    if (read) {
        double offset = 0;
        siResult = siToro_detector_getDcOffset(siDetector->detector, &offset);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the DC offset");
            return status;
        }
        acq->value.ref.d = offset;
        *value = acq->value.ref.d;
    }
    else {
        acq->value.ref.d = *value;
        siResult = siToro_detector_setDcOffset(siDetector->detector,
                                               acq->value.ref.d);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the DC offset: %f",
                   acq->value.ref.d);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(dc_tracking_mode)
{
    int status;

    SiToro_Result siResult;

    SiToro_DcTrackingMode mode = SiToro_DcTrackingMode_Off;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(dc_tracking_mode);

    if (read) {
        siResult = siToro_detector_getDcTrackingMode(siDetector->detector,
                                                     &mode);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the analog threshold");
            return status;
        }
        switch (mode) {
            case SiToro_DcTrackingMode_Off:
                acq->value.ref.u32 = 0;
                break;
            case SiToro_DcTrackingMode_Slow:
                acq->value.ref.u32 = 1;
                break;
            case SiToro_DcTrackingMode_Medium:
                acq->value.ref.u32 = 2;
                break;
            case SiToro_DcTrackingMode_Fast:
                acq->value.ref.u32 = 3;
                break;
            default:
                status = XIA_INVALID_VALUE;
                pslLog(PSL_LOG_ERROR, status,
                       "The DC tracking mode is invalid: %d", mode);
                return status;
        }
        *value = (double) acq->value.ref.u32;
    }
    else {
        switch ((int) *value) {
            case 0:
                mode = SiToro_DcTrackingMode_Off;
                acq->value.ref.u32 = 0;
                break;
            case 1:
                mode = SiToro_DcTrackingMode_Slow;
                acq->value.ref.u32 = 1;
                break;
            case 2:
                mode = SiToro_DcTrackingMode_Medium;
                acq->value.ref.u32 = 2;
                break;
            case 3:
                mode = SiToro_DcTrackingMode_Fast;
                acq->value.ref.u32 = 3;
                break;
            default:
                status = XIA_INVALID_VALUE;
                pslLog(PSL_LOG_ERROR, status,
                       "Unable to convert the DC tracking mode: %f", *value);
                return status;
        }

        siResult = siToro_detector_setDcTrackingMode(siDetector->detector,
                                                     mode);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the DC tracking mode: %u",
                   acq->value.ref.u32);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(operating_mode)
{
    int status;

    SiToro_Result siResult;

    SiToro_OperatingMode mode = SiToro_OperatingMode_OptimalResolution;
    uint32_t target = 0;

    AcquisitionValue* dependent_acq =
        psl__GetAcquisition(siDetector, "operating_mode_target");

    UNUSED(detector);
    UNUSED(defaults);

    ASSERT(dependent_acq);

    ACQ_HANDLER_LOG(operating_mode);

    if (read) {
        siResult = siToro_detector_getOperatingMode(siDetector->detector,
                                                    &mode,
                                                    &target);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the operating mode");
            return status;
        }
        switch (mode) {
            case SiToro_OperatingMode_OptimalResolution:
                acq->value.ref.u32 = 0;
                break;
            case SiToro_OperatingMode_ConstantResolution:
                acq->value.ref.u32 = 1;
                break;
            default:
                status = XIA_INVALID_VALUE;
                pslLog(PSL_LOG_ERROR, status,
                       "The operating mode is invalid: %d", mode);
                return status;
        }
        *value = (double) acq->value.ref.u32;
        if (dependent_acq) {
            dependent_acq->value.ref.u32 = target;
        }
    }
    else {
        switch ((int) *value) {
            case 0:
                mode = SiToro_OperatingMode_OptimalResolution;
                acq->value.ref.u32 = 0;
                break;
            case 1:
                mode = SiToro_OperatingMode_ConstantResolution;
                acq->value.ref.u32 = 1;
                break;
            default:
                status = XIA_INVALID_VALUE;
                pslLog(PSL_LOG_ERROR, status,
                       "Unable to convert the operating mode: %f", *value);
                return status;
        }

        if (dependent_acq) {
            target = dependent_acq->value.ref.u32;
        }

        siResult = siToro_detector_setOperatingMode(siDetector->detector,
                                                    mode,
                                                    target);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the operating mode: %u (%u)",
                   acq->value.ref.u32, target);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(operating_mode_target)
{
    int status;

    SiToro_Result siResult;

    SiToro_OperatingMode mode = SiToro_OperatingMode_OptimalResolution;
    uint32_t target = 0;

    AcquisitionValue* dependent_acq =
        psl__GetAcquisition(siDetector, "operating_mode");

    ASSERT(dependent_acq);

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(operating_mode_target);

    if (read) {
        siResult = siToro_detector_getOperatingMode(siDetector->detector,
                                                    &mode,
                                                    &target);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the operating mode");
            return status;
        }
        switch (mode) {
            case SiToro_OperatingMode_OptimalResolution:
                dependent_acq->value.ref.u32 = 0;
                break;
            case SiToro_OperatingMode_ConstantResolution:
                dependent_acq->value.ref.u32 = 1;
                break;
            default:
                status = XIA_INVALID_VALUE;
                pslLog(PSL_LOG_ERROR, status,
                       "The operating mode is invalid: %d", mode);
                return status;
        }
        acq->value.ref.u32 = target;
        *value = (double) acq->value.ref.u32;
    }
    else {
        if (dependent_acq) {
            switch (dependent_acq->value.ref.u32) {
                case 0:
                    mode = SiToro_OperatingMode_OptimalResolution;
                    break;
                case 1:
                    mode = SiToro_OperatingMode_ConstantResolution;
                    break;
                default:
                    status = XIA_INVALID_VALUE;
                    pslLog(PSL_LOG_ERROR, status,
                           "Unable to convert the operating mode: %d", mode);
                    return status;
            }
        }

        status = psl__ConvertTo_uint32_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the operating mode target: %f", *value);
            return status;
        }

        siResult = siToro_detector_setOperatingMode(siDetector->detector,
                                                    mode,
                                                    acq->value.ref.u32);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the operating mode target: %u (%d)",
                   acq->value.ref.u32, mode);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(reset_blanking_enable)
{
    int status;

    SiToro_Result siResult;

    siBool enable = SIBOOL_FALSE;
    double threshold = 0;
    uint16_t presamples = 0;
    uint16_t postsamples = 0;

    AcquisitionValue* dep_rb_threshold;
    AcquisitionValue* dep_rb_presamples;
    AcquisitionValue* dep_rb_postsamples;

    dep_rb_threshold = psl__GetAcquisition(siDetector, "reset_blanking_threshold");
    dep_rb_presamples = psl__GetAcquisition(siDetector, "reset_blanking_presamples");
    dep_rb_postsamples = psl__GetAcquisition(siDetector, "reset_blanking_postsamples");

    UNUSED(defaults);
    UNUSED(detector);

    ASSERT(dep_rb_threshold);
    ASSERT(dep_rb_presamples);
    ASSERT(dep_rb_postsamples);

    ACQ_HANDLER_LOG(reset_blanking_enable);

    if (read) {
        siResult = siToro_detector_getResetBlanking(siDetector->detector,
                                                    &enable,
                                                    &threshold,
                                                    &presamples,
                                                    &postsamples);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the reset blanking enable setting");
            return status;
        }
        acq->value.ref.b = enable ? 1 : 0;
        *value = (double) acq->value.ref.b;
        if (dep_rb_threshold)
            dep_rb_threshold->value.ref.d = threshold;
        if (dep_rb_presamples)
            dep_rb_presamples->value.ref.u16 = presamples;
        if (dep_rb_postsamples)
            dep_rb_postsamples->value.ref.u16 = postsamples;
    }
    else {
        if (dep_rb_threshold)
            threshold = dep_rb_threshold->value.ref.d;
        if (dep_rb_presamples)
            presamples = dep_rb_presamples->value.ref.u16;
        if (dep_rb_postsamples)
            postsamples = dep_rb_postsamples->value.ref.u16;

        status = psl__ConvertTo_boolean_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the reset blanking enable setting: %f", *value);
            return status;
        }

        siResult = siToro_detector_setResetBlanking(siDetector->detector,
                                                    acq->value.ref.b,
                                                    threshold,
                                                    presamples,
                                                    postsamples);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the reset blanking enable setting: %d",
                   acq->value.ref.b);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(reset_blanking_threshold)
{
    int status;

    SiToro_Result siResult;

    siBool enable = SIBOOL_FALSE;
    double threshold = 0;
    uint16_t presamples = 0;
    uint16_t postsamples = 0;

    AcquisitionValue* dep_rb_enable;
    AcquisitionValue* dep_rb_presamples;
    AcquisitionValue* dep_rb_postsamples;

    dep_rb_enable = psl__GetAcquisition(siDetector, "reset_blanking_enable");
    dep_rb_presamples = psl__GetAcquisition(siDetector, "reset_blanking_presamples");
    dep_rb_postsamples = psl__GetAcquisition(siDetector, "reset_blanking_postsamples");

    UNUSED(defaults);
    UNUSED(detector);

    ASSERT(dep_rb_enable);
    ASSERT(dep_rb_presamples);
    ASSERT(dep_rb_postsamples);

    ACQ_HANDLER_LOG(reset_blanking_threshold);

    if (read) {
        siResult = siToro_detector_getResetBlanking(siDetector->detector,
                                                    &enable,
                                                    &threshold,
                                                    &presamples,
                                                    &postsamples);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the reset blanking threshold");
            return status;
        }
        acq->value.ref.d = threshold;
        *value = acq->value.ref.d;
        if (dep_rb_enable)
            dep_rb_enable->value.ref.b = (boolean_t) enable;
        if (dep_rb_presamples)
            dep_rb_presamples->value.ref.u16 = presamples;
        if (dep_rb_postsamples)
            dep_rb_postsamples->value.ref.u16 = postsamples;
    }
    else {
        if (dep_rb_enable)
            enable = dep_rb_enable->value.ref.b;
        if (dep_rb_presamples)
            presamples = dep_rb_presamples->value.ref.u16;
        if (dep_rb_postsamples)
            postsamples = dep_rb_postsamples->value.ref.u16;

        acq->value.ref.d = *value;

        siResult = siToro_detector_setResetBlanking(siDetector->detector,
                                                    enable,
                                                    acq->value.ref.d,
                                                    presamples,
                                                    postsamples);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the reset blanking threshold: %f",
                   acq->value.ref.d);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(reset_blanking_presamples)
{
    int status;

    SiToro_Result siResult;

    siBool enable = SIBOOL_FALSE;
    double threshold = 0;
    uint16_t presamples = 0;
    uint16_t postsamples = 0;

    AcquisitionValue* dep_rb_enable;
    AcquisitionValue* dep_rb_threshold;
    AcquisitionValue* dep_rb_postsamples;

    UNUSED(defaults);
    UNUSED(detector);

    dep_rb_enable = psl__GetAcquisition(siDetector, "reset_blanking_enable");
    dep_rb_threshold = psl__GetAcquisition(siDetector, "reset_blanking_threshold");
    dep_rb_postsamples = psl__GetAcquisition(siDetector, "reset_blanking_postsamples");

    ASSERT(dep_rb_enable);
    ASSERT(dep_rb_threshold);
    ASSERT(dep_rb_postsamples);

    ACQ_HANDLER_LOG(reset_blanking_presamples);

    if (read) {
        siResult = siToro_detector_getResetBlanking(siDetector->detector,
                                                    &enable,
                                                    &threshold,
                                                    &presamples,
                                                    &postsamples);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the reset blanking presamples");
            return status;
        }
        acq->value.ref.u16 = presamples;
        *value = (double) acq->value.ref.u16;
        if (dep_rb_enable)
            dep_rb_enable->value.ref.b = (boolean_t) enable;
        if (dep_rb_threshold)
            dep_rb_threshold->value.ref.d = threshold;
        if (dep_rb_postsamples)
            dep_rb_postsamples->value.ref.u16 = postsamples;
    }
    else {
        if (dep_rb_enable)
            enable = dep_rb_enable->value.ref.b;
        if (dep_rb_threshold)
            threshold = dep_rb_threshold->value.ref.d;
        if (dep_rb_postsamples)
            postsamples = dep_rb_postsamples->value.ref.u16;

        status = psl__ConvertTo_uint16_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the reset blanking presamples: %f", *value);
            return status;
        }

        siResult = siToro_detector_setResetBlanking(siDetector->detector,
                                                    enable,
                                                    threshold,
                                                    acq->value.ref.u16,
                                                    postsamples);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the reset blanking presamples: %u",
                   acq->value.ref.u16);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(reset_blanking_postsamples)
{
    int status;

    SiToro_Result siResult;

    siBool enable = SIBOOL_FALSE;
    double threshold = 0;
    uint16_t presamples = 0;
    uint16_t postsamples = 0;

    AcquisitionValue* dep_rb_enable;
    AcquisitionValue* dep_rb_threshold;
    AcquisitionValue* dep_rb_presamples;

    UNUSED(defaults);
    UNUSED(detector);

    dep_rb_enable = psl__GetAcquisition(siDetector, "reset_blanking_enable");
    dep_rb_threshold = psl__GetAcquisition(siDetector, "reset_blanking_threshold");
    dep_rb_presamples = psl__GetAcquisition(siDetector, "reset_blanking_presamples");

    ASSERT(dep_rb_enable);
    ASSERT(dep_rb_threshold);
    ASSERT(dep_rb_presamples);

    ACQ_HANDLER_LOG(reset_blanking_postsamples);

    if (read) {
        siResult = siToro_detector_getResetBlanking(siDetector->detector,
                                                    &enable,
                                                    &threshold,
                                                    &presamples,
                                                    &postsamples);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the reset blanking presamples");
            return status;
        }
        acq->value.ref.u16 = postsamples;
        *value = (double) acq->value.ref.u16;
        if (dep_rb_enable)
            dep_rb_enable->value.ref.b = (boolean_t) enable;
        if (dep_rb_threshold)
            dep_rb_threshold->value.ref.d = threshold;
        if (dep_rb_presamples)
            dep_rb_presamples->value.ref.u16 = presamples;
    }
    else {
        if (dep_rb_enable)
            enable = dep_rb_presamples->value.ref.b;
        if (dep_rb_threshold)
            threshold = dep_rb_threshold->value.ref.d;
        if (dep_rb_presamples)
            presamples = dep_rb_presamples->value.ref.u16;

        status = psl__ConvertTo_uint16_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the reset blanking postsamples: %f", *value);
            return status;
        }

        siResult = siToro_detector_setResetBlanking(siDetector->detector,
                                                    enable,
                                                    threshold,
                                                    presamples,
                                                    acq->value.ref.u16);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the reset blanking postsamples: %u",
                   acq->value.ref.u16);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(min_pulse_pair_separation)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(min_pulse_pair_separation);

    if (read) {
        uint32_t samples = 0;
        siResult = siToro_detector_getMinPulsePairSeparation(siDetector->detector,
                                                             &samples);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the min pulse pair separation");
            return status;
        }
        acq->value.ref.u32 = samples;
        *value = (double) acq->value.ref.u32;
    }
    else {
        status = psl__ConvertTo_uint32_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the min pulse pair separation: %f", *value);
            return status;
        }

        siResult = siToro_detector_setMinPulsePairSeparation(siDetector->detector,
                                                             acq->value.ref.u32);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the min pulse pair separation: %u",
                   acq->value.ref.u32);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(detection_threshold)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(detection_threshold);

    if (read) {
        double threshold = 0;
        siResult = siToro_detector_getDetectionThreshold(siDetector->detector, &threshold);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the detection threshold");
            return status;
        }
        acq->value.ref.d = threshold;
        *value = acq->value.ref.d;
    }
    else {
        acq->value.ref.d = *value;
        siResult = siToro_detector_setDetectionThreshold(siDetector->detector,
                                                         acq->value.ref.d);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the detection threshold: %f",
                   acq->value.ref.d);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(validator_threshold_fixed)
{
    int status;

    SiToro_Result siResult;

    double fixed = 0;
    double proport = 0;

    AcquisitionValue* dep_proport;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(validator_threshold_fixed);

    dep_proport = psl__GetAcquisition(siDetector, "validator_threshold_proport");
    ASSERT(dep_proport);

    if (read) {
        siResult = siToro_detector_getValidatorThresholds(siDetector->detector,
                                                          &fixed,
                                                          &proport);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the validator threshold (fixed)");
            return status;
        }
        if (dep_proport)
            dep_proport->value.ref.d = proport;
        acq->value.ref.d = fixed;
        *value = acq->value.ref.d;
    }
    else {
        if (dep_proport)
            proport = dep_proport->value.ref.d;
        acq->value.ref.d = *value;
        siResult = siToro_detector_setValidatorThresholds(siDetector->detector,
                                                          acq->value.ref.d,
                                                          proport);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the validation threadhold (fixed): %f",
                   acq->value.ref.d);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(validator_threshold_proport)
{
    int status;

    SiToro_Result siResult;

    double fixed = 0;
    double proport = 0;

    AcquisitionValue* dep_fixed;

    UNUSED(defaults);
    UNUSED(detector);

    dep_fixed = psl__GetAcquisition(siDetector, "validator_threshold_fixed");

    ASSERT(dep_fixed);

    ACQ_HANDLER_LOG(validator_threshold_proport);

    if (read) {
        siResult = siToro_detector_getValidatorThresholds(siDetector->detector,
                                                          &fixed,
                                                          &proport);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the validator threshold (proportional)");
            return status;
        }
        if (dep_fixed)
            dep_fixed->value.ref.d = fixed;
        acq->value.ref.d = proport;
        *value = acq->value.ref.d;
    }
    else {
        if (dep_fixed)
            fixed = dep_fixed->value.ref.d;
        acq->value.ref.d = *value;
        siResult = siToro_detector_setValidatorThresholds(siDetector->detector,
                                                          fixed,
                                                          acq->value.ref.d);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the validation threadhold (proportional): %f",
                   acq->value.ref.d);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(pulse_scale_factor)
{
    int status;

    SiToro_Result siResult;

    double factor = 0;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(pulse_scale_factor);

    if (read) {
        siResult = siToro_detector_getPulseScaleFactor(siDetector->detector, &factor);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the pulse scale factor");
            return status;
        }
        acq->value.ref.d = factor;
        *value = acq->value.ref.d;
    }
    else {
        acq->value.ref.d = *value;
        siResult = siToro_detector_setPulseScaleFactor(siDetector->detector,
                                                       acq->value.ref.d);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the pulse scale factor: %f",
                   acq->value.ref.d);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(cal_noise_floor)
{
    int status;

    SiToro_Result siResult;

    double noise_floor = 0;
    double min_pulse_amp = 0;
    double max_pulse_amp = 0;

    AcquisitionValue* dep_min_pulse_amp;
    AcquisitionValue* dep_max_pulse_amp;

    boolean_t do_read = FALSE_;

    dep_min_pulse_amp = psl__GetAcquisition(siDetector, "cal_min_pulse_amp");
    dep_max_pulse_amp = psl__GetAcquisition(siDetector, "cal_max_pulse_amp");

    UNUSED(detector);

    ASSERT(dep_min_pulse_amp);
    ASSERT(dep_max_pulse_amp);

    ACQ_HANDLER_LOG(cal_noise_floor);

    if (read ||
         PSL_ACQ_FLAG_SET(dep_min_pulse_amp, PSL_ACQ_HAS_DEFAULT) ||
         PSL_ACQ_FLAG_SET(dep_max_pulse_amp, PSL_ACQ_HAS_DEFAULT)) {
        do_read = TRUE_;
    }

    if (do_read) {
        pslLog(PSL_LOG_DEBUG,
               "Cal noise floor: reading defaults (%d or %d or %d)",
               read,
               PSL_ACQ_FLAG_SET(dep_min_pulse_amp, PSL_ACQ_HAS_DEFAULT),
               PSL_ACQ_FLAG_SET(dep_max_pulse_amp, PSL_ACQ_HAS_DEFAULT));
        siResult = siToro_detector_getCalibrationThresholds(siDetector->detector,
                                                            &noise_floor,
                                                            &min_pulse_amp,
                                                            &max_pulse_amp);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the calibration noise floor");
            return status;
        }
        acq->value.ref.d = noise_floor;
        if (read)
            *value = (double) acq->value.ref.d;
        if (dep_min_pulse_amp)
            dep_min_pulse_amp->value.ref.d = min_pulse_amp;
        if (dep_max_pulse_amp)
            dep_max_pulse_amp->value.ref.d = max_pulse_amp;
    }

    if (!read) {
        if (dep_min_pulse_amp)
            min_pulse_amp = dep_min_pulse_amp->value.ref.d;
        if (dep_max_pulse_amp)
            max_pulse_amp = dep_max_pulse_amp->value.ref.d;
        acq->value.ref.d = *value;

        pslLog(PSL_LOG_DEBUG,
               "Cal noise floor: %f (%f, %f)",
               acq->value.ref.d, min_pulse_amp, max_pulse_amp);

        siResult = siToro_detector_setCalibrationThresholds(siDetector->detector,
                                                            acq->value.ref.d,
                                                            min_pulse_amp,
                                                            max_pulse_amp);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
             pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the calibration noise floor: %f",
                   acq->value.ref.d);
            return status;
        }

        if (dep_min_pulse_amp) {
            status = psl__UpdateDefault(siDetector, defaults, dep_min_pulse_amp);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Unable to set default for: min_pulse_amp");
                return status;
            }
        }

        if (dep_max_pulse_amp) {
            status = psl__UpdateDefault(siDetector, defaults, dep_max_pulse_amp);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Unable to set default for: max_pulse_amp");
                return status;
            }
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(cal_min_pulse_amp)
{
    int status;

    SiToro_Result siResult;

    double noise_floor = 0;
    double min_pulse_amp = 0;
    double max_pulse_amp = 0;

    AcquisitionValue* dep_noise_floor;
    AcquisitionValue* dep_max_pulse_amp;

    UNUSED(defaults);
    UNUSED(detector);

    dep_noise_floor = psl__GetAcquisition(siDetector, "cal_noise_floor");
    dep_max_pulse_amp = psl__GetAcquisition(siDetector, "cal_max_pulse_amp");

    ASSERT(dep_noise_floor);
    ASSERT(dep_max_pulse_amp);

    ACQ_HANDLER_LOG(cal_min_pulse_amp);

    if (read) {
        siResult = siToro_detector_getCalibrationThresholds(siDetector->detector,
                                                            &noise_floor,
                                                            &min_pulse_amp,
                                                            &max_pulse_amp);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the calibration minimum pulse amplitude");
            return status;
        }
        acq->value.ref.d = min_pulse_amp;
        *value = (double) acq->value.ref.d;
        if (dep_noise_floor)
            dep_noise_floor->value.ref.d = noise_floor;
        if (dep_max_pulse_amp)
            dep_max_pulse_amp->value.ref.d = max_pulse_amp;
    }
    else {
        if (dep_noise_floor)
            noise_floor = dep_noise_floor->value.ref.d;
        if (dep_max_pulse_amp)
            max_pulse_amp = dep_max_pulse_amp->value.ref.d;
        acq->value.ref.d = *value;

        siResult = siToro_detector_setCalibrationThresholds(siDetector->detector,
                                                            noise_floor,
                                                            acq->value.ref.d,
                                                            max_pulse_amp);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the calibration minimum pulse amplitude: %f",
                   acq->value.ref.d);
            return status;
        }

        /* We should set update defaults for the noise_floor and max_pulse_amp here,
         * except that they're being deprecated. */
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(cal_max_pulse_amp)
{
    int status;

    SiToro_Result siResult;

    double noise_floor = 0;
    double min_pulse_amp = 0;
    double max_pulse_amp = 0;

    AcquisitionValue* dep_noise_floor;
    AcquisitionValue* dep_min_pulse_amp;

    UNUSED(defaults);
    UNUSED(detector);

    dep_noise_floor = psl__GetAcquisition(siDetector, "cal_noise_floor");
    dep_min_pulse_amp = psl__GetAcquisition(siDetector, "cal_min_pulse_amp");

    ASSERT(dep_noise_floor);
    ASSERT(dep_min_pulse_amp);

    ACQ_HANDLER_LOG(cal_max_pulse_amp);

    if (read) {
        siResult = siToro_detector_getCalibrationThresholds(siDetector->detector,
                                                            &noise_floor,
                                                            &min_pulse_amp,
                                                            &max_pulse_amp);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the calibration maximum pulse amplitude");
            return status;
        }
        acq->value.ref.d = max_pulse_amp;
        *value = (double) acq->value.ref.d;
        if (dep_noise_floor)
            dep_noise_floor->value.ref.d = noise_floor;
        if (dep_min_pulse_amp)
            dep_min_pulse_amp->value.ref.d = min_pulse_amp;
    }
    else {
        if (dep_noise_floor)
            noise_floor = dep_noise_floor->value.ref.d;
        if (dep_min_pulse_amp)
            min_pulse_amp = dep_min_pulse_amp->value.ref.d;
        acq->value.ref.d = *value;

        siResult = siToro_detector_setCalibrationThresholds(siDetector->detector,
                                                            noise_floor,
                                                            min_pulse_amp,
                                                            acq->value.ref.d);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the calibration maximum pulse amplitude: %f",
                   acq->value.ref.d);
            return status;
        }

        /* We should set update defaults for the noise_floor and min_pulse_amp here,
         * except that they're being deprecated. */
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(cal_source_type)
{
    int status;

    SiToro_Result siResult;

    SiToro_SourceType sourceType = SiToro_SourceType_LowEnergy;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(cal_source_type);

    if (read) {
        siResult = siToro_detector_getSourceType(siDetector->detector,
                                                 &sourceType);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the source type");
            return status;
        }
        switch (sourceType) {
            case SiToro_SourceType_LowEnergy:
                acq->value.ref.u32 = 0;
                break;
            case SiToro_SourceType_LowRate:
                acq->value.ref.u32 = 1;
                break;
            case SiToro_SourceType_MidRate:
                acq->value.ref.u32 = 2;
                break;
            case SiToro_SourceType_HighRate:
                acq->value.ref.u32 = 3;
                break;
            default:
                status = XIA_INVALID_VALUE;
                pslLog(PSL_LOG_ERROR, status,
                       "The source type is invalid: %d", sourceType);
                return status;
        }
        *value = (double) acq->value.ref.u32;
    }
    else {
        switch ((int) *value) {
            case 0:
                sourceType = SiToro_SourceType_LowEnergy;
                acq->value.ref.u32 = 0;
                break;
            case 1:
                sourceType = SiToro_SourceType_LowEnergy;
                acq->value.ref.u32 = 1;
                break;
            case 2:
                sourceType = SiToro_SourceType_MidRate;
                acq->value.ref.u32 = 2;
                break;
            case 3:
                sourceType = SiToro_SourceType_HighRate;
                acq->value.ref.u32 = 3;
                break;
            default:
                status = XIA_INVALID_VALUE;
                pslLog(PSL_LOG_ERROR, status,
                       "Unable to convert the source type: %f", *value);
                return status;
        }

        siResult = siToro_detector_setSourceType(siDetector->detector,
                                                 sourceType);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the source type: %u",
                   acq->value.ref.u32);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(cal_pulses_needed)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(cal_pulses_needed);

    if (read) {
        uint32_t pulses = 0;
        siResult = siToro_detector_getCalibrationPulsesNeeded(siDetector->detector,
                                                             &pulses);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the calibration pulses needed");
            return status;
        }
        acq->value.ref.u32 = pulses;
        *value = (double) acq->value.ref.u32;
    }
    else {
        status = psl__ConvertTo_uint32_t(acq, *value);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to convert the calibration pulses needed: %f", *value);
            return status;
        }

        siResult = siToro_detector_setCalibrationPulsesNeeded(siDetector->detector,
                                                             acq->value.ref.u32);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the calibration pulses needed: %u",
                   acq->value.ref.u32);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(cal_filter_cutoff)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(cal_filter_cutoff);

    if (read) {
        double cutoff = 0;
        siResult = siToro_detector_getFilterCutoff(siDetector->detector, &cutoff);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the filter cutoff");
            return status;
        }
        acq->value.ref.d = cutoff;
        *value = acq->value.ref.d;
    }
    else {
        acq->value.ref.d = *value;
        siResult = siToro_detector_setFilterCutoff(siDetector->detector,
                                                   acq->value.ref.d);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the filter cutoff: %f",
                   acq->value.ref.d);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(cal_est_count_rate)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(cal_est_count_rate);

    if (read) {
        double count_rate = 0;
        siResult = siToro_detector_getCalibrationEstimatedCountRate(siDetector->detector,
                                                                    &count_rate);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the calibration count rate");
            return status;
        }
        acq->value.ref.d = count_rate;
        *value = acq->value.ref.d;
    }
    else {
        status = XIA_READ_ONLY;
        pslLog(PSL_LOG_ERROR, status,
               "Variable is read only");
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(hist_bin_count)
{
    int status;

    SiToro_Result siResult;

    SiToro_HistogramBinSize bins = SiToro_HistogramBinSize_8192;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(hist_bin_count);

    if (read) {
        siResult = siToro_detector_getNumHistogramBins(siDetector->detector,
                                                       &bins);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram bin count");
            return status;
        }
        switch (bins) {
            case SiToro_HistogramBinSize_1024:
                acq->value.ref.u32 = 1024;
                break;
            case SiToro_HistogramBinSize_2048:
                acq->value.ref.u32 = 2048;
                break;
            case SiToro_HistogramBinSize_4096:
                acq->value.ref.u32 = 4096;
                break;
            case SiToro_HistogramBinSize_8192:
                acq->value.ref.u32 = 8192;
                break;
            default:
                status = XIA_INVALID_VALUE;
                pslLog(PSL_LOG_ERROR, status,
                       "The histogram bin count is invalid: %d", bins);
                return status;
        }
        *value = (double) acq->value.ref.u32;
    }
    else {
        switch ((int) *value) {
            case 1024:
                bins = SiToro_HistogramBinSize_1024;
                acq->value.ref.u32 = 1024;
                break;
            case 2048:
                bins = SiToro_HistogramBinSize_2048;
                acq->value.ref.u32 = 2048;
                break;
            case 4096:
                bins = SiToro_HistogramBinSize_4096;
                acq->value.ref.u32 = 4096;
                break;
            case 8192:
            case 0:
                bins = SiToro_HistogramBinSize_8192;
                acq->value.ref.u32 = 8192;
                break;
            default:
                status = XIA_INVALID_VALUE;
                pslLog(PSL_LOG_ERROR, status,
                       "Unable to convert the histogram bin count: %f", *value);
                return status;
        }

        siResult = siToro_detector_setNumHistogramBins(siDetector->detector,
                                                       bins);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to set the hist bin count: %u",
                   acq->value.ref.u32);
            return status;
        }
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(hist_samples_detected)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(hist_samples_detected);

    if (read) {
        uint64_t samples = 0;
        siResult = siToro_detector_getHistogramSamplesDetected(siDetector->detector,
                                                               &samples);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram samples detected");
            return status;
        }
        acq->value.ref.u64 = samples;
        *value = (double)acq->value.ref.u64;
    }
    else {
        status = XIA_READ_ONLY;
        pslLog(PSL_LOG_ERROR, status,
               "Variable is read only");
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(hist_samples_erased)
{
    int status;

    SiToro_Result siResult;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(hist_samples_erased);

    if (read) {
        uint64_t samples = 0;
        siResult = siToro_detector_getHistogramSamplesErased(siDetector->detector,
                                                             &samples);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram samples erased");
            return status;
        }
        acq->value.ref.u64 = samples;
        *value = (double)acq->value.ref.u64;
    }
    else {
        status = XIA_READ_ONLY;
        pslLog(PSL_LOG_ERROR, status,
               "Variable is read only");
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(hist_pulses_detected)
{
    int status;

    SiToro_Result siResult;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(hist_pulses_detected);

    if (read) {
        uint64_t pulses = 0;
        siResult = siToro_detector_getHistogramPulsesDetected(siDetector->detector,
                                                              &pulses);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram pulses detected");
            return status;
        }
        acq->value.ref.u64 = pulses;
        *value = (double)acq->value.ref.u64;
    }
    else {
        status = XIA_READ_ONLY;
        pslLog(PSL_LOG_ERROR, status,
               "Variable is read only");
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(hist_pulses_accepted)
{
    int status;

    SiToro_Result siResult;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(hist_pulses_accepted);

    if (read) {
        uint64_t pulses = 0;
        siResult = siToro_detector_getHistogramPulsesAccepted(siDetector->detector,
                                                              &pulses);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram pulses accepted");
            return status;
        }
        acq->value.ref.u64 = pulses;
        *value = (double)acq->value.ref.u64;
    }
    else {
        status = XIA_READ_ONLY;
        pslLog(PSL_LOG_ERROR, status,
               "Variable is read only");
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(hist_pulses_rejected)
{
    int status;

    SiToro_Result siResult;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(hist_pulses_rejected);

    if (read) {
        uint64_t pulses = 0;
        siResult = siToro_detector_getHistogramPulsesRejected(siDetector->detector,
                                                              &pulses);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram pulses rejected");
            return status;
        }
        acq->value.ref.u64 = pulses;
        *value = (double)acq->value.ref.u64;
    }
    else {
        status = XIA_READ_ONLY;
        pslLog(PSL_LOG_ERROR, status,
               "Variable is read only");
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(hist_input_count_rate)
{
    int status;

    SiToro_Result siResult;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(hist_input_count_rate);

    if (read) {
        double count_rate = 0;
        siResult = siToro_detector_getHistogramInputCountRate(siDetector->detector,
                                                              &count_rate);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram input count rate");
            return status;
        }
        acq->value.ref.d = count_rate;
        *value = acq->value.ref.d;
    }
    else {
        status = XIA_READ_ONLY;
        pslLog(PSL_LOG_ERROR, status,
               "Variable is read only");
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(hist_output_count_rate)
{
    int status;

    SiToro_Result siResult;

    UNUSED(defaults);
    UNUSED(detector);

    ACQ_HANDLER_LOG(hist_output_count_rate);

    if (read) {
        double count_rate = 0;
        siResult = siToro_detector_getHistogramOutputCountRate(siDetector->detector,
                                                               &count_rate);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram output count rate");
            return status;
        }
        acq->value.ref.d = count_rate;
        *value = acq->value.ref.d;
    }
    else {
        status = XIA_READ_ONLY;
        pslLog(PSL_LOG_ERROR, status,
               "Variable is read only");
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(hist_dead_time)
{
    int status;

    SiToro_Result siResult;

    UNUSED(detector);
    UNUSED(defaults);

    ACQ_HANDLER_LOG(hist_dead_time);

    if (read) {
        double count_rate = 0;
        siResult = siToro_detector_getHistogramDeadTime(siDetector->detector,
                                                        &count_rate);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Unable to get the histogram dead time");
            return status;
        }
        acq->value.ref.d = count_rate;
        *value = acq->value.ref.d;
    }
    else {
        status = XIA_READ_ONLY;
        pslLog(PSL_LOG_ERROR, status,
               "Variable is read only");
        return status;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(mapping_mode)
{
    int status;

    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);

    ACQ_HANDLER_LOG(mapping_mode);

    if (read) {
        *value = acq->value.ref.u32;
    }
    else {
        unsigned int mapping_mode = (uint32_t)*value;

        if (mapping_mode >= 2) {
            status = XIA_INVALID_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid mapping_mode: %u", mapping_mode);
            return status;
        }

        acq->value.ref.u32 = mapping_mode;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(preset_type)
{
    int status;

    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);

    ACQ_HANDLER_LOG(preset_type);

    if (read) {
        *value = acq->value.ref.u32;
    }
    else {
        unsigned int preset_type = (uint32_t)*value;

        if (preset_type >= 5) {
            status = XIA_INVALID_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid preset_type: %u", preset_type);
            return status;
        }

        acq->value.ref.u32 = preset_type;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(preset_value)
{
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);

    ACQ_HANDLER_LOG(preset_value);

    if (read) {
        *value = acq->value.ref.u32;
    }
    else {
        acq->value.ref.u32 = (uint32_t)*value;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(preset_baseline)
{
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);

    ACQ_HANDLER_LOG(preset_baseline);

    if (read) {
        *value = acq->value.ref.u32;
    }
    else {
        acq->value.ref.u32 = (uint32_t)*value;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(preset_get_timing)
{
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);

    ACQ_HANDLER_LOG(preset_get_timing);

    if (read) {
        *value = acq->value.ref.u32;
    }
    else {
        acq->value.ref.u32 = (uint32_t)*value;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(number_of_scas)
{
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);
    UNUSED(acq);
    UNUSED(value);

    ACQ_HANDLER_LOG(number_of_scas);

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(sca)
{
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);
    UNUSED(acq);
    UNUSED(value);

    ACQ_HANDLER_LOG(sca);

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(num_map_pixels_per_buffer)
{
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);
    UNUSED(acq);
    UNUSED(value);

    ACQ_HANDLER_LOG(num_map_pixels_per_buffer);

    if (read) {
        *value = acq->value.ref.u32;
    }
    else {
        if (siDetector->mmc.listMode_Running)
            return XIA_NOT_IDLE;
        acq->value.ref.u32 = (uint32_t)*value;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(num_map_pixels)
{
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);
    UNUSED(acq);
    UNUSED(value);

    ACQ_HANDLER_LOG(num_map_pixels);

    if (read) {
        *value = acq->value.ref.u32;
    }
    else {
        if (siDetector->mmc.listMode_Running)
            return XIA_NOT_IDLE;
        acq->value.ref.u32 = (uint32_t)*value;
    }

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(buffer_check_period)
{
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);
    UNUSED(acq);
    UNUSED(value);

    ACQ_HANDLER_LOG(buffer_check_period);

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(input_logic_polarity)
{
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);
    UNUSED(acq);
    UNUSED(value);

    ACQ_HANDLER_LOG(input_logic_polarity);

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(gate_ignore)
{
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);
    UNUSED(acq);
    UNUSED(value);

    ACQ_HANDLER_LOG(gate_ignore);

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(pixel_advance_mode)
{
    UNUSED(detector);
    UNUSED(defaults);
    UNUSED(siDetector);
    UNUSED(acq);
    UNUSED(value);

    ACQ_HANDLER_LOG(pixel_advance_mode);

    return XIA_SUCCESS;
}

ACQ_HANDLER_DECL(number_mca_channels)
{
    int status = ACQ_HANDLER_CALL(hist_bin_count);

    ACQ_HANDLER_LOG(number_mca_channels);

    if (status == XIA_SUCCESS) {
        AcquisitionValue* number_mca_channels;
        AcquisitionValue* hist_bin_count;

        number_mca_channels = psl__GetAcquisition(siDetector, "number_mca_channels");
        hist_bin_count = psl__GetAcquisition(siDetector, "hist_bin_count");

        number_mca_channels->value.ref.u32 = hist_bin_count->value.ref.u32;
    }

    return status;
}

ACQ_HANDLER_DECL(preamp_gain)
{
    int status = XIA_SUCCESS;

    ACQ_HANDLER_LOG(preamp_gain);

    if (read) {
        *value = acq->value.ref.d;
    }
    else {
        acq->value.ref.d = *value;

        if (siDetector->valid_acq_values)
            status = psl__UpdateGain(detector, siDetector, defaults,
                                     TRUE_, FALSE_);
    }

    return status;
}

ACQ_HANDLER_DECL(dynamic_range)
{
    int status = XIA_SUCCESS;

    ACQ_HANDLER_LOG(dynamic_range);

    if (read) {
        *value = acq->value.ref.d;
    }
    else {
        AcquisitionValue* adc_percent_rule;
        AcquisitionValue* calibration_energy;

        if (*value == 0.0) {
            status = XIA_INVALID_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid dynamic_range: %f", *value);
            return status;
        }

        acq->value.ref.d = *value;

        adc_percent_rule = psl__GetAcquisition(siDetector, "adc_percent_rule");
        calibration_energy = psl__GetAcquisition(siDetector, "calibration_energy");

        adc_percent_rule->value.ref.d =
            (calibration_energy->value.ref.d * 40.0) / acq->value.ref.d;

        if (siDetector->valid_acq_values) {
            status = psl__UpdateGain(detector, siDetector, defaults,
                                     TRUE_, TRUE_);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Error updating the gain for dynamic_range");
                return status;
            }

            status = psl__UpdateDefault(siDetector, defaults, adc_percent_rule);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Unable to set default for: adc_percent_rule");
                return status;
            }
        }
    }

    return status;
}

ACQ_HANDLER_DECL(adc_percent_rule)
{
    int status = XIA_SUCCESS;

    ACQ_HANDLER_LOG(adc_percent_rule);

    if (read) {
        *value = acq->value.ref.d;
    }
    else {
        AcquisitionValue* calibration_energy;
        AcquisitionValue* dynamic_range;

        if (*value == 0.0) {
            status = XIA_INVALID_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid adc_percent_rule: %f", *value);
            return status;
        }

        acq->value.ref.d = *value;

        calibration_energy = psl__GetAcquisition(siDetector, "calibration_energy");
        dynamic_range = psl__GetAcquisition(siDetector, "dynamic_range");

        dynamic_range->value.ref.d =
            (calibration_energy->value.ref.d / acq->value.ref.d) * 40.0;

        if (siDetector->valid_acq_values) {
            status = psl__UpdateGain(detector, siDetector, defaults,
                                     TRUE_, TRUE_);

            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Error updating the gain for adc_percent_rule");
                return status;
            }

            status = psl__UpdateDefault(siDetector, defaults, dynamic_range);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Unable to set default for: dynamic_range");
                return status;
            }
        }
    }

    return status;
}

ACQ_HANDLER_DECL(calibration_energy)
{
    int status = XIA_SUCCESS;

    ACQ_HANDLER_LOG(calibration_energy);

    if (read) {
        *value = acq->value.ref.d;
    }
    else {
        AcquisitionValue* adc_percent_rule;
        AcquisitionValue* dynamic_range;

        acq->value.ref.d = *value;

        adc_percent_rule = psl__GetAcquisition(siDetector, "adc_percent_rule");
        dynamic_range = psl__GetAcquisition(siDetector, "dynamic_range");

        adc_percent_rule->value.ref.d =
            acq->value.ref.d / (dynamic_range->value.ref.d / 40.0);

        if (siDetector->valid_acq_values) {
            status = psl__UpdateGain(detector, siDetector, defaults,
                                     TRUE_, TRUE_);

            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Error updating the gain for calibration_energy");
                return status;
            }

            status = psl__UpdateDefault(siDetector, defaults, adc_percent_rule);
            if (status != XIA_SUCCESS) {
                pslLog(PSL_LOG_ERROR, status,
                       "Unable to set default for: adc_percent_rule");
                return status;
            }
        }
    }

    return status;
}

ACQ_HANDLER_DECL(mca_bin_width)
{
    int status = XIA_SUCCESS;

    ACQ_HANDLER_LOG(mca_bin_width);

    if (read) {
        *value = acq->value.ref.d;
    }
    else {
        acq->value.ref.d = *value;

        if (siDetector->valid_acq_values)
            status = psl__UpdateGain(detector, siDetector, defaults,
                                     FALSE_, TRUE_);
    }

    return status;
}

/** @brief Updates the current gain setting based on the current acquisition
* values.
*
*/
PSL_STATIC int psl__UpdateGain(Detector*         detector,
                               SiToroDetector*   siDetector,
                               XiaDefaults*      defaults,
                               boolean_t         gain,
                               boolean_t         scaling)
{
    double gaindac = 0.0;
    double gaincoarse = 0;
    double scalefactor = 0.0;
    int    status;

    status = psl__CalculateGain(siDetector,
                                gain ? &gaindac : NULL,
                                gain ? &gaincoarse : NULL,
                                scaling ? &scalefactor : NULL);

    if (status == XIA_SUCCESS)
    {
        if (gain)
        {
            AcquisitionValue* analog_gain;
            AcquisitionValue* analog_gain_boost;

            analog_gain       = psl__GetAcquisition(siDetector, "analog_gain");
            analog_gain_boost = psl__GetAcquisition(siDetector, "analog_gain_boost");

            status = ACQ_HANDLER(analog_gain)(detector, siDetector, defaults,
                                              analog_gain, &gaindac, FALSE_);

            if (status != XIA_SUCCESS)
            {
                pslLog(PSL_LOG_ERROR, status,
                       "Setting analog gain");
                return status;
            }

            status = ACQ_HANDLER(analog_gain_boost)(detector, siDetector, defaults,
                                                    analog_gain_boost, &gaincoarse, FALSE_);

            if (status != XIA_SUCCESS)
            {
                pslLog(PSL_LOG_ERROR, status,
                       "Setting analog gain boost");
                return status;
            }
        }

        if (scaling)
        {
            AcquisitionValue* pulse_scale_factor;

            pulse_scale_factor = psl__GetAcquisition(siDetector, "pulse_scale_factor");

            status = ACQ_HANDLER(pulse_scale_factor)(detector, siDetector, defaults,
                                                     pulse_scale_factor, &scalefactor, FALSE_);

            if (status != XIA_SUCCESS)
            {
                pslLog(PSL_LOG_ERROR, status,
                       "Setting pulse scale factor");
                return status;
            }
        }
    }

    return status;
}

/** @brief Calculates the variable gain.
*
* Calculates the variable gain based on existing acquisition values and the
* preamplifier gain and returns the value of @a GAINDAC and @a COARSEGAIN.
*
* The total gain of the FalconX system is defined as:
*
* Gtot = Gcoarse * Gdac,
*
* where Gcourse is the coarse gain setting (analog_gain_boost) and Gdac is the
* gain due to the variable gain amplifier setting. The coarse gain setting is
* either x1 or x6. The Gdac controls the gain from x1 to x16 and the coarse
* gain extends the range to x96.
*
* The control for the variable gain amplifier is 'linear in dB', meaning a
* fixed change in the control voltage produces a fixed change in the gain
* expressed in dB, which mean the gain will change by a fixed multiplicative
* factor. A 16-bit DAC control the gain and in order to ensure the DAC output
* covers the full-scale range of the VGA the output full-scale range is 20%
* greater that the full-scale control range. The bottom and top 10% of the
* 'gaindac' control setting is unused. In terms of dB, the variable gain as a
* funciton of the 16-bit Gdac value is expressed for the following ranges:
*
* 1. For Gdac < 6554 (where 6554 is 10% of 65536):
*
*  Gdbvar = 0 dB
*
* 2. For 6554 <= Gdac < 58982:
*
*           (Gdac - 6554)
*  Gdbvar = ------------- x 24 dB
*               52428
*
* 3. For Gdac >= 58982:
*
*  Gdbvar = 24 dB
*
* To convert to a multiplicative gain just convert from dB:
*
*  Gdac = 10^(Gdbvar / 20)
*
* For software control there are two control parameters:
*
*  analog_gain       : 16bit value controls gain ranging from 1 to 24 dB.
*  analog_gain_boost : if true updates the gain x16.
*
* The gain is linear in dB so gain changes by:
*
*  24 dB / 54428 = 0.000441 dB
*
* with a Gdac change of 1. A change in Gdac required to change the gain by a
* value k use:
*
*                20 x log10(k)
*   delta Gdac = -------------
*                  0.000441
*
* The user defines the total gain via the calibration energy, preamplifier gain
* and ADC percent rule. This gain is then scaled by another user-defined value,
* eV/bin.
*/
PSL_STATIC int psl__CalculateGain(SiToroDetector* siDetector,
                                  double*         gaindac,
                                  double*         gaincoarse,
                                  double*         scalefactor)
{
    double ADCRule;
    double CalEnergy;

    AcquisitionValue* preamp_gain;
    AcquisitionValue* adc_percent_rule;
    AcquisitionValue* calibration_energy;
    AcquisitionValue* mca_bin_width;
    AcquisitionValue* hist_bin_count;

    preamp_gain        = psl__GetAcquisition(siDetector, "preamp_gain");
    adc_percent_rule   = psl__GetAcquisition(siDetector, "adc_percent_rule");
    calibration_energy = psl__GetAcquisition(siDetector, "calibration_energy");
    mca_bin_width      = psl__GetAcquisition(siDetector, "mca_bin_width");
    hist_bin_count     = psl__GetAcquisition(siDetector, "hist_bin_count");

    /*
     * Make a scaling value from the %. No units.
     */
    ADCRule = adc_percent_rule->value.ref.d / 100.0;

    CalEnergy = calibration_energy->value.ref.d;

    if (gaincoarse)
        *gaincoarse = 0;

    if (gaindac)
    {
        const double maxDb = 20.0 * log10(16);
        const double dbUnit = maxDb / (ADC_COUNT_MAX * ADC_INPUT_RANGE_PERCENT);

        /*
         * The voltage step at the input of the board.
         *
         *                    mV
         * Units: mV = KeV x ---
         *                   KeV
         */
        const double Vstep = CalEnergy * preamp_gain->value.ref.d;

        /*
         * The total gain.
         *
         *                   no-units x mV
         * Units: no-units = -------------
         *                        mV
         *
         */
        const double Gtot = (ADCRule * ADC_INPUT_RANGE_MV) / Vstep;

        if ((Gtot < ADC_GAIN_MIN) || (Gtot > ADC_GAIN_MAX))
        {
            pslLog(PSL_LOG_ERROR, XIA_GAIN_OOR,
                   "Total gain out of range: %f", Gtot);
            return XIA_GAIN_OOR;
        }

        /*
         * The regions overlap and 12 is a middle area to switch over.
         *
         * @todo Add hysterisa around the switching.
         */
        if (Gtot < 12)
        {
            *gaindac = (20.0 * log10(Gtot) / dbUnit) + ADC_DEADZONE_COUNT;
        }
        else
        {
            *gaindac = ((20.0 * log10(Gtot / ADC_COARSE_GAIN_MULTIPLIER) / dbUnit) +
                        ADC_DEADZONE_COUNT);
            *gaincoarse = 1;
        }

        pslLog(PSL_LOG_DEBUG,
               "Gtot=%3.4f gaindac=%3.4f gaincoarse=%s dBs=%3.4f",
               Gtot, *gaindac, *gaincoarse ? "ON" : "OFF", 20.0 * log10(Gtot));
    }

    if (scalefactor)
    {
        const double eVPerADC = CalEnergy * 1000 / (ADCRule * ADC_COUNT_MAX);

        /*
         * Map the ADC count on to number of histogram bins. We use the
         * histogram bins because this is the value is value configured in
         * SiToro and is normally never changed by Handel.
         */
        const double eVPerBin_default =
            (ADC_COUNT_MAX / hist_bin_count->value.ref.u32) * eVPerADC;

        *scalefactor = eVPerBin_default / mca_bin_width->value.ref.d;

        pslLog(PSL_LOG_DEBUG,
               "scalefactor=%3.4f CalEnergy=%3.4f ADCRule=%3.4f eVPerADC=%3.4f eVPerBin_default=%3.4f",
               *scalefactor, CalEnergy, ADCRule, eVPerADC, eVPerBin_default);
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__GainCalibrate(int detChan, Detector *detector, int modChan, Module *m,
                                  XiaDefaults *def, double delta)
{
    int status = XIA_SUCCESS;

    SiToroDetector* siDetector = detector->pslData;
    XiaDefaults *defaults;

    UNUSED(modChan);
    UNUSED(m);
    UNUSED(def);

    pslLog(PSL_LOG_DEBUG,
           "Detector %s (%d), delta = %3.4f", detector->alias, detChan, delta);

    if (delta <= 0) {
        pslLog(PSL_LOG_ERROR, XIA_GAIN_SCALE,
               "Invalid gain scale factor %3.4f", delta);
        return XIA_GAIN_SCALE;
    }

    defaults = xiaGetDefaultFromDetChan(detChan);

    if ((0.98 < delta) && (delta < 1.02)) {
        AcquisitionValue* pulse_scale_factor;
        double newScaleFactor;

        pulse_scale_factor = psl__GetAcquisition(siDetector, "pulse_scale_factor");

        newScaleFactor = pulse_scale_factor->value.ref.d * delta;

        pslLog(PSL_LOG_DEBUG,
               "Scaling pulse scale factor from %3.4f to %3.4f",
               pulse_scale_factor->value.ref.d, newScaleFactor);

        status = ACQ_HANDLER(pulse_scale_factor)(detector, siDetector, defaults,
                                                 pulse_scale_factor,
                                                 &newScaleFactor,
                                                 FALSE_);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Setting pulse scale factor");
            return status;
        }
    }
    else {
        AcquisitionValue* preamp_gain;
        double newGain;

        preamp_gain = psl__GetAcquisition(siDetector, "preamp_gain");

        newGain = preamp_gain->value.ref.d / delta;

        pslLog(PSL_LOG_DEBUG,
               "Scaling preamp gain from %3.4f to %3.4f",
               preamp_gain->value.ref.d, newGain);

        status = ACQ_HANDLER(preamp_gain)(detector, siDetector, defaults,
                                          preamp_gain, &newGain, FALSE_);

        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Setting preamp gain");
            return status;
        }
    }

    return status;
}

/*
 * Data Formatter Helpers
 */

PSL_STATIC boolean_t psl__Buffers_Full(MM_Buffers* buffers, int buffer)
{
    return buffers->buffer[buffer].full;
}

PSL_STATIC boolean_t psl__Buffers_ActiveFull(MM_Buffers* buffers)
{
    return psl__Buffers_Full(buffers, buffers->active);
}

PSL_STATIC int psl__Buffers_Done(MM_Buffers* buffers, int buffer)
{
    int status = XIA_SUCCESS;
    buffers->buffer[buffer].level = 0;
    buffers->buffer[buffer].full = FALSE_;
    return status;
}

PSL_STATIC int psl__Buffers_Copy(MM_Buffers* buffers, int buffer, void* value)
{
    int status = XIA_SUCCESS;

    if (buffers->active == buffer) {
        status = XIA_INVALID_VALUE;
        pslLog(PSL_LOG_ERROR, status,
               "Buffer %c is active", buffers->active == 0 ? 'A' : 'B');
        return status;
    }

    memcpy(value,
           buffers->buffer[buffer].buffer,
           buffers->buffer[buffer].level * sizeof(uint32_t));

    return XIA_SUCCESS;
}

PSL_STATIC int psl__Buffers_ActiveUpdate(MM_Buffers* buffers)
{
    int status = XIA_SUCCESS;
    if (psl__Buffers_ActiveFull(buffers)) {
        int next = buffers->active ? 0 : 1;
        if (psl__Buffers_Full(buffers, next)) {
            status = XIA_INTERNAL_BUFFER_OVERRUN;
            pslLog(PSL_LOG_DEBUG,
                   "Cannot update active; all buffers are full");
            return status;
        }
        buffers->active = next;
    }
    return status;
}

PSL_STATIC int psl__MappingModeBuffer_Open(MM_Buffer* buffer, size_t size)
{
    int status = XIA_SUCCESS;

    if (!buffer->buffer) {
        memset(buffer, 0, sizeof(*buffer));

        buffer->buffer = handel_md_alloc(size * sizeof(uint32_t));
        if (!buffer->buffer) {
            status = XIA_NOMEM;
            pslLog(PSL_LOG_ERROR, status,
                   "Error allocating memory for MM buffer");
            return status;
        }

        memset(buffer->buffer, 0, size * sizeof(uint32_t));
        buffer->next = buffer->buffer;
        buffer->pixel = 0;
        buffer->bufferPixel = 0;
        buffer->size = size;
    }

    return status;
}

PSL_STATIC int psl__MappingModeBuffer_Close(MM_Buffer* buffer)
{
    int status = XIA_SUCCESS;

    if (buffer->buffer) {
        handel_md_free(buffer->buffer);
        memset(buffer, 0, sizeof(*buffer));
    }

    return status;
}

PSL_STATIC int psl__MappingModeBuffers_Open(MM_Buffers* buffers, size_t size)
{
    int status = XIA_SUCCESS;
    int buffer = 0;

    pslLog(PSL_LOG_INFO,
           "size:%u (%u)", (uint32_t) size, (uint32_t) (size * sizeof(uint32_t)));

    while (buffer < MMC_BUFFERS) {
        status = psl__MappingModeBuffer_Open(&buffers->buffer[buffer], size);
        if (status != XIA_SUCCESS) {
            while (buffer > 0) {
                --buffer;
                psl__MappingModeBuffer_Close(&buffers->buffer[buffer]);
            }
            return status;
        }
        ++buffer;
    }

    return status;
}

PSL_STATIC int psl__MappingModeBuffers_Close(MM_Buffers* buffers)
{
    int status = XIA_SUCCESS;
    int buffer = MMC_BUFFERS;

    while (buffer > 0) {
        int this_status;
        --buffer;
        this_status = psl__MappingModeBuffer_Close(&buffers->buffer[buffer]);
        if ((status == XIA_SUCCESS) && (this_status != XIA_SUCCESS)) {
            status = this_status;
        }
    }

    return status;
}

PSL_STATIC int psl__MappingModeBinner_Open(MM_Binner* binner,
                                           size_t     bins,
                                           uint32_t   bufferSize)
{
    int status = XIA_SUCCESS;

    if (!binner->bins) {
        binner->bins = handel_md_alloc(bins * sizeof(uint64_t));
        if (!binner->bins) {
            status = XIA_NOMEM;
            pslLog(PSL_LOG_ERROR, status,
                   "Error allocating memory for MM bins");
            return status;
        }
        binner->buffer = handel_md_alloc(bufferSize * sizeof(uint32_t));
        if (!binner->buffer) {
            handel_md_free(binner->bins);
            binner->bins = NULL;
            status = XIA_NOMEM;
            pslLog(PSL_LOG_ERROR, status,
                   "Error allocating memory for list mode buffer");
            return status;
        }
        memset(binner->bins, 0, bins * sizeof(uint64_t));
        memset(binner->buffer, 0, bufferSize * sizeof(uint32_t));
        binner->flags = MM_BINNER_GATE_HIGH;
        binner->numberOfBins = bins;
        binner->outOfRange = 0;
        binner->errorBits = 0;
        memset(&binner->stats, 0, sizeof(binner->stats));
        binner->bufferSize = bufferSize;
    }

    return status;
}

PSL_STATIC int psl__MappingModeBinner_Close(MM_Binner* binner)
{
    int status = XIA_SUCCESS;

    if (binner->bins) {
        handel_md_free(binner->buffer);
        handel_md_free(binner->bins);
        memset(binner, 0, sizeof(*binner));
    }

    return status;
}

#ifdef CODE_UNUSED
PSL_STATIC int psl__MappingModeBinner_BinAdd(MM_Binner* binner,
                                             uint32_t   bin,
                                             uint32_t   amount)
{
    if (bin >= binner->numberOfBins) {
        ++binner->outOfRange;
    }
    else {
        binner->bins[bin] += amount;
    }
    return XIA_SUCCESS;
}
#endif

PSL_STATIC int psl__NumberMCAChannels(SiToroDetector* siDetector, uint32_t* value)
{
    AcquisitionValue* number_mca_channels;

    number_mca_channels = psl__GetAcquisition(siDetector, "number_mca_channels");

    ASSERT(number_mca_channels);

    *value = number_mca_channels->value.ref.u32;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__ProcessListData_Copy(SiToroDetector* siDetector,
                                         MM_Binner*      binner,
                                         MM_Buffers*     buffers)
{
    int status = XIA_SUCCESS;

    MM_Buffer* buffer = &buffers->buffer[buffers->active];

    UNUSED(siDetector);

    if (buffer->full)  {
        status = XIA_INTERNAL_BUFFER_OVERRUN;
    }
    else {
        const size_t srcSize = binner->bufferLevel;
        const size_t dstSize = buffer->size - buffer->level;
        const size_t copy = dstSize < srcSize ? dstSize : srcSize;

        if (copy) {
            pslLog(PSL_LOG_INFO,
                   "buffer:%c dst:%u src:%u copy:%u full:%s",
                   buffers->active == 0 ? 'A' : 'B',
                   (int) dstSize, (int) srcSize, (int) copy,
                   (buffer->level + copy) >= buffer->size ? "YES" : "NO");

            /*
             * Copy the list mode data to the output buffer.
             */
            memcpy(buffer->buffer + buffer->level, binner->buffer, copy * sizeof(uint32_t));
            buffer->level += copy;
            binner->bufferLevel -= (uint32_t) copy;

            /*
             * Compact the list mode input buffer.
             */
            if (copy < srcSize)
                memmove(binner->buffer,
                        binner->buffer + copy,
                        (srcSize - copy) * sizeof(uint32_t));

            /*
             * Update the buffer full flag.
             */
            if (buffer->level >= buffer->size)
                buffer->full = TRUE_;
        }
    }

    return status;
}

#ifdef CODE_UNUSED
PSL_STATIC int psl__SiToroEventSize(uint8_t id)
{
    /*
     * The size is in uint32_t units and this lines up with the buffer
     * management.
     */
    int size = 0;

    /*
     * There is no sensible way to manage the length of the record being
     * processed. The SiToro API does not provide any information on the amount
     * of data to move forward. The event id's are not linear so a switch is
     * needed.
     *
     * The information contained here is from the API documentation under
     * Data Server and the section "List mode bit-packed format".
     */
    switch (id)
    {
        case SiToro_ListModeEvent_TimeStampShortWrap:
            size = 1;
            break;
        case SiToro_ListModeEvent_TimeStampLongWrap:
            size = 2;
            break;
        case SiToro_ListModeEvent_StatsSmallCounters:
            size = 8;
            break;
        case SiToro_ListModeEvent_StatsLargeCounters:
            size = 12;
            break;
        case SiToro_ListModeEvent_SpatialOneAxis:
            size = 2;
            break;
        case SiToro_ListModeEvent_SpatialTwoAxis:
            size = 3;
            break;
        case SiToro_ListModeEvent_SpatialThreeAxis:
            size = 4;
            break;
        case SiToro_ListModeEvent_SpatialFourAxis:
            size = 5;
            break;
        case SiToro_ListModeEvent_GateState:
            size = 1;
            break;
        case SiToro_ListModeEvent_PulseWithTimeOfArrival:
            size = 2;
            break;
        case SiToro_ListModeEvent_PulseNoTimeOfArrival:
            size = 1;
            break;
        default:
            pslLog(PSL_LOG_INFO, "id: %d", (int) id);
            break;
    }

    return size;
}

PSL_STATIC uint16_t psl__Lower16(uint32_t value)
{
    return (uint16_t) value;
}

PSL_STATIC uint16_t psl__Upper16(uint32_t value)
{
    return (uint16_t) (value >> 16);
}

PSL_STATIC void psl__Write32(uint16_t* buffer, uint32_t value)
{
    buffer[0] = psl__Lower16(value);
    buffer[1] = psl__Upper16(value);
}

PSL_STATIC int psl__WriteXMAPHeader(SiToroDetector* siDetector,
                                    MM_Binner*      binner,
                                    MM_Buffer*      buffer)
{
    int status = XIA_SUCCESS;

    uint16_t* in = (uint16_t*) (buffer->buffer + buffer->level);

    psl__Write32(in, 0xcc3333cc);
    in += 2;
    *in = XMAP_BUFFER_HEADER_SIZE;
    in += 1;
    *in = (uint16_t) siDetector->mmc.mode;
    in += 1;
    psl__Write32(in, buffer->pixel);
    in += 2;
    in[3] = (uint16_t) (siDetector->mmc.pixelHeaderSize + binner->numberOfBins);

    ++buffer->pixel;

    return status;
}
#endif

#ifdef CODE_UNUSED
PSL_STATIC int psl__ProcessListData_MCA(SiToroDetector* siDetector,
                                        MM_Binner*      binner,
                                        MM_Buffers*     buffers)
{
    int status = XIA_SUCCESS;

    MM_Buffer* buffer = &buffers->buffer[buffers->active];

    UNUSED(siDetector);

    if (buffer->full)  {
        status = XIA_INTERNAL_BUFFER_OVERRUN;
    }
    else {
        uint32_t* in = buffer->buffer;

        /* boolean_t gateChanged = FALSE_; */

        while (binner->bufferLevel) {
            psl__SiToroListModeStats*  stats = &binner->stats;
            psl__SiToroListModeStats32 stats32;
            uint8_t                    dataType;
            int                        eventSize;
            siBool                     ok;
            uint8_t                    rejected = 0;
            int16_t                    energy = 0;
            uint32_t                   timestamp = 0;
            uint8_t                    subSample = 0;
            uint8_t                    gateState = 0;

            dataType = siToro_decode_getListModeDataType(*in);

            if (dataType == SiToro_ListModeEvent_Error) {
                status = XIA_FORMAT_ERROR;
                pslLog(PSL_LOG_ERROR, status,
                       "Invalid data type: data type: %d", (int) dataType);
                return status;
            }

            /*
             * Get the size of the data this event has. The number of uint32_ts.
             */
            eventSize = psl__SiToroEventSize(dataType);

            /*
             * An event size of 0 is an error.
             */
            if (eventSize == 0) {
                status = XIA_FORMAT_ERROR;
                pslLog(PSL_LOG_ERROR, status,
                       "Invalid event size: data type: %d", (int) dataType);
                return status;
            }

            /*
             * Is there enough data in the buffer to process the record.
             */
            if (eventSize > (int) binner->bufferLevel) {
                break;
            }

            switch (dataType)
            {
                case SiToro_ListModeEvent_PulseWithTimeOfArrival:
                    ok = siToro_decode_getListModePulseEventWithTimeOfArrival(in,
                                                                              &rejected,
                                                                              &energy,
                                                                              &timestamp,
                                                                              &subSample);
                    if (energy < 0)
                        energy = 0;
                    psl__MappingModeBinner_BinAdd(binner, (uint32_t) energy, 1);
                    /* event count per buffer */
                    /* timestamp saved */
                    break;
                case SiToro_ListModeEvent_PulseNoTimeOfArrival:
                    ok = siToro_decode_getListModePulseEventNoTimeOfArrival(in,
                                                                            &rejected,
                                                                            &energy);
                    if (energy < 0)
                        energy = 0;
                    psl__MappingModeBinner_BinAdd(binner, (uint32_t) energy, 1);
                    /* event count per buffer */
                    break;
                case SiToro_ListModeEvent_StatsLargeCounters:
                    ok = siToro_decode_getListModeStatisticsLarge(in,
                                                                  &stats->statsType,
                                                                  &stats->samplesDetected,
                                                                  &stats->samplesErased,
                                                                  &stats->pulsesDetected,
                                                                  &stats->pulsesAccepted,
                                                                  &stats->inputCountRate,
                                                                  &stats->outputCountRate,
                                                                  &stats->deadTimePercent);
                    if (ok) {
                        binner->flags |= MM_BINNER_STATS_VALID;
                    }
                    break;
                case SiToro_ListModeEvent_StatsSmallCounters:
                    ok = siToro_decode_getListModeStatisticsSmall(in,
                                                                  &stats->statsType,
                                                                  &stats32.samplesDetected,
                                                                  &stats32.samplesErased,
                                                                  &stats32.pulsesDetected,
                                                                  &stats32.pulsesAccepted,
                                                                  &stats->inputCountRate,
                                                                  &stats->outputCountRate,
                                                                  &stats->deadTimePercent);
                    if (ok) {
                        /*
                         * SiToro API wart, we need to manage data variations.
                         */
                        stats->samplesDetected = stats32.samplesDetected;
                        stats->samplesErased   = stats32.samplesErased;
                        stats->pulsesDetected  = stats32.pulsesDetected;
                        stats->pulsesAccepted  = stats32.pulsesAccepted;
                        binner->flags |= MM_BINNER_STATS_VALID;
                    }
                    break;
                case SiToro_ListModeEvent_GateState:
                    ok = siToro_decode_getListModeGateState(in, &gateState);
                    if (ok) {
                        if ((!gateState && ((binner->flags & MM_BINNER_GATE_HIGH) != 0)) ||
                            (gateState && ((binner->flags & MM_BINNER_GATE_HIGH) == 0))) {
                            binner->flags |= MM_BINNER_GATE_TRIGGER;
                        }
                    }
                    break;
            }

            in += eventSize;
            binner->bufferLevel -= (uint32_t) eventSize;

            if (MM_BINNER_PIXEL_VALID(binner)) {

            }
        }

       /*
        * Move any remaining data to the bottom of the binner's buffer.
        */

        if (binner->bufferLevel) {
            memcpy(binner->buffer, in, binner->bufferLevel * sizeof(uint32_t));
        }
    }

    return status;
}
#endif

PSL_STATIC int psl__ProcessListData_Worker(SiToroDetector* siDetector,
                                           MM_Binner*      binner,
                                           MM_Buffers*     buffers)
{
    int status;

    status = psl__ProcessListData_Copy(siDetector, binner, buffers);

    if (status == XIA_SUCCESS)
        status = psl__Buffers_ActiveUpdate(buffers);

    return status;
}

PSL_STATIC int psl__ProcessListData(SiToroDetector* siDetector,
                                    MM_Binner*      binner,
                                    MM_Buffers*     buffers)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    uint32_t numWritten = 0;

    if (!binner->bins || !binner->buffer) {
        status = XIA_ILLEGAL_OPERATION;
        pslLog(PSL_LOG_ERROR, status,
               "No binner buffers found");
        return status;
    }

    status = psl__ProcessListData_Worker(siDetector, binner, buffers);
    if (status != XIA_SUCCESS)
        return status;

    siResult = siToro_detector_getListModeData(siDetector->detector,
                                               0,
                                               binner->buffer + binner->bufferLevel,
                                               binner->bufferSize - binner->bufferLevel,
                                               &numWritten,
                                               &binner->errorBits);
    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Failed to get list mode data");
            return status;
    }

    binner->bufferLevel += numWritten;

    status = psl__ProcessListData_Worker(siDetector, binner, buffers);
    if (status != XIA_SUCCESS)
        return status;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BufferSize(SiToroDetector* siDetector, uint32_t* value)
{
    AcquisitionValue* number_mca_channels;
    AcquisitionValue* num_map_pixels_per_buffer;

    number_mca_channels = psl__GetAcquisition(siDetector, "number_mca_channels");
    num_map_pixels_per_buffer = psl__GetAcquisition(siDetector, "num_map_pixels_per_buffer");

    ASSERT(number_mca_channels);
    ASSERT(num_map_pixels_per_buffer);

    *value  = number_mca_channels->value.ref.u32 * num_map_pixels_per_buffer->value.ref.u32;
    *value += siDetector->mmc.pixelHeaderSize * num_map_pixels_per_buffer->value.ref.u32;
    *value += siDetector->mmc.bufferHeaderSize;

    pslLog(PSL_LOG_INFO,
           "number_mca_channels:%u num_map_pixels_per_buffer:%u size:%u",
           number_mca_channels->value.ref.u32,
           num_map_pixels_per_buffer->value.ref.u32,
           *value);

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BufferFull(SiToroDetector* siDetector,
                               int             bufferSelect,
                               boolean_t*      isFull)
{
    int status = XIA_SUCCESS;

    MMC1_Data*  mmd = siDetector->mmc.dataFormatter;
    MM_Buffers* buffers;

    *isFull = FALSE_;

    if (!mmd) {
        status = XIA_ILLEGAL_OPERATION;
        pslLog(PSL_LOG_ERROR, status,
               "No mapping mode data");
        return status;
    }

    if (bufferSelect >= MMC_BUFFERS) {
        status = XIA_ILLEGAL_OPERATION;
        pslLog(PSL_LOG_ERROR, status,
               "Bad buffer selector: %d", bufferSelect);
        return status;
    }

    buffers = &mmd->buffers;

    status = psl__ProcessListData(siDetector, &mmd->bins, buffers);
    if (status != XIA_SUCCESS)
        return status;

    *isFull = psl__Buffers_Full(buffers, bufferSelect);

    return status;
}

PSL_STATIC int psl__DataFormatterOpen_MM1(SiToroDetector* siDetector)
{
    int status = XIA_SUCCESS;

    MMC1_Data* data = handel_md_alloc(sizeof(MMC1_Data));

    uint32_t number_of_bins;
    uint32_t buffer_size;

    if (!data) {
        status = XIA_NOMEM;
        pslLog(PSL_LOG_ERROR, status,
               "Error allocating memory for MMC1 data");
        return status;
    }

    memset(data, 0, sizeof(MMC1_Data));

    status = psl__NumberMCAChannels(siDetector, &number_of_bins);
    if (status != XIA_SUCCESS) {
        handel_md_free(data);
        return status;
    }

    /*
     * 32M is the input buffer size.
     */
    status = psl__MappingModeBinner_Open(&data->bins,
                                         number_of_bins,
                                         32UL * 1024UL * 1024UL);
    if (status != XIA_SUCCESS) {
        handel_md_free(data);
        return status;
    }

    /*
     * Set the buffer overheads for MM1 mode.
     */
    siDetector->mmc.bufferHeaderSize = 0;
    siDetector->mmc.pixelHeaderSize = 27;

    status = psl__BufferSize(siDetector, &buffer_size);
    if (status != XIA_SUCCESS) {
        psl__MappingModeBinner_Close(&data->bins);
        handel_md_free(data);
        return status;
    }

    if (buffer_size == 0) {
        status = XIA_MEMORY_LENGTH;
        pslLog(PSL_LOG_ERROR, status,
               "Buffer size is invalid, check MCA channel count or pixels per buffer");
        return status;
    }

    status = psl__MappingModeBuffers_Open(&data->buffers, buffer_size);
    if (status != XIA_SUCCESS) {
        psl__MappingModeBinner_Close(&data->bins);
        handel_md_free(data);
        return status;
    }

    /*
     * Set the handle.
     */
    siDetector->mmc.dataFormatter = data;

    return status;
}

PSL_STATIC int psl__DataFormatterClose_MM1(SiToroDetector* siDetector)
{
    int status = XIA_SUCCESS;

    if (siDetector->mmc.dataFormatter) {
        MMC1_Data* data = siDetector->mmc.dataFormatter;
        int        this_status;
        this_status = psl__MappingModeBuffers_Close(&data->buffers);
        if ((status == XIA_SUCCESS) && (this_status != XIA_SUCCESS))
            status = this_status;
        this_status = psl__MappingModeBinner_Close(&data->bins);
        if ((status == XIA_SUCCESS) && (this_status != XIA_SUCCESS))
            status = this_status;
        handel_md_free(siDetector->mmc.dataFormatter);
        siDetector->mmc.dataFormatter = NULL;
    }

    return status;
}

#ifdef NOT_USED_MAY_GOAWAY
PSL_STATIC int psl__DataFormatterOpen(int mode, SiToroDetector* siDetector)
{
    int status = XIA_SUCCESS;

    switch (mode)
    {
        case 1:
            siDetector->mmc.mode = mode;
            status = psl__DataFormatterOpen_MM1(siDetector);
            break;

        default:
            status = XIA_NO_MAPPING;
            break;
    }

    return status;
}

PSL_STATIC int psl__DataFormatterClose(int mode, SiToroDetector* siDetector)
{
    int status = XIA_SUCCESS;

    switch (siDetector->mmc.mode)
    {
        case 1:
            status = psl__DataFormatterClose_MM1(siDetector);
            break;

        default:
            status = XIA_NO_MAPPING;
            break;
    }

    return status;
}
#endif

PSL_STATIC int psl__Start_MappingMode_0(unsigned short resume, SiToroDetector* siDetector)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    AcquisitionValue* preset_type;
    AcquisitionValue* preset_value;
    AcquisitionValue* preset_baseline;

    SiToro_HistogramMode histMode = SiToro_HistogramMode_Continuous;

    preset_type = psl__GetAcquisition(siDetector, "preset_type");
    preset_value = psl__GetAcquisition(siDetector, "preset_value");
    preset_baseline = psl__GetAcquisition(siDetector, "preset_baseline");

    ASSERT(preset_type);
    ASSERT(preset_value);
    ASSERT(preset_baseline);

    switch (preset_type->value.ref.u32)
    {
        case 0:
            histMode = SiToro_HistogramMode_Continuous;
            break;
        case 1:
            histMode = SiToro_HistogramMode_FixedTime;
            break;
        case 2:
            histMode = SiToro_HistogramMode_FixedInputCount;
            break;
        case 3:
            histMode = SiToro_HistogramMode_FixedOutputCount;
            break;
        case 4:
            histMode = SiToro_HistogramMode_MovingAverage;
            break;
        case 5:
            histMode = SiToro_HistogramMode_Gated;
            break;
        default:
            status = XIA_INVALID_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid preset_type: %u", preset_type->value.ref.u32);
            return status;
    }

    siResult = siToro_detector_startHistogramCapture(siDetector->detector,
                                                     histMode,
                                                     preset_value->value.ref.u32,
                                                     preset_baseline->value.ref.u32,
                                                     resume ? SIBOOL_TRUE : SIBOOL_FALSE);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status, "Unable to start MCA run");
        return status;
    }

    return status;
}

PSL_STATIC int psl__Stop_MappingMode_0(SiToroDetector* siDetector)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    siResult = siToro_detector_stopHistogramCapture(siDetector->detector);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to stop MCA run");
        return status;
    }

    return status;
}

/*
 * Mapping Mode 1: Full Spectrum Mapping.
 */

PSL_STATIC int psl__Start_MappingMode_1(unsigned short resume, SiToroDetector* siDetector)
{
    int status = XIA_SUCCESS;

    UNUSED(resume);

    status = psl__DataFormatterOpen_MM1(siDetector);

    if (status == XIA_SUCCESS) {
        SiToro_Result siResult;

        AcquisitionValue* preset_baseline;
        AcquisitionValue* preset_get_timing;

        preset_baseline = psl__GetAcquisition(siDetector, "preset_baseline");
        preset_get_timing = psl__GetAcquisition(siDetector, "preset_get_timing");

        ASSERT(preset_baseline);
        ASSERT(preset_get_timing);

        pslLog(PSL_LOG_INFO,
               "List start baeline: %u msec get-timing: %u msecs",
               preset_baseline->value.ref.u32, preset_get_timing->value.ref.u32);

        siResult = siToro_detector_startListMode(siDetector->detector,
                                                 preset_baseline->value.ref.u32,
                                                 preset_get_timing->value.ref.u32);

        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status, "Unable to start list mode");
            return status;
        }

        siDetector->mmc.listMode_Running = TRUE_;
    }

    return status;
}

PSL_STATIC int psl__Stop_MappingMode_1(SiToroDetector* siDetector)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    siResult = siToro_detector_stopListMode(siDetector->detector);

    siDetector->mmc.listMode_Running = FALSE_;

    status = psl__DataFormatterClose_MM1(siDetector);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to stop list mode");
        return status;
    }

    return status;
}

PSL_STATIC int psl__StartRun(int detChan, unsigned short resume, XiaDefaults *defs,
                             Detector *detector, Module *module)
{
    int status = XIA_SUCCESS;

    SiToroDetector* siDetector = detector->pslData;

    AcquisitionValue* mapping_mode;

    UNUSED(defs);

    xiaPSLBadArgs(module, detector, "psl__StartRun");

    mapping_mode = psl__GetAcquisition(siDetector, "mapping_mode");

    ASSERT(mapping_mode);

    pslLog(PSL_LOG_DEBUG, "Detector:%d Mapping Mode:%u",
           detChan, mapping_mode->value.ref.u32);

    switch (mapping_mode->value.ref.u32)
    {
        case 0:
            return psl__Start_MappingMode_0(resume, siDetector);
        case 1:
            return psl__Start_MappingMode_1(resume, siDetector);
        default:
            status = XIA_INVALID_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid mapping_mode: %u", mapping_mode->value.ref.u32);
            return status;
    }
}

PSL_STATIC int psl__StopRun(int detChan, Detector *detector, Module *module)
{
    int status = XIA_SUCCESS;

    AcquisitionValue* mapping_mode;

    SiToroDetector* siDetector = detector->pslData;

    xiaPSLBadArgs(module, detector, "psl__StopRun");

    mapping_mode = psl__GetAcquisition(siDetector, "mapping_mode");

    ASSERT(mapping_mode);

    pslLog(PSL_LOG_DEBUG, "Detector:%d Mapping Mode:%u",
           detChan, mapping_mode->value.ref.u32);

    switch (mapping_mode->value.ref.u32)
    {
        case 0:
            return psl__Stop_MappingMode_0(siDetector);
        case 1:
            return psl__Stop_MappingMode_1(siDetector);
        default:
            status = XIA_INVALID_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid mapping_mode: %u", mapping_mode->value.ref.u32);
            return status;
    }
}

PSL_STATIC int psl__mm0_mca_length(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    SiToroDetector* siDetector = detector->pslData;

    AcquisitionValue* hist_bin_count;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    hist_bin_count = psl__GetAcquisition(siDetector, "hist_bin_count");
    ASSERT(hist_bin_count);
    *((int*) value) = (int) hist_bin_count->value.ref.u32;
    return XIA_SUCCESS;
}

PSL_STATIC int psl__mm0_mca(int detChan,
                            Detector* detector, Module* module,
                            const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    SiToroDetector* siDetector = detector->pslData;

    uint32_t*         rejected;
    AcquisitionValue* hist_bin_count;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    hist_bin_count = psl__GetAcquisition(siDetector, "hist_bin_count");
    ASSERT(hist_bin_count);

    siResult = siToro_detector_updateHistogram(siDetector->detector,
                                               &siDetector->timeToNextMsec);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to update the MCA data");
        return status;
    }

    rejected = handel_md_alloc(sizeof(uint32_t) * hist_bin_count->value.ref.u32);
    if (!rejected) {
        status = XIA_NOMEM;
        pslLog(PSL_LOG_ERROR, status,
               "Error allocating memory for rejected array");
            return status;
    }

    siResult = siToro_detector_getHistogramData(siDetector->detector,
                                                value, rejected);

    handel_md_free(rejected);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the histogram accepted data");
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__mm0_baseline_length(int detChan,
                                        Detector* detector, Module* module,
                                        const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(module);
    UNUSED(name);
    UNUSED(value);

    return XIA_UNIMPLEMENTED;
}

PSL_STATIC int psl__mm0_runtime(int detChan,
                                Detector* detector, Module* module,
                                const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    SiToroDetector* siDetector = detector->pslData;

    double secs = 0;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    siResult = siToro_detector_getHistogramTimeElapsed(siDetector->detector, &secs);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the histogram time elapsed");
        return status;
    }

    *((double*) value) = secs;

    return XIA_SUCCESS;
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
    UNUSED(module);
    UNUSED(name);
    UNUSED(value);

    return XIA_UNIMPLEMENTED;
}

PSL_STATIC int psl__mm0_livetime(int detChan,
                                 Detector* detector, Module* module,
                                 const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(module);
    UNUSED(name);
    UNUSED(value);

    return XIA_UNIMPLEMENTED;
}

PSL_STATIC int psl__mm0_input_count_rate(int detChan,
                                         Detector* detector, Module* module,
                                         const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    SiToroDetector* siDetector = detector->pslData;

    double rate = 0;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    siResult = siToro_detector_getHistogramInputCountRate(siDetector->detector,
                                                          &rate);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the histogram input count rate");
        return status;
    }

    *((double*) value) = rate;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__mm0_output_count_rate(int detChan,
                                          Detector* detector, Module* module,
                                          const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    SiToroDetector* siDetector = detector->pslData;

    double rate = 0;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    siResult = siToro_detector_getHistogramOutputCountRate(siDetector->detector,
                                                           &rate);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get the histogram output count rate");
        return status;
    }

    *((double*) value) = rate;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__mm0_run_active(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    SiToroDetector* siDetector = detector->pslData;

    siBool running = SIBOOL_FALSE;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    siResult = siToro_detector_getHistogramRunning(siDetector->detector,
                                                   &running);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get MCA run status");
        return status;
    }

    *((int*) value) = running ? 1 : 0;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__mm0_module_statistics_2(int detChan,
                                            Detector* detector, Module* module,
                                            const char *name, void *value)
{
    double* stats = value;
    int     i;

    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(module);
    UNUSED(name);

    for (i = 0; i < (4 * 9); ++i)
        stats[i] = 0;
    return XIA_SUCCESS;
}

PSL_STATIC int psl__mm0_module_mca(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(module);
    UNUSED(name);
    UNUSED(value);

    return XIA_UNIMPLEMENTED;
}

PSL_STATIC int psl__mm0_mca_events(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(module);
    UNUSED(name);
    UNUSED(value);

    return XIA_UNIMPLEMENTED;
}

PSL_STATIC int psl__mm0_total_output_events(int detChan,
                                            Detector* detector, Module* module,
                                            const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    SiToroDetector* siDetector = detector->pslData;

    uint64_t pulses = 0;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    siResult = siToro_detector_getHistogramPulsesDetected(siDetector->detector,
                                                          &pulses);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get histogram pulses detected");
        return status;
    }

    *((unsigned long*) value) = (unsigned long)pulses;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__mm1_run_active(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    SiToroDetector* siDetector = detector->pslData;

    siBool running = SIBOOL_FALSE;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    siResult = siToro_detector_getListModeRunning(siDetector->detector,
                                                  &running);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to get MM1 run status");
        return status;
    }

    *((int*) value) = running ? 1 : 0;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__mm1_buffer_full_a(int detChan,
                                      Detector* detector, Module* module,
                                      const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToroDetector* siDetector = detector->pslData;

    boolean_t isFull = FALSE_;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    *((int*) value) = 0;

    status =  psl__BufferFull(siDetector, 0, &isFull);
    if (status == XIA_SUCCESS)
        *((int*) value) = isFull ? 1 : 0;

    return status;
}

PSL_STATIC int psl__mm1_buffer_full_b(int detChan,
                                      Detector* detector, Module* module,
                                      const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToroDetector* siDetector = detector->pslData;

    boolean_t isFull = FALSE_;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    *((int*) value) = 0;

    status =  psl__BufferFull(siDetector, 1, &isFull);
    if (status == XIA_SUCCESS)
        *((int*) value) = isFull ? 1 : 0;

    return status;
}

PSL_STATIC int psl__mm1_buffer_len(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToroDetector* siDetector = detector->pslData;

    uint32_t size;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    status = psl__BufferSize(siDetector, &size);

    if (status == XIA_SUCCESS) {
      *((int*) value) = (int) size;
    }

    return status;
}

PSL_STATIC int psl__mm1_buffer_done(int detChan,
                                    Detector* detector, Module* module,
                                    const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToroDetector* siDetector = detector->pslData;

    MMC1_Data*  mmd = siDetector->mmc.dataFormatter;
    MM_Buffers* buffers;
    const char* selector = (const char*) value;
    int         buffer;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    if (!mmd) {
        status = XIA_ILLEGAL_OPERATION;
        pslLog(PSL_LOG_ERROR, status,
               "No mapping mode data");
        return status;
    }

    switch (*selector)
    {
        case 'a':
        case 'A':
            buffer = 0;
            break;
        case 'b':
        case 'B':
            buffer = 1;
            break;
        default:
            status = XIA_INVALID_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Buffer value is invalid: %c", *selector);
            return status;
    }

    buffers = &mmd->buffers;

    status = psl__Buffers_Done(buffers, buffer);

    return status;
}

PSL_STATIC int psl__mm1_buffer_a(int detChan,
                                 Detector* detector, Module* module,
                                 const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToroDetector* siDetector = detector->pslData;

    MMC1_Data*  mmd = siDetector->mmc.dataFormatter;
    MM_Buffers* buffers;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    if (!mmd) {
        status = XIA_ILLEGAL_OPERATION;
        pslLog(PSL_LOG_ERROR, status,
               "No mapping mode data");
        return status;
    }

    buffers = &mmd->buffers;

    return psl__Buffers_Copy(buffers, 0, value);
}

PSL_STATIC int psl__mm1_buffer_b(int detChan,
                                 Detector* detector, Module* module,
                                 const char *name, void *value)
{
    int status = XIA_SUCCESS;

    SiToroDetector* siDetector = detector->pslData;

    MMC1_Data*  mmd = siDetector->mmc.dataFormatter;
    MM_Buffers* buffers;

    UNUSED(detChan);
    UNUSED(module);
    UNUSED(name);

    if (!mmd) {
        status = XIA_ILLEGAL_OPERATION;
        pslLog(PSL_LOG_ERROR, status,
               "No mapping mode data");
        return status;
    }

    buffers = &mmd->buffers;

    return psl__Buffers_Copy(buffers, 1, value);
}

PSL_STATIC int psl__mm1_current_pixel(int detChan,
                                      Detector* detector, Module* module,
                                      const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(detector);
    UNUSED(module);
    UNUSED(name);
    UNUSED(value);

    return XIA_SUCCESS;
}

/*
 * Get run data handlers. The order of the handlers must
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
    "sca_length",
    "sca",
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
};

#define MAPPING_MODE_COUNT         (3)
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
        NULL,   /* psl__mm0_sca_length */
        NULL,   /* psl__mm0_sca */
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
        NULL,   /* psl__mm0_list_buffer_len_ */
    },
    {
        NULL,   /* psl__mm0_mca_length */
        NULL,   /* psl__mm1_mca */
        NULL,   /* psl__mm1_baseline_length */
        NULL,   /* psl__mm1_runtime */
        NULL,   /* psl__mm1_realtime */
        NULL,   /* psl__mm1_trigger_livetime */
        NULL,   /* psl__mm1_livetime */
        NULL,   /* psl__mm1_input_count_rate */
        NULL,   /* psl__mm1_output_count_rate */
        NULL,   /* psl__mm1_sca_length */
        NULL,   /* psl__mm1_sca */
        psl__mm1_run_active,
        psl__mm1_buffer_len,
        psl__mm1_buffer_done,
        psl__mm1_buffer_full_a,
        psl__mm1_buffer_full_b,
        psl__mm1_buffer_a,
        psl__mm1_buffer_b,
        psl__mm1_current_pixel,
        NULL,   /* psl__mm1_buffer_overrun */
        NULL,   /* psl__mm1_module_statistics_2 */
        NULL,   /* psl__mm1_module_mca */
        NULL,   /* psl__mm1_mca_events */
        NULL,   /* psl__mm1_total_output_events */
        NULL,   /* psl__mm1_list_buffer_len_a */
        NULL,   /* psl__mm1_list_buffer_len_ */
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
        NULL,   /* psl__mm2_sca_length */
        NULL,   /* psl__mm2_sca */
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
        NULL,   /* psl__mm2_list_buffer_len_ */
    },
};

PSL_STATIC int psl__GetRunData(int detChan, const char *name, void *value,
                               XiaDefaults *defs, Detector *detector, Module *module)
{
    int status = XIA_SUCCESS;
    int h;

    SiToroDetector* siDetector = detector->pslData;

    AcquisitionValue* mapping_mode;

    UNUSED(defs);

    xiaPSLBadArgs(module, detector, "psl__GetRunData");

    mapping_mode = psl__GetAcquisition(siDetector, "mapping_mode");

    ASSERT(mapping_mode);

    pslLog(PSL_LOG_DEBUG, "Detector:%d Mapping Mode:%u Name:%s",
           detChan, mapping_mode->value.ref.u32, name);

    if (mapping_mode->value.ref.u32 >= MAPPING_MODE_COUNT) {
        status = XIA_INVALID_VALUE;
        pslLog(PSL_LOG_ERROR, status,
               "Invalid mapping_mode: %u", mapping_mode->value.ref.u32);
        return status;
    }

    for (h = 0; h < (int) GET_RUN_DATA_HANDLER_COUNT; ++h) {
        if (STREQ(name, getRunDataLabels[h])) {
            DoBoardOperation_FP handler;
            handler = getRunDataHandlers[mapping_mode->value.ref.u32][h];
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


PSL_STATIC int psl__SetDetectorTypeValue(int detChan, Detector *detector)
{
    pslLog(PSL_LOG_DEBUG,
           "Detector %s (%d)", detector->alias, detChan);

    return XIA_SUCCESS;
}

PSL_STATIC int psl__SpecialRun(int detChan, const char *name, void *info,
                               XiaDefaults *defaults, Detector *detector, Module *module)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    SiToroDetector* siDetector = detector->pslData;

    UNUSED(defaults);

    xiaPSLBadArgs(module, detector, "psl__SpecialRun");

    pslLog(PSL_LOG_DEBUG,
           "Detector %s (%d): %s", detector->alias, detChan, name);

    if (STREQ(name, "adc_trace")) {
        if (siDetector->osc_buffer) {
            handel_md_free(siDetector->osc_buffer);
            siDetector->osc_buffer = NULL;
        }
        siDetector->osc_buffer_length = (uint32_t) *((double*) info);
        if (siDetector->osc_buffer_length) {
            siDetector->osc_buffer = handel_md_alloc(sizeof(int16_t) *
                                                     siDetector->osc_buffer_length);
            if (!siDetector->osc_buffer) {
                siDetector->osc_buffer_length = 0;
                status = XIA_NOMEM;
                pslLog(PSL_LOG_ERROR, status,
                       "Error allocating memory for osc array");
                return status;
            }
            siResult = siToro_detector_getOscilloscopeData(siDetector->detector,
                                                           siDetector->osc_buffer,
                                                           NULL,
                                                           siDetector->osc_buffer_length);
            if (siResult != SiToro_Result_Success) {
                handel_md_free(siDetector->osc_buffer);
                siDetector->osc_buffer = NULL;
                siDetector->osc_buffer_length = 0;
                status = siToroResultToHandel(siResult);
                pslLog(PSL_LOG_ERROR, status,
                       "Error reading oscilloscope data");
                return status;
            }
        }
    }
    else if (STREQ(name, "detc-start")) {
        status = psl__DetCharacterizeStart(detChan, detector, module);
        if (status != XIA_SUCCESS) {
            pslLog(PSL_LOG_ERROR, status,
                   "Special run '%s' failed", name);
            return status;
        }
    }
    else if (STREQ(name, "detc-stop")) {
        siResult = siToro_detector_cancelCalibration(siDetector->detector);
        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Error stopping characterization");
            return status;
        }
    }
    else {
        siBool running = SIBOOL_FALSE;
        siBool successful = SIBOOL_FALSE;
        uint32_t percentage = 0;
        char progressText[100];
        uint32_t progressTextLength = sizeof(progressText);

        siResult = siToro_detector_getCalibrationProgress(siDetector->detector,
                                                          &running,
                                                          &successful,
                                                          &percentage,
                                                          progressText,
                                                          progressTextLength);

        if ((siResult != SiToro_Result_Success) &&
            ((siResult != SiToro_Result_CalibrationNotRunning))) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Error getting characterization status");
            return status;
        }
        if (STREQ(name, "detc-running")) {
            pslLog(PSL_LOG_INFO,
                   "Running: %d (%d): %s", running, successful, progressText);
            *((int*) info) = running ? 1 : 0;
        }
        else if (STREQ(name, "detc-successful")) {
            pslLog(PSL_LOG_INFO,
                   "Successful: %d (%d): %s", successful, running, progressText);
            *((int*) info) = successful ? 1 : 0;
        }
        else if (STREQ(name, "detc-percentage")) {
            *((int*) info) = (int) percentage;
        }
        else if (STREQ(name, "detc-progress-text")) {
            strcpy((char*) info, progressText);
        }
        else {
            status = XIA_BAD_NAME;
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid name: %s", name);
        }
    }

    return status;
}

/*
 * Types of the handlers for each part of the waveform.
 */
typedef SiToro_Result (*siToro_CalcPulseHandler)(SiToro_DetectorHandle handle,
                                                 double* x,
                                                 double* y,
                                                 uint32_t* length,
                                                 uint32_t maxLength);

PSL_STATIC int psl__GetPulseSize(SiToro_DetectorHandle   detector,
                                 siToro_CalcPulseHandler handler,
                                 int*                    size)
{
    int      status = XIA_SUCCESS;
    uint32_t alloc_size = 1000;

    for (;;) {
        SiToro_Result siResult;
        double*       x;
        double*       y;
        uint32_t      length = 0;

        x = handel_md_alloc(alloc_size * sizeof(double));
        if (!x) {
            status = XIA_NOMEM;
            pslLog(PSL_LOG_ERROR, status,
                   "Error allocating memory while data sizing");
            return status;
        }

        y = handel_md_alloc(alloc_size * sizeof(double));
        if (!y) {
            handel_md_free(x);
            status = XIA_NOMEM;
            pslLog(PSL_LOG_ERROR, status,
                   "Error allocating memory while data sizing");
            return status;
        }

        siResult = handler(detector, x, y, &length, alloc_size);

        handel_md_free(x);
        handel_md_free(y);

        if ((siResult != SiToro_Result_Success) &&
            (siResult != SiToro_Result_BufferTooSmall)) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Error getting pulse");
            return status;
        }

        if (siResult == SiToro_Result_Success) {
            status = XIA_SUCCESS;
            *size = (int) length;
            break;
        }

        alloc_size *= 2;
    }

    return status;
}

PSL_STATIC int psl__GetPulseX(SiToro_DetectorHandle   detector,
                              siToro_CalcPulseHandler handler,
                              double*                 x)
{
    int      status = XIA_SUCCESS;
    uint32_t alloc_size = 1000;

    /*
     * Assumes x is the correct size.
     */
    for (;;) {
        SiToro_Result siResult;
        double*       y;
        uint32_t      length = 0;

        y = handel_md_alloc(alloc_size * sizeof(double));
        if (!y) {
            status = XIA_NOMEM;
            pslLog(PSL_LOG_ERROR, status,
                   "Error allocating memory while data sizing for y array");
            return status;
        }

        siResult = handler(detector, x, y, &length, alloc_size);

        handel_md_free(y);

        if ((siResult != SiToro_Result_Success) &&
            (siResult != SiToro_Result_BufferTooSmall)) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Error getting example pulse");
            return status;
        }

        if (siResult == SiToro_Result_Success) {
            status = XIA_SUCCESS;
            break;
        }

        alloc_size *= 2;
    }

    return status;
}

PSL_STATIC int psl__GetPulseY(SiToro_DetectorHandle   detector,
                              siToro_CalcPulseHandler handler,
                              double*                 y)
{
    int      status = XIA_SUCCESS;
    uint32_t alloc_size = 1000;

    /*
     * Assumes y is the correct size.
     */
    for (;;) {
        SiToro_Result siResult;
        double*       x;
        uint32_t      length = 0;

        x = handel_md_alloc(alloc_size * sizeof(double));
        if (!x) {
            status = XIA_NOMEM;
            pslLog(PSL_LOG_ERROR, status,
                   "Error allocating memory while data sizing for x array");
            return status;
        }

        siResult = handler(detector, x, y, &length, alloc_size);

        handel_md_free(x);

        if ((siResult != SiToro_Result_Success) &&
            (siResult != SiToro_Result_BufferTooSmall)) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Error getting example pulse");
            return status;
        }

        if (siResult == SiToro_Result_Success) {
            status = XIA_SUCCESS;
            break;
        }

        alloc_size *= 2;
    }

    return status;
}

PSL_STATIC int psl__GetSpecialRunData(int detChan, const char *name, void *value, XiaDefaults *defaults,
                                      Detector *detector, Module *module)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    SiToroDetector* siDetector = detector->pslData;

    UNUSED(defaults);

    xiaPSLBadArgs(module, detector, "psl__GetSpecialRunData");

    pslLog(PSL_LOG_DEBUG,
           "Detector %s (%d): %s", detector->alias, detChan, name);

    if (STREQ(name, "adc_trace")) {
        if (siDetector->osc_buffer) {
            int16_t*      in = siDetector->osc_buffer;
            unsigned int* out = ((unsigned int*) value);
            uint32_t s;
            for (s = 0; s < siDetector->osc_buffer_length; ++s) {
                *out++ = (unsigned int) (*in++) + (0x10000 / 2);
            }
            handel_md_free(siDetector->osc_buffer);
            siDetector->osc_buffer = NULL;
            siDetector->osc_buffer_length = 0;
        }
        else {
            status = XIA_BAD_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Error no osc length set");
            return status;
        }
    }
#ifdef DISABLE_ADC_TRACE_RESET_BLANK
    else if (STREQ(name, "osc-get-reset-blanked")) {
        if (siDetector->osc_buffer_length) {
            int16_t* in = handel_md_alloc(sizeof(int16_t) * siDetector->osc_buffer_length);
            int*     out = ((int*) value);
            uint32_t s;
            if (!in) {
                status = XIA_NOMEM;
                pslLog(PSL_LOG_ERROR, status,
                       "Error allocating memory for osc array");
                return status;
            }
            siResult = siToro_detector_getOscilloscopeData(siDetector->detector,
                                                           NULL,
                                                           in,
                                                           siDetector->osc_buffer_length);
            if (siResult != SiToro_Result_Success) {
                handel_md_free(in);
                status = siToroResultToHandel(siResult);
                pslLog(PSL_LOG_ERROR, status,
                       "Error reading oscilloscope data");
                return status;
            }
            for (s = 0; s < siDetector->osc_buffer_length; ++s) {
                *out++ = *in++;
            }
        }
        else {
            status = XIA_BAD_VALUE;
            pslLog(PSL_LOG_ERROR, status,
                   "Error no osc length set");
            return status;
        }
    }
#endif
    else if (STREQ(name, "detc-progress-text-size")) {
        *((int*) value) = SITORO_PROGRESS_TEXT_SIZE;
    }
    else if (STREQ(name, "detc-string-size")) {
        char* calString = NULL;

        siResult = siToro_detector_getCalibrationData(siDetector->detector,
                                                      &calString);

        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Error reading detector characterization string");
            return status;
        }

        *((int*) value) = (int) strlen(calString);
    }
    else if (STREQ(name, "detc-string")) {
        char* calString = NULL;

        siResult = siToro_detector_getCalibrationData(siDetector->detector,
                                                      &calString);

        if (siResult != SiToro_Result_Success) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Error reading detector characterization string");
            return status;
        }

        memcpy(value, calString, strlen(calString));
    }
    else if (STREQ(name, "detc-example-pulse-size")) {
        int size = 0;
        status = psl__GetPulseSize(siDetector->detector,
                                   siToro_detector_getCalibrationExamplePulse,
                                   &size);
        *((int*) value) = size;
    }
    else if (STREQ(name, "detc-example-pulse-x")) {
        double*  x = (double*) value;
        status = psl__GetPulseX(siDetector->detector,
                                siToro_detector_getCalibrationExamplePulse,
                                x);
    }
    else if (STREQ(name, "detc-example-pulse-y")) {
        double*  y = (double*) value;
        status = psl__GetPulseY(siDetector->detector,
                                siToro_detector_getCalibrationExamplePulse,
                                y);
    }
    else if (STREQ(name, "detc-model-pulse-size")) {
        int size = 0;
        status = psl__GetPulseSize(siDetector->detector,
                                   siToro_detector_getCalibrationModelPulse,
                                   &size);
        *((int*) value) = size;
    }
    else if (STREQ(name, "detc-model-pulse-x")) {
        double*  x = (double*) value;
        status = psl__GetPulseX(siDetector->detector,
                                siToro_detector_getCalibrationModelPulse,
                                x);
    }
    else if (STREQ(name, "detc-model-pulse-y")) {
        double*  y = (double*) value;
        status = psl__GetPulseY(siDetector->detector,
                                siToro_detector_getCalibrationModelPulse,
                                y);
    }
    else if (STREQ(name, "detc-final-pulse-size")) {
        int size = 0;
        status = psl__GetPulseSize(siDetector->detector,
                                   siToro_detector_getCalibrationFinalPulse,
                                   &size);
        *((int*) value) = size;
    }
    else if (STREQ(name, "detc-final-pulse-x")) {
        double*  x = (double*) value;
        status = psl__GetPulseX(siDetector->detector,
                                siToro_detector_getCalibrationFinalPulse,
                                x);
    }
    else if (STREQ(name, "detc-final-pulse-y")) {
        double*  y = (double*) value;
        status = psl__GetPulseY(siDetector->detector,
                                siToro_detector_getCalibrationFinalPulse,
                                y);
    }
    else {
        siBool running = SIBOOL_FALSE;
        siBool successful = SIBOOL_FALSE;
        uint32_t percentage = 0;
        char progressText[SITORO_PROGRESS_TEXT_SIZE];
        uint32_t progressTextLength = sizeof(progressText);

        siResult = siToro_detector_getCalibrationProgress(siDetector->detector,
                                                          &running,
                                                          &successful,
                                                          &percentage,
                                                          progressText,
                                                          progressTextLength);

        if ((siResult != SiToro_Result_Success) &&
            ((siResult != SiToro_Result_CalibrationNotRunning))) {
            status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Error getting characterization status");
            return status;
        }
        if (STREQ(name, "detc-running")) {
            pslLog(PSL_LOG_INFO,
                   "Running: %d (%d): %s", running, successful, progressText);
            *((int*) value) = running ? 1 : 0;
        }
        else if (STREQ(name, "detc-successful")) {
            pslLog(PSL_LOG_INFO,
                   "Successful: %d (%d): %s", successful, running, progressText);
            *((int*) value) = successful ? 1 : 0;
        }
        else if (STREQ(name, "detc-percentage")) {
            *((int*) value) = (int) percentage;
        }
        else if (STREQ(name, "detc-progress-text")) {
            strcpy((char*) value, progressText);
        }
        else {
            status = XIA_BAD_NAME;
            pslLog(PSL_LOG_ERROR, status,
                   "Invalid name: %s", name);
        }
    }

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

        return psl__UnloadDetCharacterization(detector, filename);
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__SetupModule(Module *module)
{
    SiToro_Result siResult;
    SiToroModule* siModule = NULL;

    uint32_t v1;
    uint32_t v2;
    uint32_t v3;

    pslLog(PSL_LOG_DEBUG, "Module %s", module->alias);

    ASSERT(module);
    ASSERT(module->pslData == NULL);

    psl__SetupSiToro();

    siModule = handel_md_alloc(sizeof(SiToroModule));
    if (siModule == NULL) {
        psl__EndSiToro();
        pslLog(PSL_LOG_ERROR, XIA_NOMEM,
               "No memory for SiToro module PSL data");
        return XIA_NOMEM;
    }

    /*
     * The change to siToro to manage the "static type" safety of the handle
     * breaks the initialiser and so the extra nested braces and knowledge of
     * the make up the handle's is needed.
     */
    siModule->instrument.instrument = NULL;
    siModule->instrumentValid = FALSE_;
    siModule->card.card = NULL;
    siModule->cardValid = FALSE_;

    module->pslData = siModule;

    /*
     * SiToro has a layer called an instrument. No idea why this layer exist in
     * a detector API or how it maps to a physical arrangement of SiToro
     * hardware. There is a range of gating and spatial control interfaces at
     * the instrument level which must allow ganging of the cards in some way
     * for grid type sampling how-ever the term instrument is awkward and
     * misleading when considered in the more generic world of
     * "instruments". Having it as a separate API would have made the SiToro
     * API cleaner.
     *
     * Just merge the instrument and card into one, ie a module.
     */
    siModule->instrumentId = 0;
    siResult = siToro_instrument_open((uint32_t) siModule->instrumentId,
                                      &siModule->instrument);

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        psl__EndModule(module);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to open the FalconX instrument: %d", siModule->instrumentId);
        return status;
    }

    siModule->instrumentValid = TRUE_;

    siModule->cardId = 0;
    siResult = siToro_card_open(siModule->instrument, (uint32_t) siModule->cardId, &siModule->card);

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        psl__EndModule(module);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to open the FalconX card: %d", siModule->cardId);
        return status;
    }

    siModule->cardValid = TRUE_;

    siToro_getApiVersion(&v1, &v2, &v3);

    siModule->apiVersionMajor = v1;
    siModule->apiVersionMinor = v2;
    siModule->apiVersionRevision = v3;

    /*
     * Request the card details and load the various module settings.
     */

#ifdef THIS_FIRST_CALL_IS_BROKEN
    siResult = siToro_instrument_getCardSerialNumber(siModule->instrument,
                                                     siModule->cardId,
                                                     &siModule->serialNum);
#else
    siResult = siToro_card_getSerialNumber(siModule->card, &siModule->serialNum);
#endif

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        psl__EndModule(module);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting the card's serial number");
        return status;
    }

    pslLog(PSL_LOG_INFO,
           "Serial number: %d", siModule->serialNum);

    siResult = siToro_card_getNumDetectors(siModule->card, &v1);

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        psl__EndModule(module);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting the card's number of detectors");
        return status;
    }

    siModule->detChannels = (int) v1;

    siResult = siToro_card_getDspVersion(siModule->card, &v1, &v2, &v3);

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        psl__EndModule(module);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting the DSP's software version");
        return status;
    }

    siModule->geminiVerMajor = v1;
    siModule->geminiVerMinor = v2;
    siModule->geminiVerRevision = v3;

    siResult = siToro_card_getFpgaVersion(siModule->card, &v1);

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        psl__EndModule(module);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting the DSP's software version");
        return status;
    }

    siModule->fpgaVersion = v1;

    pslLog(PSL_LOG_INFO,
           "SIToro Versions: API:%lu.%lu.%lu Gemini:%lu.%lu.%lu FPGA:%08lx",
           siModule->apiVersionMajor,
           siModule->apiVersionMinor,
           siModule->apiVersionRevision,
           siModule->geminiVerMajor,
           siModule->geminiVerMinor,
           siModule->geminiVerRevision,
           siModule->fpgaVersion);

    return XIA_SUCCESS;
}

PSL_STATIC int psl__EndModule(Module *module)
{
    SiToro_Result siResult = SiToro_Result_Success;

    if (module && module->pslData) {
        SiToroModule* siModule = siModule = module->pslData;

        pslLog(PSL_LOG_DEBUG, "Module %s", module->alias);

        if (siModule->cardValid) {
            siModule->cardValid = FALSE_;
            siResult = siToro_card_close(siModule->card);
            if (siResult != SiToro_Result_Success) {
                pslLog(PSL_LOG_ERROR,
                       siToroResultToHandel(siResult),
                       "psl__EndModule: %s",
                       "Error closing the card");
            }
        }

        if (siModule->instrumentValid) {
            siModule->instrumentValid = FALSE_;
            siResult = siToro_instrument_close(siModule->instrument);
            if (siResult != SiToro_Result_Success) {
                pslLog(PSL_LOG_ERROR,
                       siToroResultToHandel(siResult),
                       "psl__EndModule: %s",
                       "Error closing the instrument");
            }
        }

        handel_md_free(module->pslData);
        module->pslData = NULL;
    }

    psl__EndSiToro();

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to close the FalconX card: %d", 0);
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__SetupDetChan(int detChan, Detector *detector, Module *module)
{
    int status;

    SiToro_Result siResult;
    SiToroModule* siModule = NULL;
    SiToroDetector* siDetector = NULL;

    int modDetChan;

    double value;

    int av;

    pslLog(PSL_LOG_DEBUG,
           "Detector %s (%d)", detector->alias, detChan);

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

    siModule = module->pslData;

    modDetChan = xiaGetModDetectorChan(detChan);

    if (modDetChan == 999) {
        pslLog(PSL_LOG_ERROR, XIA_INVALID_DETCHAN,
               "Unable to get the FalconX module channel for detector channel: %d",
               detChan);
        return XIA_INVALID_DETCHAN;
    }

    siDetector = handel_md_alloc(sizeof(SiToroDetector));
    if (siDetector == NULL) {
        pslLog(PSL_LOG_ERROR, XIA_NOMEM,
               "No memory for SiToro detector PSL data");
        return XIA_NOMEM;
    }

    /*
     * <sigh> Need to use memset to clear this struct.
     */
    memset(siDetector, 0, sizeof(*siDetector));

    siResult = siToro_detector_open(siModule->card, (uint32_t) modDetChan, &siDetector->detector);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        handel_md_free(siDetector);
        pslLog(PSL_LOG_ERROR, status,
               "Unable to open the FalconX detector channel: %d", modDetChan);
        return status;
    }

    detector->pslData = siDetector;

    /*
     * Set up the detector's local copy.
     */
    for (av = 0; av < SI_DET_NUM_OF_ACQ_VALUES; ++av) {
        siDetector->acqValues[av] = DEFAULT_ACQ_VALUES[av];
        status = psl__SetAcqValue(&siDetector->acqValues[av],
                                  DEFAULT_ACQ_VALUES[av].defaultValue);
    }

    /*
     * Set up the ACQ values from the defaults.
     */
    status = psl__ReloadDefaults(siDetector);
    if (status != XIA_SUCCESS) {
        detector->pslData = NULL;
        siToro_detector_close(siDetector->detector);
        handel_md_free(siDetector);
        pslLog(PSL_LOG_ERROR, status,
               "Detector channel default load failed: %d", modDetChan);
        return status;
    }

    siDetector->detChan = detChan;

    status = xiaGetDefaultStrFromDetChan(detChan, siDetector->defaultStr);
    if (status != XIA_SUCCESS) {
        detector->pslData = NULL;
        siToro_detector_close(siDetector->detector);
        handel_md_free(siDetector);
        pslLog(PSL_LOG_ERROR, status,
               "Detector channel default string failed: %d", modDetChan);
        return status;
    }

    /*
     * Load the detector characterization string if there is one.
     */
    status = psl__LoadDetCharacterization(detChan, detector, module);

    if ((status != XIA_SUCCESS) && (status != XIA_NOT_FOUND)) {
        pslLog(PSL_LOG_ERROR, status,
               "Error setting the detector characterization string: %s (%d)",
               detector->alias, detChan);
        return status;
    }

    /*
     * Write the current default values into the hardware.
     */
    status = psl__UserSetup(detChan, detector, module);
    if (status != XIA_SUCCESS) {
        detector->pslData = NULL;
        siToro_detector_close(siDetector->detector);
        handel_md_free(siDetector);
        pslLog(PSL_LOG_ERROR, status,
               "User setup of detector channel failed: %d", modDetChan);
        return status;
    }

    /*
     * Get the default values from the hardware.
     */
    for (av = 0; av < SI_DET_NUM_OF_ACQ_VALUES; ++av)
        psl__GetValueByIndex(detector, siDetector, av, &value);

    siDetector->valid_acq_values = TRUE_;

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
        SiToroDetector* siDetector = detector->pslData;
        SiToro_Result siResult;

        siResult = siToro_detector_close(siDetector->detector);

        if (siDetector->osc_buffer)
            handel_md_free(siDetector->osc_buffer);

        handel_md_free(siDetector);

        detector->pslData = NULL;

        if (siResult != SiToro_Result_Success) {
            int status = siToroResultToHandel(siResult);
            pslLog(PSL_LOG_ERROR, status,
                   "Error closing previously open detector channel: %d", detChan);
            return status;
        }
    }

    return XIA_SUCCESS;
}


PSL_STATIC int psl__UserSetup(int detChan, Detector *detector, Module *module)
{
    int status;

    XiaDefaults *defaults = NULL;

    XiaDaqEntry* entry;

    SiToroDetector* siDetector = detector->pslData;

    xiaPSLBadArgs(module, detector, "psl__UserSetup");

    pslLog(PSL_LOG_DEBUG,
           "Detector %s (%d)", detector->alias, detChan);

    defaults = xiaGetDefaultFromDetChan(detChan);

    entry = defaults->entry;

    /*
     * Must be at least one entry?
     */

    ASSERT(entry);

    while (entry) {
        if (strlen(entry->name) > 0) {
            AcquisitionValue* acq = psl__GetAcquisition(siDetector, entry->name);

            if (!acq) {
                status = XIA_UNKNOWN_VALUE;
                pslLog(PSL_LOG_ERROR, status,
                       "invalid entry: %s\n", entry->name);
                return status;
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

    pslLog(PSL_LOG_DEBUG,
           "Finished %s (%d)", detector->alias, detChan);

    return XIA_SUCCESS;
}


PSL_STATIC int psl__GetDefaultAlias(char *alias, char **names, double *values)
{
    int i;
    int def_idx;

    const char *aliasName = "defaults_falconx";

    ASSERT(alias  != NULL);
    ASSERT(names  != NULL);
    ASSERT(values != NULL);

    for (i = 0, def_idx = 0; i < SI_DET_NUM_OF_ACQ_VALUES; i++) {
      if (DEFAULT_ACQ_VALUES[i].flags & PSL_ACQ_HAS_DEFAULT) {

        strcpy(names[def_idx], DEFAULT_ACQ_VALUES[i].name);
        values[def_idx] = DEFAULT_ACQ_VALUES[i].defaultValue;
        def_idx++;
      }
    }

    strcpy(alias, aliasName);

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


PSL_STATIC unsigned int psl__GetNumDefaults(void)
{
    int i;
    unsigned int count = 0;

    for (i = 0; i < SI_DET_NUM_OF_ACQ_VALUES; i++) {
      if (DEFAULT_ACQ_VALUES[i].flags & PSL_ACQ_HAS_DEFAULT) {
        count++;
      }
    }

    return count;
}


PSL_STATIC boolean_t psl__CanRemoveName(const char *name)
{
    UNUSED(name);

    return FALSE_;
}

PSL_STATIC int psl__DetCharacterizeStart(int detChan, Detector* detector, Module* module)
{
    int status;

    SiToro_Result siResult;

    SiToroDetector* siDetector;

    char item[MAXITEM_LEN];
    char firmware[MAXITEM_LEN];
    char filename[MAXITEM_LEN];

    siDetector = detector->pslData;

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

    status = xiaGetFirmwareItem(firmware, 0, "filename", filename);

    if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_ERROR, status,
               "Error getting the filename from the firmware set alias: %s",
               firmware);
        return status;
    }

    /*
     * SiToro calls detector characterization calibration.
     */
    siResult = siToro_detector_startCalibration(siDetector->detector);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Error starting characterization");
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__UnloadDetCharacterization(Detector* detector, const char *filename)
{
    int status = XIA_SUCCESS;

    SiToro_Result siResult;

    SiToroDetector* siDetector;

    char* calString = NULL;

    FILE* dcFile = NULL;

    siDetector = detector->pslData;

    siResult = siToro_detector_getCalibrationData(siDetector->detector,
                                                  &calString);

    if (siResult != SiToro_Result_Success) {
        status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Error reading detector characterization string");
        return status;
    }

    xiaLog(XIA_LOG_INFO, "psl__UnloadDetCharacterization",
           "write detector characterization file: %s", filename);

    dcFile = xia_file_open(filename, "w");

    if (dcFile == NULL) {
        status = XIA_NOT_FOUND;
        xiaLog(XIA_LOG_ERROR, status, "psl__UnloadDetCharacterization",
               "Could not open: %s", filename);
    }
    else {
        size_t toWrite = strlen(calString);
        size_t haveWritten = 0;
        size_t written =  0;

        while (toWrite) {
            written = fwrite(calString + haveWritten, 1, toWrite, dcFile);

            if (((ssize_t) written) < 0) {
                xia_file_close(dcFile);
                status = XIA_BAD_FILE_WRITE;
                xiaLog(XIA_LOG_ERROR, status, "psl__UnloadDetCharacterization",
                       "Writing to detector characterization file failed: %s: (%d) %s",
                       filename, errno, strerror(errno));
                return status;
            }

            haveWritten += written;
            toWrite -= written;
        }

        xia_file_close(dcFile);
    }

    return status;
}

PSL_STATIC int psl__LoadDetCharacterization(int detChan, Detector *detector, Module *module)
{
    int status;

    SiToroDetector* siDetector = (SiToroDetector*) detector->pslData;

    char item[MAXITEM_LEN];
    char firmware[MAXITEM_LEN];
    char filename[MAXITEM_LEN];

    /*
     * Check a firmware set is present for this channel.
     */
    sprintf(item, "firmware_set_chan%d", detChan);

    /*
     * It is not an error to not have a detector configuration. It just
     * means the detector's SiToro calibration has not been run.
     */

    status = xiaGetModuleItem(module->alias, item, firmware);

    xiaLog(XIA_LOG_INFO, "psl__LoadDetCharacterization",
           "module item[%s] = %s", item, firmware);

    if ((status == XIA_SUCCESS) && !STREQ(firmware, "null")) {
        status = xiaGetFirmwareItem(firmware, 0, "filename", filename);

        if (status == XIA_SUCCESS) {
            SiToro_Result siResult;

            char newFile[MAXFILENAME_LEN];

            FILE* dcFile;

            char* detCharacterizationStr;

            xiaLog(XIA_LOG_INFO, "psl__LoadDetCharacterization",
                   "read detector characterization: %s", filename);

            dcFile = xiaFindFile(filename, "rb", newFile);

            if (dcFile) {
                struct stat sb;

                if (stat(newFile, &sb) < 0) {
                    xiaLog(XIA_LOG_ERROR, XIA_NOT_FOUND, "psl__LoadDetCharacterization",
                           "Could not stat: %s", newFile);
                }

                detCharacterizationStr = handel_md_alloc((size_t) (sb.st_size + 1));

                if (detCharacterizationStr == NULL) {
                    xiaLog(XIA_LOG_ERROR, XIA_NOMEM, "psl__LoadDetCharacterization",
                           "No memory when loading detector characterizartion string: %d (%s)",
                           (int) sb.st_size, filename);
                }
                else {
                    size_t objsRead;

                    memset(detCharacterizationStr, 0, (size_t) (sb.st_size + 1));

                    objsRead = fread(detCharacterizationStr, (size_t) sb.st_size, 1, dcFile);

                    if (objsRead != 1) {
                        xiaLog(XIA_LOG_ERROR, XIA_BAD_FILE_READ, "psl__LoadDetCharacterization",
                               "Could not read: %s", filename);
                        handel_md_free(detCharacterizationStr);
                    }

                    siResult = siToro_detector_setCalibrationData(siDetector->detector,
                                                                  detCharacterizationStr);

                    handel_md_free(detCharacterizationStr);

                    if (siResult != SiToro_Result_Success) {
                        status = siToroResultToHandel(siResult);
                        pslLog(PSL_LOG_ERROR, status,
                               "Error setting the detector characterization string: %s (%d)",
                               detector->alias, detChan);
                    }
                }

                xia_file_close(dcFile);
            }
        }
    }

    return status;
}

PSL_STATIC int psl__BoardOp_Apply(int detChan, Detector* detector, Module* module,
                                  const char *name, void *value)
{
    UNUSED(detChan);
    UNUSED(name);
    UNUSED(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_Apply");

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

PSL_STATIC int psl__BoardOp_GetSiToroAPIVersion(int detChan, Detector* detector, Module* module,
                                                const char *name, void *value)
{
    SiToroModule* siModule;

    char *version = (char *)value;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetSiToroAPIVersion");

    siModule = module->pslData;

    sprintf(version, "%lu.%lu.%lu",
            siModule->apiVersionMajor,
            siModule->apiVersionMinor,
            siModule->apiVersionRevision);

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetSiToroBuildDate(int detChan, Detector* detector, Module* module,
                                               const char *name, void *value)
{
    const char *buildDate;

    UNUSED(detChan);
    UNUSED(name);
    UNUSED(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetSiToroBuildDate");

    buildDate = siToro_getLibraryBuildDate();

    strcpy((char*) value, buildDate);

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetBootLoaderVersion(int detChan, Detector* detector, Module* module,
                                                 const char *name, void *value)
{
    SiToro_Result siResult;

    SiToroModule* siModule;

    char *version = (char *)value;

    uint32_t major;
    uint32_t minor;
    uint32_t revision;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetBootLoaderVersion");

    siModule = module->pslData;

    siResult = siToro_card_getBootLoaderVersion(siModule->card,
                                                &major, &minor, &revision);

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting boot loader version");
        return status;
    }

    sprintf(version, "%u.%u.%u", major, minor, revision);

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetCardName(int detChan, Detector* detector, Module* module,
                                        const char *name, void *value)
{
    SiToro_Result siResult;

    SiToroModule* siModule;

    char *cardName = (char *)value;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetCardName");

    siModule = module->pslData;

    siResult = siToro_card_getName(siModule->card, cardName, 32);

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting the card's name");
        return status;
    }

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetCardChannels(int detChan, Detector* detector, Module* module,
                                            const char *name, void *value)
{
    SiToroModule* siModule;

    int *channels = (int *)value;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetCardChannels");

    siModule = module->pslData;

    *channels = siModule->detChannels;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetSerialNumber(int detChan, Detector* detector, Module* module,
                                            const char *name, void *value)
{
    SiToroModule* siModule;

    char *serialNum = (char *)value;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetSerialNumber");

    siModule = module->pslData;

    sprintf(serialNum, "%u", siModule->serialNum);

    return XIA_SUCCESS;
}


PSL_STATIC int psl__BoardOp_GetFPGAVersion(int detChan, Detector* detector, Module* module,
                                           const char *name, void *value)
{
    SiToroModule* siModule;

    unsigned long *version = (unsigned long *)value;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetFPGAVersion");

    siModule = module->pslData;

    *version = siModule->fpgaVersion;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetAppId(int detChan, Detector* detector, Module* module,
                                     const char *name, void *value)
{
    SiToro_Result siResult;

    SiToroModule* siModule;

    int *id = (int *)value;

    uint8_t slot = 0;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetAppId");

    siModule = module->pslData;

    siResult = siToro_card_getCurrentDspSlot(siModule->card, &slot);

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting the card's app slot");
        return status;
    }

    *id = (int) slot;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetFPGAId(int detChan, Detector* detector, Module* module,
                                      const char *name, void *value)
{
    SiToro_Result siResult;

    SiToroModule* siModule;

    int *id = (int *)value;

    uint8_t slot = 0;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetFPGAId");

    siModule = module->pslData;

    siResult = siToro_card_getCurrentFpgaSlot(siModule->card, &slot);

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting card details");
    }

    *id = (int) slot;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetFPGARunning(int detChan, Detector* detector, Module* module,
                                           const char *name, void *value)
{
    SiToro_Result siResult;

    SiToroModule* siModule;

    int *runState = (int *)value;

    siBool state = SIBOOL_FALSE;

    UNUSED(detChan);
    UNUSED(name);

    ASSERT(value);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetFPGARunning");

    siModule = module->pslData;

    siResult = siToro_card_getFpgaRunning(siModule->card, &state);

    if (siResult != SiToro_Result_Success) {
        int status = siToroResultToHandel(siResult);
        pslLog(PSL_LOG_ERROR, status,
               "Error getting card details");
        return status;
    }

    *runState = state ? 1 : 0;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetSiToroDetector(int detChan, Detector* detector, Module* module,
                                              const char *name, void *value)
{
    SiToroDetector* siDetector;
    SiToro_DetectorHandle *p = value;

    UNUSED(detChan);
    UNUSED(value);
    UNUSED(name);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetSitoroDetector");

    siDetector = detector->pslData;

    ASSERT(siDetector);

    *p = siDetector->detector;

    return XIA_SUCCESS;
}

PSL_STATIC int psl__BoardOp_GetConnected(int detChan, Detector* detector, Module* module,
                                         const char *name, void *value)
{
    SiToroDetector* siDetector;
    siBool isOpen;

    UNUSED(detChan);
    UNUSED(value);
    UNUSED(name);

    xiaPSLBadArgs(module, detector, "psl__BoardOp_GetConnected");

    siDetector = detector->pslData;
    ASSERT(siDetector);

    isOpen = siToro_detector_isOpen(siDetector->detector);
    *((int*)value) = isOpen ? 1 : 0;

    return XIA_SUCCESS;
}
