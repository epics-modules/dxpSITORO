/*
 * Copyright (c) 2015 XIA LLC
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


#ifndef FALCONXN_PSL_H
#define FALCONXN_PSL_H

/*
 * This code only works with the SiToro interface version 2.1.3 or later. The
 * interface was completely changed at this point in time and there is no
 * compatibility. A wrapper was provided but never used.
 */

#include <sinc.h>

#include "falconx_mm.h"

#define FALCONXN_MAX_CHANNELS (8)

/*
 * Timeout to wait for an ADC trace to be sent inseconds.
 */
#define FALCONXN_ADC_TRACE_TIMEOUT (10)

/*
 * Max number of ADC samples supported.
 */
#define FALCONXN_MAX_ADC_SAMPLES (0x80000)

/*
 * Timeout to wait for a channel state change after sending a command.
 */
#define FALCONXN_CHANNEL_STATE_TIMEOUT (5)

/*
 * Timeout to wait for a response.
 */
#define FALCONXN_RESPONSE_TIMEOUT (2)

/*
 * Sinc response handle. This allows us to map the response back to
 * the type and so the call to free a response.
 */
typedef struct
{
    int   channel;
    int   type;
    void* response;
} Sinc_Response;

/* The state of the Sinc channel. This tracks the Sinc parameter channel.state
 * and allows the PSL to remember whether it started a run, characterization, etc.
 */
typedef enum {
    ChannelDisconnected,
    ChannelReady,
    ChannelError,
    ChannelADC,
    ChannelHistogram,
    ChannelListMode,
    ChannelCharacterizing
} ChannelState;

typedef enum {
    CalibrationNone,
    CalibrationNeedRefresh,
    CalibrationReady
} CalibrationState;

#define DAC_OFFSET_MIN        -2048
#define DAC_OFFSET_MAX         2047
#define DISCHARGE_THRESH_MIN       0.0
#define DISCHARGE_THRESH_MAX   65535.0
#define DISCHARGE_PERIOD_MIN       0.0
/* Defined by SI as (2 ^ 15) - 1 * (~16.7ns) and then I rounded up again. */
#define DISCHARGE_PERIOD_MAX  547209.0

#define ADC_COUNT_MAX              65535.0 /* 16bit ADC */
#define ADC_INPUT_RANGE_PERCENT    0.8     /* 80% of the ADC is usable. 10% head and floor room */
#define ADC_INPUT_RANGE_MV         2250.0  /* valid input range milli-volts */
#define ADC_DEADZONE_COUNT         (ADC_COUNT_MAX * ((1.0 - ADC_INPUT_RANGE_PERCENT) / 2.0))
#define ADC_GAIN_MULTIPLIER        16.0
#define ADC_GAIN_MIN               (1.0)
#define ADC_GAIN_MAX               (ADC_GAIN_MULTIPLIER)

#define SCALE_FACTOR_MIN   0.5 /* min pulse scale factor */
#define SCALE_FACTOR_MAX 200.0 /* arbitrary max scale factor */

#define MIN_MCA_CHANNELS    128.0
#define MAX_MCA_CHANNELS    4096.0

#define MM1_MAX_BIN_COUNT          (32 * 1024)


/** FORWARD DECLARATION **/

struct _FalconXNDetector;
typedef struct _FalconXNDetector FalconXNDetector;
struct _FalconXNModule;
typedef struct _FalconXNModule FalconXNModule;
typedef struct _AcquisitionValue AcquisitionValue;
typedef struct _MappingModeControl MappingModeControl;

/** FUNCTION POINTERS **/

typedef int (*AcqValue_FP)(Module*           module,
                           Detector*         detector,
                           int               channel,
                           FalconXNDetector* fDetector,
                           XiaDefaults*      defaults,
                           const char        *name,
                           double*           value,
                           boolean_t         read);
typedef int (*SynchAcqValue_FP)(int          detChan,
                                int          channel,
                                Module*      m,
                                Detector*    det,
                                XiaDefaults* defs);
typedef boolean_t (*SupportedAcqValue_FP)(FalconXNDetector* fDetector);
typedef int (*DoBoardOperation_FP)(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value);
typedef int (*DoRunData_FP)(int detChan,
                            int modChan, Module* module,
                            const char *name, void *value);

/** STRUCTURES **/

/* Types of Acquisition Values */
typedef enum acqValueTypes {
    acqFloat,
    acqInt,
    acqBool,
    acqString
} acqValueTypes;

typedef struct acqValue {
    acqValueTypes type;
    union {
        double       f;
        int64_t      i;
        boolean_t    b;
        const char*  s;
    } ref;  /* The handler's private shadow if it needs one. */
} acqValue;

#define PSL_ACQ_EMPTY       (0)
#define PSL_ACQ_READ_ONLY   (1 << 0) /* There is no set */
#define PSL_ACQ_RUNNING     (1 << 1) /* Handel needs to be in the running state. */
#define PSL_ACQ_HAS_DEFAULT (1 << 2) /* There is a default value. */
#define PSL_ACQ_LOCAL       (1 << 3) /* Local, not present in FalconXN. */

#define PSL_ACQ_FLAG_SET(_a, _m) (((_a)->flags & (_m)) != 0)

/* Acquisition Values */
struct _AcquisitionValue {
    const char*          name;
    double               defaultValue;
    acqValueTypes        type;
    uint32_t             flags;
    AcqValue_FP          handler;
    SynchAcqValue_FP     sync;
    SupportedAcqValue_FP supported;
};

/* A generic board operation */
typedef struct _BoardOperation {
    const char         *name;
    DoBoardOperation_FP fn;
} BoardOperation;

/*
 * The stats we keep in realtime. Written into the pixel headers.
 */
#define FALCONXN_STATS_NUMOF            (14)
/* module_statistics_2 stats */
#define FALCONXN_STATS_TIME_ELAPSED      (0)
#define FALCONXN_STATS_TRIGGER_LIVETIME  (1)
#define FALCONXN_STATS_ENERGY_LIVETIME   (2)
#define FALCONXN_STATS_TRIGGERS          (3)
#define FALCONXN_STATS_MCA_EVENTS        (4)
#define FALCONXN_STATS_INPUT_COUNT_RATE  (5)
#define FALCONXN_STATS_OUTPUT_COUNT_RATE (6)
#define FALCONXN_STATS_RESERVED_7        (7)
#define FALCONXN_STATS_RESERVED_8        (8)
/* other stats */
#define FALCONXN_STATS_SAMPLES_DETECTED  (9)
#define FALCONXN_STATS_SAMPLES_ERASED   (10)
#define FALCONXN_STATS_PULSES_ACCEPTED  (11)
#define FALCONXN_STATS_PULSES_REJECTED  (12)
#define FALCONXN_STATS_DEADTIME         (13)

/*
 * The SiToro Module PSL Data. It contains the detectors.
 */
struct _FalconXNModule {

    /* SINC protocol is TCP per card.
     */
    char hostAddress[32];
    int  portBase;
    int  timeout;

    /* The card's serial number.*/
    uint32_t serialNum;

    /* The number of channels in the card. */
    int detChannels;

    /* Number of runs made by the module. This is saved in mm1 buffer
     * headers to allow correlating data across a module by run
     * number.
     */
    uint32_t runNumber;

    /* Table of active channels. */
    boolean_t channelActive[FALCONXN_MAX_CHANNELS];

    /* Lock for shared data in this structure.
     */
    handel_md_Mutex lock;

    /* Module's Sinc data receiver thread.
     */
    handel_md_Thread receiver;

    /* Module's receive thread controls.
     */
    boolean_t receiverActive;
    boolean_t receiverRunning;

    /* Module's receiver event.
     */
    handel_md_Event receiverEvent;

    /* Lock to make all sends sequential. The event is awaited by the
     * sender and signalled by the receive processor.
     */
    handel_md_Mutex sendLock;
    handel_md_Event sendEvent;
    int             sendStatus;

    /* Response comes from the receive thread, which decodes the data
     * from the FalconXN and places the response here. Commands and
     * responses are sequential so we only need a single instance.
     */
    Sinc_Response response;

    /* One Sinc connection for the module.
     */
    Sinc sinc;
};

/*
 * The SiToro Detector PSL Data.
 */

struct _FalconXNDetector {
    /* The Sinc channel is the module's detector channel */
    int modDetChan;

    /* The detector channel */
    int detChan;

    /* Set to true once all ACQ values have been set. */
    boolean_t valid_acq_values;

    /* Lock for shared data in this structure. */
    handel_md_Mutex lock;

    /* Event use for waiting and signalling asychronous data arrival. The
     * status is the XIA status for the receive process. This status is
     * asychronous to the command/response status held in the Module.
     */
    boolean_t       asyncReady;
    handel_md_Event asyncEvent;
    int             asyncStatus;

    /* Track the Sinc channel.state, for returning data like
     * run_active and detc-running.
     */
    ChannelState channelState;

    /* Track state of calibration data */
    CalibrationState calibrationState;

    struct FirmwareFeatures {
        boolean_t mcaGateVeto;
        boolean_t termination50ohm;
        boolean_t attenuationGround;
        boolean_t risetimeOptimization;
        int64_t   sampleRate;
    } features;

    /* Characterization data returned when valid. */
    double calibPercentage;
    char calibStage[256];
    SincCalibrationData calibData;
    SincCalibrationPlot calibExample;
    SincCalibrationPlot calibModel;
    SincCalibrationPlot calibFinal;

    /* The buffer size used when reading OSC data. */
    SincOscPlot adcTrace;

    /* Detector real-time stats */
    double stats[FALCONXN_STATS_NUMOF];

    /* MM0 stats, per histogram */
    double mm0_stats[FALCONXN_STATS_NUMOF];

    /* The time until the next update.*/
    uint32_t timeToNextMsec;

    /* Mapping Mode control. */
    MM_Control mmc;
};

#endif /* FALCONXN_PSL_H */
