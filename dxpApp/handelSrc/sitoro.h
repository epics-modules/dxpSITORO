/*
===============================================================================
Copyright (c) 2012, 2013, Southern Innovation International Pty Ltd (SII).
All rights reserved.

Redistribution and use of this Header File only, with or without modification
is permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
  Redistributions in any other form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
  Neither the name of SII nor the names of its contributors may be used to
    endorse or promote products derived from this software without specific
    prior written permission.
  For the avoidance of doubt this license applies only to source files and not
    any associated binary files or Dynamic Link Libraries (DLLs).

EXCEPT AS EXPRESSLY STATED IN THIS LICENCE AND TO THE FULL EXTENT PERMITTED BY
APPLICABLE LAW, THE SOFTWARE IS PROVIDED "AS-IS". SII AND ITS CONTRIBUTORS MAKE
NO REPRESENTATIONS, WARRANTIES OR CONDITIONS OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO ANY REPRESENTATIONS, WARRANTIES OR CONDITIONS
REGARDING THE CONTENTS OR ACCURACY OF THE SOFTWARE, OR OF TITLE,
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, NON-INFRINGEMENT, THE
ABSENCE OF LATENT OR OTHER DEFECTS, OR THE PRESENCE OR ABSENCE OF ERRORS,
WHETHER OR NOT DISCOVERABLE.

TO THE FULL EXTENT PERMITTED BY APPLICABLE LAW, IN NO EVENT SHALL SII OR ITS
CONTRIBUTORS BE LIABLE ON ANY LEGAL THEORY (INCLUDING, WITHOUT LIMITATION, IN
AN ACTION FOR BREACH OF CONTRACT, NEGLIGENCE OR OTHERWISE) FOR ANY CLAIM, LOSS,
DAMAGES OR OTHER LIABILITY HOWSOEVER INCURRED. WITHOUT LIMITING THE SCOPE OF
THE PREVIOUS SENTENCE THE EXCLUSION OF LIABILITY SHALL INCLUDE: LOSS OF
PRODUCTION OR OPERATION TIME, LOSS, DAMAGE OR CORRUPTION OF DATA OR RECORDS; OR
LOSS OF ANTICIPATED SAVINGS, OPPORTUNITY, REVENUE, PROFIT OR GOODWILL, OR OTHER
ECONOMIC LOSS; OR ANY SPECIAL, INCIDENTAL, INDIRECT, CONSEQUENTIAL, PUNITIVE OR
EXEMPLARY DAMAGES, ARISING OUT OF OR IN CONNECTION WITH THIS LICENCE, THE USE
OF THE SOFTWARE OR THE USE OF OR OTHER DEALINGS WITH THE SOFTWARE, EVEN IF SII
OR ITS CONTRIBUTORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH CLAIM, LOSS,
DAMAGES OR OTHER LIABILITY.

APPLICABLE LEGISLATION SUCH AS THE AUSTRALIAN CONSUMER LAW MAY IMPLY
REPRESENTATIONS, WARRANTIES, OR CONDITIONS, OR IMPOSE OBLIGATIONS OR LIABILITY
ON SII OR ONE OF ITS CONTRIBUTORS IN RESPECT OF THE SOFTWARE THAT CANNOT BE
WHOLLY OR PARTLY EXCLUDED, RESTRICTED OR MODIFIED "CONSUMER GUARANTEES". IF
SUCH CONSUMER GUARANTEES APPLY THEN THE LIABILITY OF SII AND ITS CONTRIBUTORS
IS LIMITED, TO THE FULL EXTENT PERMITTED BY THE APPLICABLE LEGISLATION. WHERE
THE APPLICABLE LEGISLATION PERMITS THE FOLLOWING REMEDIES TO BE PROVIDED FOR
BREACH OF THE CONSUMER GUARANTEES THEN, AT ITS OPTION, SII'S LIABILITY IS
LIMITED TO ANY ONE OR MORE OF THEM:
  THE REPLACEMENT OF THE SOFTWARE, THE SUPPLY OF EQUIVALENT SOFTWARE, OR
    SUPPLYING RELEVANT SERVICES AGAIN;
  THE REPAIR OF THE SOFTWARE;
  THE PAYMENT OF THE COST OF REPLACING THE SOFTWARE, OF ACQUIRING EQUIVALENT
    SOFTWARE, HAVING THE RELEVANT SERVICES SUPPLIED AGAIN, OR HAVING THE
    SOFTWARE REPAIRED.

===============================================================================
*/


#ifndef SITORO_H
#define SITORO_H

#ifndef _WIN32
#pragma clang system_header
#endif


/**
 * \file
 */

/* =========================================================================
 *
 *   Compiler workarounds
 *
 * TODO: Update for VS2010 and later
 *
 * ========================================================================= */
#if defined(_MSC_VER)
    typedef __int8 int8_t;
    typedef unsigned __int8 uint8_t;
    typedef __int16 int16_t;
    typedef unsigned __int16 uint16_t;
    typedef __int32 int32_t;
    typedef unsigned __int32 uint32_t;
    typedef __int64 int64_t;
    typedef unsigned __int64 uint64_t;
#define SITORO_INLINE __inline
#else
#include <stdint.h>
#define SITORO_INLINE static inline
#endif

/* =========================================================================
 *
 *   Symbol visibility
 *
 * ========================================================================= */
#if !defined(__gcc__)
#define __gcc__ 0
#endif

#ifndef DOXYGEN
#if defined(SITORO_NO_IMPORTEXPORT) || defined(SWIG)
#define SITORO_EXPORT_FUNC_PRE
#define SITORO_EXPORT_FUNC_POST
#else
#ifdef _WIN32

#ifdef SITORO_EXPORTS
/* Windows uses different attributes for export and import */
#define SITORO_EXPORT_FUNC_PRE  __declspec(dllexport)
#define SITORO_EXPORT_FUNC_POST
#else
#define SITORO_EXPORT_FUNC_PRE  __declspec(dllimport)
#define SITORO_EXPORT_FUNC_POST
#endif

#elif __gcc__
/* GCC uses the same attribute for import and export of public symbols */
#define SITORO_EXPORT_FUNC_PRE
#define SITORO_EXPORT_FUNC_POST __attribute__((visibility("default")))

#else
#define SITORO_EXPORT_FUNC_PRE
#define SITORO_EXPORT_FUNC_POST

#endif
#endif
#endif

#if !defined(BETA_FEATURES)
#define BETA_FEATURES 0
#endif

#ifdef __cplusplus
    extern "C" {
#endif


/* =========================================================================
 *
 *   Data types
 *
 * ========================================================================= */

typedef unsigned siBool;

typedef struct
{
    void*  instrument;
} SiToro_InstrumentHandle;

typedef struct
{
    void*  card;
} SiToro_CardHandle;

typedef struct
{
    void*  detector;
} SiToro_DetectorHandle;

typedef void (*SiToro_ProgressFunction)(uint8_t percent, void* data);

typedef enum
{
    SiToro_FileSystemFlags_Empty                  = (1 << 0),
    SiToro_FileSystemFlags_Private                = (1 << 1),
    SiToro_FileSystemFlags_Zip                    = (1 << 2),
    SiToro_FileSystemFlags_Factory                = (1 << 3),
    SiToro_FileSystemFlags_AutoLoad               = (1 << 4),
    SiToro_FileSystemFlags_Dsp                    = (1 << 5),
    SiToro_FileSystemFlags_Fpga                   = (1 << 6)
} SiToro_FileSystemFlags;

typedef enum
{
    SiToro_DcTrackingMode_Off,
    SiToro_DcTrackingMode_Slow,
    SiToro_DcTrackingMode_Medium,
    SiToro_DcTrackingMode_Fast
} SiToro_DcTrackingMode;

#if BETA_FEATURES
typedef enum
{
    SiToro_DcTrackingSuspend_Off,
    SiToro_DcTrackingSuspend_OnSpectrumRun
} SiToro_DcTrackingSuspendMode;
#endif

typedef enum
{
    SiToro_SourceType_LowEnergy,
    SiToro_SourceType_LowRate,
    SiToro_SourceType_MidRate,
    SiToro_SourceType_HighRate
} SiToro_SourceType;

typedef enum
{
    SiToro_OperatingMode_OptimalResolution,
    SiToro_OperatingMode_ConstantResolution
} SiToro_OperatingMode;

typedef enum
{
    SiToro_HistogramBinSize_1024 = 1024,
    SiToro_HistogramBinSize_2048 = 2048,
    SiToro_HistogramBinSize_4096 = 4096,
    SiToro_HistogramBinSize_8192 = 8192
} SiToro_HistogramBinSize;

typedef enum
{
    SiToro_HistogramMode_Continuous,
    SiToro_HistogramMode_FixedTime,
    SiToro_HistogramMode_FixedInputCount,
    SiToro_HistogramMode_FixedOutputCount,
    SiToro_HistogramMode_MovingAverage,
    SiToro_HistogramMode_Gated
} SiToro_HistogramMode;

typedef enum
{
    SiToro_ListModeErrorCode_Healthy              = 0,
    SiToro_ListModeErrorCode_ApiInternalError     = 1000,
    SiToro_ListModeErrorCode_ApiBufferWriteFail   = 1001,
    SiToro_ListModeErrorCode_DspInternalError     = 2000,
    SiToro_ListModeErrorCode_DspBadParseState     = 2001,
    SiToro_ListModeErrorCode_DspBadStatsSubtype   = 2002,
    SiToro_ListModeErrorCode_DspBadSpatialSubtype = 2003,
    SiToro_ListModeErrorCode_DspBadPacketType     = 2004,
#if BETA_FEATURES
    SiToro_ListModeErrorCode_AdcPositiveRailHit   = 3000,
    SiToro_ListModeErrorCode_AdcNegativeRailHit   = 3001,
#endif
} SiToro_ListModeErrorCode;

typedef enum
{
    SiToro_ListModeErrorState_None                = 0,
    SiToro_ListModeErrorState_ApiBuffer           = (1 << 0),
    SiToro_ListModeErrorState_DspInternal         = (1 << 1),
    SiToro_ListModeErrorState_DspBadParseState    = (1 << 2),
    SiToro_ListModeErrorState_DspBadPacketType    = (1 << 3),
    SiToro_ListModeErrorState_FpgaBuffer          = (1 << 4),
#if BETA_FEATURES
    SiToro_ListModeErrorState_AdcPositiveRailHit  = (1 << 5),
    SiToro_ListModeErrorState_AdcNegativeRailHit  = (1 << 6),
#endif
} SiToro_ListModeErrorState;

typedef enum
{
    SiToro_ListModeOutputTimeStamp_Off            = 0xFFFFFFFF,
    SiToro_ListModeOutputTimeStamp_ShortWrap      = 0x00,
    SiToro_ListModeOutputTimeStamp_LongWrap       = 0x01
} SiToro_ListModeOutputTimeStamp;

typedef enum
{
    SiToro_ListModeOutputStats_Off                = 0xFFFFFFFF,
    SiToro_ListModeOutputStats_SmallCounters      = 0x02,
    SiToro_ListModeOutputStats_LargeCounters      = 0x03
} SiToro_ListModeOutputStats;

typedef enum
{
    SiToro_ListModeOutputSpatial_Off              = 0xFFFFFFFF,
    SiToro_ListModeOutputSpatial_OneAxis          = 0x06,
    SiToro_ListModeOutputSpatial_TwoAxis          = 0x07,
    SiToro_ListModeOutputSpatial_ThreeAxis        = 0x04,
    SiToro_ListModeOutputSpatial_FourAxis         = 0x08
} SiToro_ListModeOutputSpatial;

typedef enum
{
    SiToro_ListModeOutputGate_Off                 = 0xFFFFFFFF,
    SiToro_ListModeOutputGate_State               = 0x05
} SiToro_ListModeOutputGate;

typedef enum
{
    SiToro_ListModeOutputPulses_Off               = 0xFFFFFFFF,
    SiToro_ListModeOutputPulses_WithTimeOfArrival = 0x10,
    SiToro_ListModeOutputPulses_NoTimeOfArrival   = 0x11
} SiToro_ListModeOutputPulses;

typedef enum
{
    SiToro_ListModeEvent_TimeStampShortWrap       = SiToro_ListModeOutputTimeStamp_ShortWrap,
    SiToro_ListModeEvent_TimeStampLongWrap        = SiToro_ListModeOutputTimeStamp_LongWrap,
    SiToro_ListModeEvent_StatsSmallCounters       = SiToro_ListModeOutputStats_SmallCounters,
    SiToro_ListModeEvent_StatsLargeCounters       = SiToro_ListModeOutputStats_LargeCounters,
    SiToro_ListModeEvent_SpatialOneAxis           = SiToro_ListModeOutputSpatial_OneAxis,
    SiToro_ListModeEvent_SpatialTwoAxis           = SiToro_ListModeOutputSpatial_TwoAxis,
    SiToro_ListModeEvent_SpatialThreeAxis         = SiToro_ListModeOutputSpatial_ThreeAxis,
    SiToro_ListModeEvent_SpatialFourAxis          = SiToro_ListModeOutputSpatial_FourAxis,
    SiToro_ListModeEvent_GateState                = SiToro_ListModeOutputGate_State,
    SiToro_ListModeEvent_PulseWithTimeOfArrival   = SiToro_ListModeOutputPulses_WithTimeOfArrival,
    SiToro_ListModeEvent_PulseNoTimeOfArrival     = SiToro_ListModeOutputPulses_NoTimeOfArrival,
    SiToro_ListModeEvent_Error                    = 0x40
} SiToro_ListModeEvent;

typedef enum
{
    SiToro_SpatialInputType_Quadrature,
    SiToro_SpatialInputType_StepDirection
} SiToro_SpatialInputType;

typedef enum
{
    SiToro_GatingEventsReport_Off,
    SiToro_GatingEventsReport_RisingEdge,
    SiToro_GatingEventsReport_FallingEdge,
    SiToro_GatingEventsReport_AnyEdge
} SiToro_GatingEventsReport;

typedef enum
{
    SiToro_GatingStatsGather_Off,
    SiToro_GatingStatsGather_WhenHigh,
    SiToro_GatingStatsGather_WhenLow,
    SiToro_GatingStatsGather_Always
} SiToro_GatingStatsGather;

typedef enum
{
    SiToro_GatingStatsReport_Off,
    SiToro_GatingStatsReport_RisingEdge,
    SiToro_GatingStatsReport_FallingEdge
} SiToro_GatingStatsReport;

typedef enum
{
    SiToro_GatingStatsReset_Off,
    SiToro_GatingStatsReset_RisingEdge,
    SiToro_GatingStatsReset_FallingEdge
} SiToro_GatingStatsReset;

typedef enum
{
    SiToro_Result_Success,
    SiToro_Result_DetectorDisconnected,
    SiToro_Result_CardNotFound,
    SiToro_Result_DetectorNotFound,
    SiToro_Result_AlreadyOpen,
    SiToro_Result_HandleInvalid,
    SiToro_Result_NotOpen,
    SiToro_Result_InternalError,
    SiToro_Result_BadValue,
    SiToro_Result_InvalidCardSoftwareVersion,
    SiToro_Result_FeatureNotImplemented,
    SiToro_Result_OperationRunning,
    SiToro_Result_NoEnergyData,
    SiToro_Result_NoCalibrationData,
    SiToro_Result_NullPointerPassed,
    SiToro_Result_InvalidMemoryHandling,
    SiToro_Result_InvalidCalibrationString,
    SiToro_Result_StaleCalibration,
    SiToro_Result_ConfigChangeNotPermitted,
    SiToro_Result_BufferTooSmall,
    SiToro_Result_NotFound,
    SiToro_Result_TooBig,
    SiToro_Result_TooMany,
    SiToro_Result_CardHasBeenReset,
    SiToro_Result_FpgaFailure,
    SiToro_Result_InvalidFpgaVersion,
    SiToro_Result_HistogramNotRunning,
    SiToro_Result_ListModeNotRunning,
    SiToro_Result_CalibrationNotRunning,
    SiToro_Result_StartupBaselineFailed,
    SiToro_Result_HistogramFpgaBadData,
#if BETA_FEATURES
    SiToro_Result_AdcPositiveRailExceeded,
    SiToro_Result_AdcNegativeRailExceeded,
#endif
    SiToro_Result_NotReadable,
    SiToro_Result_NotWritable,
    SiToro_Result_AlreadyExists,
    SiToro_Result_InvalidFirmware,
    SiToro_Result_InvalidFormat,
    SiToro_Result_CorruptContents,
    SiToro_Result_MismatchedProductId,
    SiToro_Result_NoRollBackAvailable,
    SiToro_Result_HistogramFetchTooSlow,
    SiToro_Result_CpuCouldNotKeepUp,
    SiToro_Result_GenericError = 10000
} SiToro_Result;


/* =========================================================================
 *
 *   Function prototypes
 *
 * ========================================================================= */

/*
 * Library level
 */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_getApiVersion(uint32_t* major,
                                                          uint32_t* minor,
                                                          uint32_t* revision) SITORO_EXPORT_FUNC_POST;
SITORO_EXPORT_FUNC_PRE const char*   siToro_getLibraryBuildDate() SITORO_EXPORT_FUNC_POST;
SITORO_EXPORT_FUNC_PRE const char*   siToro_getErrorMessage(SiToro_Result result) SITORO_EXPORT_FUNC_POST;

/*
 * Instrument level
 */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_findAll(uint32_t* numFound,
                                                               uint32_t* ids,
                                                               uint32_t maxIds) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_open(uint32_t id, SiToro_InstrumentHandle* handle) SITORO_EXPORT_FUNC_POST;
siBool        SITORO_EXPORT_FUNC_PRE siToro_instrument_isOpen(SiToro_InstrumentHandle handle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_close(SiToro_InstrumentHandle handle) SITORO_EXPORT_FUNC_POST;
void          SITORO_EXPORT_FUNC_PRE siToro_instrument_closeAll() SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getId(SiToro_InstrumentHandle handle, uint32_t* id) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getProductId(SiToro_InstrumentHandle handle, uint16_t* id) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getName(SiToro_InstrumentHandle handle,
                                                               char* buffer,
                                                               uint32_t maxLength) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getNumCards(SiToro_InstrumentHandle handle, uint32_t* cardCount) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getCardSerialNumber(SiToro_InstrumentHandle handle,
                                                                           uint32_t cardIndex,
                                                                           uint32_t* serialNum) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getCardIndex(SiToro_InstrumentHandle handle,
                                                                    uint32_t serialNum,
                                                                    uint32_t* cardIndex) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_reboot(SiToro_InstrumentHandle handle) SITORO_EXPORT_FUNC_POST;

/* Spatial system */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getSpatialStatsReporting(SiToro_InstrumentHandle handle, siBool* enabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setSpatialStatsReporting(SiToro_InstrumentHandle handle, siBool enabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getSpatialEventsReporting(SiToro_InstrumentHandle handle, siBool* enabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setSpatialEventsReporting(SiToro_InstrumentHandle handle, siBool enabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getNumSpatialAxes(SiToro_InstrumentHandle handle, uint8_t* axisCount) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getSpatialAxisEnabled(SiToro_InstrumentHandle handle,
                                                                             uint8_t axis,
                                                                             siBool* enabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setSpatialAxisEnabled(SiToro_InstrumentHandle handle,
                                                                             uint8_t axis,
                                                                             siBool enabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getSpatialAxisType(SiToro_InstrumentHandle handle,
                                                                          uint8_t axis,
                                                                          SiToro_SpatialInputType* type) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setSpatialAxisType(SiToro_InstrumentHandle handle,
                                                                          uint8_t axis,
                                                                          SiToro_SpatialInputType type) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getSpatialAxisStepsPerUnit(SiToro_InstrumentHandle handle,
                                                                                  uint8_t axis,
                                                                                  uint32_t* stepsPerUnit) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setSpatialAxisStepsPerUnit(SiToro_InstrumentHandle handle,
                                                                                  uint8_t axis,
                                                                                  uint32_t stepsPerUnit) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getSpatialAxisStepOnReset(SiToro_InstrumentHandle handle,
                                                                                 uint8_t axis,
                                                                                 uint32_t* stepOnReset) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setSpatialAxisStepOnReset(SiToro_InstrumentHandle handle,
                                                                                 uint8_t axis,
                                                                                 uint32_t stepOnReset) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getSpatialAxisUnitOnReset(SiToro_InstrumentHandle handle,
                                                                                 uint8_t axis,
                                                                                 int32_t* unitOnReset) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setSpatialAxisUnitOnReset(SiToro_InstrumentHandle handle,
                                                                                 uint8_t axis,
                                                                                 int32_t unitOnReset) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_resetSpatialSystem(SiToro_InstrumentHandle handle) SITORO_EXPORT_FUNC_POST;

/* Gating system */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getNumGates(SiToro_InstrumentHandle handle, uint16_t* gateCount) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getGatingEnabled(SiToro_InstrumentHandle handle, siBool* enabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setGatingEnabled(SiToro_InstrumentHandle handle, siBool enabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getGatingPeriodicNotify(SiToro_InstrumentHandle handle, siBool* enabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setGatingPeriodicNotify(SiToro_InstrumentHandle handle, siBool enabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getGatingEventsReportMode(SiToro_InstrumentHandle handle,
                                                                                 uint16_t gate,
                                                                                 SiToro_GatingEventsReport* mode) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setGatingEventsReportMode(SiToro_InstrumentHandle handle,
                                                                                 uint16_t gate,
                                                                                 SiToro_GatingEventsReport mode) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getGatingStatsGatherMode(SiToro_InstrumentHandle handle,
                                                                                uint16_t gate,
                                                                                SiToro_GatingStatsGather* mode) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setGatingStatsGatherMode(SiToro_InstrumentHandle handle,
                                                                                uint16_t gate,
                                                                                SiToro_GatingStatsGather mode) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getGatingStatsReportMode(SiToro_InstrumentHandle handle,
                                                                                uint16_t gate,
                                                                                SiToro_GatingStatsReport* mode) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setGatingStatsReportMode(SiToro_InstrumentHandle handle,
                                                                                uint16_t gate,
                                                                                SiToro_GatingStatsReport mode) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_getGatingStatsResetMode(SiToro_InstrumentHandle handle,
                                                                               uint16_t gate,
                                                                               SiToro_GatingStatsReset* mode) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_setGatingStatsResetMode(SiToro_InstrumentHandle handle,
                                                                               uint16_t gate,
                                                                               SiToro_GatingStatsReset mode) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_instrument_resetGatingSystem(SiToro_InstrumentHandle handle) SITORO_EXPORT_FUNC_POST;


/*
 * Firmware management
 */
siBool        SITORO_EXPORT_FUNC_PRE siToro_firmware_isFirmwareFile(const char* fileName) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_firmware_getFirmwareDetails(const char* fileName,
                                                                        uint64_t* contentsVersion,
                                                                        uint32_t* numProductIds,
                                                                        uint16_t* productIds,
                                                                        uint32_t maxProductIds) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_firmware_uploadFromFile(SiToro_InstrumentHandle handle,
                                                                    const char* fileName,
                                                                    SiToro_ProgressFunction progressFunc,
                                                                    void* funcData) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_firmware_factoryRevert(SiToro_InstrumentHandle handle) SITORO_EXPORT_FUNC_POST;


/*
 * Card level
 */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_open(SiToro_InstrumentHandle instrumentHandle,
                                                      uint32_t cardIndex,
                                                      SiToro_CardHandle* cardHandle) SITORO_EXPORT_FUNC_POST;
siBool        SITORO_EXPORT_FUNC_PRE siToro_card_isOpen(SiToro_CardHandle handle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_close(SiToro_CardHandle handle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getInstrument(SiToro_CardHandle cardHandle,
                                                               SiToro_InstrumentHandle* instrumentHandle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_reset(SiToro_CardHandle handle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getSerialNumber(SiToro_CardHandle handle, uint32_t* num) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getIndex(SiToro_CardHandle handle, uint32_t* index) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getProductId(SiToro_CardHandle handle, uint16_t* id) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getNumDetectors(SiToro_CardHandle handle, uint32_t* numDetectors) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_checkVersions(SiToro_CardHandle handle,
                                                               siBool* dspOk,
                                                               siBool* fpgaOk,
                                                               siBool* bootLoaderOk) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getDspVersion(SiToro_CardHandle handle,
                                                               uint32_t* versionMajor,
                                                               uint32_t* versionMinor,
                                                               uint32_t* versionRevision) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getFpgaVersion(SiToro_CardHandle handle, uint32_t* version) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getBootLoaderVersion(SiToro_CardHandle handle,
                                                                      uint32_t* major,
                                                                      uint32_t* minor,
                                                                      uint32_t* revision) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getCurrentDspSlot(SiToro_CardHandle handle, uint8_t* slot) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getCurrentFpgaSlot(SiToro_CardHandle handle, uint8_t* slot) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getFpgaRunning(SiToro_CardHandle handle, siBool* isRunning) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_getName(SiToro_CardHandle handle, char* name, uint32_t maxLength) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_card_setName(SiToro_CardHandle handle, const char* name) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_getNumSlots(SiToro_CardHandle handle, uint8_t* numSlots) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_getFlags(SiToro_CardHandle handle, uint8_t slot, uint32_t* flags) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_setFlags(SiToro_CardHandle handle, uint8_t slot, uint32_t flags) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_getVersion(SiToro_CardHandle handle, uint8_t slot, uint32_t* version) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_setVersion(SiToro_CardHandle handle, uint8_t slot, uint32_t version) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_getFileSize(SiToro_CardHandle handle, uint8_t slot, uint32_t* fileSize) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_getSlotSize(SiToro_CardHandle handle, uint8_t slot, uint32_t* slotSize) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_getChecksum(SiToro_CardHandle handle, uint8_t slot, uint32_t* checksum) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_getFileName(SiToro_CardHandle handle,
                                                                       uint8_t slot,
                                                                       char* fileName,
                                                                       uint32_t maxLength) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_setFileName(SiToro_CardHandle handle, uint8_t slot, const char* fileName) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_remove(SiToro_CardHandle handle, uint8_t slot) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_cardFileSystem_write(SiToro_CardHandle handle,
                                                                 uint8_t slot,
                                                                 const char* fileName,
                                                                 uint32_t version,
                                                                 uint32_t flags,
                                                                 const void* buffer,
                                                                 uint32_t numBytes) SITORO_EXPORT_FUNC_POST;


/*
 * Detector level
 */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_open(SiToro_CardHandle cardHandle,
                                                          uint32_t detectorIndex,
                                                          SiToro_DetectorHandle* detectorHandle) SITORO_EXPORT_FUNC_POST;
siBool        SITORO_EXPORT_FUNC_PRE siToro_detector_isOpen(SiToro_DetectorHandle handle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_close(SiToro_DetectorHandle handle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getCard(SiToro_DetectorHandle detectorHandle,
                                                             SiToro_CardHandle* cardHandle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getInstrument(SiToro_DetectorHandle detectorHandle,
                                                                   SiToro_InstrumentHandle* instrumentHandle) SITORO_EXPORT_FUNC_POST;


/* Analog detector settings */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getAnalogEnabled(SiToro_DetectorHandle handle, siBool* isEnabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setAnalogEnabled(SiToro_DetectorHandle handle, siBool enabled) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getAnalogOffset(SiToro_DetectorHandle handle, int16_t* offset) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setAnalogOffset(SiToro_DetectorHandle handle, int16_t offset) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getAnalogGain(SiToro_DetectorHandle handle, uint16_t* gain) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setAnalogGain(SiToro_DetectorHandle handle, uint16_t gain) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getAnalogGainBoost(SiToro_DetectorHandle handle, siBool* boost) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setAnalogGainBoost(SiToro_DetectorHandle handle, siBool boost) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getAnalogInvert(SiToro_DetectorHandle handle, siBool* on) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setAnalogInvert(SiToro_DetectorHandle handle, siBool on) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getAnalogDischarge(SiToro_DetectorHandle handle, siBool* enabled) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setAnalogDischarge(SiToro_DetectorHandle handle, siBool enabled) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getAnalogDischargeThreshold(SiToro_DetectorHandle handle, uint16_t* threshold) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setAnalogDischargeThreshold(SiToro_DetectorHandle handle, uint16_t threshold) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getAnalogDischargePeriod(SiToro_DetectorHandle handle, uint16_t* samples) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setAnalogDischargePeriod(SiToro_DetectorHandle handle, uint16_t samples) SITORO_EXPORT_FUNC_POST;

/* Digital detector settings */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getSampleRate(SiToro_DetectorHandle handle, double* rateHz) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getDcOffset(SiToro_DetectorHandle handle, double* offset) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setDcOffset(SiToro_DetectorHandle handle, double offset) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_computeDcOffset(SiToro_DetectorHandle handle, double* computedDcOffset) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getDcTrackingMode(SiToro_DetectorHandle handle, SiToro_DcTrackingMode* mode) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setDcTrackingMode(SiToro_DetectorHandle handle, SiToro_DcTrackingMode mode) SITORO_EXPORT_FUNC_POST;

#if BETA_FEATURES
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getDcTrackingSuspendMode(SiToro_DetectorHandle handle, SiToro_DcTrackingSuspendMode *suspendMode) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setDcTrackingSuspendMode(SiToro_DetectorHandle handle, SiToro_DcTrackingSuspendMode suspendMode) SITORO_EXPORT_FUNC_POST;
#endif

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getOperatingMode(SiToro_DetectorHandle handle,
                                                                      SiToro_OperatingMode* mode,
                                                                      uint32_t* target) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setOperatingMode(SiToro_DetectorHandle handle,
                                                                      SiToro_OperatingMode mode,
                                                                      uint32_t target) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getResetBlanking(SiToro_DetectorHandle handle,
                                                                      siBool* enabled,
                                                                      double* threshold,
                                                                      uint16_t* preSamples,
                                                                      uint16_t* postSamples) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setResetBlanking(SiToro_DetectorHandle handle,
                                                                      siBool enabled,
                                                                      double threshold,
                                                                      uint16_t preSamples,
                                                                      uint16_t postSamples) SITORO_EXPORT_FUNC_POST;

/* Calibration */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getCalibrated(SiToro_DetectorHandle handle, siBool* isCalibrated) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getCalibrationRunning(SiToro_DetectorHandle handle, siBool* isRunning) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getCalibrationThresholds(SiToro_DetectorHandle handle,
                                                                              double* noiseFloor,
                                                                              double* minPulseAmplitude,
                                                                              double* maxPulseAmplitude) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setCalibrationThresholds(SiToro_DetectorHandle handle,
                                                                              double noiseFloor,
                                                                              double minPulseAmplitude,
                                                                              double maxPulseAmplitude) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getSourceType(SiToro_DetectorHandle handle,
                                                                   SiToro_SourceType* sourceType) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setSourceType(SiToro_DetectorHandle handle,
                                                                   SiToro_SourceType sourceType) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getCalibrationPulsesNeeded(SiToro_DetectorHandle handle, uint32_t* pulsesNeeded) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setCalibrationPulsesNeeded(SiToro_DetectorHandle handle, uint32_t pulsesNeeded) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getFilterCutoff(SiToro_DetectorHandle handle, double* filterCutoff) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setFilterCutoff(SiToro_DetectorHandle handle, double filterCutoff) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_startCalibration(SiToro_DetectorHandle handle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_cancelCalibration(SiToro_DetectorHandle handle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getCalibrationProgress(SiToro_DetectorHandle handle,
                                                                            siBool* calibrating,
                                                                            siBool* successful,
                                                                            uint32_t* progressPercent,
                                                                            char* stageDescription,
                                                                            uint32_t maxDescriptionLength) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getCalibrationExamplePulse(SiToro_DetectorHandle handle,
                                                                                double* x,
                                                                                double* y,
                                                                                uint32_t* length,
                                                                                uint32_t maxLength) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getCalibrationModelPulse(SiToro_DetectorHandle handle,
                                                                              double* x,
                                                                              double* y,
                                                                              uint32_t* length,
                                                                              uint32_t maxLength) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getCalibrationFinalPulse(SiToro_DetectorHandle handle,
                                                                              double* x,
                                                                              double* y,
                                                                              uint32_t* length,
                                                                              uint32_t maxLength) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getCalibrationEstimatedCountRate(SiToro_DetectorHandle handle, double* rate) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getCalibrationData(SiToro_DetectorHandle handle, char** encryptedData) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setCalibrationData(SiToro_DetectorHandle handle, const char* encryptedData) SITORO_EXPORT_FUNC_POST;


/* Oscilloscope */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getOscilloscopeData(SiToro_DetectorHandle handle,
                                                                         int16_t* rawBuffer,
                                                                         int16_t* resetBlankedBuffer,
                                                                         uint32_t bufferLength) SITORO_EXPORT_FUNC_POST;

/* Histogram */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getMinPulsePairSeparation(SiToro_DetectorHandle handle, uint32_t* samples) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setMinPulsePairSeparation(SiToro_DetectorHandle handle, uint32_t samples) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getDetectionThreshold(SiToro_DetectorHandle handle, double* threshold) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setDetectionThreshold(SiToro_DetectorHandle handle, double threshold) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getValidatorThresholds(SiToro_DetectorHandle handle,
                                                                            double* fixedThreshold,
                                                                            double* proportionalThreshold) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setValidatorThresholds(SiToro_DetectorHandle handle,
                                                                            double fixedThreshold,
                                                                            double proportionalThreshold) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getPulseScaleFactor(SiToro_DetectorHandle handle, double* factor) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setPulseScaleFactor(SiToro_DetectorHandle handle, double factor) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramRunning(SiToro_DetectorHandle handle, siBool* isRunning) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getNumHistogramBins(SiToro_DetectorHandle handle, SiToro_HistogramBinSize* numBins) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setNumHistogramBins(SiToro_DetectorHandle handle, SiToro_HistogramBinSize numBins) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_startHistogramCapture(SiToro_DetectorHandle handle,
                                                                           SiToro_HistogramMode mode,
                                                                           uint32_t target,
                                                                           uint32_t startupBaselineMsec,
                                                                           siBool resume) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_stopHistogramCapture(SiToro_DetectorHandle handle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_updateHistogram(SiToro_DetectorHandle handle, uint32_t* timeToNextMsec) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramData(SiToro_DetectorHandle handle,
                                                                      uint32_t* accepted,                           /* These buffers must be at */
                                                                      uint32_t* rejected) SITORO_EXPORT_FUNC_POST;  /* least numBins long       */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramTimeElapsed(SiToro_DetectorHandle handle, double* timeElapsed) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramSamplesDetected(SiToro_DetectorHandle handle, uint64_t* samples) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramSamplesErased(SiToro_DetectorHandle handle, uint64_t* samples) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramPulsesDetected(SiToro_DetectorHandle handle, uint64_t* pulses) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramPulsesAccepted(SiToro_DetectorHandle handle, uint64_t* pulses) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramPulsesRejected(SiToro_DetectorHandle handle, uint64_t* pulses) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramInputCountRate(SiToro_DetectorHandle handle, double* rate) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramOutputCountRate(SiToro_DetectorHandle handle, double* rate) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramDeadTime(SiToro_DetectorHandle handle, double* deadTimePercent) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getHistogramGateState(SiToro_DetectorHandle handle, siBool* state) SITORO_EXPORT_FUNC_POST;

/* List mode */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getListModeOutputTimeStamp(SiToro_DetectorHandle handle,
                                                                                SiToro_ListModeOutputTimeStamp* type) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setListModeOutputTimeStamp(SiToro_DetectorHandle handle,
                                                                                SiToro_ListModeOutputTimeStamp type) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getListModeOutputStats(SiToro_DetectorHandle handle,
                                                                            SiToro_ListModeOutputStats* type) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setListModeOutputStats(SiToro_DetectorHandle handle,
                                                                            SiToro_ListModeOutputStats type) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getListModeOutputSpatial(SiToro_DetectorHandle handle,
                                                                              SiToro_ListModeOutputSpatial* type) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setListModeOutputSpatial(SiToro_DetectorHandle handle,
                                                                              SiToro_ListModeOutputSpatial type) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getListModeOutputGate(SiToro_DetectorHandle handle,
                                                                           SiToro_ListModeOutputGate* type) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setListModeOutputGate(SiToro_DetectorHandle handle,
                                                                           SiToro_ListModeOutputGate type) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getListModeOutputPulses(SiToro_DetectorHandle handle,
                                                                             SiToro_ListModeOutputPulses* type) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setListModeOutputPulses(SiToro_DetectorHandle handle,
                                                                             SiToro_ListModeOutputPulses type) SITORO_EXPORT_FUNC_POST;

SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getListModeRunning(SiToro_DetectorHandle handle, siBool* isRunning) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_startListMode(SiToro_DetectorHandle handle,
                                                                   uint32_t startupBaselineMsec,
                                                                   uint32_t timeBetweenDataGetsMsec) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_stopListMode(SiToro_DetectorHandle handle) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getListModeData(SiToro_DetectorHandle handle,
                                                                     uint32_t timeout,
                                                                     uint32_t* buffer,
                                                                     uint32_t  maxBufferSize,
                                                                     uint32_t* numWritten,
                                                                     uint32_t* errorBits) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getListModeDataAvailable(SiToro_DetectorHandle handle, uint32_t* wordsAvailable) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getListModeStatus(SiToro_DetectorHandle handle, uint32_t* errorBits) SITORO_EXPORT_FUNC_POST;


/* Deprecated functions */
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_getDigitalGain(SiToro_DetectorHandle handle, double* gain) SITORO_EXPORT_FUNC_POST;
SiToro_Result SITORO_EXPORT_FUNC_PRE siToro_detector_setDigitalGain(SiToro_DetectorHandle handle, double gain) SITORO_EXPORT_FUNC_POST;


/* Decoding data streams (eg list mode) */
SITORO_INLINE uint32_t siToro_decode_getNativeEndian(uint32_t bigEndianWord)
{
    uint32_t output = 0;
    const int testEndian = 1;
    if( *(char *) &testEndian == 1)
    {
        // Machine is little-endian so need to do byte swap
        output = (bigEndianWord << 24) |
                ((bigEndianWord << 8) & 0x00FF0000) |
                ((bigEndianWord >> 8) & 0x0000FF00) |
                 (bigEndianWord >> 24);
    }
    else
    {
         // Machine is already big-endian
        output = bigEndianWord;
    }

    return output;
}

SITORO_INLINE uint8_t siToro_decode_getListModeDataType(uint32_t data)
{
    return (data & 0x7F);
}

SITORO_INLINE siBool siToro_decode_getListModeTimeStampShort(const uint32_t* data, uint32_t* timeStamp)
{
    // "data" is assumed to be in native format, not necessarily big endian
    if (siToro_decode_getListModeDataType(data[0]) != SiToro_ListModeEvent_TimeStampShortWrap)
        return 0;

    *timeStamp = (data[0] & 0xFFFFFF00) >> 8;
    return 1;
}

SITORO_INLINE siBool siToro_decode_getListModeTimeStampLong(const uint32_t* data, uint64_t* timeStamp)
{
    // "data" is assumed to be in native format, not necessarily big endian
    if (siToro_decode_getListModeDataType(data[0]) != SiToro_ListModeEvent_TimeStampLongWrap)
        return 0;

    *timeStamp = ((data[0] & 0xFFFFFF00) >> 8) | (data[1] << 24);
    return 1;
}

SITORO_INLINE siBool siToro_decode_getListModeStatisticsSmall(const uint32_t* data,
                                                              uint8_t*  statsType,
                                                              uint32_t* samplesDetected,
                                                              uint32_t* samplesErased,
                                                              uint32_t* pulsesDetected,
                                                              uint32_t* pulsesAccepted,
                                                              double*   inputCountRate,
                                                              double*   outputCountRate,
                                                              double*   deadTimePercent)
{
    // "data" is assumed to be in native format, not necessarily big endian
    if (siToro_decode_getListModeDataType(data[0]) != SiToro_ListModeEvent_StatsSmallCounters)
        return 0;

    *statsType       = (data[0] & 0x0F00) >> 8;
    *samplesDetected = data[1];
    *samplesErased   = data[2];
    *pulsesDetected  = data[3];
    *pulsesAccepted  = data[4];
    *inputCountRate  = data[5] * 1e-2;   /* Stored in 2-decimal place fixed format */
    *outputCountRate = data[6] * 1e-2;   /* Stored in 2-decimal place fixed format */
    *deadTimePercent = data[7] * 1e-2;   /* Stored in 2-decimal place fixed format */
    return 1;
}

SITORO_INLINE siBool siToro_decode_getListModeStatisticsLarge(const uint32_t* data,
                                                              uint8_t*  statsType,
                                                              uint64_t* samplesDetected,
                                                              uint64_t* samplesErased,
                                                              uint64_t* pulsesDetected,
                                                              uint64_t* pulsesAccepted,
                                                              double*   inputCountRate,
                                                              double*   outputCountRate,
                                                              double*   deadTimePercent)
{
    // "data" is assumed to be in native format, not necessarily big endian
    if (siToro_decode_getListModeDataType(data[0]) != SiToro_ListModeEvent_StatsLargeCounters)
        return 0;

    *statsType       = (data[0] & 0x0F00) >> 8;
    *samplesDetected = data[1] | ((uint64_t)(data[2]) << 32);
    *samplesErased   = data[3] | ((uint64_t)(data[4]) << 32);
    *pulsesDetected  = data[5] | ((uint64_t)(data[6]) << 32);
    *pulsesAccepted  = data[7] | ((uint64_t)(data[8]) << 32);
    *inputCountRate  = data[9]  * 1e-2;   /* Stored in 2-decimal place fixed format */
    *outputCountRate = data[10] * 1e-2;   /* Stored in 2-decimal place fixed format */
    *deadTimePercent = data[11] * 1e-2;   /* Stored in 2-decimal place fixed format */
    return 1;
}

SITORO_INLINE siBool siToro_decode_getListModeSpatialPositions(const uint32_t* data, int32_t* axes)
{
    // "data" is assumed to be in native format, not necessarily big endian
    int numAxes = 0;
    int i;
    switch (siToro_decode_getListModeDataType(data[0]))
    {
    case SiToro_ListModeEvent_SpatialOneAxis:
        numAxes = 1;
        break;
    case SiToro_ListModeEvent_SpatialTwoAxis:
        numAxes = 2;
        break;
    case SiToro_ListModeEvent_SpatialThreeAxis:
        numAxes = 3;
        break;
    case SiToro_ListModeEvent_SpatialFourAxis:
        numAxes = 4;
        break;
    default:
        return 0;
    }
    for (i = 0; i < numAxes; ++i)
    {
        axes[i] = (int32_t)(data[i+1]);
    }
    return 1;
}

SITORO_INLINE siBool siToro_decode_getListModeGateState(const uint32_t* data, uint8_t* state)
{
    // "data" is assumed to be in native format, not necessarily big endian
    switch (siToro_decode_getListModeDataType(data[0]))
    {
    case SiToro_ListModeEvent_GateState:
        *state = (data[0] & 0x0100) >> 8;
        return 1;
    default:
        return 0;
    }
}

SITORO_INLINE siBool siToro_decode_getListModePulseEventWithTimeOfArrival(const uint32_t* data,
                                                                          uint8_t*  rejected,
                                                                          int16_t*  energy,
                                                                          uint32_t* timeStamp,
                                                                          uint8_t*  subSample)
{
    // "data" is assumed to be in native format, not necessarily big endian
    if (siToro_decode_getListModeDataType(data[0]) != SiToro_ListModeEvent_PulseWithTimeOfArrival)
        return 0;

    *rejected  = (data[0] & 0x0100) >> 8;
    *energy    = (int16_t)((data[0] & 0x07FFF800) >> 11);
    *timeStamp = (data[1] & 0x3FFFFFC0) >> 6;
    *subSample = (data[1] & 0x3F);
    return 1;
}

SITORO_INLINE siBool siToro_decode_getListModePulseEventNoTimeOfArrival(const uint32_t* data,
                                                                        uint8_t* rejected,
                                                                        int16_t* energy)
{
    // "data" is assumed to be in native format, not necessarily big endian
    if (siToro_decode_getListModeDataType(data[0]) != SiToro_ListModeEvent_PulseNoTimeOfArrival)
        return 0;

    *rejected  = (data[0] & 0x0100) >> 8;
    *energy    = (int16_t)((data[0] & 0x07FFF800) >> 11);
    return 1;
}

SITORO_INLINE siBool siToro_decode_getListModeError(const uint32_t* data, SiToro_ListModeErrorCode* errorCode)
{
    // "data" is assumed to be in native format, not necessarily big endian
    if (siToro_decode_getListModeDataType(data[0]) != SiToro_ListModeEvent_Error)
        return 0;

    switch (data[0] >> 8)
    {
    case SiToro_ListModeErrorCode_Healthy:
        *errorCode = SiToro_ListModeErrorCode_Healthy;
        return 1;
    case SiToro_ListModeErrorCode_ApiInternalError:
        *errorCode = SiToro_ListModeErrorCode_ApiInternalError;
        return 1;
    case SiToro_ListModeErrorCode_ApiBufferWriteFail:
        *errorCode = SiToro_ListModeErrorCode_ApiBufferWriteFail;
        return 1;
    case SiToro_ListModeErrorCode_DspInternalError:
        *errorCode = SiToro_ListModeErrorCode_DspInternalError;
        return 1;
    case SiToro_ListModeErrorCode_DspBadParseState:
        *errorCode = SiToro_ListModeErrorCode_DspBadParseState;
        return 1;
    case SiToro_ListModeErrorCode_DspBadStatsSubtype:
        *errorCode = SiToro_ListModeErrorCode_DspBadStatsSubtype;
        return 1;
    case SiToro_ListModeErrorCode_DspBadSpatialSubtype:
        *errorCode = SiToro_ListModeErrorCode_DspBadSpatialSubtype;
        return 1;
    case SiToro_ListModeErrorCode_DspBadPacketType:
        *errorCode = SiToro_ListModeErrorCode_DspBadPacketType;
        return 1;
#if BETA_FEATURES
    case SiToro_ListModeErrorCode_AdcPositiveRailHit:
        *errorCode = SiToro_ListModeErrorCode_AdcPositiveRailHit;
        return 1;
    case SiToro_ListModeErrorCode_AdcNegativeRailHit:
        *errorCode = SiToro_ListModeErrorCode_AdcNegativeRailHit;
        return 1;
#endif
    default:
        return 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif  // SITORO_H
