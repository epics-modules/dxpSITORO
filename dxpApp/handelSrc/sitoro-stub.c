/*
 * Stubs for all SiToro API functions so code can be built on platforms
 * that are not supported.
 */

#include <sitoro.h>

#define _EB SITORO_EXPORT_FUNC_PRE
#define _EA SITORO_EXPORT_FUNC_POST

#ifdef _MSC_VER
#pragma warning(disable: 4100) /* unreferenced formal parameter */
#pragma warning(disable: 4127) /* conditional expression is constant */
#endif

SiToro_Result _EB siToro_getApiVersion(uint32_t* major,
                                       uint32_t* minor,
                                       uint32_t* revision) _EA
{
  return SiToro_Result_InternalError;
}

_EB const char* siToro_getLibraryBuildDate() _EA
{
  return "No date (stub library)";
}

_EB const char* siToro_getErrorMessage(SiToro_Result result) _EA
{
  return "No error messages (stub library)";
}

SiToro_Result _EB siToro_instrument_findAll(uint32_t* numFound,
                                            uint32_t* ids,
                                            uint32_t maxIds) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_open(uint32_t id, SiToro_InstrumentHandle* handle) _EA
{
  return SiToro_Result_InternalError;
}

siBool _EB siToro_instrument_isOpen(SiToro_InstrumentHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_close(SiToro_InstrumentHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

void _EB siToro_instrument_closeAll() _EA
{
}

SiToro_Result _EB siToro_instrument_getId(SiToro_InstrumentHandle handle, uint32_t* id) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getName(SiToro_InstrumentHandle handle,
                                            char* buffer,
                                            uint32_t maxLength) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getNumCards(SiToro_InstrumentHandle handle,
                                                uint32_t* cardCount) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getCardSerialNumber(SiToro_InstrumentHandle handle,
                                                        uint32_t cardIndex,
                                                        uint32_t* serialNum) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getCardIndex(SiToro_InstrumentHandle handle,
                                                 uint32_t serialNum,
                                                 uint32_t* cardIndex) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_instrument_getSpatialStatsReporting(SiToro_InstrumentHandle handle,
                                                             siBool* enabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setSpatialStatsReporting(SiToro_InstrumentHandle handle,
                                                             siBool enabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getSpatialEventsReporting(SiToro_InstrumentHandle handle,
                                                              siBool* enabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setSpatialEventsReporting(SiToro_InstrumentHandle handle,
                                                              siBool enabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getSpatialAxisEnabled(SiToro_InstrumentHandle handle,
                                                          uint8_t axis,
                                                          siBool* enabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setSpatialAxisEnabled(SiToro_InstrumentHandle handle,
                                                          uint8_t axis,
                                                          siBool enabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getSpatialAxisType(SiToro_InstrumentHandle handle,
                                                       uint8_t axis,
                                                       SiToro_SpatialInputType* type) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setSpatialAxisType(SiToro_InstrumentHandle handle,
                                                       uint8_t axis,
                                                       SiToro_SpatialInputType type) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getSpatialAxisStepsPerUnit(SiToro_InstrumentHandle handle,
                                                               uint8_t axis,
                                                               uint32_t* stepsPerUnit) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setSpatialAxisStepsPerUnit(SiToro_InstrumentHandle handle,
                                                               uint8_t axis,
                                                               uint32_t stepsPerUnit) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getSpatialAxisStepOnReset(SiToro_InstrumentHandle handle,
                                                              uint8_t axis,
                                                              uint32_t* stepOnReset) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setSpatialAxisStepOnReset(SiToro_InstrumentHandle handle,
                                                              uint8_t axis,
                                                              uint32_t stepOnReset) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getSpatialAxisUnitOnReset(SiToro_InstrumentHandle handle,
                                                              uint8_t axis,
                                                              int32_t* unitOnReset) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setSpatialAxisUnitOnReset(SiToro_InstrumentHandle handle,
                                                              uint8_t axis,
                                                              int32_t unitOnReset) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_resetSpatialSystem(SiToro_InstrumentHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getNumGates(SiToro_InstrumentHandle handle, uint16_t* gateCount) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getGatingEnabled(SiToro_InstrumentHandle handle, siBool* enabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setGatingEnabled(SiToro_InstrumentHandle handle, siBool enabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getGatingPeriodicNotify(SiToro_InstrumentHandle handle,
                                                            siBool* enabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setGatingPeriodicNotify(SiToro_InstrumentHandle handle,
                                                            siBool enabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getGatingEventsReportMode(SiToro_InstrumentHandle handle,
                                                              uint16_t gate,
                                                              SiToro_GatingEventsReport* mode) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setGatingEventsReportMode(SiToro_InstrumentHandle handle,
                                                              uint16_t gate,
                                                              SiToro_GatingEventsReport mode) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getGatingStatsGatherMode(SiToro_InstrumentHandle handle,
                                                             uint16_t gate,
                                                             SiToro_GatingStatsGather* mode) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setGatingStatsGatherMode(SiToro_InstrumentHandle handle,
                                                             uint16_t gate,
                                                             SiToro_GatingStatsGather mode) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getGatingStatsReportMode(SiToro_InstrumentHandle handle,
                                                             uint16_t gate,
                                                             SiToro_GatingStatsReport* mode) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setGatingStatsReportMode(SiToro_InstrumentHandle handle,
                                                             uint16_t gate,
                                                             SiToro_GatingStatsReport mode) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_getGatingStatsResetMode(SiToro_InstrumentHandle handle,
                                                            uint16_t gate,
                                                            SiToro_GatingStatsReset* mode) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_setGatingStatsResetMode(SiToro_InstrumentHandle handle,
                                                            uint16_t gate,
                                                            SiToro_GatingStatsReset mode) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_instrument_resetGatingSystem(SiToro_InstrumentHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_open(SiToro_InstrumentHandle instrumentHandle,
                                   uint32_t cardIndex,
                                   SiToro_CardHandle* cardHandle) _EA
{
  return SiToro_Result_InternalError;
}

siBool        _EB siToro_card_isOpen(SiToro_CardHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_close(SiToro_CardHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_getInstrument(SiToro_CardHandle cardHandle,
                                            SiToro_InstrumentHandle* instrumentHandle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_reset(SiToro_CardHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_getSerialNumber(SiToro_CardHandle handle, uint32_t* num) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_getIndex(SiToro_CardHandle handle, uint32_t* index) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_getProductId(SiToro_CardHandle handle, uint16_t* id) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_getNumDetectors(SiToro_CardHandle handle, uint32_t* numDetectors) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_checkVersions(SiToro_CardHandle handle,
                                            siBool* dspOk,
                                            siBool* fpgaOk,
                                            siBool* bootLoaderOk) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_getDspVersion(SiToro_CardHandle handle,
                                            uint32_t* versionMajor,
                                            uint32_t* versionMinor,
                                            uint32_t* versionRevision) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_getFpgaVersion(SiToro_CardHandle handle, uint32_t* version) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_getBootLoaderVersion(SiToro_CardHandle handle,
                                                   uint32_t* major,
                                                   uint32_t* minor,
                                                   uint32_t* revision) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_getCurrentDspSlot(SiToro_CardHandle handle, uint8_t* slot) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_getCurrentFpgaSlot(SiToro_CardHandle handle, uint8_t* slot) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_getFpgaRunning(SiToro_CardHandle handle, siBool* isRunning) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_card_getName(SiToro_CardHandle handle, char* name, uint32_t maxLength) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_card_setName(SiToro_CardHandle handle, const char* name) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_cardFileSystem_getNumSlots(SiToro_CardHandle handle, uint8_t* numSlots) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_cardFileSystem_getFlags(SiToro_CardHandle handle,
                                                 uint8_t slot,
                                                 uint32_t* flags) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_cardFileSystem_setFlags(SiToro_CardHandle handle,
                                                 uint8_t slot,
                                                 uint32_t flags) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_cardFileSystem_getVersion(SiToro_CardHandle handle,
                                                   uint8_t slot,
                                                   uint32_t* version) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_cardFileSystem_setVersion(SiToro_CardHandle handle,
                                                   uint8_t slot,
                                                   uint32_t version) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_cardFileSystem_getFileSize(SiToro_CardHandle handle,
                                                    uint8_t slot,
                                                    uint32_t* fileSize) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_cardFileSystem_getSlotSize(SiToro_CardHandle handle,
                                                    uint8_t slot,
                                                    uint32_t* slotSize) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_cardFileSystem_getChecksum(SiToro_CardHandle handle,
                                                    uint8_t slot,
                                                    uint32_t* checksum) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_cardFileSystem_getFileName(SiToro_CardHandle handle,
                                                    uint8_t slot,
                                                    char* fileName,
                                                    uint32_t maxLength) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_cardFileSystem_setFileName(SiToro_CardHandle handle,
                                                    uint8_t slot,
                                                    const char* fileName) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_cardFileSystem_remove(SiToro_CardHandle handle, uint8_t slot) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_cardFileSystem_write(SiToro_CardHandle handle,
                                              uint8_t slot,
                                              const char* fileName,
                                              uint32_t version,
                                              uint32_t flags,
                                              const void* buffer,
                                              uint32_t numBytes) _EA
{
  return SiToro_Result_InternalError;
}



/*
 * Detector level
 */
SiToro_Result _EB siToro_detector_open(SiToro_CardHandle cardHandle,
                                       uint32_t detectorIndex,
                                       SiToro_DetectorHandle* detectorHandle) _EA
{
  return SiToro_Result_InternalError;
}

siBool        _EB siToro_detector_isOpen(SiToro_DetectorHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_close(SiToro_DetectorHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getCard(SiToro_DetectorHandle detectorHandle,
                                          SiToro_CardHandle* cardHandle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getInstrument(SiToro_DetectorHandle detectorHandle,
                                                SiToro_InstrumentHandle* instrumentHandle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getAnalogEnabled(SiToro_DetectorHandle handle, siBool* isEnabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setAnalogEnabled(SiToro_DetectorHandle handle, siBool enabled) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getAnalogOffset(SiToro_DetectorHandle handle, int16_t* offset) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setAnalogOffset(SiToro_DetectorHandle handle, int16_t offset) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getAnalogGain(SiToro_DetectorHandle handle, uint16_t* gain) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setAnalogGain(SiToro_DetectorHandle handle, uint16_t gain) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getAnalogGainBoost(SiToro_DetectorHandle handle, siBool* boost) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setAnalogGainBoost(SiToro_DetectorHandle handle, siBool boost) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getAnalogInvert(SiToro_DetectorHandle handle, siBool* on) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setAnalogInvert(SiToro_DetectorHandle handle, siBool on) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getAnalogDischarge(SiToro_DetectorHandle handle, siBool* enabled) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setAnalogDischarge(SiToro_DetectorHandle handle, siBool enabled) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getAnalogDischargeThreshold(SiToro_DetectorHandle handle,
                                                              uint16_t* threshold) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setAnalogDischargeThreshold(SiToro_DetectorHandle handle,
                                                              uint16_t threshold) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getAnalogDischargePeriod(SiToro_DetectorHandle handle,
                                                           uint16_t* samples) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setAnalogDischargePeriod(SiToro_DetectorHandle handle,
                                                           uint16_t samples) _EA
{
  return SiToro_Result_InternalError;
}


/* Digital detector settings */
SiToro_Result _EB siToro_detector_getSampleRate(SiToro_DetectorHandle handle, double* rateHz) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getDcOffset(SiToro_DetectorHandle handle, double* offset) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setDcOffset(SiToro_DetectorHandle handle, double offset) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_computeDcOffset(SiToro_DetectorHandle handle,
                                                  double* computedDcOffset) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getDcTrackingMode(SiToro_DetectorHandle handle,
                                                    SiToro_DcTrackingMode* mode) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setDcTrackingMode(SiToro_DetectorHandle handle,
                                                    SiToro_DcTrackingMode mode) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getOperatingMode(SiToro_DetectorHandle handle,
                                                   SiToro_OperatingMode* mode,
                                                   uint32_t* target) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setOperatingMode(SiToro_DetectorHandle handle,
                                                   SiToro_OperatingMode mode,
                                                   uint32_t target) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getResetBlanking(SiToro_DetectorHandle handle,
                                                   siBool* enabled,
                                                   double* threshold,
                                                   uint16_t* preSamples,
                                                   uint16_t* postSamples) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setResetBlanking(SiToro_DetectorHandle handle,
                                                   siBool enabled,
                                                   double threshold,
                                                   uint16_t preSamples,
                                                   uint16_t postSamples) _EA
{
  return SiToro_Result_InternalError;
}


/* Calibration */
SiToro_Result _EB siToro_detector_getCalibrated(SiToro_DetectorHandle handle,
                                                siBool* isCalibrated) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getCalibrationRunning(SiToro_DetectorHandle handle,
                                                        siBool* isRunning) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getCalibrationThresholds(SiToro_DetectorHandle handle,
                                                           double* noiseFloor,
                                                           double* minPulseAmplitude,
                                                           double* maxPulseAmplitude) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setCalibrationThresholds(SiToro_DetectorHandle handle,
                                                           double noiseFloor,
                                                           double minPulseAmplitude,
                                                           double maxPulseAmplitude) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getSourceType(SiToro_DetectorHandle handle,
                                                SiToro_SourceType* sourceType) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setSourceType(SiToro_DetectorHandle handle,
                                                SiToro_SourceType sourceType) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getCalibrationPulsesNeeded(SiToro_DetectorHandle handle,
                                                             uint32_t* pulsesNeeded) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setCalibrationPulsesNeeded(SiToro_DetectorHandle handle,
                                                             uint32_t pulsesNeeded) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getFilterCutoff(SiToro_DetectorHandle handle,
                                                  double* filterCutoff) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setFilterCutoff(SiToro_DetectorHandle handle,
                                                  double filterCutoff) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_startCalibration(SiToro_DetectorHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_cancelCalibration(SiToro_DetectorHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getCalibrationProgress(SiToro_DetectorHandle handle,
                                                         siBool* calibrating,
                                                         siBool* successful,
                                                         uint32_t* progressPercent,
                                                         char* stageDescription,
                                                         uint32_t maxDescriptionLength) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getCalibrationExamplePulse(SiToro_DetectorHandle handle,
                                                             double* x,
                                                             double* y,
                                                             uint32_t* length,
                                                             uint32_t maxLength) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getCalibrationModelPulse(SiToro_DetectorHandle handle,
                                                           double* x,
                                                           double* y,
                                                           uint32_t* length,
                                                           uint32_t maxLength) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getCalibrationFinalPulse(SiToro_DetectorHandle handle,
                                                           double* x,
                                                           double* y,
                                                           uint32_t* length,
                                                           uint32_t maxLength) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getCalibrationEstimatedCountRate(SiToro_DetectorHandle handle,
                                                                   double* rate) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getCalibrationData(SiToro_DetectorHandle handle,
                                                     char** encryptedData) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setCalibrationData(SiToro_DetectorHandle handle,
                                                     const char* encryptedData) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getOscilloscopeData(SiToro_DetectorHandle handle,
                                                      int16_t* rawBuffer,
                                                      int16_t* resetBlankedBuffer,
                                                      uint32_t bufferLength) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getMinPulsePairSeparation(SiToro_DetectorHandle handle,
                                                            uint32_t* samples) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setMinPulsePairSeparation(SiToro_DetectorHandle handle,
                                                            uint32_t samples) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getDetectionThreshold(SiToro_DetectorHandle handle,
                                                        double* threshold) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setDetectionThreshold(SiToro_DetectorHandle handle,
                                                        double threshold) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getValidatorThresholds(SiToro_DetectorHandle handle,
                                                         double* fixedThreshold,
                                                         double* proportionalThreshold) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setValidatorThresholds(SiToro_DetectorHandle handle,
                                                         double fixedThreshold,
                                                         double proportionalThreshold) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getPulseScaleFactor(SiToro_DetectorHandle handle, double* factor) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setPulseScaleFactor(SiToro_DetectorHandle handle, double factor) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getHistogramRunning(SiToro_DetectorHandle handle,
                                                      siBool* isRunning) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getNumHistogramBins(SiToro_DetectorHandle handle,
                                                      SiToro_HistogramBinSize* numBins) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setNumHistogramBins(SiToro_DetectorHandle handle,
                                                      SiToro_HistogramBinSize numBins) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_startHistogramCapture(SiToro_DetectorHandle handle,
                                                        SiToro_HistogramMode mode,
                                                        uint32_t target,
                                                        uint32_t startupBaselineMsec,
                                                        siBool resume) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_stopHistogramCapture(SiToro_DetectorHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_updateHistogram(SiToro_DetectorHandle handle,
                                                  uint32_t* timeToNextMsec) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getHistogramData(SiToro_DetectorHandle handle,
                                                   uint32_t* accepted,
                                                   uint32_t* rejected) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getHistogramTimeElapsed(SiToro_DetectorHandle handle,
                                                          double* timeElapsed) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getHistogramSamplesDetected(SiToro_DetectorHandle handle,
                                                              uint64_t* samples) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getHistogramSamplesErased(SiToro_DetectorHandle handle,
                                                            uint64_t* samples) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getHistogramPulsesDetected(SiToro_DetectorHandle handle,
                                                             uint64_t* pulses) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getHistogramPulsesAccepted(SiToro_DetectorHandle handle,
                                                             uint64_t* pulses) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getHistogramPulsesRejected(SiToro_DetectorHandle handle,
                                                             uint64_t* pulses) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getHistogramInputCountRate(SiToro_DetectorHandle handle,
                                                             double* rate) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getHistogramOutputCountRate(SiToro_DetectorHandle handle,
                                                              double* rate) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getHistogramDeadTime(SiToro_DetectorHandle handle,
                                                       double* deadTimePercent) _EA
{
  return SiToro_Result_InternalError;
}


/* List mode */
SiToro_Result _EB siToro_detector_getListModeOutputTimeStamp(SiToro_DetectorHandle handle,
                                                             SiToro_ListModeOutputTimeStamp* type) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setListModeOutputTimeStamp(SiToro_DetectorHandle handle,
                                                             SiToro_ListModeOutputTimeStamp type) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getListModeOutputStats(SiToro_DetectorHandle handle,
                                                         SiToro_ListModeOutputStats* type) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setListModeOutputStats(SiToro_DetectorHandle handle,
                                                         SiToro_ListModeOutputStats type) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getListModeOutputSpatial(SiToro_DetectorHandle handle,
                                                           SiToro_ListModeOutputSpatial* type) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setListModeOutputSpatial(SiToro_DetectorHandle handle,
                                                           SiToro_ListModeOutputSpatial type) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getListModeOutputGate(SiToro_DetectorHandle handle,
                                                        SiToro_ListModeOutputGate* type) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setListModeOutputGate(SiToro_DetectorHandle handle,
                                                        SiToro_ListModeOutputGate type) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getListModeOutputPulses(SiToro_DetectorHandle handle,
                                                          SiToro_ListModeOutputPulses* type) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setListModeOutputPulses(SiToro_DetectorHandle handle,
                                                          SiToro_ListModeOutputPulses type) _EA
{
  return SiToro_Result_InternalError;
}


SiToro_Result _EB siToro_detector_getListModeRunning(SiToro_DetectorHandle handle, siBool* isRunning) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_startListMode(SiToro_DetectorHandle handle,
                                                uint32_t startupBaselineMsec,
                                                uint32_t timeBetweenDataGetsMsec) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_stopListMode(SiToro_DetectorHandle handle) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getListModeData(SiToro_DetectorHandle handle,
                                                  uint32_t timeout,
                                                  uint32_t* buffer,
                                                  uint32_t  maxBufferSize,
                                                  uint32_t* numWritten,
                                                  uint32_t* errorBits) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getListModeDataAvailable(SiToro_DetectorHandle handle,
                                                           uint32_t* wordsAvailable) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getListModeStatus(SiToro_DetectorHandle handle,
                                                    uint32_t* errorBits) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_getDigitalGain(SiToro_DetectorHandle handle,
                                                 double* gain) _EA
{
  return SiToro_Result_InternalError;
}

SiToro_Result _EB siToro_detector_setDigitalGain(SiToro_DetectorHandle handle,
                                                 double gain) _EA
{
  return SiToro_Result_InternalError;
}
