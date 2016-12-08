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


#ifndef FALCONX_PSL_H
#define FALCONX_PSL_H

/*
 * This code only works with the SiToro interface version 2.1.3 or later. The
 * interface was completely changed at this point in time and there is no
 * compatibility. A wrapper was provided but never used.
 */

#include <sitoro.h>

#define DAC_OFFSET_MIN        -32768
#define DAC_OFFSET_MAX         32767
#define DAC_GAIN_MIN               0.0
#define DAC_GAIN_MAX           65535.0
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
#define ADC_COARSE_GAIN_MULTIPLIER 6.0
#define ADC_GAIN_MIN               (1.0)
#define ADC_GAIN_MAX               (ADC_GAIN_MULTIPLIER * ADC_COARSE_GAIN_MULTIPLIER)

#define MM1_MAX_BIN_COUNT          (32 * 1024)

#define SITORO_PROGRESS_TEXT_SIZE 100

/** FORWARD DECLARATION **/

struct _SiToroDetector;
typedef struct _SiToroDetector SiToroDetector;
struct _SiToroModule;
typedef struct _SiToroModule SiToroModule;
typedef struct _AcquisitionValue AcquisitionValue;
typedef struct _MappingModeControl MappingModeControl;

/** FUNCTION POINTERS **/

typedef int (*acqValueFP)(Detector*         detector,
                          SiToroDetector*   siDetector,
                          XiaDefaults*      defaults,
                          AcquisitionValue* acq,
                          double*           value,
                          boolean_t         read);
typedef int (*DoBoardOperation_FP)(int detChan,
                                   Detector* detector, Module* module,
                                   const char *name, void *value);

/** STRUCTURES **/

/* Types of Acquisition Values */
typedef enum acqValueTypes {
    acqDouble,
    acqUint16,
    acqInt16,
    acqUint32,
    acqInt32,
    acqUint64,
    acqInt64,
    acqBool,
    acqString,
} acqValueTypes;

typedef struct acqValye {
    acqValueTypes type;
    union {
        double    d;
        uint16_t  u16;
        int16_t   i16;
        uint32_t  u32;
        int32_t   i32;
        uint64_t  u64;
        int64_t   i64;
        boolean_t b;
    } ref;  /* The handler's private shadow if it needs one. */
} acqValue;

#define PSL_ACQ_EMPTY       (0)
#define PSL_ACQ_READ_ONLY   (1 << 0) /* There is no set */
#define PSL_ACQ_RUNNING     (1 << 1) /* Handel needs to be in the running state. */
#define PSL_ACQ_HAS_DEFAULT (1 << 2) /* There is a default value. */

#define PSL_ACQ_FLAG_SET(_a, _m) (((_a)->flags & (_m)) != 0)

/* Acquisition Values */
struct _AcquisitionValue {
    const char*   name;
    double        defaultValue;
    acqValue      value;
    uint32_t      flags;
    acqValueFP    handler;
};

/* A generic board operation */
typedef struct _BoardOperation {
    const char         *name;
    DoBoardOperation_FP fn;
} BoardOperation;


/* Mapping mode control. */
struct _MappingModeControl {
    /* List mode is running. */
    boolean_t listMode_Running;

    /* The mode.. */
    uint32_t mode;

    /* Data formatter, an opaque handle. */
    void* dataFormatter;

    /* Pixel header size added to the buffer size. Units is uin32_t's. */
    uint32_t pixelHeaderSize;

    /* Buffer header size added to the buffer size. Units is uint32_t's.*/
    uint32_t bufferHeaderSize;
};

/*
 * The SiToro Module PSL Data. It contains the detectors.
 */
struct _SiToroModule {
    /* The instrument handle. Keep first and do not move. */
    SiToro_InstrumentHandle instrument;
    /* The card handle. Keep second and do not move. */
    SiToro_CardHandle card;

    /* The instrument handle valid flag. The SI API provides no clear or test
     * interface to the handle so we need to keep extra data to manage it. This
     * should be part of the API and not something a user should need to be
     * concerned with when opaque types of this nature are being used. */
    boolean_t instrumentValid;
    /* See the intstrument's valid flag. */
    boolean_t cardValid;

    /* API Version number. Repeated per module. */
    unsigned long apiVersionMajor;
    unsigned long apiVersionMinor;
    unsigned long apiVersionRevision;

    /* The Instrument Id as recognized by the SiToro API. No idea what it is. */
    int instrumentId;

    /* The (Card) Id as recognized by the SiToro API. */
    int cardId;

    /* The card's serial number.*/
    uint32_t serialNum;

    /* The Detector Id as recognized by the SiToro API. The channel on a card. */
    int detId;

    /* The number of channels in the card. Currently only 1. */
    int detChannels;

    /* The FalconX card's software version. */
    unsigned long geminiVerMajor;
    unsigned long geminiVerMinor;
    unsigned long geminiVerRevision;

    /* The FalconX FPGA version number. Currently there is only
     * one FPGA per module. This may change. */
    unsigned long fpgaVersion;
};

/*
 * The SiToro Detector PSL Data.
 */

struct _SiToroDetector {
    /* The detector handle. Keep first, do not move. */
    SiToro_DetectorHandle detector;

    /* The detector channel */
    int detChan;

    /* Set to true once all ACQ values have been set. */
    boolean_t valid_acq_values;

    /* The defaults name for this detector. */
    char defaultStr[MAXALIAS_LEN];

    /* The number of acquisition values. */
    #define SI_DET_NUM_OF_ACQ_VALUES (58)
    AcquisitionValue acqValues[SI_DET_NUM_OF_ACQ_VALUES];

    /* The buffer and buffer size used when reading OSC data. */
    int16_t* osc_buffer;
    uint32_t osc_buffer_length;

    /* The time until the next update.*/
    uint32_t timeToNextMsec;

    /* Mapping Mode control. */
    MappingModeControl mmc;
};

#endif /* FALCONX_PSL_H */
