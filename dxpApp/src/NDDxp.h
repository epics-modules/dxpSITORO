#ifndef NDDXP_H
#define NDDXP_H

#include <epicsTypes.h>
#include <epicsEvent.h>

#include <asynNDArrayDriver.h>

#define MAX_CHANNELS_PER_SYSTEM    8
#define DXP_MAX_SCAS              24
#define MAX_ATTR_NAME_LEN        256

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

typedef enum {
    dxpNDArrayModeRawBuffers,
    dxpNDArrayModeMCASpectra
} dxpNDArrayMode_t;

/* These structures must be packed */
#pragma pack(1)
typedef struct falconBufferHeader {
    epicsUInt16 tag0;            /* Tag word 0 */
    epicsUInt16 tag1;            /* Tag word 1 */
    epicsUInt16 headerSize;      /* Buffer header size */
    epicsUInt16 mappingMode;     /* Mapping mode (1=Full spectrum, 2=Multiple ROI, 3=List mode) */
    epicsUInt16 runNumber;       /* Run number */
    epicsUInt32 bufferNumber;    /* Sequential buffer number, low word first */
    epicsUInt16 bufferID;        /* 0=A, 1=B */
    epicsUInt16 numPixels;       /* Number of pixels in buffer */
    epicsUInt32 firstPixel;      /* Starting pixel number, low word first */
    epicsUInt16 moduleNumber;
    epicsUInt16 channelID;
    epicsUInt16 channelElement;
    epicsUInt16 reserved1[6];
    epicsUInt16 channelSize;
    epicsUInt16 reserved2[3];
    epicsUInt16 bufferErrors;
} falconBufferHeader;

typedef struct falconMCAPixelHeader {
    epicsUInt16 tag0;            /* Tag word 0 */
    epicsUInt16 tag1;            /* Tag word 1 */
    epicsUInt16 headerSize;      /* Buffer header size */
    epicsUInt16 mappingMode;     /* Mapping mode (1=Full spectrum, 2=Multiple ROI, 3=List mode) */
    epicsUInt32 pixelNumber;     /* Pixel number */
    epicsUInt32 blockSize;       /* Total pixel block size, low word first */
    epicsUInt16 spectrumSize;
    epicsUInt16 reserved1[23];
    epicsUInt32 realTime;
    epicsUInt32 triggerLiveTime;
    epicsUInt32 triggers;
    epicsUInt32 outputCounts;
} falconMCAPixelHeader;
#pragma pack()

/* Mapping mode parameters */
#define NDDxpCollectModeString              "DxpCollectMode"
#define NDDxpNDArrayModeString              "DxpNDArrayMode"
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
#define NDDxpRisetimeOptimizationString     "DxpRisetimeOptimization"
#define NDDxpNumMCAChannelsString           "DxpNumMCAChannels"
#define NDDxpMCARefreshPeriodString         "DxpMCARefreshPeriod"
#define NDDxpPresetModeString               "DxpPresetMode"
#define NDDxpPresetRealString               "DxpPresetReal"
#define NDDxpPresetEventsString             "DxpPresetEvents"
#define NDDxpPresetTriggersString           "DxpPresetTriggers"

/* Which of these to implement? */
#define NDDxpDetectorPolarityString         "DxpDetectorPolarity"
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
/* For each SCA there are 2 parameters
  * DXPSCA$(N)Low
  * DXPSCA$(N)High
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
    void getModuleInfo();
    asynStatus setPresets(asynUser *pasynUser, int addr);
    asynStatus setDxpParam(asynUser *pasynUser, int addr, int function, double value);
    asynStatus getDxpParams(asynUser *pasynUser, int addr);
    asynStatus setSCAs(asynUser *pasynUser, int addr);
    asynStatus getSCAs(asynUser *pasynUser, int addr);
    asynStatus getAcquisitionStatus(asynUser *pasynUser, int addr);
    asynStatus getModuleStatistics(asynUser *pasynUser, int addr, moduleStatistics *stats);
    asynStatus getAcquisitionStatistics(asynUser *pasynUser, int addr);
    asynStatus getMcaData(asynUser *pasynUser, int addr);
    asynStatus getMappingData();
    asynStatus getTrace(asynUser* pasynUser, int addr,
                        epicsInt32* data, size_t maxLen, size_t *actualLen);
    asynStatus configureCollectMode();
    asynStatus setNumChannels(asynUser *pasynUser, epicsInt32 newsize, epicsInt32 *rbValue);
    asynStatus startAcquiring(asynUser *pasynUser);
    asynStatus stopAcquiring(asynUser *pasynUser);

protected:
    /* Mapping mode parameters */
    int NDDxpCollectMode;                   /** < Change mapping mode (0=mca; 1=spectra mapping; 2=sca mapping) (int32 read/write) addr: all/any */
    #define FIRST_DXP_PARAM NDDxpCollectMode
    int NDDxpNDArrayMode;                   /** < NDArray mode (=Raw buffers, 1=MCA spectra) */
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
    int NDDxpTraceData;            /** < The trace array data (read) */
    int NDDxpTraceTimeArray;       /** < The trace timebase array (read) */

    /* High-level DXP parameters */
    int NDDxpDetectionThreshold;
    int NDDxpMinPulsePairSeparation;
    int NDDxpDetectionFilter;
    int NDDxpScaleFactor;
    int NDDxpRisetimeOptimization;
    int NDDxpNumMCAChannels;
    int NDDxpMCARefreshPeriod;
    int NDDxpPresetMode;
    int NDDxpPresetReal;
    int NDDxpPresetEvents;
    int NDDxpPresetTriggers;
    
    /* Which of these to implement? */
    int NDDxpDetectorPolarity;
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

private:
    /* Data */
    epicsUInt32 **pMcaRaw;
    epicsUInt16 *pMapRaw;
    epicsFloat64 *tmpStats;

    int nChannels;
    unsigned int numModules;
    int *channelsPerModule;
    int *firstChanOnModule;
    int maxSCAs;

    epicsEvent *cmdStartEvent;
    epicsEvent *cmdStopEvent;
    epicsEvent *stoppedEvent;

    epicsUInt32 *currentBuf;
    int traceLength;
    epicsInt32 *traceBuffer;
    epicsFloat64 *traceTimeBuffer;
    epicsFloat64 *spectrumXAxisBuffer;
    
    moduleStatistics moduleStats[MAX_CHANNELS_PER_SYSTEM];

    bool polling;
    int uniqueId;
    char attrRealTimeName           [MAX_CHANNELS_PER_SYSTEM][MAX_ATTR_NAME_LEN];
    char attrRealTimeDescription    [MAX_CHANNELS_PER_SYSTEM][MAX_ATTR_NAME_LEN];
    char attrLiveTimeName           [MAX_CHANNELS_PER_SYSTEM][MAX_ATTR_NAME_LEN];
    char attrLiveTimeDescription    [MAX_CHANNELS_PER_SYSTEM][MAX_ATTR_NAME_LEN];
    char attrTriggersName           [MAX_CHANNELS_PER_SYSTEM][MAX_ATTR_NAME_LEN];
    char attrTriggersDescription    [MAX_CHANNELS_PER_SYSTEM][MAX_ATTR_NAME_LEN];
    char attrOutputCountsName       [MAX_CHANNELS_PER_SYSTEM][MAX_ATTR_NAME_LEN];
    char attrOutputCountsDescription[MAX_CHANNELS_PER_SYSTEM][MAX_ATTR_NAME_LEN];
 
};

#ifdef __cplusplus
extern "C"
{
#endif

int NDDxpConfig(const char *portName, int nChannels, int maxBuffers, size_t maxMemory);

#ifdef __cplusplus
}
#endif

#endif
