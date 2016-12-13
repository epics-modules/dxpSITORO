/* NDDxp.cpp
 *
 * Driver for XIA DSP modules (Saturn, DXP4C2X, xMAP, and Mercury 
 * 
 * Mark Rivers
 * University of Chicago
 *
 */

/* Standard includes... */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* EPICS includes */
#include <epicsString.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsExit.h>
#include <envDefs.h>
#include <iocsh.h>

/* Handel includes */
#include <handel.h>
#include <handel_errors.h>
#include <handel_generic.h>
#include <md_generic.h>
#include <handel_constants.h>

/* MCA includes */
#include <drvMca.h>

/* Area Detector includes */
#include <asynNDArrayDriver.h>

#include <epicsExport.h>
#include "NDDxp.h"

#define MAX_CHANNELS_PER_CARD      4
#define DXP_ALL                   -1
#define MAX_MCA_BINS           8192
#define DXP_MAX_SCAS              64
#define LEN_SCA_NAME              10
#define MAPPING_CLOCK_PERIOD     320e-9

/** < The maximum number of bytes in the 2MB mapping mode buffer */
#define MAPPING_BUFFER_SIZE 2097152
/** < The XMAP buffer takes up 2MB of 16bit words. Unfortunatly the transfer over PCI
  * uses 32bit words, so the data we receive from from the Handel library is 2x2MB. */
#define XMAP_BUFFER_READ_SIZE 2*MAPPING_BUFFER_SIZE
#define MEGABYTE             1048576

#define CALLHANDEL( handel_call, msg ) { \
    xiastatus = handel_call; \
    status = this->xia_checkError( pasynUser, xiastatus, msg ); \
}

/** Only used for debugging/error messages to identify where the message comes from*/
static const char *driverName = "NDDxp";

typedef enum {
    NDDxpModeMCA, 
    NDDxpModeMCAMapping,
    NDDxpModeSCAMapping,
    NDDxpModeListMapping
} NDDxpCollectMode_t;

typedef enum {
    NDDxpListModeGate, 
    NDDxpListModeSync,
    NDDxpListModeClock
} NDDxpListMode_t;

typedef enum {
    NDDxpPresetModeNone,
    NDDxpPresetModeReal,
    NDDxpPresetModeEvents,
    NDDxpPresetModeTriggers
} NDDxpPresetMode_t;

typedef enum {
    NDDxpPixelAdvanceGate,
    NDDxpPixelAdvanceSync,
} NDDxpPixelAdvanceMode_t;

static const char *NDDxpBufferCharString[2]     = {"a", "b"};
static const char *NDDxpBufferFullString[2]     = {"buffer_full_a", "buffer_full_b"};
static const char *NDDxpBufferString[2]         = {"buffer_a", "buffer_b"};
static const char *NDDxpListBufferLenString[2]  = {"list_buffer_len_a", "list_buffer_len_b"};

static char SCA_NameLow[DXP_MAX_SCAS][LEN_SCA_NAME];
static char SCA_NameHigh[DXP_MAX_SCAS][LEN_SCA_NAME];

typedef struct moduleStatistics {
    double realTime;
    double triggerLiveTime;
    double reserved1;
    double triggers;
    double events;
    double icr;
    double ocr;
    double reserved2;
    double reserved3;
} moduleStatistics;

/* Mapping mode parameters */
#define NDDxpCollectModeString              "DxpCollectMode"
#define NDDxpPixelsPerRunString             "DxpPixelsPerRun"
#define NDDxpPixelsPerBufferString          "DxpPixelsPerBuffer"
#define NDDxpAutoPixelsPerBufferString      "DxpAutoPixelsPerBuffer"
#define NDDxpPixelAdvanceModeString         "DxpPixelAdvanceMode"
#define NDDxpInputLogicPolarityString       "DxpInputLogicPolarity"
#define NDDxpIgnoreGateString               "DxpIgnoreGate"
#define NDDxpSyncCountString                "DxpSyncCount"

#define NDDxpListModeString                 "DxpListMode"
#define NDDxpCurrentPixelString             "DxpCurrentPixel"
#define NDDxpNextPixelString                "DxpNextPixel"
#define NDDxpBufferOverrunString            "DxpBufferOverrun"
#define NDDxpMBytesReadString               "DxpMBytesRead"
#define NDDxpReadRateString                 "DxpReadRate"

/* Internal asyn driver parameters */
#define NDDxpErasedString                   "DxpErased"
#define NDDxpAcquiringString                "NDDxpAcquiring"  /* Internal use only !!! */
#define NDDxpBufferCounterString            "DxpBufferCounter"
#define NDDxpPollTimeString                 "DxpPollTime"
#define NDDxpForceReadString                "DxpForceRead"

/* Diagnostic trace parameters */
#define NDDxpTraceModeString                "DxpTraceMode"
#define NDDxpTraceTimeString                "DxpTraceTime"
#define NDDxpNewTraceTimeString             "DxpNewTraceTime" /* Internal use only !!! */
#define NDDxpTraceDataString                "DxpTraceData"
#define NDDxpTraceTimeArrayString           "DxpTraceTimeArray"

/* Runtime statistics */
#define NDDxpTriggerLiveTimeString          "DxpTriggerLiveTime"
#define NDDxpTriggersString                 "DxpTriggers"
#define NDDxpEventsString                   "DxpEvents"
#define NDDxpInputCountRateString           "DxpInputCountRate"
#define NDDxpOutputCountRateString          "DxpOutputCountRate"

/* High-level DXP parameters */
#define NDDxpDetectionThresholdString       "DxpDetectionThreshold"
#define NDDxpMinPulsePairSeparationString   "DxpMinPulsePairSeparation"
#define NDDxpDetectionFilterString          "DxpDetectionFilter"
#define NDDxpScaleFactorString              "DxpScaleFactor"
#define NDDxpNumMCAChannelsString           "DxpNumMCAChannels"
#define NDDxpMCARefreshPeriodString         "DxpMCARefreshPeriod"
#define NDDxpPresetModeString               "DxpPresetMode"
#define NDDxpPresetRealString               "DxpPresetReal"
#define NDDxpPresetEventsString             "DxpPresetEvents"
#define NDDxpPresetTriggersString           "DxpPresetTriggers"

/* Which of these to implement? */
#define NDDxpDetectorPolarityString         "DxpDetectorPolarity"
#define NDDxpResetDelayString               "DxpResetDelay"
#define NDDxpDecayTimeString                "DxpDecayTime"
#define NDDxpSpectrumXAxisString            "DxpSpectrumXAxis"
#define NDDxpTriggerOutputString            "DxpTriggerOutput"
#define NDDxpLiveTimeOutputString           "DxpLiveTimeOutput"

/* SCA parameters */
#define NDDxpSCATriggerModeString           "DxpSCATriggerMode"
#define NDDxpSCAPulseDurationString         "DxpSCAPulseDuration"
#define NDDxpMaxSCAsString                  "DxpMaxSCAs"
#define NDDxpNumSCAsString                  "DxpNumSCAs"
#define NDDxpSCALowString                   "DxpSCALow"
#define NDDxpSCAHighString                  "DxpSCAHigh"
#define NDDxpSCACountsString                "DxpSCACounts"
/* For each SCA there are 3 parameters
  * DXPSCA$(N)Low
  * DXPSCA$(N)High
  * DXPSCA$(N)Counts
*/

/* INI file parameters */
#define NDDxpSaveSystemFileString           "DxpSaveSystemFile"
#define NDDxpSaveSystemString               "DxpSaveSystem"

/* Module information */
#define NDDxpSerialNumberString             "DxpSerialNumber"
#define NDDxpFirmwareVersionString          "DxpFirmwareVersion"


class NDDxp : public asynNDArrayDriver
{
public:
    NDDxp(const char *portName, int nCChannels, int maxBuffers, size_t maxMemory);

    /* virtual methods to override from asynNDArrayDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn);
    void report(FILE *fp, int details);

    /* Local methods to this class */
    asynStatus xia_checkError( asynUser* pasynUser, epicsInt32 xiastatus, const char *xiacmd );
    void shutdown();

    void acquisitionTask();
    asynStatus pollMappingMode();
    int getChannel(asynUser *pasynUser, int *addr);
    int getModuleType();
    asynStatus apply(int channel, int forceApply=0);
    asynStatus setPresets(asynUser *pasynUser, int addr);
    asynStatus setDxpParam(asynUser *pasynUser, int addr, int function, double value);
    asynStatus getDxpParams(asynUser *pasynUser, int addr);
    asynStatus setLLDxpParam(asynUser *pasynUser, int addr, int value);
    asynStatus getLLDxpParams(asynUser *pasynUser, int addr);
    asynStatus setSCAs(asynUser *pasynUser, int addr);
    asynStatus getSCAs(asynUser *pasynUser, int addr);
    asynStatus getSCAData(asynUser *pasynUser, int addr);
    asynStatus getAcquisitionStatus(asynUser *pasynUser, int addr);
    asynStatus getModuleStatistics(asynUser *pasynUser, int addr, moduleStatistics *stats);
    asynStatus getAcquisitionStatistics(asynUser *pasynUser, int addr);
    asynStatus getMcaData(asynUser *pasynUser, int addr);
    asynStatus getMappingData();
    asynStatus getTrace(asynUser* pasynUser, int addr,
                        epicsInt32* data, size_t maxLen, size_t *actualLen);
    asynStatus getBaselineHistogram(asynUser* pasynUser, int addr,
                        epicsInt32* data, size_t maxLen, size_t *actualLen);
    asynStatus configureCollectMode();
    asynStatus setNumChannels(asynUser *pasynUser, epicsInt32 newsize, epicsInt32 *rbValue);
    asynStatus startAcquiring(asynUser *pasynUser);
    asynStatus stopAcquiring(asynUser *pasynUser);

protected:
    /* Mapping mode parameters */
    int NDDxpCollectMode;                   /** < Change mapping mode (0=mca; 1=spectra mapping; 2=sca mapping) (int32 read/write) addr: all/any */
    #define FIRST_DXP_PARAM NDDxpCollectMode
    int NDDxpPixelsPerRun;                  /** < Preset value how many pixels to acquire in one run (r/w) mapping mode*/
    int NDDxpPixelsPerBuffer;
    int NDDxpAutoPixelsPerBuffer;
    int NDDxpPixelAdvanceMode;              /** < Mapping mode only: pixel advance mode (int) */
    int NDDxpInputLogicPolarity;
    int NDDxpIgnoreGate;
    int NDDxpSyncCount;

    /* Used in SITORO? */
    int NDDxpListMode;                      /** < Change list mode variant (0=Gate; 1=Sync; 2=Clock) (int32 read/write) addr: all/any */
    int NDDxpCurrentPixel;                  /** < Mapping mode only: read the current pixel that is being acquired into (int) */
    int NDDxpNextPixel;                     /** < Mapping mode only: force a pixel increment in the mapping buffer (write only int). Value is ignored. */
    int NDDxpBufferOverrun;
    int NDDxpMBytesRead;
    int NDDxpReadRate;

    /* Internal asyn driver parameters */
    int NDDxpErased;               /** < Erased flag. (0=not erased; 1=erased) */
    int NDDxpAcquiring;            /** < Internal acquiring flag, not exposed via drvUser */
    int NDDxpBufferCounter;        /** < Count how many buffers have been collected (read) mapping mode */
    int NDDxpPollTime;             /** < Status/data polling time in seconds */
    int NDDxpForceRead;            /** < Force reading MCA spectra - used for mcaData when addr=ALL */

    /* Runtime statistics */
    int NDDxpTriggerLiveTime;           /** < live time in seconds (double) */
    int NDDxpTriggers;                  /** < number of triggers received (double) */
    int NDDxpEvents;                    /** < total number of events registered (double) */
    int NDDxpInputCountRate;            /** < input count rate in Hz (double) */
    int NDDxpOutputCountRate;           /** < output count rate in Hz (double) */

    /* Diagnostic trace parameters */
    int NDDxpTraceMode;            /** < Select what type of trace to do: ADC, baseline hist, .. etc. */
    int NDDxpTraceTime;            /** < Set the trace sample time in us. */
    int NDDxpNewTraceTime;         /** < Flag indicating trace time changed */
    int NDDxpTraceData;            /** < The trace array data (read) */
    int NDDxpTraceTimeArray;       /** < The trace timebase array (read) */

    /* High-level DXP parameters */
    int NDDxpDetectionThreshold;
    int NDDxpMinPulsePairSeparation;
    int NDDxpDetectionFilter;
    int NDDxpScaleFactor;
    int NDDxpNumMCAChannels;
    int NDDxpMCARefreshPeriod;
    int NDDxpPresetMode;
    int NDDxpPresetReal;
    int NDDxpPresetEvents;
    int NDDxpPresetTriggers;
    
    /* Which of these to implement? */
    int NDDxpDetectorPolarity;
    int NDDxpResetDelay;
    int NDDxpDecayTime;
    int NDDxpSpectrumXAxis;
    int NDDxpTriggerOutput;
    int NDDxpLiveTimeOutput;

    /* SCA parameters */
    int NDDxpSCATriggerMode;
    int NDDxpSCAPulseDuration;
    int NDDxpMaxSCAs;
    int NDDxpNumSCAs;
    int NDDxpSCALow[DXP_MAX_SCAS];
    int NDDxpSCAHigh[DXP_MAX_SCAS];
    int NDDxpSCACounts[DXP_MAX_SCAS];

    /* INI file parameters */
    int NDDxpSaveSystemFile;
    int NDDxpSaveSystem;

    /* Module information */
    int NDDxpSerialNumber;
    int NDDxpFirmwareVersion;

    /* Commands from MCA interface */
    int mcaData;                   /* int32Array, write/read */
    int mcaStartAcquire;           /* int32, write */
    int mcaStopAcquire;            /* int32, write */
    int mcaErase;                  /* int32, write */
    int mcaReadStatus;             /* int32, write */
    int mcaChannelAdvanceSource;   /* int32, write */
    int mcaNumChannels;            /* int32, write */
    int mcaAcquireMode;            /* int32, write */
    int mcaSequence;               /* int32, write */
    int mcaPrescale;               /* int32, write */
    int mcaPresetSweeps;           /* int32, write */
    int mcaPresetLowChannel;       /* int32, write */
    int mcaPresetHighChannel;      /* int32, write */
    int mcaDwellTime;              /* float64, write/read */
    int mcaPresetLiveTime;         /* float64, write */
    int mcaPresetRealTime;         /* float64, write */
    int mcaPresetCounts;           /* float64, write */
    int mcaAcquiring;              /* int32, read */
    int mcaElapsedLiveTime;        /* float64, read */
    int mcaElapsedRealTime;        /* float64, read */
    int mcaElapsedCounts;          /* float64, read */

    #define LAST_DXP_PARAM mcaElapsedCounts

private:
    /* Data */
    epicsUInt32 **pMcaRaw;
    epicsUInt32 *pMapRaw;
    epicsFloat64 *tmpStats;

    int nChannels;
    int channelsPerCard;

    epicsEvent *cmdStartEvent;
    epicsEvent *cmdStopEvent;
    epicsEvent *stoppedEvent;

    epicsUInt32 *currentBuf;
    int traceLength;
    epicsInt32 *traceBuffer;
    epicsFloat64 *traceTimeBuffer;
    epicsFloat64 *spectrumXAxisBuffer;
    
    moduleStatistics moduleStats[MAX_CHANNELS_PER_CARD];

    bool polling;

};

/** Number of asyn parameters (asyn commands) this driver supports. This algorithm does NOT include the
  * low-level parameters whose number we can only determine at run-time.
  * That value is passed to the constructor. */
#define NUM_DXP_PARAMS (&LAST_DXP_PARAM - &FIRST_DXP_PARAM + 1)

static void c_shutdown(void* arg)
{
    NDDxp *pNDDxp = (NDDxp*)arg;
    pNDDxp->shutdown();
    free(pNDDxp);
}

static void acquisitionTaskC(void *drvPvt)
{
    NDDxp *pNDDxp = (NDDxp *)drvPvt;
    pNDDxp->acquisitionTask();
}


extern "C" int NDDxpConfig(const char *portName, int nChannels,
                            int maxBuffers, size_t maxMemory)
{
    new NDDxp(portName, nChannels, maxBuffers, maxMemory);
    return 0;
}

/* Note: we use nChannels+1 for maxAddr because the last address is used for "all" channels" */
NDDxp::NDDxp(const char *portName, int nChannels, int maxBuffers, size_t maxMemory)
    : asynNDArrayDriver(portName, nChannels + 1, NUM_DXP_PARAMS, maxBuffers, maxMemory,
            asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask | asynFloat64ArrayMask | asynGenericPointerMask | asynOctetMask | asynDrvUserMask,
            asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask | asynFloat64ArrayMask | asynGenericPointerMask | asynOctetMask,
            ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, 0, 0)
{
    int status = asynSuccess;
    int i, ch;
    int sca;
    char tmpStr[100];
    double tmpDbl;
    int xiastatus = 0;
    const char *functionName = "NDDxp";

    this->nChannels = nChannels;

    /* Mapping mode parameters */
    createParam(NDDxpCollectModeString,            asynParamInt32,   &NDDxpCollectMode);
    createParam(NDDxpPixelsPerRunString,           asynParamInt32,   &NDDxpPixelsPerRun);
    createParam(NDDxpPixelsPerBufferString,        asynParamInt32,   &NDDxpPixelsPerBuffer);
    createParam(NDDxpAutoPixelsPerBufferString,    asynParamInt32,   &NDDxpAutoPixelsPerBuffer);
    createParam(NDDxpPixelAdvanceModeString,       asynParamInt32,   &NDDxpPixelAdvanceMode);
    createParam(NDDxpInputLogicPolarityString,     asynParamInt32,   &NDDxpInputLogicPolarity);
    createParam(NDDxpIgnoreGateString,             asynParamInt32,   &NDDxpIgnoreGate);
    createParam(NDDxpSyncCountString,              asynParamInt32,   &NDDxpSyncCount);

    /* Used in SITORO? */
    createParam(NDDxpListModeString,               asynParamInt32,   &NDDxpListMode);
    createParam(NDDxpCurrentPixelString,           asynParamInt32,   &NDDxpCurrentPixel);
    createParam(NDDxpNextPixelString,              asynParamInt32,   &NDDxpNextPixel);
    createParam(NDDxpBufferOverrunString,          asynParamInt32,   &NDDxpBufferOverrun);
    createParam(NDDxpMBytesReadString,             asynParamFloat64, &NDDxpMBytesRead);
    createParam(NDDxpReadRateString,               asynParamFloat64, &NDDxpReadRate);

    /* Internal asyn driver parameters */
    createParam(NDDxpErasedString,                 asynParamInt32,   &NDDxpErased);
    createParam(NDDxpAcquiringString,              asynParamInt32,   &NDDxpAcquiring);
    createParam(NDDxpBufferCounterString,          asynParamInt32,   &NDDxpBufferCounter);
    createParam(NDDxpPollTimeString,               asynParamFloat64, &NDDxpPollTime);
    createParam(NDDxpForceReadString,              asynParamInt32,   &NDDxpForceRead);

    /* Diagnostic trace parameters */
    createParam(NDDxpTraceModeString,              asynParamInt32,   &NDDxpTraceMode);
    createParam(NDDxpTraceTimeString,              asynParamFloat64, &NDDxpTraceTime);
    createParam(NDDxpNewTraceTimeString,           asynParamInt32,   &NDDxpNewTraceTime);
    createParam(NDDxpTraceDataString,              asynParamInt32Array, &NDDxpTraceData);
    createParam(NDDxpTraceTimeArrayString,         asynParamFloat64Array, &NDDxpTraceTimeArray);

    /* Runtime statistics */
    createParam(NDDxpTriggerLiveTimeString,        asynParamFloat64, &NDDxpTriggerLiveTime);
    createParam(NDDxpTriggersString,               asynParamInt32,   &NDDxpTriggers);
    createParam(NDDxpEventsString,                 asynParamInt32,   &NDDxpEvents);
    createParam(NDDxpInputCountRateString,         asynParamFloat64, &NDDxpInputCountRate);
    createParam(NDDxpOutputCountRateString,        asynParamFloat64, &NDDxpOutputCountRate);

    /* High-level DXP parameters */
    createParam(NDDxpDetectionThresholdString,     asynParamFloat64, &NDDxpDetectionThreshold);
    createParam(NDDxpMinPulsePairSeparationString, asynParamInt32,   &NDDxpMinPulsePairSeparation);
    createParam(NDDxpDetectionFilterString,        asynParamInt32  , &NDDxpDetectionFilter);
    createParam(NDDxpScaleFactorString,            asynParamFloat64, &NDDxpScaleFactor);
    createParam(NDDxpNumMCAChannelsString,         asynParamInt32,   &NDDxpNumMCAChannels);
    createParam(NDDxpPresetModeString,             asynParamInt32,   &NDDxpPresetMode);
    createParam(NDDxpPresetRealString,             asynParamFloat64, &NDDxpPresetReal);
    createParam(NDDxpPresetEventsString,           asynParamInt32,   &NDDxpPresetEvents);
    createParam(NDDxpPresetTriggersString,         asynParamInt32,   &NDDxpPresetTriggers);

    /* Which of these to implement? */
    createParam(NDDxpDetectorPolarityString,       asynParamInt32,   &NDDxpDetectorPolarity);
    createParam(NDDxpResetDelayString,             asynParamFloat64, &NDDxpResetDelay);
    createParam(NDDxpDecayTimeString,              asynParamFloat64, &NDDxpDecayTime);
    createParam(NDDxpSpectrumXAxisString,          asynParamFloat64Array, &NDDxpSpectrumXAxis);
    createParam(NDDxpTriggerOutputString,          asynParamInt32,   &NDDxpTriggerOutput);
    createParam(NDDxpLiveTimeOutputString,         asynParamInt32,   &NDDxpLiveTimeOutput);

    /* SCA parameters */
    createParam(NDDxpSCATriggerModeString,         asynParamInt32,   &NDDxpSCATriggerMode);
    createParam(NDDxpSCAPulseDurationString,       asynParamInt32,   &NDDxpSCAPulseDuration);
    createParam(NDDxpMaxSCAsString,                asynParamInt32,   &NDDxpMaxSCAs);
    createParam(NDDxpNumSCAsString,                asynParamInt32,   &NDDxpNumSCAs);
    for (sca=0; sca<DXP_MAX_SCAS; sca++) {
        /* Create SCA name strings that Handel uses */
        sprintf(SCA_NameLow[sca],  "sca%d_lo", sca);
        sprintf(SCA_NameHigh[sca], "sca%d_hi", sca);
        /* Create asyn parameters for SCAs */
        sprintf(tmpStr, "DxpSCA%dLow", sca);
        createParam(tmpStr,                        asynParamInt32,   &NDDxpSCALow[sca]);
        sprintf(tmpStr, "DxpSCA%dHigh", sca);
        createParam(tmpStr,                        asynParamInt32,   &NDDxpSCAHigh[sca]);
        sprintf(tmpStr, "DxpSCA%dCounts", sca);
        createParam(tmpStr,                        asynParamInt32,   &NDDxpSCACounts[sca]);
    }

    /* INI file parameters */
    createParam(NDDxpSaveSystemFileString,         asynParamOctet,   &NDDxpSaveSystemFile);
    createParam(NDDxpSaveSystemString,             asynParamInt32,   &NDDxpSaveSystem);

    /* Module information */
    createParam(NDDxpSerialNumberString,           asynParamOctet,   &NDDxpSerialNumber);
    createParam(NDDxpFirmwareVersionString,        asynParamOctet,   &NDDxpFirmwareVersion);
    
    /* Commands from MCA interface */
    createParam(mcaDataString,                     asynParamInt32Array, &mcaData);
    createParam(mcaStartAcquireString,             asynParamInt32,   &mcaStartAcquire);
    createParam(mcaStopAcquireString,              asynParamInt32,   &mcaStopAcquire);
    createParam(mcaEraseString,                    asynParamInt32,   &mcaErase);
    createParam(mcaReadStatusString,               asynParamInt32,   &mcaReadStatus);
    createParam(mcaChannelAdvanceSourceString,     asynParamInt32,   &mcaChannelAdvanceSource);
    createParam(mcaNumChannelsString,              asynParamInt32,   &mcaNumChannels);
    createParam(mcaAcquireModeString,              asynParamInt32,   &mcaAcquireMode);
    createParam(mcaSequenceString,                 asynParamInt32,   &mcaSequence);
    createParam(mcaPrescaleString,                 asynParamInt32,   &mcaPrescale);
    createParam(mcaPresetSweepsString,             asynParamInt32,   &mcaPresetSweeps);
    createParam(mcaPresetLowChannelString,         asynParamInt32,   &mcaPresetLowChannel);
    createParam(mcaPresetHighChannelString,        asynParamInt32,   &mcaPresetHighChannel);
    createParam(mcaDwellTimeString,                asynParamFloat64, &mcaDwellTime);
    createParam(mcaPresetLiveTimeString,           asynParamFloat64, &mcaPresetLiveTime);
    createParam(mcaPresetRealTimeString,           asynParamFloat64, &mcaPresetRealTime);
    createParam(mcaPresetCountsString,             asynParamFloat64, &mcaPresetCounts);
    createParam(mcaAcquiringString,                asynParamInt32,   &mcaAcquiring);
    createParam(mcaElapsedLiveTimeString,          asynParamFloat64, &mcaElapsedLiveTime);
    createParam(mcaElapsedRealTimeString,          asynParamFloat64, &mcaElapsedRealTime);
    createParam(mcaElapsedCountsString,            asynParamFloat64, &mcaElapsedCounts);

    /* Register the epics exit function to be called when the IOC exits... */
    xiastatus = epicsAtExit(c_shutdown, this);

    /* Set the parameters in param lib */
    status |= setIntegerParam(NDDxpCollectMode, 0);
    /* Clear the acquiring flag, must do this or things don't work right because acquisitionTask does not set till 
     * acquire first starts */
    for (i=0; i<=this->nChannels; i++) setIntegerParam(i, mcaAcquiring, 0);

    /* Create the start and stop events that will be used to signal our
     * acquisitionTask when to start/stop polling the HW     */
    this->cmdStartEvent = new epicsEvent();
    this->cmdStopEvent = new epicsEvent();
    this->stoppedEvent = new epicsEvent();

    /* Allocate a memory pointer for each of the channels */
    this->pMcaRaw = (epicsUInt32**) calloc(this->nChannels, sizeof(epicsUInt32*));
    /* Allocate a memory area for each spectrum */
    for (ch=0; ch<this->nChannels; ch++) {
        this->pMcaRaw[ch] = (epicsUInt32*)calloc(MAX_MCA_BINS, sizeof(epicsUInt32));
    }
    
    this->tmpStats = (epicsFloat64*)calloc(28, sizeof(epicsFloat64));
    this->currentBuf = (epicsUInt32*)calloc(this->nChannels, sizeof(epicsUInt32));

    xiastatus = xiaGetSpecialRunData(0, "adc_trace_length",  &tmpDbl);
    if (xiastatus != XIA_SUCCESS) printf("Error calling xiaGetSpecialRunData for adc_trace_length");
    this->traceLength = (int)tmpDbl;

    /* Allocate a buffer for the trace data */
    this->traceBuffer = (epicsInt32 *)malloc(this->traceLength * sizeof(epicsInt32));

    /* Allocate a buffer for the trace time array */
    this->traceTimeBuffer = (epicsFloat64 *)malloc(this->traceLength * sizeof(epicsFloat64));

    /* Allocating a temporary buffer for use in mapping mode. */
    this->pMapRaw = (epicsUInt32*)malloc(XMAP_BUFFER_READ_SIZE);
    
    /* Allocate an internal buffer long enough to hold all the energy values in a spectrum */
    this->spectrumXAxisBuffer = (epicsFloat64*)calloc(MAX_MCA_BINS, sizeof(epicsFloat64));

    /* Start up acquisition thread */
    setDoubleParam(NDDxpPollTime, 0.001);
    this->polling = 1;
    status = (epicsThreadCreate("acquisitionTask",
                epicsThreadPriorityMedium,
                epicsThreadGetStackSize(epicsThreadStackMedium),
                (EPICSTHREADFUNC)acquisitionTaskC, this) == NULL);
    if (status)
    {
        printf("%s:%s epicsThreadCreate failure for acquisition task\n",
                driverName, functionName);
        return;
    }

    /* Set default values for parameters that cannot be read from Handel */
    for (i=0; i<=this->nChannels; i++) {
        setDoubleParam (i, NDDxpTraceTime, 0.1);
        setIntegerParam(i, NDDxpNewTraceTime, 1);
        setIntegerParam(i, NDDxpPresetMode, NDDxpPresetModeNone);
        setIntegerParam(i, NDDxpPresetEvents, 0);
        setIntegerParam(i, NDDxpPresetTriggers, 0);
        setIntegerParam(i, NDDxpForceRead, 0);
        setDoubleParam (i, mcaPresetCounts, 0.0);
        setDoubleParam (i, mcaElapsedCounts, 0.0);
        setDoubleParam (i, mcaPresetRealTime, 0.0);
        setIntegerParam(i, NDDxpCurrentPixel, 0);
    }

    /* Read the MCA and DXP parameters once */
    this->getDxpParams(this->pasynUserSelf, DXP_ALL);
    this->getAcquisitionStatus(this->pasynUserSelf, DXP_ALL);
    this->getAcquisitionStatistics(this->pasynUserSelf, DXP_ALL);
    
    /* Read the serial number and firmware version */
    xiastatus = xiaBoardOperation(0, "get_serial_number", &tmpStr);
    setStringParam(NDDxpSerialNumber, tmpStr);
    xiastatus = xiaBoardOperation(0, "get_firmware_version", &tmpStr);
    setStringParam(NDDxpFirmwareVersion, tmpStr);
    
    
    // Enable array callbacks by default
    setIntegerParam(NDArrayCallbacks, 1);

}

/* virtual methods to override from ADDriver */
asynStatus NDDxp::writeInt32( asynUser *pasynUser, epicsInt32 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int channel, rbValue, xiastatus;
    int addr, i;
    int acquiring, numChans, mode;
    const char* functionName = "writeInt32";
    int firstCh, ignored;
    char fileName[MAX_FILENAME_LEN];

    channel = this->getChannel(pasynUser, &addr);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: [%s]: function=%d value=%d addr=%d channel=%d\n",
        driverName, functionName, this->portName, function, value, addr, channel);

    /* Set the parameter and readback in the parameter library.  This may be overwritten later but that's OK */
    status = setIntegerParam(addr, function, value);

    if ((function == NDDxpCollectMode)         ||
        (function == NDDxpListMode)            ||
        (function == NDDxpPixelsPerRun)        ||
        (function == NDDxpPixelsPerBuffer)     ||
        (function == NDDxpAutoPixelsPerBuffer) ||
        (function == NDDxpSyncCount)           ||
        (function == NDDxpIgnoreGate)          ||
        (function == NDDxpPixelAdvanceMode)    ||
        (function == NDDxpInputLogicPolarity))
    {
        status = this->configureCollectMode();
    } 
    else if (function == NDDxpNextPixel) 
    {
        for (firstCh=0; firstCh<this->nChannels; firstCh+=this->channelsPerCard)
        {
            CALLHANDEL( xiaBoardOperation(firstCh, "mapping_pixel_next", &ignored), "mapping_pixel_next" )
        }
        setIntegerParam(addr, function, 0);
    }
    else if (function == mcaErase) 
    {
        getIntegerParam(addr, mcaNumChannels, &numChans);
        getIntegerParam(addr, mcaAcquiring, &acquiring);
        if (acquiring) {
            xiaStopRun(channel);
            CALLHANDEL(xiaStartRun(channel, 0), "xiaStartRun(channel, 0)");
        } else {
            setIntegerParam(addr, NDDxpErased, 1);
            if (channel == DXP_ALL) {
                for (i=0; i<this->nChannels; i++) {
                    setIntegerParam(i, NDDxpErased, 1);
                    memset(this->pMcaRaw[i], 0, numChans * sizeof(epicsUInt32));
                }
            } else {
                memset(this->pMcaRaw[addr], 0, numChans * sizeof(epicsUInt32));
            }
            /* Need to call getAcquisitionStatistics to set elapsed values to 0 */
            this->getAcquisitionStatistics(pasynUser, addr);
        }
    } 
    else if (function == mcaStartAcquire) 
    {
        status = this->startAcquiring(pasynUser);
    } 
    else if (function == mcaStopAcquire) 
    {
        CALLHANDEL(xiaStopRun(channel), "xiaStopRun(detChan)");
        /* Wait for the acquisition task to realize the run has stopped and do the callbacks */
        while (1) {
            getIntegerParam(addr, mcaAcquiring, &acquiring);
            if (!acquiring) break;
            this->unlock();
            epicsThreadSleep(epicsThreadSleepQuantum());
            this->lock();
        }
    } 
    else if (function == mcaNumChannels) 
    {
        // rbValue not used here, call setIntegerParam if needed.
        status = this->setNumChannels(pasynUser, value, &rbValue);
    } 
    else if (function == mcaReadStatus) 
    {
        getIntegerParam(NDDxpCollectMode, &mode);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s mcaReadStatus [%d] mode=%d\n", 
            driverName, functionName, function, mode);
        /* We let the polling task set the acquiring flag, so that we can be sure that
         * the statistics and data have been read when needed.  DON'T READ ACQUIRE STATUS HERE */
        getIntegerParam(addr, mcaAcquiring, &acquiring);
        if (mode == NDDxpModeMCA) {
            /* If we are acquiring then read the statistics, else we use the cached values */
            if (acquiring) status = this->getAcquisitionStatistics(pasynUser, addr);
        }
    }
    else if ((function == NDDxpPresetMode)   ||
             (function == NDDxpPresetEvents) ||
             (function == NDDxpPresetTriggers)) 
    {
        this->setPresets(pasynUser, addr);
    } 
    else if ((function == NDDxpDetectorPolarity) ||
             (function == NDDxpTriggerOutput)    ||
             (function == NDDxpLiveTimeOutput)) 
    {
        this->setDxpParam(pasynUser, addr, function, (double)value);
    }
    else if ((function == NDDxpNumSCAs)    ||
             ((function >= NDDxpSCALow[0]) &&
              (function <= NDDxpSCAHigh[DXP_MAX_SCAS-1]))) 
    {
        this->setSCAs(pasynUser, addr);
    }
    else if (function == NDDxpSaveSystem) 
    {
        if (value) {
            callParamCallbacks(addr, addr);
            status = getStringParam(NDDxpSaveSystemFile, sizeof(fileName), fileName);
            if (status || (strlen(fileName) == 0)) {
                asynPrint(pasynUser, ASYN_TRACE_ERROR,
                    "%s::%s error, bad system file name, status=%d, fileName=%s\n",
                    driverName, functionName, status, fileName);
                goto done;
            }
            CALLHANDEL(xiaSaveSystem("handel_ini", fileName), "xiaSaveSystem(handel_ini, fileName)");
            /* Set the save command back to 0 */
            setIntegerParam(addr, NDDxpSaveSystem, 0);
        }
    }
    done:

    /* Call the callback */
    callParamCallbacks(addr, addr);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}

asynStatus NDDxp::writeFloat64( asynUser *pasynUser, epicsFloat64 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int addr;
    int channel;
    const char *functionName = "writeFloat64";

    channel = this->getChannel(pasynUser, &addr);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s:%s: [%s]: function=%d value=%f addr=%d channel=%d\n",
        driverName, functionName, this->portName, function, value, addr, channel);

    /* Set the parameter and readback in the parameter library.  This may be overwritten later but that's OK */
    status = setDoubleParam(addr, function, value);

    if ((function == mcaPresetRealTime) ||
        (function == mcaPresetLiveTime)) 
    {
        this->setPresets(pasynUser, addr);
    } 
    else if 
       ((function == NDDxpDetectorPolarity) ||
        (function == NDDxpResetDelay) ||
        (function == NDDxpDecayTime))
    {
        this->setDxpParam(pasynUser, addr, function, value);
    }
    else if  (function == NDDxpTraceTime)
    {
        /* Set a flag indicating the trace time has changed for this channel */
        setIntegerParam(addr, NDDxpNewTraceTime, 1);
    }
    /* Call the callback */
    callParamCallbacks(addr, addr);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}

asynStatus NDDxp::readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int addr;
    int channel;
    int nBins, acquiring,mode;
    int ch;
    const char *functionName = "readInt32Array";

    channel = this->getChannel(pasynUser, &addr);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::%s addr=%d channel=%d function=%d\n",
        driverName, functionName, addr, channel, function);
    if (function == NDDxpTraceData) 
    {
        status = this->getTrace(pasynUser, channel, value, nElements, nIn);
    } 
    else if (function == mcaData) 
    {
        if (channel == DXP_ALL)
        {
            // if the MCA ALL channel is being read - force reading of all individual
            // channels using the NDDxpForceRead command.
            for (ch=0; ch<this->nChannels; ch++)
            {
                setIntegerParam(ch, NDDxpForceRead, 1);
                callParamCallbacks(ch, ch);
                setIntegerParam(ch, NDDxpForceRead, 0);
                callParamCallbacks(ch, ch);
            }
            goto done;
        }
        getIntegerParam(channel, mcaNumChannels, &nBins);
        if (nBins > (int)nElements) nBins = (int)nElements;
        getIntegerParam(channel, mcaAcquiring, &acquiring);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s getting mcaData. ch=%d mcaNumChannels=%d mcaAcquiring=%d\n",
            driverName, functionName, channel, nBins, acquiring);
        *nIn = nBins;
        getIntegerParam(NDDxpCollectMode, &mode);

        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s mode=%d acquiring=%d\n",
            driverName, functionName, mode, acquiring);
        if (acquiring)
        {
            if (mode == NDDxpModeMCA)
            {
                /* While acquiring we'll force reading the data from the HW */
                this->getMcaData(pasynUser, addr);
            } else if ((mode == NDDxpModeMCAMapping) || (mode == NDDxpModeSCAMapping))
            {
                /*  Nothing needed here, the last data read from the mapping buffer has already been
                 *  copied to the buffer pointed to by pMcaRaw. */
            }
        }
        memcpy(value, pMcaRaw[addr], nBins * sizeof(epicsUInt32));
    } 
    else {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s::%s Function not implemented: [%d]\n",
                driverName, functionName, function);
            status = asynError;
    }
    done:
    
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return(status);
}


int NDDxp::getChannel(asynUser *pasynUser, int *addr)
{
    int channel;
    pasynManager->getAddr(pasynUser, addr);

    channel = *addr;
    if (*addr == this->nChannels) channel = DXP_ALL;
    return channel;
}

asynStatus NDDxp::setPresets(asynUser *pasynUser, int addr)
{
    asynStatus status = asynSuccess;
    NDDxpPresetMode_t presetMode;
    double presetReal;
    int presetEvents;
    int presetTriggers;
    double presetValue;
    double PresetMode;
    unsigned long runActive=0;
    int channel=addr;
    int channel0;
    const char* functionName = "setPresets";

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d\n",
        driverName, functionName, addr);
    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) channel0 = 0; else channel0 = channel;

    getDoubleParam(addr,  mcaPresetRealTime,   &presetReal);
    getIntegerParam(addr, NDDxpPresetEvents,   &presetEvents);
    getIntegerParam(addr, NDDxpPresetTriggers, &presetTriggers);
    getIntegerParam(addr, NDDxpPresetMode,     (int *)&presetMode);

    xiaGetRunData(channel0, "run_active", &runActive);
    if (runActive) xiaStopRun(channel);
    
    switch (presetMode) {
        case  NDDxpPresetModeNone:
            presetValue = 0;
            PresetMode = XIA_PRESET_NONE;
            break;

        case NDDxpPresetModeReal:
            presetValue = presetReal;
            PresetMode = XIA_PRESET_FIXED_REAL;
            break;

        case NDDxpPresetModeEvents:
            presetValue = presetEvents;
            PresetMode = XIA_PRESET_FIXED_EVENTS;
            break;

        case NDDxpPresetModeTriggers:
            presetValue = presetTriggers;
            PresetMode = XIA_PRESET_FIXED_TRIGGERS;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s:%s: unknown presetMode=%d\n",
                driverName, functionName, presetMode);
            break;
    }

    xiaSetAcquisitionValues(channel, "preset_type", &PresetMode);
    xiaSetAcquisitionValues(channel, "preset_value", &presetValue);
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
        "%s:%s: addr=%d channel=%d set presets mode=%d, value=%f\n",
        driverName, functionName, addr, channel, presetMode, presetValue);

    if (runActive) xiaStartRun(channel, 1);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return(status);
}

asynStatus NDDxp::setDxpParam(asynUser *pasynUser, int addr, int function, double value)
{
    int channel = addr;
    int channel0;
    unsigned long runActive=0;
    double dvalue=value;
    int xiastatus;
    asynStatus status=asynSuccess;
    static const char *functionName = "setDxpParam";

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d, function=%d, value=%f\n",
        driverName, functionName, addr, function, value);
    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) channel0 = 0; else channel0 = channel;

    xiaGetRunData(channel0, "run_active", &runActive);
    if (runActive) xiaStopRun(channel);

    if (function == NDDxpDetectorPolarity) {
        xiastatus = xiaSetAcquisitionValues(channel, "detector_polarity", &dvalue);
        status = this->xia_checkError(pasynUser, xiastatus, "setting detector_polarity");
    } else if (function == NDDxpResetDelay) {
        xiastatus = xiaSetAcquisitionValues(channel, "reset_delay", &dvalue);
    } else if (function == NDDxpDecayTime) {
        xiastatus = xiaSetAcquisitionValues(channel, "decay_time", &dvalue);
    }
    this->getDxpParams(pasynUser, addr);
    if (runActive) xiaStartRun(channel, 1);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: status=%d, exit\n",
        driverName, functionName, status);
    return asynSuccess;
}

asynStatus NDDxp::setSCAs(asynUser *pasynUser, int addr)
{
    int channel = addr;
    int channel0;
    unsigned long runActive=0;
    int xiastatus;
    int i;
    int numSCAs, maxSCAs;
    int low, high;
    double dTmp;
    asynStatus status=asynSuccess;
    static const char *functionName = "setSCAs";

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d\n",
        driverName, functionName, addr);
    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) channel0 = 0; else channel0 = channel;
    xiaGetRunData(channel0, "run_active", &runActive);
    if (runActive) xiaStopRun(channel);

    /* We get the number of SCAs from the channel 0, force all detectors to be the same */
    getIntegerParam(0, NDDxpNumSCAs, &numSCAs);
    getIntegerParam(0, NDDxpMaxSCAs, &maxSCAs);
    if (numSCAs > maxSCAs) {
        numSCAs = maxSCAs;
        setIntegerParam(NDDxpNumSCAs, numSCAs);
    }
    dTmp = numSCAs;
    CALLHANDEL(xiaSetAcquisitionValues(DXP_ALL, "number_of_scas", &dTmp), "number_of_scas");
    for (i=0; i<numSCAs; i++) {
        status = getIntegerParam(addr, NDDxpSCALow[i], &low);
        if (status || (low < 0)) {
            low = 0;
            setIntegerParam(addr, NDDxpSCALow[i], low);
        }
        status = getIntegerParam(addr, NDDxpSCAHigh[i], &high);
        if (status || (high < 0)) {
            high = 0;
            setIntegerParam(addr, NDDxpSCAHigh[i], high);
        }
        if (high < low) {
            high = low;
            setIntegerParam(addr, NDDxpSCAHigh[i], high);
        }
        dTmp = (double) low;
        CALLHANDEL(xiaSetAcquisitionValues(channel, SCA_NameLow[i], &dTmp), SCA_NameLow[i]);
        dTmp = (double) high;
        CALLHANDEL(xiaSetAcquisitionValues(channel, SCA_NameHigh[i], &dTmp), SCA_NameHigh[i]);
    }
    getSCAs(pasynUser, addr);
    if (runActive) xiaStartRun(channel, 1);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return(status);
}

asynStatus NDDxp::getSCAs(asynUser *pasynUser, int addr)
{
    int channel = addr;
    int xiastatus;
    int i;
    int numSCAs;
    int low, high;
    double dTmp;
    asynStatus status=asynSuccess;
    static const char *functionName = "getSCAs";

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d\n",
        driverName, functionName, addr);
    if (addr == this->nChannels) channel = DXP_ALL;

    CALLHANDEL(xiaGetAcquisitionValues(channel, "number_of_scas", &dTmp), "number_of_scas");
    numSCAs = (int) dTmp;
    setIntegerParam(addr, NDDxpNumSCAs, numSCAs);
    for (i=0; i<numSCAs; i++) {
        CALLHANDEL(xiaGetAcquisitionValues(channel, SCA_NameLow[i], &dTmp), SCA_NameLow[i]);
        low = (int) dTmp;
        setIntegerParam(addr, NDDxpSCALow[i], low);
        CALLHANDEL(xiaGetAcquisitionValues(channel, SCA_NameHigh[i], &dTmp), SCA_NameHigh[i]);
        high = (int) dTmp;
        setIntegerParam(addr, NDDxpSCAHigh[i], high);
    }
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return(status);
}

asynStatus NDDxp::setNumChannels(asynUser* pasynUser, epicsInt32 value, epicsInt32 *rbValue)
{
    asynStatus status = asynSuccess;
    int xiastatus;
    int i;
    int mode=0;
    double dblNbins;
    static const char* functionName = "setNumChannels";

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s:%s: new number of bins: %d\n", 
        driverName, functionName, value);

    for (i=0; i<this->nChannels; i++)
    {
        dblNbins = (double)value;
        *rbValue = value;
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "xiaSetAcquisitionValues ch=%d nbins=%.1f\n", 
            i, dblNbins);
        xiastatus = xiaSetAcquisitionValues(i, "number_mca_channels", &dblNbins);
        status = this->xia_checkError(pasynUser, xiastatus, "number_mca_channels");
        if (status == asynError)
            {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s [%s] can not set nbins=%u (%.3f) ch=%u\n",
                driverName, functionName, this->portName, *rbValue, dblNbins, i);
            return status;
            }
        setIntegerParam(i, mcaNumChannels, *rbValue);
        callParamCallbacks(i);
    }
    
    /* We also need to set the number of channels for the DXP_ALL channel, it is used in mcaErase */
    setIntegerParam(this->nChannels, mcaNumChannels, *rbValue);
    callParamCallbacks(this->nChannels);

    /* If in mapping mode we need to modify the Y size as well in order to fit in the 2MB buffer!
     * We also need to read out the new length of the mapping buffer because that can possibly change... */
    getIntegerParam(NDDxpCollectMode, &mode);
    if (mode != NDDxpModeMCA) configureCollectMode();

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}


asynStatus NDDxp::configureCollectMode()
{
    asynStatus status = asynSuccess;
    NDDxpCollectMode_t collectMode;
    NDDxpListMode_t listMode;
    int xiastatus, acquiring;
    int i;
    int firstCh, bufLen;
    double dTmp;
    int pixelsPerBuffer;
    int autoPixelsPerBuffer;
    int pixelsPerRun;
    int syncCount;
    int ignoreGate;
    int inputLogicPolarity;
    NDDxpPixelAdvanceMode_t pixelAdvanceMode;
    const char* functionName = "configureCollectMode";

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s:%s: enter\n",
        driverName, functionName);
    getIntegerParam(NDDxpCollectMode, (int *)&collectMode);
    
    if (collectMode < NDDxpModeMCA || collectMode > NDDxpModeListMapping) {
	      asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s:%s: invalid collect mode = %d\n",
            driverName, functionName, collectMode);
        return asynError;
    }

    getIntegerParam(mcaAcquiring, &acquiring);
    if (acquiring) {
	      asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s:%s: cannot change collect mode while acquiring\n",
            driverName, functionName);
        return asynError;
    }
    
    /* Changing the mapping mode is time-consuming.  
     * If the current mapping mode matches the desired one then skip this */
    xiastatus = xiaGetAcquisitionValues(0, "mapping_mode", &dTmp);
    status = this->xia_checkError(pasynUserSelf, xiastatus, "GET mapping_mode");
    if ((int)dTmp != collectMode) {
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s Changing collectMode to %d\n",
            driverName, functionName, collectMode);
        dTmp = (double)collectMode;
        xiastatus = xiaSetAcquisitionValues(DXP_ALL, "mapping_mode", &dTmp);
        status = this->xia_checkError(pasynUserSelf, xiastatus, "mapping_mode");
        if (status == asynError) return status;
    }

    switch(collectMode)
    {
    case NDDxpModeMCA:
        getIntegerParam(mcaNumChannels, (int*)&bufLen);
        setIntegerParam(NDDataType, NDUInt32);
        for (i=0; i<this->nChannels; i++)
        {
            /* Clear the normal mapping mode status settings */
            setIntegerParam(i, NDDxpEvents, 0);
            setDoubleParam(i, NDDxpInputCountRate, 0);
            setDoubleParam(i, NDDxpOutputCountRate, 0);

            /* Set the new ArraySize down to just one buffer length */
            setIntegerParam(i, NDArraySize, (int)bufLen);
            callParamCallbacks(i,i);
        }
        break;
    case NDDxpModeMCAMapping:
    case NDDxpModeSCAMapping:
    case NDDxpModeListMapping:
        getIntegerParam(NDDxpPixelAdvanceMode, (int *)&pixelAdvanceMode);
        getIntegerParam(NDDxpPixelsPerRun, &pixelsPerRun);
        getIntegerParam(NDDxpPixelsPerBuffer, &pixelsPerBuffer);
        if (pixelsPerBuffer == 0) pixelsPerBuffer = -1; /* Handel will compute maximum */
        getIntegerParam(NDDxpAutoPixelsPerBuffer, &autoPixelsPerBuffer);
        if (autoPixelsPerBuffer) pixelsPerBuffer = -1;  /* Handel will compute maximum */
        getIntegerParam(NDDxpSyncCount, &syncCount);
        if (syncCount < 1) syncCount = 1;
        getIntegerParam(NDDxpIgnoreGate, &ignoreGate);
        getIntegerParam(NDDxpInputLogicPolarity, &inputLogicPolarity);
        setIntegerParam(NDDataType, NDUInt16);
            
        if (collectMode == NDDxpModeListMapping) {
            getIntegerParam(NDDxpListMode, (int *)&listMode);
            if ((listMode < NDDxpListModeGate) || (listMode > NDDxpListModeClock)) listMode = NDDxpListModeClock;
            dTmp = (double)listMode;
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] setting list_mode_variant = %f\n", 
                driverName, functionName, DXP_ALL, dTmp);
            xiastatus = xiaSetAcquisitionValues(DXP_ALL, "list_mode_variant", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "list_mode_variant");
        }
        for (firstCh=0; firstCh<this->nChannels; firstCh+=this->channelsPerCard)
        {
            dTmp = XIA_MAPPING_CTL_SYNC;
            if (pixelAdvanceMode == NDDxpPixelAdvanceGate) dTmp = XIA_MAPPING_CTL_GATE;
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] setting pixel_advance_mode = %f\n", 
                driverName, functionName, firstCh, dTmp);
            xiastatus = xiaSetAcquisitionValues(firstCh, "pixel_advance_mode", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "pixel_advance_mode");

            dTmp = pixelsPerRun;
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] setting num_map_pixels = %f\n", 
                driverName, functionName, firstCh, dTmp);
            xiastatus = xiaSetAcquisitionValues(firstCh, "num_map_pixels", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "num_map_pixels");

            dTmp = pixelsPerBuffer;
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] setting num_map_pixels_per_buffer = %f\n", 
                driverName, functionName, firstCh, dTmp);
            xiastatus = xiaSetAcquisitionValues(firstCh, "num_map_pixels_per_buffer", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "num_map_pixels_per_buffer");

            /* The xMAP and Mercury actually divides the sync input by N+1, e.g. sync_count=0 does no division
             * of sync clock.  We subtract 1 so user-units are intuitive. */
            dTmp = syncCount-1;
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] setting sync_count = %f\n", 
                driverName, functionName, firstCh, dTmp);
            xiastatus = xiaSetAcquisitionValues(firstCh, "sync_count", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "sync_count");

            dTmp = ignoreGate;
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] setting gate_ignore = %f\n", 
                driverName, functionName, firstCh, dTmp);
            xiastatus = xiaSetAcquisitionValues(firstCh, "gate_ignore", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "gate_ignore");

            dTmp = inputLogicPolarity;
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] setting input_logic_polarity = %f\n", 
                driverName, functionName, firstCh, dTmp);
            xiastatus = xiaSetAcquisitionValues(firstCh, "input_logic_polarity", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "input_logic_polarity");
            
            /* Clear the normal MCA mode status settings */
            for (i=0; i<this->channelsPerCard; i++)
            {
                setIntegerParam(firstCh+i, NDDxpTriggers, 0);
                setDoubleParam(firstCh+i, mcaElapsedRealTime, 0);
                setDoubleParam(firstCh+i, NDDxpTriggerLiveTime, 0);
                setDoubleParam(firstCh+i, mcaElapsedLiveTime, 0);
                callParamCallbacks(firstCh+i, firstCh+i);
            }
            /* Read back the actual settings */
            getDxpParams(this->pasynUserSelf, firstCh);
        }
        break;
    }

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s:%s: returning status=%d\n",
        driverName, functionName, status);
    return status;
}

asynStatus NDDxp::getAcquisitionStatus(asynUser *pasynUser, int addr)
{
    int acquiring=0;
    unsigned long run_active;
    int ivalue;
    int channel=addr;
    asynStatus status=asynSuccess;
    int xiastatus;
    int i;
    //static const char *functionName = "getAcquisitionStatus";
    
    /* Note: we use the internal parameter NDDxpAcquiring rather than mcaAcquiring here
     * because we need to do callbacks in acquisitionTask() on all other parameters before
     * we do callbacks on mcaAcquiring, and callParamCallbacks does not allow control over the order. */

    if (addr == this->nChannels) channel = DXP_ALL;
    else if (addr == DXP_ALL) addr = this->nChannels;
    if (channel == DXP_ALL) { /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getAcquisitionStatus(pasynUser, i);
            getIntegerParam(i, NDDxpAcquiring, &ivalue);
            acquiring = MAX(acquiring, ivalue);
        }
        setIntegerParam(addr, NDDxpAcquiring, acquiring);
    } else {
        /* Get the run time status from the handel library - informs whether the
         * HW is acquiring or not.        */
        CALLHANDEL( xiaGetRunData(channel, "run_active", &run_active), "xiaGetRunData (run_active)" )
        setIntegerParam(addr, NDDxpAcquiring, run_active);
    }
    //asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
    //    "%s::%s addr=%d channel=%d: acquiring=%d\n",
    //    driverName, functionName, addr, channel, acquiring);
    return(status);
}

asynStatus NDDxp::getModuleStatistics(asynUser *pasynUser, int addr, moduleStatistics *stats)
{
    /* This function returns the module statistics with a single block read.
     * It is more than 30 times faster on the USB Saturn than reading individual
     * parameters. */
    int status;
    static const char *functionName = "getModuleStatistics";
     
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d\n",
        driverName, functionName, addr);
    status = xiaGetRunData(addr, "module_statistics_2", (double *)stats);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return((asynStatus)status);
}     
     

asynStatus NDDxp::getAcquisitionStatistics(asynUser *pasynUser, int addr)
{
    double dvalue, triggerLiveTime=0, energyLiveTime=0, realTime=0, icr=0, ocr=0;
    moduleStatistics *stats;
    int events=0, triggers=0;
    int ivalue;
    int channel=addr;
    int erased;
    int i;
    const char *functionName = "getAcquisitionStatistics";

    if (addr == this->nChannels) channel = DXP_ALL;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::%s addr=%d channel=%d\n", 
        driverName, functionName, addr, channel);
    if (channel == DXP_ALL) { /* All channels */
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s start DXP_ALL\n", 
            driverName, functionName);
        addr = this->nChannels;
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getAcquisitionStatistics(pasynUser, i);
            getDoubleParam(i, mcaElapsedLiveTime, &dvalue);
            energyLiveTime = MAX(energyLiveTime, dvalue);
            getDoubleParam(i, NDDxpTriggerLiveTime, &dvalue);
            triggerLiveTime = MAX(triggerLiveTime, dvalue);
            getDoubleParam(i, mcaElapsedRealTime, &realTime);
            realTime = MAX(realTime, dvalue);
            getDoubleParam(i, mcaElapsedLiveTime, &energyLiveTime);
            energyLiveTime = MAX(energyLiveTime, dvalue);
            getIntegerParam(i, NDDxpEvents, &ivalue);
            events = MAX(events, ivalue);
            getIntegerParam(i, NDDxpTriggers, &ivalue);
            triggers = MAX(triggers, ivalue);
            getDoubleParam(i, NDDxpInputCountRate, &dvalue);
            icr = MAX(icr, dvalue);
            getDoubleParam(i, NDDxpOutputCountRate, &dvalue);
            ocr = MAX(ocr, dvalue);
        }
        setDoubleParam(addr, mcaElapsedLiveTime, energyLiveTime);
        setDoubleParam(addr, NDDxpTriggerLiveTime, triggerLiveTime);
        setDoubleParam(addr, mcaElapsedRealTime, realTime);
        setDoubleParam(addr, mcaElapsedLiveTime, energyLiveTime);
        setIntegerParam(addr,NDDxpEvents, events);
        setIntegerParam(addr,NDDxpTriggers, triggers);
        setDoubleParam(addr, NDDxpInputCountRate, icr);
        setDoubleParam(addr, NDDxpOutputCountRate, ocr);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s end DXP_ALL\n", 
            driverName, functionName);
    } else {
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s start channel %d\n", 
            driverName, functionName, addr);
        getIntegerParam(addr, NDDxpErased, &erased);
        if (erased) {
            setDoubleParam(addr, mcaElapsedLiveTime, 0);
            setDoubleParam(addr, mcaElapsedRealTime, 0);
            setIntegerParam(addr, NDDxpEvents, 0);
            setDoubleParam(addr, NDDxpInputCountRate, 0);
            setDoubleParam(addr, NDDxpOutputCountRate, 0);
            setIntegerParam(addr, NDDxpTriggers, 0);
            setDoubleParam(addr, NDDxpTriggerLiveTime, 0);
        } else {
            /* We only read the module statistics data if this is the first channel in a module.
             * This assumes we are reading in numerical order, else we may get stale data! */
            if ((channel % this->channelsPerCard) == 0) getModuleStatistics(pasynUser, channel, &moduleStats[0]);
            stats = &moduleStats[channel % this->channelsPerCard];
            setIntegerParam(addr, NDDxpTriggers, (int)stats->triggers);
            setIntegerParam(addr, NDDxpEvents, (int)stats->events);
            setDoubleParam(addr, mcaElapsedRealTime, stats->realTime);
            setDoubleParam(addr, NDDxpTriggerLiveTime, stats->triggerLiveTime);
            if (stats->triggers == 0) {
                energyLiveTime = stats->triggerLiveTime;
            } else {
                energyLiveTime = stats->triggerLiveTime * stats->events / stats->triggers;
            }
            setDoubleParam(addr, mcaElapsedLiveTime, energyLiveTime);
            setDoubleParam(addr, NDDxpInputCountRate, stats->icr);
            setDoubleParam(addr, NDDxpOutputCountRate, stats->ocr);

            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s  channel %d \n"
                "              events=%f\n" 
                "            triggers=%f\n" 
                "           real time=%f\n" 
                "    trigger livetime=%f\n" 
                "    input count rate=%f\n" 
                "   output count rate=%f\n",
                driverName, functionName, addr, 
                stats->events,
                stats->triggers,
                stats->realTime,
                stats->triggerLiveTime,
                stats->icr,
                stats->ocr);
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return(asynSuccess);
}

asynStatus NDDxp::getDxpParams(asynUser *pasynUser, int addr)
{
    int i;
    double numMcaChannels;
    double dvalue;
    int channel=addr;
    asynStatus status = asynSuccess;
    int xiastatus;
    unsigned long bufLen;
    double dTmp;
    int collectMode;
    //int listMode;
    int pixelsPerBuffer;
    int pixelsPerRun;
    int syncCount;
    int ignoreGate;
    int inputLogicPolarity;
    NDDxpPixelAdvanceMode_t pixelAdvanceMode;
    const char* functionName = "getDxpParams";

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d\n",
        driverName, functionName, addr);
    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getDxpParams(pasynUser, i);
        }
    } else {
        xiaGetAcquisitionValues(channel, "number_mca_channels", &numMcaChannels);
        setIntegerParam(channel, mcaNumChannels, (int)numMcaChannels);
        xiaGetAcquisitionValues(channel, "detector_polarity", &dvalue);
        setIntegerParam(channel, NDDxpDetectorPolarity, (int)dvalue);
        xiaGetAcquisitionValues(channel, "decay_time", &dvalue);
        setDoubleParam(channel, NDDxpDecayTime, dvalue);
        
        // Read mapping parameters, which are assumed to be the same for all modules 
        dTmp = 0;
        xiastatus = xiaGetAcquisitionValues(channel, "mapping_mode", &dTmp);
        status = this->xia_checkError(pasynUserSelf, xiastatus, "GET mapping_mode");
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s [%d] Got mapping_mode = %.1f\n", 
            driverName, functionName, channel, dTmp);
        collectMode = (int)dTmp;
        setIntegerParam(NDDxpCollectMode, collectMode);

        if (collectMode != NDDxpModeMCA) {
            /* list_mode_variant does not seem to be able to be read? */
            //dTmp = 0;
            //xiastatus = xiaGetAcquisitionValues(channel, "list_mode_variant", &dTmp);
            //status = this->xia_checkError(pasynUserSelf, xiastatus, "GET list_mode_variant");
            //asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            //    "%s::%s [%d] Got list_mode_variant = %.1f\n", 
            //    driverName, functionName, channel, dTmp);
            //listMode = (int)dTmp;
            //setIntegerParam(NDDxpListMode, listMode);

            dTmp = 0;
            xiastatus = xiaGetAcquisitionValues(channel, "pixel_advance_mode", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "GET pixel_advance_mode");
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] Got pixel_advance_mode = %.1f\n", 
                driverName, functionName, channel, dTmp);
            pixelAdvanceMode = NDDxpPixelAdvanceSync;
            if (dTmp == XIA_MAPPING_CTL_GATE) pixelAdvanceMode = NDDxpPixelAdvanceGate;
            setIntegerParam(NDDxpPixelAdvanceMode, pixelAdvanceMode);

            dTmp = 0;
            xiastatus = xiaGetAcquisitionValues(channel, "num_map_pixels", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "GET num_map_pixels");
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] Got num_map_pixels = %.1f\n", 
                driverName, functionName, channel, dTmp);
            pixelsPerRun = (int)dTmp;
            setIntegerParam(NDDxpPixelsPerRun, pixelsPerRun);

            dTmp = 0;
            xiastatus = xiaGetAcquisitionValues(channel, "num_map_pixels_per_buffer", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "GET num_map_pixels_per_buffer");
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] Got num_map_pixels_per_buffer = %.1f\n", 
                driverName, functionName, channel, dTmp);
            pixelsPerBuffer = (int)dTmp;
            setIntegerParam(NDDxpPixelsPerBuffer, pixelsPerBuffer);

            dTmp = 0;
            xiastatus = xiaGetAcquisitionValues(channel, "sync_count", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "GET sync_count");
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] Got sync_count = %.1f\n", 
                driverName, functionName, channel, dTmp);
            /* We add 1 to sync count because xMAP and Mercury actually divides by N+1 */
            syncCount = (int)dTmp + 1;
            setIntegerParam(NDDxpSyncCount, syncCount);

            dTmp = 0;
            xiastatus = xiaGetAcquisitionValues(channel, "gate_ignore", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "GET gate_ignore");
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] Got gate_ignore = %.1f\n", 
                driverName, functionName, channel, dTmp);
            ignoreGate = (int)dTmp;
            setIntegerParam(NDDxpIgnoreGate, ignoreGate);

            dTmp = 0;
            xiastatus = xiaGetAcquisitionValues(channel, "input_logic_polarity", &dTmp);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "GET input_logic_polarity");
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] Got input_logic_polarity = %.1f\n", 
                driverName, functionName, channel, dTmp);
            inputLogicPolarity = (int)dTmp;
            setIntegerParam(NDDxpInputLogicPolarity, inputLogicPolarity);

            /* In list mode Handel returns an error reading buffer_len, so hardcode it */
            if (collectMode == NDDxpModeListMapping) {
                bufLen = MAPPING_BUFFER_SIZE / 2;
            } 
            else {
                bufLen = 0;
                xiastatus = xiaGetRunData(channel, "buffer_len", &bufLen);
                status = this->xia_checkError(pasynUserSelf, xiastatus, "GET buffer_len");
                asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                    "%s::%s [%d] Got buffer_len = %lu\n", 
                    driverName, functionName, channel, bufLen);
            }
            setIntegerParam(channel, NDArraySize, (int)bufLen);
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: status=%d, exit\n",
        driverName, functionName, status);
    return(asynSuccess);
}


asynStatus NDDxp::getMcaData(asynUser *pasynUser, int addr)
{
    asynStatus status = asynSuccess;
    int xiastatus, paramStatus;
    int arrayCallbacks;
    int nChannels;
    int channel=addr;
    int i;
    //NDArray *pArray;
    NDDataType_t dataType;
    epicsTimeStamp now;
    const char* functionName = "getMcaData";

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d\n",
        driverName, functionName, addr);
    if (addr == this->nChannels) channel = DXP_ALL;

    paramStatus  = getIntegerParam(mcaNumChannels, &nChannels);
    paramStatus |= getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    paramStatus |= getIntegerParam(NDDataType, (int *)&dataType);

    epicsTimeGetCurrent(&now);

    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getMcaData(pasynUser, i);
        }
    } else {
        /* Read the MCA spectrum from Handel.
        * For most devices this means getting 1 channel spectrum here.
        * For the XMAP we get all 4 channels on the board in one go here */
        CALLHANDEL( xiaGetRunData(addr, "mca", this->pMcaRaw[addr]),"xiaGetRunData")
        asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, (const char *)pMcaRaw[addr], nChannels*sizeof(epicsUInt32),
            "%s::%s Got MCA spectrum channel:%d ptr:%p\n",
            driverName, functionName, channel, pMcaRaw[addr]);

// In the future we may want to do array callbacks with the MCA data.  For now we are not doing this.
//        if (arrayCallbacks)
//       {
//            /* Allocate a buffer for callback */
//            pArray = this->pNDArrayPool->alloc(1, &nChannels, dataType, 0, NULL);
//            pArray->timeStamp = now.secPastEpoch + now.nsec / 1.e9;
//            pArray->uniqueId = spectrumCounter;
//            /* TODO: Need to copy the data here */
//            //this->unlock();
//            doCallbacksGenericPointer(pArray, NDArrayData, addr);
//            //this->lock();
//            pArray->release();
//       }
    }
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}

/** Reads the mapping data for all of the modules in the system */
asynStatus NDDxp::getMappingData()
{
    asynStatus status = asynSuccess;
    int xiastatus;
    int arrayCallbacks;
    NDDataType_t dataType;
    int buf = 0, channel, i, k;
    NDArray *pArray=NULL;
    epicsUInt32 *pIn=NULL;
    epicsUInt32 *pStats;
    epicsUInt16 *pOut=NULL;
    int mappingMode, pixelOffset, dataOffset, events, triggers, nChans;
    double realTime, triggerLiveTime, energyLiveTime, icr, ocr;
    size_t dims[2];
    int bufferCounter, arraySize;
    epicsTimeStamp now, after;
    double mBytesRead;
    double readoutTime, readoutBurstRate, MBbufSize;
    const char* functionName = "getMappingData";

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s:%s: enter\n",
        driverName, functionName);

    getIntegerParam(NDDataType, (int *)&dataType);
    getIntegerParam(NDDxpBufferCounter, &bufferCounter);
    bufferCounter++;
    setIntegerParam(NDDxpBufferCounter, bufferCounter);
    getIntegerParam(NDArraySize, &arraySize);
    getDoubleParam(NDDxpMBytesRead, &mBytesRead);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    MBbufSize = (double)((arraySize)*sizeof(epicsUInt16)) / (double)MEGABYTE;

    /* First read and reset the buffers, do this as quickly as possible */
    for (channel=0; channel<this->nChannels; channel+=this->channelsPerCard)
    {
        buf = this->currentBuf[channel];

        /* The buffer is full so read it out */
        epicsTimeGetCurrent(&now);
        xiastatus = xiaGetRunData(channel, NDDxpBufferString[buf], this->pMapRaw);
        status = xia_checkError(this->pasynUserSelf, xiastatus, "GetRunData mapping");
        epicsTimeGetCurrent(&after);
        readoutTime = epicsTimeDiffInSeconds(&after, &now);
        readoutBurstRate = MBbufSize / readoutTime;
        mBytesRead += MBbufSize;
        setDoubleParam(NDDxpMBytesRead, mBytesRead);
        setDoubleParam(NDDxpReadRate, readoutBurstRate);
        /* Notify system that we read out the buffer */
        xiastatus = xiaBoardOperation(channel, "buffer_done", (void*)NDDxpBufferCharString[buf]);
        status = xia_checkError(this->pasynUserSelf, xiastatus, "buffer_done");
        if (buf == 0) this->currentBuf[channel] = 1;
        else this->currentBuf[channel] = 0;
        callParamCallbacks();
        asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, 
            "%s::%s Got data! size=%.3fMB (%d) dt=%.3fs speed=%.3fMB/s\n",
            driverName, functionName, MBbufSize, arraySize, readoutTime, readoutBurstRate);
        asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, 
            "%s::%s channel=%d, bufferNumber=%d, firstPixel=%d, numPixels=%d\n",
            driverName, functionName, channel, pMapRaw[5], pMapRaw[9], pMapRaw[8]);
    
        /* If this is MCA mapping mode then copy the spectral data for the first pixel
         * in this buffer to the mcaRaw buffers.
         * This provides an update of the spectra and statistics while mapping is in progress
         * if the user sets the MCA spectra to periodically read. */
        mappingMode = pMapRaw[3];
        if (mappingMode == NDDxpModeMCAMapping) {
            pixelOffset = 256;
            dataOffset = pixelOffset + 256;
            for (i=0; i<this->channelsPerCard; i++) {
                k = channel + i;
                nChans = pMapRaw[pixelOffset + 8 + i];
                memcpy(pMcaRaw[k], &pMapRaw[dataOffset], nChans*sizeof(epicsUInt32));
                dataOffset += nChans;
                pStats = &pMapRaw[pixelOffset + 32 + i*8];
                realTime        = (pStats[0] + (pStats[1]<<16)) * MAPPING_CLOCK_PERIOD;
                triggerLiveTime = (pStats[2] + (pStats[3]<<16)) * MAPPING_CLOCK_PERIOD;
                triggers        =  pStats[4] + (pStats[5]<<16);
                events          =  pStats[6] + (pStats[7]<<16);
                if (triggers > 0.) 
                    energyLiveTime = (triggerLiveTime * events) / triggers;
                else
                    energyLiveTime = triggerLiveTime;
                if (triggerLiveTime > 0.)
                    icr = triggers / triggerLiveTime;
                else
                    icr = 0.;
                if (realTime > 0.)
                    ocr = events / realTime;
                else
                    ocr = 0.;
                setDoubleParam(k, mcaElapsedRealTime, realTime);
                setDoubleParam(k, mcaElapsedLiveTime, energyLiveTime);
                setDoubleParam(k, NDDxpTriggerLiveTime, triggerLiveTime);
                setIntegerParam(k,NDDxpEvents, events);
                setIntegerParam(k, NDDxpTriggers, triggers);
                setDoubleParam(k, NDDxpInputCountRate, icr);
                setDoubleParam(k, NDDxpOutputCountRate, ocr);
                callParamCallbacks(k, k);
            }
        }
            
        if (arrayCallbacks)
        {
            /* Now rearrange the data and do the callbacks */
            /* If this is the first module then allocate an NDArray for the callback */
            if (channel == 0)
            {
               dims[0] = arraySize;
               dims[1] = 1;
               pArray = this->pNDArrayPool->alloc(2, dims, dataType, 0, NULL );
               pOut = (epicsUInt16 *)pArray->pData;
            }
            
            /* First get rid of the empty parts of the 32 bit words. The Handel library
             * provides a 4MB 32 bit/word wide buffer, but the only data is in the low-order
             * 16 bits of each word. So we strip off the empty top 16 bits of each 32 bit
             * word. */
            pIn = this->pMapRaw;
            for (i=0; i<arraySize; i++)
            {
                *pOut++ = (epicsUInt16) *pIn++;
            }
        }
    }
    if (arrayCallbacks) 
    {
        pArray->timeStamp = now.secPastEpoch + now.nsec / 1.e9;
        pArray->uniqueId = bufferCounter;
        //this->unlock();
        doCallbacksGenericPointer(pArray, NDArrayData, 0);
        //this->lock();
        pArray->release();
    }
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s::%s Done reading! Ch=%d bufchar=%s\n",
        driverName, functionName, channel, NDDxpBufferCharString[buf]);

    return status;
}

/* Get trace data */
asynStatus NDDxp::getTrace(asynUser* pasynUser, int addr,
                           epicsInt32* data, size_t maxLen, size_t *actualLen)
{
    asynStatus status = asynSuccess;
    int xiastatus, channel=addr;
    int i, j;
    int newTraceTime;
    double info[2];
    double traceTime;
    int traceMode;
    const char *functionName = "getTrace";

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d\n",
        driverName, functionName, addr);
    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getTrace(pasynUser, i, data, maxLen, actualLen);
        }
    } else {
        getDoubleParam(channel, NDDxpTraceTime, &traceTime);
        getIntegerParam(channel, NDDxpNewTraceTime, &newTraceTime);
        getIntegerParam(channel, NDDxpTraceMode, &traceMode);
        info[0] = 0.;
        /* Convert from us to ns */
        info[1] = traceTime * 1000.;

        xiastatus = xiaDoSpecialRun(channel, "adc_trace", info);
        status = this->xia_checkError(pasynUser, xiastatus, "adc_trace");
        // Don't return error, read it out or we get stuck permanently with module busy
        // if (status == asynError) return asynError;

        *actualLen = this->traceLength;
        if (maxLen < *actualLen) *actualLen = maxLen;

        xiastatus = xiaGetSpecialRunData(channel, "adc_trace", this->traceBuffer);
        status = this->xia_checkError( pasynUser, xiastatus, "adc_trace" );
        if (status == asynError) return status;

        memcpy(data, this->traceBuffer, *actualLen * sizeof(epicsInt32));
        
        if (newTraceTime) {
            setIntegerParam(channel, NDDxpNewTraceTime, 0);  /* Clear flag */
            for (j=0; j<this->traceLength; j++) this->traceTimeBuffer[j] = j*traceTime;
            doCallbacksFloat64Array(this->traceTimeBuffer, this->traceLength, NDDxpTraceTimeArray, channel);
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}


asynStatus NDDxp::startAcquiring(asynUser *pasynUser)
{
    asynStatus status = asynSuccess;
    int xiastatus;
    int channel, addr, i;
    int acquiring, erased, resume=1;
    int firstCh;
    const char *functionName = "startAcquire";

    channel = this->getChannel(pasynUser, &addr);
    getIntegerParam(addr, mcaAcquiring, &acquiring);
    getIntegerParam(addr, NDDxpErased, &erased);
    if (erased) resume=0;

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s::%s ch=%d acquiring=%d, erased=%d\n",
        driverName, functionName, channel, acquiring, erased);
    /* if already acquiring we just ignore and return */
    if (acquiring) return status;

    /* make sure we use buffer A to start with */
    for (firstCh=0; firstCh<this->nChannels; firstCh+=this->channelsPerCard) this->currentBuf[firstCh] = 0;

    // do xiaStart command
    CALLHANDEL( xiaStartRun(channel, resume), "xiaStartRun()" )

    setIntegerParam(addr, NDDxpErased, 0); /* reset the erased flag */
    setIntegerParam(addr, mcaAcquiring, 1); /* Set the acquiring flag */

    if (channel == DXP_ALL) {
        for (i=0; i<this->nChannels; i++) {
            setIntegerParam(i, mcaAcquiring, 1);
            setIntegerParam(i, NDDxpErased, 0);
            callParamCallbacks(i, i);
        }
    }

    callParamCallbacks(addr, addr);

    // signal cmdStartEvent to start the polling thread
    this->cmdStartEvent->signal();
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}

/** Thread used to poll the hardware for status and data.
 *
 */
void NDDxp::acquisitionTask()
{
    asynUser *pasynUser = this->pasynUserSelf;
    int paramStatus;
    int i;
    int mode;
    int acquiring = 0;
    epicsFloat64 pollTime, sleeptime, dtmp;
    epicsTimeStamp now, start;
    const char* functionName = "acquisitionTask";

    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s:%s acquisition task started!\n",
        driverName, functionName);
//    epicsEventTryWait(this->stopEventId); /* clear the stop event if it wasn't already */

    this->lock();

    while (this->polling) /* ... round and round and round we go until the IOC is shut down */
    {

        getIntegerParam(this->nChannels, mcaAcquiring, &acquiring);

        if (!acquiring)
        {
            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            this->unlock();
            /* Wait for someone to signal the cmdStartEvent */
            asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                "%s:%s Waiting for acquisition to start!\n",
                driverName, functionName);
            this->cmdStartEvent->wait();
            this->lock();
            getIntegerParam(NDDxpCollectMode, &mode);
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s::%s [%s]: started! (mode=%d)\n", 
                driverName, functionName, this->portName, mode);
        }
        epicsTimeGetCurrent(&start);

        /* In this loop we only read the acquisition status to minimise overhead.
         * If a transition from acquiring to done is detected then we read the statistics
         * and the data. */
        this->getAcquisitionStatus(this->pasynUserSelf, DXP_ALL);
        getIntegerParam(this->nChannels, NDDxpAcquiring, &acquiring);
        if (!acquiring)
        {
            /* There must have just been a transition from acquiring to not acquiring */

            if (mode == NDDxpModeMCA) {
                /* In MCA mode we force a read of the data */
                asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                    "%s::%s Detected acquisition stop! Now reading statistics\n",
                    driverName, functionName);
                this->getAcquisitionStatistics(this->pasynUserSelf, DXP_ALL);
                asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                    "%s::%s Detected acquisition stop! Now reading data\n",
                    driverName, functionName);
                this->getMcaData(this->pasynUserSelf, DXP_ALL);
            }
            else {
                /* In mapping modes need to make an extra call to pollMappingMode because there could be
                 * 2 mapping mode buffers that still need to be read out. 
                 * This call will read out the first one, and just below this !acquiring block
                 * there is a second call to pollMapping mode which is
                 * done on every main loop in mapping modes. */
                 this->pollMappingMode();
            }
        } 
        if (mode != NDDxpModeMCA)
        {
            this->pollMappingMode();
        }

        /* Do callbacks for all channels for everything except mcaAcquiring*/
        for (i=0; i<=this->nChannels; i++) callParamCallbacks(i, i);
        /* Copy internal acquiring flag to mcaAcquiring */
        for (i=0; i<=this->nChannels; i++) {
            getIntegerParam(i, NDDxpAcquiring, &acquiring);
            setIntegerParam(i, mcaAcquiring, acquiring);
            callParamCallbacks(i, i);
        }
        
        paramStatus |= getDoubleParam(NDDxpPollTime, &pollTime);
        epicsTimeGetCurrent(&now);
        dtmp = epicsTimeDiffInSeconds(&now, &start);
        sleeptime = pollTime - dtmp;
        if (sleeptime > 0.0)
        {
            //asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            //    "%s::%s Sleeping for %f seconds\n",
            //    driverName, functionName, sleeptime);
            this->unlock();
            epicsThreadSleep(sleeptime);
            this->lock();
        }
    }
}

/** Check if the current mapping buffer is full in which case it reads out the data */
asynStatus NDDxp::pollMappingMode()
{
    asynStatus status = asynSuccess;
    asynUser *pasynUser = this->pasynUserSelf;
    int xiastatus;
    int ignored;
    int ch, buf=0, allFull=1, anyFull=0;
    unsigned short isFull;
    unsigned long currentPixel = 0;
    const char* functionName = "pollMappingMode";
    NDDxpCollectMode_t mappingMode;
    
    getIntegerParam(NDDxpCollectMode, (int *)&mappingMode);

    for (ch=0; ch<this->nChannels; ch+=this->channelsPerCard)
    {
        buf = this->currentBuf[ch];

        if (mappingMode == NDDxpModeListMapping) {
            CALLHANDEL( xiaGetRunData(ch, NDDxpListBufferLenString[buf], &currentPixel), "NDDxpListBufferLenString[buf]")
        }
        else {
            CALLHANDEL( xiaGetRunData(ch, "current_pixel", &currentPixel) , "current_pixel" )
        }
        setIntegerParam(ch, NDDxpCurrentPixel, (int)currentPixel);
        callParamCallbacks(ch);
        CALLHANDEL( xiaGetRunData(ch, NDDxpBufferFullString[buf], &isFull), "NDDxpBufferFullString[buf]" )
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s %s isfull=%d\n",
            driverName, functionName, NDDxpBufferFullString[buf], isFull);
        if (!isFull) allFull = 0;
        if (isFull)  anyFull = 1;
    }

    /* In list mapping mode if any buffer is full then switch buffers on the non-full ones.
     * Note: this is prone to error because they could have already switched! */
    if (anyFull && (mappingMode == NDDxpModeListMapping)) {
        for (ch=0; ch<this->nChannels; ch+=this->channelsPerCard) {
            CALLHANDEL( xiaGetRunData(ch, NDDxpBufferFullString[buf], &isFull), "NDDxpBufferFullString[buf]" )
            if (isFull) continue;
            CALLHANDEL( xiaBoardOperation(ch, "buffer_switch", &ignored), "buffer_switch" )
        }
        /* Now wait until all modules report they are full */
        do {
            allFull = 1;
            for (ch=0; ch<this->nChannels; ch+=this->channelsPerCard) {
                CALLHANDEL( xiaGetRunData(ch, NDDxpBufferFullString[buf], &isFull), "NDDxpBufferFullString[buf]" )
                if (!isFull) allFull = 0;
            }
        } while (allFull != 1);
    }
    /* If all of the modules have full buffers then read them all out */
    if (allFull)
    {
        status = this->getMappingData();
    }
    return status;
}


void NDDxp::report(FILE *fp, int details)
{
    asynNDArrayDriver::report(fp, details);
}



asynStatus NDDxp::xia_checkError( asynUser* pasynUser, epicsInt32 xiastatus, const char *xiacmd )
{
    if (xiastatus == XIA_SUCCESS) return asynSuccess;

    asynPrint( pasynUser, ASYN_TRACE_ERROR, 
        "### NDDxp: XIA ERROR: %d (%s)\n", 
        xiastatus, xiacmd );
    return asynError;
}

void NDDxp::shutdown()
{
    int status;
    double pollTime;
    
    getDoubleParam(NDDxpPollTime, &pollTime);
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s: shutting down in %f seconds\n", driverName, 2*pollTime);
    this->polling = 0;
    epicsThreadSleep(2*pollTime);
    status = xiaExit();
    if (status == XIA_SUCCESS)
    {
        printf("%s shut down successfully.\n",
            driverName);
        return;
    }
    printf("xiaExit() error: %d\n", status);
    return;
}


static const iocshArg NDDxpConfigArg0 = {"Asyn port name", iocshArgString};
static const iocshArg NDDxpConfigArg1 = {"Number of channels", iocshArgInt};
static const iocshArg NDDxpConfigArg2 = {"Maximum number of buffers", iocshArgInt};
static const iocshArg NDDxpConfigArg3 = {"Maximum amount of memory (bytes)", iocshArgInt};
static const iocshArg * const NDDxpConfigArgs[] =  {&NDDxpConfigArg0,
                                                    &NDDxpConfigArg1,
                                                    &NDDxpConfigArg2,
                                                    &NDDxpConfigArg3};
static const iocshFuncDef configNDDxp = {"NDDxpConfig", 4, NDDxpConfigArgs};
static void configNDDxpCallFunc(const iocshArgBuf *args)
{
    NDDxpConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival);
}

static const iocshArg xiaLogLevelArg0 = { "logging level",iocshArgInt};
static const iocshArg * const xiaLogLevelArgs[1] = {&xiaLogLevelArg0};
static const iocshFuncDef xiaLogLevelFuncDef = {"xiaSetLogLevel",1,xiaLogLevelArgs};
static void xiaLogLevelCallFunc(const iocshArgBuf *args)
{
    xiaSetLogLevel(args[0].ival);
}

static const iocshArg xiaLogOutputArg0 = { "logging output file",iocshArgString};
static const iocshArg * const xiaLogOutputArgs[1] = {&xiaLogOutputArg0};
static const iocshFuncDef xiaLogOutputFuncDef = {"xiaSetLogOutput",1,xiaLogOutputArgs};
static void xiaLogOutputCallFunc(const iocshArgBuf *args)
{
    xiaSetLogOutput(args[0].sval);
}

static const iocshArg xiaInitArg0 = { "ini file",iocshArgString};
static const iocshArg * const xiaInitArgs[1] = {&xiaInitArg0};
static const iocshFuncDef xiaInitFuncDef = {"xiaInit",1,xiaInitArgs};
static void xiaInitCallFunc(const iocshArgBuf *args)
{
    xiaInit(args[0].sval);
}

static const iocshFuncDef xiaStartSystemFuncDef = {"xiaStartSystem",0,0};
static void xiaStartSystemCallFunc(const iocshArgBuf *args)
{
    xiaStartSystem();
}

static const iocshArg xiaSaveSystemArg0 = { "ini file",iocshArgString};
static const iocshArg * const xiaSaveSystemArgs[1] = {&xiaSaveSystemArg0};
static const iocshFuncDef xiaSaveSystemFuncDef = {"xiaSaveSystem",1,xiaSaveSystemArgs};
static void xiaSaveSystemCallFunc(const iocshArgBuf *args)
{
    xiaSaveSystem((char *)"handel_ini", args[0].sval);
}



static void NDDxpRegister(void)
{
    iocshRegister(&configNDDxp, configNDDxpCallFunc);
    iocshRegister(&xiaInitFuncDef,xiaInitCallFunc);
    iocshRegister(&xiaLogLevelFuncDef,xiaLogLevelCallFunc);
    iocshRegister(&xiaLogOutputFuncDef,xiaLogOutputCallFunc);
    iocshRegister(&xiaStartSystemFuncDef,xiaStartSystemCallFunc);
    iocshRegister(&xiaSaveSystemFuncDef,xiaSaveSystemCallFunc);
}

extern "C" {
epicsExportRegistrar(NDDxpRegister);
}
