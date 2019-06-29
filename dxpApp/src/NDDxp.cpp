/* NDDxp.cpp
 *
 * Driver for XIA FalconX modules 
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
#include <epicsTypes.h>
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

#define DXP_ALL                   -1
#define MAX_MCA_BINS           8192
#define DEFAULT_TRACE_POINTS   8192
#define LEN_SCA_NAME              10
#define MAPPING_CLOCK_PERIOD     320e-9

/** < The maximum number of 16-bit words in the mapping mode buffer */
#define MAPPING_BUFFER_SIZE 2228352*2
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
    NDDxpPixelAdvanceUser,
    NDDxpPixelAdvanceGate,
    NDDxpPixelAdvanceSync,
} NDDxpPixelAdvanceMode_t;

static const char *NDDxpBufferCharString[2]     = {"a", "b"};
static const char *NDDxpBufferFullString[2]     = {"buffer_full_a", "buffer_full_b"};
static const char *NDDxpBufferString[2]         = {"buffer_a", "buffer_b"};
static const char *NDDxpListBufferLenString[2]  = {"list_buffer_len_a", "list_buffer_len_b"};

static char SCA_NameLow[DXP_MAX_SCAS][LEN_SCA_NAME];
static char SCA_NameHigh[DXP_MAX_SCAS][LEN_SCA_NAME];


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
    : asynNDArrayDriver(portName, nChannels + 1, maxBuffers, maxMemory,
            asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask | asynFloat64ArrayMask | asynGenericPointerMask | asynOctetMask | asynDrvUserMask,
            asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask | asynFloat64ArrayMask | asynGenericPointerMask | asynOctetMask,
            ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, 0, 0),
    uniqueId(0)
{
    int status = asynSuccess;
    int i;
    int sca;
    char tmpStr[100];
    unsigned short tempUS;
    double clockSpeed;
    int xiastatus = 0;
    const char *functionName = "NDDxp";

    this->nChannels = nChannels;

    /* Register the epics exit function to be called when the IOC exits... */
    xiastatus = epicsAtExit(c_shutdown, this);

    /* Read the module information */
    getModuleInfo();

    xiastatus = xiaGetRunData(0, "max_sca_length",  &tempUS);
    if (xiastatus != 0) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error calling xiaGetRunData for max_sca_length, status=%d\n",
            driverName, functionName, xiastatus);
    }
    this->maxSCAs = (int)tempUS;

    /* There is a bug in the firmware which causes maxSCAs to be wrong. Force it to be 16/nChannels */
    //this->maxSCAs = 16/nChannels;

    /* Mapping mode parameters */
    createParam(NDDxpCollectModeString,            asynParamInt32,   &NDDxpCollectMode);
    createParam(NDDxpNDArrayModeString,            asynParamInt32,   &NDDxpNDArrayMode);
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
    createParam(NDDxpRisetimeOptimizationString,   asynParamFloat64, &NDDxpRisetimeOptimization);
    createParam(NDDxpNumMCAChannelsString,         asynParamInt32,   &NDDxpNumMCAChannels);
    createParam(NDDxpMCARefreshPeriodString,       asynParamFloat64, &NDDxpMCARefreshPeriod);
    createParam(NDDxpPresetModeString,             asynParamInt32,   &NDDxpPresetMode);
    createParam(NDDxpPresetRealString,             asynParamFloat64, &NDDxpPresetReal);
    createParam(NDDxpPresetEventsString,           asynParamInt32,   &NDDxpPresetEvents);
    createParam(NDDxpPresetTriggersString,         asynParamInt32,   &NDDxpPresetTriggers);

    /* Which of these to implement? */
    createParam(NDDxpDetectorPolarityString,       asynParamInt32,   &NDDxpDetectorPolarity);
    createParam(NDDxpDecayTimeString,              asynParamFloat64, &NDDxpDecayTime);
    createParam(NDDxpSpectrumXAxisString,          asynParamFloat64Array, &NDDxpSpectrumXAxis);
    createParam(NDDxpTriggerOutputString,          asynParamInt32,   &NDDxpTriggerOutput);
    createParam(NDDxpLiveTimeOutputString,         asynParamInt32,   &NDDxpLiveTimeOutput);

    /* SCA parameters */
    createParam(NDDxpSCATriggerModeString,         asynParamInt32,   &NDDxpSCATriggerMode);
    createParam(NDDxpSCAPulseDurationString,       asynParamInt32,   &NDDxpSCAPulseDuration);
    createParam(NDDxpMaxSCAsString,                asynParamInt32,   &NDDxpMaxSCAs);
    createParam(NDDxpNumSCAsString,                asynParamInt32,   &NDDxpNumSCAs);
    for (sca=0; sca<this->maxSCAs; sca++) {
        /* Create SCA name strings that Handel uses */
        sprintf(SCA_NameLow[sca],  "sca%d_lo", sca);
        sprintf(SCA_NameHigh[sca], "sca%d_hi", sca);
        /* Create asyn parameters for SCAs */
        sprintf(tmpStr, "DxpSCA%dLow", sca);
        createParam(tmpStr,                        asynParamInt32,   &NDDxpSCALow[sca]);
        sprintf(tmpStr, "DxpSCA%dHigh", sca);
        createParam(tmpStr,                        asynParamInt32,   &NDDxpSCAHigh[sca]);
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
    for (i=0; i<this->nChannels; i++) {
        this->pMcaRaw[i] = (epicsUInt32*)calloc(MAX_MCA_BINS, sizeof(epicsUInt32));
    }
    
    this->tmpStats = (epicsFloat64*)calloc(28, sizeof(epicsFloat64));
    this->currentBuf = (epicsUInt32*)calloc(this->nChannels, sizeof(epicsUInt32));

    this->traceLength = DEFAULT_TRACE_POINTS;

    /* Allocate a buffer for the trace data */
    this->traceBuffer = (epicsInt32 *)malloc(this->traceLength * sizeof(epicsInt32));

    /* Allocate a buffer for the trace time array */
    this->traceTimeBuffer = (epicsFloat64 *)calloc(this->traceLength, sizeof(epicsFloat64));

    /* Allocate a temporary buffer for use in mapping mode. */
    this->pMapRaw = (epicsUInt16*)malloc(MAPPING_BUFFER_SIZE * sizeof(epicsUInt16) * this->nChannels);
    
    /* Allocate an internal buffer long enough to hold all the energy values in a spectrum */
    this->spectrumXAxisBuffer = (epicsFloat64*)calloc(MAX_MCA_BINS, sizeof(epicsFloat64));

    /* Create the attribute names for MCA mapping mode */
    for (i=0; i<this->nChannels; i++) {
        sprintf(attrRealTimeName[i], "RealTime_%d", i);
        sprintf(attrRealTimeDescription[i], "Real time channel %d", i);
        sprintf(attrLiveTimeName[i], "TriggerLiveTime_%d", i); 
        sprintf(attrLiveTimeDescription[i], "Trigger live time channel %d", i); 
        sprintf(attrTriggersName[i], "Triggers_%d", i); 
        sprintf(attrTriggersDescription[i], "Trigger counts %d", i); 
        sprintf(attrOutputCountsName[i], "OutputCounts_%d", i); 
        sprintf(attrOutputCountsDescription[i], "Output counts %d", i);
    } 

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
    /* Get the clock speed in MHz, traceTime is in microseconds */
    xiastatus = xiaGetAcquisitionValues(0, "clock_speed", &clockSpeed);
    setDoubleParam(NDDxpTraceTime, 1./clockSpeed);
    for (i=0; i<=this->nChannels; i++) {
        setIntegerParam(i, NDDxpPresetMode, NDDxpPresetModeNone);
        setIntegerParam(i, NDDxpPresetEvents, 0);
        setIntegerParam(i, NDDxpPresetTriggers, 0);
        setIntegerParam(i, NDDxpForceRead, 0);
        setIntegerParam(i, NDDxpMaxSCAs, this->maxSCAs);
        setDoubleParam (i, mcaPresetCounts, 0.0);
        setDoubleParam (i, mcaElapsedCounts, 0.0);
        setDoubleParam (i, mcaPresetRealTime, 0.0);
        setIntegerParam(i, NDDxpCurrentPixel, 0);
        // We need to set this here because the NDDxpSCAHigh or Low could be processed with PINI before
        // NDDxpNumSCAs and then we would get an error
        setIntegerParam(i, NDDxpNumSCAs, this->maxSCAs);
    }
    setIntegerParam(NDDxpSCAPulseDuration, 100);

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
    int ch, ignored;
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
        for (ch=0; ch<this->nChannels; ch++) {
            CALLHANDEL( xiaBoardOperation(ch, "mapping_pixel_next", &ignored), "mapping_pixel_next" )
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
    else if ((function == NDDxpDetectionFilter)        ||
             (function == NDDxpMinPulsePairSeparation) ||
             (function == NDDxpDetectorPolarity)       ||
             (function == NDDxpTriggerOutput)          ||
             (function == NDDxpLiveTimeOutput)) 
    {
        this->setDxpParam(pasynUser, addr, function, (double)value);
    }
    else if ((function == NDDxpNumSCAs)          ||
             (function == NDDxpSCATriggerMode)   ||
             (function == NDDxpSCAPulseDuration) ||
             ((function >= NDDxpSCALow[0]) &&
              (function <= NDDxpSCAHigh[DXP_MAX_SCAS-1]))) 
    {
        this->setSCAs(pasynUser, addr);
    }
    else if (function == NDDxpSaveSystem) 
    {
        if (value) {
            callParamCallbacks(addr);
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
    else if (function < FIRST_DXP_PARAM) {
        status = asynNDArrayDriver::writeInt32(pasynUser, value);
    } 
    done:

    /* Call the callback */
    callParamCallbacks(addr);
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
       ((function == NDDxpDetectionThreshold)   ||
        (function == NDDxpScaleFactor)          ||
        (function == NDDxpRisetimeOptimization) ||
        (function == NDDxpDecayTime)            ||
        (function == NDDxpMCARefreshPeriod))
    {
        this->setDxpParam(pasynUser, addr, function, value);
    }
    else if (function < FIRST_DXP_PARAM) {
        status = asynNDArrayDriver::writeFloat64(pasynUser, value);
    } 
    /* Call the callback */
    callParamCallbacks(addr);

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
                callParamCallbacks(ch);
                setIntegerParam(ch, NDDxpForceRead, 0);
                callParamCallbacks(ch);
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
    static const char *functionName = "setDxpParam";

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d, function=%d, value=%f\n",
        driverName, functionName, addr, function, value);
    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) channel0 = 0; else channel0 = channel;

    xiastatus = xiaGetRunData(channel0, "run_active", &runActive);
    if (runActive) xiaStopRun(channel);

    if (function == NDDxpDetectorPolarity) {
        xiastatus = xiaSetAcquisitionValues(channel, "detector_polarity", &dvalue);
    } else if (function == NDDxpDetectionThreshold) {
        xiastatus = xiaSetAcquisitionValues(channel, "detection_threshold", &dvalue);
    } else if (function == NDDxpMinPulsePairSeparation) {
        xiastatus = xiaSetAcquisitionValues(channel, "min_pulse_pair_separation", &dvalue);
    } else if (function == NDDxpDetectionFilter) {
        xiastatus = xiaSetAcquisitionValues(channel, "detection_filter", &dvalue);
    } else if (function == NDDxpScaleFactor) {
        xiastatus = xiaSetAcquisitionValues(channel, "scale_factor", &dvalue);
    } else if (function == NDDxpRisetimeOptimization) {
        xiastatus = xiaSetAcquisitionValues(channel, "risetime_optimization", &dvalue);
    } else if (function == NDDxpDecayTime) {
        xiastatus = xiaSetAcquisitionValues(channel, "decay_time", &dvalue);
    } else if (function == NDDxpMCARefreshPeriod) {
        xiastatus = xiaSetAcquisitionValues(DXP_ALL, "mca_refresh", &dvalue);
    }
    this->getDxpParams(pasynUser, addr);
    if (runActive) xiaStartRun(channel, 1);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: xiastatus=%d, exit\n",
        driverName, functionName, xiastatus);
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
    int scaTriggerMode, scaPulseDuration;
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

    getIntegerParam(0, NDDxpSCATriggerMode, &scaTriggerMode);
    getIntegerParam(0, NDDxpSCAPulseDuration, &scaPulseDuration);
    dTmp = scaTriggerMode;
    CALLHANDEL(xiaSetAcquisitionValues(DXP_ALL, "sca_trigger_mode", &dTmp), "sca_trigger_mode");
    dTmp = scaPulseDuration;
    CALLHANDEL(xiaSetAcquisitionValues(DXP_ALL, "sca_pulse_duration", &dTmp), "sca_pulse_duration");

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
    int bufLen;
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

    // ignoreGate and inputLogicPolarity are used in both mca and mapping mode
    getIntegerParam(NDDxpIgnoreGate, &ignoreGate);
    getIntegerParam(NDDxpInputLogicPolarity, &inputLogicPolarity);
    dTmp = ignoreGate;
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
        "%s::%s [%d] setting gate_ignore = %f\n", 
        driverName, functionName, DXP_ALL, dTmp);
    xiastatus = xiaSetAcquisitionValues(DXP_ALL, "gate_ignore", &dTmp);
    status = this->xia_checkError(pasynUserSelf, xiastatus, "gate_ignore");

    dTmp = inputLogicPolarity;
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
        "%s::%s [%d] setting input_logic_polarity = %f\n", 
        driverName, functionName, DXP_ALL, dTmp);
    xiastatus = xiaSetAcquisitionValues(DXP_ALL, "input_logic_polarity", &dTmp);
    status = this->xia_checkError(pasynUserSelf, xiastatus, "input_logic_polarity");
    
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
            callParamCallbacks(i);
        }
        break;
    case NDDxpModeMCAMapping:
    case NDDxpModeSCAMapping:
    case NDDxpModeListMapping:
        getIntegerParam(NDDxpPixelAdvanceMode, (int *)&pixelAdvanceMode);
        getIntegerParam(NDDxpPixelsPerRun, &pixelsPerRun);
        getIntegerParam(NDDxpPixelsPerBuffer, &pixelsPerBuffer);
        if (pixelsPerBuffer == 0) pixelsPerBuffer = 0; /* Handel will compute maximum */
        getIntegerParam(NDDxpAutoPixelsPerBuffer, &autoPixelsPerBuffer);
        if (autoPixelsPerBuffer) pixelsPerBuffer = 0;  /* Handel will compute maximum */
        getIntegerParam(NDDxpSyncCount, &syncCount);
        if (syncCount < 1) syncCount = 1;
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
        dTmp = pixelAdvanceMode;
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s [%d] setting pixel_advance_mode = %f\n", 
            driverName, functionName, DXP_ALL, dTmp);
        xiastatus = xiaSetAcquisitionValues(DXP_ALL, "pixel_advance_mode", &dTmp);
        status = this->xia_checkError(pasynUserSelf, xiastatus, "pixel_advance_mode");

        dTmp = pixelsPerRun;
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s [%d] setting num_map_pixels = %f\n", 
            driverName, functionName, DXP_ALL, dTmp);
        xiastatus = xiaSetAcquisitionValues(DXP_ALL, "num_map_pixels", &dTmp);
        status = this->xia_checkError(pasynUserSelf, xiastatus, "num_map_pixels");

        dTmp = pixelsPerBuffer;
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s [%d] setting num_map_pixels_per_buffer = %f\n", 
            driverName, functionName, DXP_ALL, dTmp);
        xiastatus = xiaSetAcquisitionValues(DXP_ALL, "num_map_pixels_per_buffer", &dTmp);
        status = this->xia_checkError(pasynUserSelf, xiastatus, "num_map_pixels_per_buffer");

        /* The xMAP and Mercury actually divides the sync input by N+1, e.g. sync_count=0 does no division
         * of sync clock.  We subtract 1 so user-units are intuitive. */
        dTmp = syncCount-1;
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s [%d] setting sync_count = %f\n", 
            driverName, functionName, DXP_ALL, dTmp);
        xiastatus = xiaSetAcquisitionValues(DXP_ALL, "sync_count", &dTmp);
        status = this->xia_checkError(pasynUserSelf, xiastatus, "sync_count");

        for (i=0; i<this->nChannels; i++) {
            /* Clear the normal MCA mode status settings */
            setIntegerParam(i, NDDxpTriggers, 0);
            setDoubleParam(i, mcaElapsedRealTime, 0);
            setDoubleParam(i, NDDxpTriggerLiveTime, 0);
            setDoubleParam(i, mcaElapsedLiveTime, 0);
            callParamCallbacks(i);
            /* Read back the actual settings */
            getDxpParams(this->pasynUserSelf, i);
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
    unsigned long runActive;
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
        if (!acquiring) xiaStopRun(DXP_ALL);
    } else {
        /* Get the run time status from the handel library - informs whether the
         * HW is acquiring or not.        */
        CALLHANDEL( xiaGetRunData(channel, "run_active", &runActive), "xiaGetRunData (run_active)" )
        setIntegerParam(addr, NDDxpAcquiring, runActive);
    }
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
            if ((channel % this->channelsPerModule[0]) == 0) getModuleStatistics(pasynUser, channel, &moduleStats[0]);
            stats = &moduleStats[channel % this->channelsPerModule[0]];
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
        xiaGetAcquisitionValues(channel, "detection_threshold", &dvalue);
        setDoubleParam(channel, NDDxpDetectionThreshold, dvalue);
        xiaGetAcquisitionValues(channel, "min_pulse_pair_separation", &dvalue);
        setIntegerParam(channel, NDDxpMinPulsePairSeparation, (int)dvalue);
        xiaGetAcquisitionValues(channel, "detection_filter", &dvalue);
        setIntegerParam(channel, NDDxpDetectionFilter, (int)dvalue);
        xiaGetAcquisitionValues(channel, "scale_factor", &dvalue);
        setDoubleParam(channel, NDDxpScaleFactor, dvalue);
        xiaGetAcquisitionValues(channel, "risetime_optimization", &dvalue);
        setDoubleParam(channel, NDDxpRisetimeOptimization, dvalue);
        
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
            pixelAdvanceMode = (NDDxpPixelAdvanceMode_t)(int)dTmp;
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

            bufLen = 0;
            xiastatus = xiaGetRunData(channel, "buffer_len", &bufLen);
            status = this->xia_checkError(pasynUserSelf, xiastatus, "GET buffer_len");
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s [%d] Got buffer_len = %lu\n", 
                driverName, functionName, channel, bufLen);
            // Handel reports the buffer size in 32-bit words, we want to use 16-bit words
            setIntegerParam(channel, NDArraySize, (int)bufLen*2);
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
//            doCallbacksGenericPointer(pArray, NDArrayData, addr);
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
    int pixel;
    int numPixels=0;
    int arrayCallbacks;
    NDDataType_t dataType;
    int dxpNDArrayMode;
    int buf=0;
    int channel;
    int spectrumChannels=0;
    int blockSize = 0;
    NDArray *pArray=0;
    epicsUInt16 *pOut=0;
    falconBufferHeader *pBH=0;
    falconMCAPixelHeader *pMPH=0;
    epicsUInt16 *pData=0;
    epicsUInt16 *pBuffer;
    double realTime;
    double triggerLiveTime;
    double energyLiveTime, icr, ocr;
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
    getIntegerParam(NDDxpNDArrayMode, &dxpNDArrayMode);
    bufferCounter++;
    setIntegerParam(NDDxpBufferCounter, bufferCounter);
    getIntegerParam(NDArraySize, &arraySize);
    getDoubleParam(NDDxpMBytesRead, &mBytesRead);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    MBbufSize = (double)((arraySize)*sizeof(epicsUInt32)) / (double)MEGABYTE;

    /* First read and reset the buffers, do this as quickly as possible */
    for (channel=0, pBuffer=this->pMapRaw; channel<this->nChannels; channel++, pBuffer += arraySize) {
        buf = this->currentBuf[channel];

        /* The buffer is full so read it out */
        epicsTimeGetCurrent(&now);
        xiastatus = xiaGetRunData(channel, NDDxpBufferString[buf], pBuffer);
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
        pBH = (falconBufferHeader *)pBuffer;
        numPixels = pBH->numPixels;
        asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, 
            "%s::%s Got data! size=%.3fMB (%d) dt=%.3fs speed=%.3fMB/s\n",
            driverName, functionName, MBbufSize, arraySize, readoutTime, readoutBurstRate);
        asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, 
            "%s::%s channel=%d, bufferNumber=%d, firstPixel=%d, numPixels=%d\n",
            driverName, functionName, channel, pBH->bufferNumber, pBH->firstPixel, numPixels);
   
        /* If this is MCA mapping mode then copy the spectral data for the first pixel
         * in this buffer to the mcaRaw buffers.
         * This provides an update of the spectra and statistics while mapping is in progress
         * if the user sets the MCA spectra to periodically read. */
        if (pBH->mappingMode == NDDxpModeMCAMapping) {
            pMPH = (falconMCAPixelHeader *)(pBuffer + 256);
            spectrumChannels = pMPH->spectrumSize / 2;
            blockSize = pMPH->blockSize;
            pData = pBuffer + 512;
            memcpy(pMcaRaw[channel], pData, spectrumChannels*sizeof(epicsUInt32));
            realTime        = pMPH->realTime * MAPPING_CLOCK_PERIOD;
            triggerLiveTime = pMPH->triggerLiveTime * MAPPING_CLOCK_PERIOD;
            if (pMPH->triggers > 0.) 
                energyLiveTime = (triggerLiveTime * pMPH->outputCounts) / pMPH->triggers;
            else
                energyLiveTime = triggerLiveTime;
            if (triggerLiveTime > 0.)
                icr = pMPH->triggers / triggerLiveTime;
            else
                icr = 0.;
            if (realTime > 0.)
                ocr = pMPH->outputCounts / realTime;
            else
                ocr = 0.;
            setDoubleParam(channel, mcaElapsedRealTime, realTime);
            setDoubleParam(channel, mcaElapsedLiveTime, energyLiveTime);
            setDoubleParam(channel, NDDxpTriggerLiveTime, triggerLiveTime);
            setIntegerParam(channel,NDDxpEvents, pMPH->outputCounts);
            setIntegerParam(channel, NDDxpTriggers, pMPH->triggers);
            setDoubleParam(channel, NDDxpInputCountRate, icr);
            setDoubleParam(channel, NDDxpOutputCountRate, ocr);
            callParamCallbacks(channel);
        }
    }
    
    // We have now read the mapping data into a large buffer
        
    if (arrayCallbacks) {
        if (dxpNDArrayMode == dxpNDArrayModeRawBuffers) {
            dims[0] = arraySize;
            dims[1] = this->nChannels;
            pArray = this->pNDArrayPool->alloc(2, dims, NDUInt16, 0, NULL );
            memcpy(pArray->pData, this->pMapRaw, arraySize * this->nChannels * sizeof(epicsUInt16));
            updateTimeStamp(&pArray->epicsTS);
            pArray->timeStamp = pArray->epicsTS.secPastEpoch + pArray->epicsTS.nsec / 1.e9;
            /* Get any attributes that have been defined for this driver */
            this->getAttributes(pArray->pAttributeList);
            pArray->uniqueId = this->uniqueId++;
            // Get any other attributes
            doCallbacksGenericPointer(pArray, NDArrayData, 0);
            pArray->release();
        }
            
        else if (dxpNDArrayMode == dxpNDArrayModeMCASpectra) {
            for (pixel=0; pixel<numPixels; pixel++)  {
                spectrumChannels = pMPH->spectrumSize / 2;
                dims[0] = spectrumChannels;
                dims[1] = this->nChannels;
                pArray = this->pNDArrayPool->alloc(2, dims, NDUInt32, 0, NULL );
                pBuffer = this->pMapRaw;
                pOut = (epicsUInt16 *)pArray->pData;
                for (channel=0; channel<this->nChannels; channel++) {
                    pBH = (falconBufferHeader *)pBuffer;
                    pMPH = (falconMCAPixelHeader *)(pBuffer + 256 + pixel * blockSize);
                    pData = (pBuffer + 512 + pixel * blockSize);
                    memcpy(pOut, pData, pMPH->spectrumSize*sizeof(epicsUInt16));
                    // Create attributes for statistics
                    realTime        = pMPH->realTime * MAPPING_CLOCK_PERIOD;
                    triggerLiveTime = pMPH->triggerLiveTime * MAPPING_CLOCK_PERIOD;
                    pArray->pAttributeList->add(attrRealTimeName[channel],     attrRealTimeDescription[channel],     NDAttrFloat64, &realTime);
                    pArray->pAttributeList->add(attrLiveTimeName[channel],     attrLiveTimeDescription[channel],     NDAttrFloat64, &triggerLiveTime);
                    pArray->pAttributeList->add(attrTriggersName[channel],     attrTriggersDescription[channel],     NDAttrInt32,   &pMPH->triggers);
                    pArray->pAttributeList->add(attrOutputCountsName[channel], attrOutputCountsDescription[channel], NDAttrInt32,   &pMPH->outputCounts);
                    pBuffer += arraySize;
                    pOut += pMPH->spectrumSize;
                }
                /* Get any attributes that have been defined for this driver */
                this->getAttributes(pArray->pAttributeList);
                updateTimeStamp(&pArray->epicsTS);
                pArray->timeStamp = pArray->epicsTS.secPastEpoch + pArray->epicsTS.nsec / 1.e9;
                pArray->uniqueId = this->uniqueId++;
                doCallbacksGenericPointer(pArray, NDArrayData, 0);
                pArray->release();
            }
        }
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
    int i;
    double info[2];
    const char *functionName = "getTrace";

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d\n",
        driverName, functionName, addr);

    if (this->traceTimeBuffer[1] == 0) {
        double traceTime;
        /* We would like to do this in the constructor, but we can't do array callbacks until after iocInit */
        getDoubleParam(NDDxpTraceTime, &traceTime);
        for (i=0; i<this->traceLength; i++) this->traceTimeBuffer[i] = i*traceTime;
        doCallbacksFloat64Array(this->traceTimeBuffer, this->traceLength, NDDxpTraceTimeArray, 0);
    }
    if (addr == this->nChannels) channel = DXP_ALL;
    if (channel == DXP_ALL) {  /* All channels */
        for (i=0; i<this->nChannels; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getTrace(pasynUser, i, data, maxLen, actualLen);
        }
    } else {
        info[0] = this->traceLength;
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
    const char *functionName = "startAcquiring";

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
    for (i=0; i<this->nChannels; i++) this->currentBuf[i] = 0;

    // do xiaStart command
    CALLHANDEL( xiaStartRun(DXP_ALL, resume), "xiaStartRun()" )

    setIntegerParam(addr, NDDxpErased, 0); /* reset the erased flag */
    setIntegerParam(addr, mcaAcquiring, 1); /* Set the acquiring flag */

    if (channel == DXP_ALL) {
        for (i=0; i<this->nChannels; i++) {
            setIntegerParam(i, mcaAcquiring, 1);
            setIntegerParam(i, NDDxpErased, 0);
            callParamCallbacks(i);
        }
    }

    callParamCallbacks(addr);

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
                asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                    "%s::%s Detected acquisition stop! Now reading data\n",
                    driverName, functionName);
                this->getMcaData(this->pasynUserSelf, DXP_ALL);
                asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                    "%s::%s Detected acquisition stop! Now reading statistics\n",
                    driverName, functionName);
                this->getAcquisitionStatistics(this->pasynUserSelf, DXP_ALL);
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
        for (i=0; i<=this->nChannels; i++) callParamCallbacks(i);
        /* Copy internal acquiring flag to mcaAcquiring */
        for (i=0; i<=this->nChannels; i++) {
            getIntegerParam(i, NDDxpAcquiring, &acquiring);
            setIntegerParam(i, mcaAcquiring, acquiring);
            callParamCallbacks(i);
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
    int isFull;
    epicsUInt32 currentPixel = 0;
    const char* functionName = "pollMappingMode";
    NDDxpCollectMode_t mappingMode;
    
    getIntegerParam(NDDxpCollectMode, (int *)&mappingMode);

    for (ch=0; ch<this->nChannels; ch++)
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
            "%s::%s ch=%d, currentPixel=%d, buffer %s isfull=%d\n",
            driverName, functionName, ch, currentPixel, NDDxpBufferFullString[buf], isFull);
        if (!isFull) allFull = 0;
        if (isFull)  anyFull = 1;
    }

    /* In list mapping mode if any buffer is full then switch buffers on the non-full ones.
     * Note: this is prone to error because they could have already switched! */
    if (anyFull && (mappingMode == NDDxpModeListMapping)) {
        for (ch=0; ch<this->nChannels; ch++) {
            CALLHANDEL( xiaGetRunData(ch, NDDxpBufferFullString[buf], &isFull), "NDDxpBufferFullString[buf]" )
            if (isFull) continue;
            CALLHANDEL( xiaBoardOperation(ch, "buffer_switch", &ignored), "buffer_switch" )
        }
        /* Now wait until all modules report they are full */
        do {
            allFull = 1;
            for (ch=0; ch<this->nChannels; ch++) {
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

void NDDxp::getModuleInfo()
{
    char module_alias[MAXALIAS_LEN];
    char module_type[MAXITEM_LEN];
    unsigned int numDetectors;
    int status = 0;

    /* Get the number of detectors */
    xiaGetNumDetectors(&numDetectors);
    xiaGetNumModules(&this->numModules);
    /* Get the module alias for the first channel */
    status |= xiaGetModules_VB(0, module_alias);
    /* Get the module type for this module */
    status |= xiaGetModuleItem(module_alias, "module_type", module_type);
    /* Get the number of channels for this module */
    this->channelsPerModule = (int *)malloc(this->numModules * sizeof(int));
    status |= xiaGetModuleItem(module_alias, "number_of_channels", &this->channelsPerModule[0]);
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
