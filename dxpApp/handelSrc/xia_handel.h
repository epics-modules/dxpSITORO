/*
 * Copyright (c) 2004 X-ray Instrumentation Associates
 *               2005-2011 XIA LLC
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

#ifndef XIA_HANDEL_H
#define XIA_HANDEL_H

#include <stdlib.h>
#include <stdio.h>

#include "xia_handel_structures.h"
#include "handel_generic.h"
#include "xia_module.h"
#include "xia_system.h"
#include "xia_common.h"

#include "md_generic.h"

#include "handeldef.h"


/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif


HANDEL_EXPORT int HANDEL_API xiaInit(const char *inifile);
HANDEL_EXPORT int HANDEL_API xiaInitHandel(void);
HANDEL_EXPORT int HANDEL_API xiaEnableLogOutput(void);
HANDEL_EXPORT int HANDEL_API xiaSuppressLogOutput(void);
HANDEL_EXPORT int HANDEL_API xiaSetLogLevel(int level);
HANDEL_EXPORT int HANDEL_API xiaSetLogOutput(const char *filename);

HANDEL_EXPORT int HANDEL_API xiaNewDetector(const char *alias);
HANDEL_EXPORT int HANDEL_API xiaAddDetectorItem(const char *alias,
                                                const char *name, void *value);
HANDEL_EXPORT int HANDEL_API xiaModifyDetectorItem(const char *alias,
                                                   const char *name, void *value);
HANDEL_EXPORT int HANDEL_API xiaGetDetectorItem(const char *alias,
                                                const char *name, void *value);
HANDEL_EXPORT int HANDEL_API xiaGetNumDetectors(unsigned int *numDetectors);
HANDEL_EXPORT int HANDEL_API xiaGetDetectors(char *detectors[]);
HANDEL_EXPORT int HANDEL_API xiaGetDetectors_VB(unsigned int index,
                                                char *alias);
HANDEL_EXPORT int HANDEL_API xiaDetectorFromDetChan(int detChan, char *alias);
HANDEL_EXPORT int HANDEL_API xiaRemoveDetector(const char *alias);
HANDEL_EXPORT int HANDEL_API xiaRemoveAllDetectors(void);

HANDEL_EXPORT int HANDEL_API xiaNewFirmware(const char *alias);
HANDEL_EXPORT int HANDEL_API xiaAddFirmwareItem(const char *alias,
                                                const char *name,
                                                void *value);
HANDEL_EXPORT int HANDEL_API xiaModifyFirmwareItem(const char *alias,
                                                   unsigned short ptrr,
                                                   const char *name,
                                                   void *value);
HANDEL_EXPORT int HANDEL_API xiaGetFirmwareItem(const char *alias,
                                                unsigned short ptrr,
                                                const char *name, void *value);
HANDEL_EXPORT int HANDEL_API xiaGetNumFirmwareSets(unsigned int *numFirmware);
HANDEL_EXPORT int HANDEL_API xiaGetFirmwareSets(char *firmwares[]);
HANDEL_EXPORT int HANDEL_API xiaGetFirmwareSets_VB(unsigned int index, char *alias);
HANDEL_EXPORT int HANDEL_API xiaGetNumPTRRs(const char *alias, unsigned int *numPTRR);
HANDEL_EXPORT int HANDEL_API xiaRemoveFirmware(const char *alias);
HANDEL_EXPORT int HANDEL_API xiaRemoveAllFirmware(void);

HANDEL_EXPORT int HANDEL_API xiaNewModule(const char *alias);
HANDEL_EXPORT int HANDEL_API xiaAddModuleItem(const char *alias,
                                              const char *name,
                                              void *value);
HANDEL_EXPORT int HANDEL_API xiaModifyModuleItem(const char *alias,
                                                 const char *name,
                                                 void *value);
HANDEL_EXPORT int HANDEL_API xiaGetModuleItem(const char *alias,
                                              const char *name,
                                              void *value);
HANDEL_EXPORT int HANDEL_API xiaGetNumModules(unsigned int *numModules);
HANDEL_EXPORT int HANDEL_API xiaGetModules(char *modules[]);
HANDEL_EXPORT int HANDEL_API xiaGetModules_VB(unsigned int index, char *alias);
HANDEL_EXPORT int HANDEL_API xiaModuleFromDetChan(int detChan, char *alias);
HANDEL_EXPORT int HANDEL_API xiaRemoveModule(const char *alias);
HANDEL_EXPORT int HANDEL_API xiaRemoveAllModules(void);

HANDEL_EXPORT int HANDEL_API xiaAddChannelSetElem(int detChanSet, int newChan);
HANDEL_EXPORT int HANDEL_API xiaRemoveChannelSetElem(int detChan, int chan);
HANDEL_EXPORT int HANDEL_API xiaRemoveChannelSet(int detChan);

HANDEL_EXPORT int HANDEL_API xiaStartSystem(void);
HANDEL_EXPORT int HANDEL_API xiaEndSystem(void);

HANDEL_EXPORT int HANDEL_API xiaDownloadFirmware(int detChan, const char *type);
HANDEL_EXPORT int HANDEL_API xiaSetAcquisitionValues(int detChan,
                                                     const char *name,
                                                     void *value);
HANDEL_EXPORT int HANDEL_API xiaGetAcquisitionValues(int detChan,
                                                     const char *name,
                                                     void *value);
HANDEL_EXPORT int HANDEL_API xiaRemoveAcquisitionValues(int detChan,
                                                        const char *name);
HANDEL_EXPORT int HANDEL_API xiaUpdateUserParams(int detChan);
HANDEL_EXPORT int HANDEL_API xiaGainOperation(int detChan,
                                              const char *name,
                                              void *value);
HANDEL_EXPORT int HANDEL_API xiaGainCalibrate(int detChan, double deltaGain);
HANDEL_EXPORT int HANDEL_API xiaStartRun(int detChan, unsigned short resume);
HANDEL_EXPORT int HANDEL_API xiaStopRun(int detChan);
HANDEL_EXPORT int HANDEL_API xiaGetRunData(int detChan,
                                           const char *name,
                                           void *value);
HANDEL_EXPORT int HANDEL_API xiaDoSpecialRun(int detChan,
                                             const char *name,
                                             void *info);
HANDEL_EXPORT int HANDEL_API xiaGetSpecialRunData(int detChan,
                                                  const char *name,
                                                  void *value);
HANDEL_EXPORT int HANDEL_API xiaLoadSystem(const char *type, const char *filename);
HANDEL_EXPORT int HANDEL_API xiaSaveSystem(const char *type, const char *filename);
HANDEL_EXPORT int HANDEL_API xiaBoardOperation(int detChan,
                                               const char *name,
                                               void *value);

HANDEL_EXPORT int HANDEL_API xiaExit(void);

HANDEL_EXPORT int HANDEL_API xiaBuildErrorTable(void);

HANDEL_EXPORT int HANDEL_API xiaSetIOPriority(int pri);

HANDEL_EXPORT void HANDEL_API xiaGetVersionInfo(int *rel, int *min, int *maj,
                                                char *pretty);

#ifdef _DEBUG

HANDEL_EXPORT void HANDEL_API xiaDumpDetChanStruct(const char *fileName);
HANDEL_EXPORT void HANDEL_API xiaDumpDSPParameters(int detChan, const char *fileName);
HANDEL_EXPORT void HANDEL_API xiaDumpFirmwareStruct(const char *fileName);
HANDEL_EXPORT void HANDEL_API xiaDumpModuleStruct(const char *fileName);
HANDEL_EXPORT void HANDEL_API xiaDumpDefaultsStruct(const char *fileName);

  HANDEL_EXPORT void HANDEL_API xiaUnitTests(unsigned short tests);

#endif /* _DEBUG */

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
}
#endif

HANDEL_SHARED boolean_t HANDEL_API xiaHandelSystemStarting(void);
HANDEL_SHARED boolean_t HANDEL_API xiaHandelSystemRunning(void);
HANDEL_SHARED boolean_t HANDEL_API xiaHandelSystemEnding(void);

HANDEL_SHARED int HANDEL_API xiaInitModuleDS(void);
HANDEL_SHARED int HANDEL_API xiaInitDetChanDS(void);
HANDEL_SHARED int HANDEL_API xiaInitXiaDefaultsDS(void);
HANDEL_SHARED int HANDEL_API xiaInitDetectorDS(void);
HANDEL_SHARED int HANDEL_API xiaInitFirmwareSetDS(void);

HANDEL_SHARED int HANDEL_API xiaValidateFirmwareSets(void);
HANDEL_SHARED int HANDEL_API xiaValidateDetector(void);
HANDEL_SHARED int HANDEL_API xiaValidateDetSets(void);

HANDEL_SHARED int HANDEL_API xiaSetupModules(void);
HANDEL_SHARED int HANDEL_API xiaEndModules(void);
HANDEL_SHARED int HANDEL_API xiaSetupDetectors(void);
HANDEL_SHARED int HANDEL_API xiaEndDetectors(void);

HANDEL_SHARED Module * HANDEL_API xiaGetModuleHead(void);
HANDEL_SHARED Detector * HANDEL_API xiaGetDetectorHead(void);
HANDEL_SHARED DetChanElement * HANDEL_API xiaGetDetChanHead(void);
HANDEL_SHARED FirmwareSet * HANDEL_API xiaGetFirmwareSetHead(void);
HANDEL_SHARED XiaDefaults * HANDEL_API xiaGetDefaultsHead(void);

HANDEL_SHARED int HANDEL_API xiaNewDefault(const char *alias);
HANDEL_SHARED int HANDEL_API xiaAddDefaultItem(const char *alias,
                                               const char *name, void *value);
HANDEL_SHARED int HANDEL_API xiaModifyDefaultItem(const char *alias,
                                                  const char *name, void *value);
HANDEL_SHARED int HANDEL_API xiaGetDefaultItem(const char *alias,
                                               const char *name, void *value);
HANDEL_SHARED int HANDEL_API xiaRemoveDefault(const char *alias);
HANDEL_SHARED int HANDEL_API xiaRemoveAllDefaults(void);

HANDEL_SHARED int HANDEL_API xiaReadIniFile(const char *inifile);

HANDEL_SHARED void HANDEL_API xiaLog(int level, const char* file, int line,
                                     int status, const char* func,
                                     const char* fmt, ...) HANDEL_PRINTF(6, 7);

HANDEL_SHARED int HANDEL_API xiaSetupModule(const char* alias);
HANDEL_SHARED int HANDEL_API xiaEndModule(const char* alias);

HANDEL_SHARED int HANDEL_API xiaSetupDetector(const char* alias);
HANDEL_SHARED int HANDEL_API xiaEndDetector(const char* alias);
HANDEL_SHARED int HANDEL_API xiaSetupDetectorChannel(int detChan);
HANDEL_SHARED int HANDEL_API xiaEndDetectorChannel(int detChan);

HANDEL_SHARED Module* HANDEL_API xiaFindModule(const char *alias);
HANDEL_SHARED Detector* HANDEL_API xiaFindDetector(const char *alias);
HANDEL_SHARED FirmwareSet* HANDEL_API xiaFindFirmware(const char *alias);
HANDEL_SHARED XiaDefaults* HANDEL_API xiaFindDefault(const char *alias);
HANDEL_SHARED int HANDEL_API xiaGetDetectorType(Detector* detector, char* type);

HANDEL_SHARED Module* HANDEL_API xiaFindModuleFromDetAlias(const char *alias);
HANDEL_SHARED int HANDEL_API xiaFindDetChanFromDetAlias(const char *alias);
HANDEL_SHARED int HANDEL_API xiaFindDetIndexFromDetAlias(const char *alias);
HANDEL_SHARED int HANDEL_API xiaFindModuleAndDetector(int detChan,
                                                      Module** module,
                                                      Detector** detector);

HANDEL_SHARED boolean_t HANDEL_API xiaIsDetChanFree(int detChan);
HANDEL_SHARED int HANDEL_API xiaCleanDetChanList(void);
HANDEL_SHARED int HANDEL_API xiaAddDetChan(int type, int detChan, void *data);
HANDEL_SHARED int HANDEL_API xiaRemoveDetChan(int detChan);
HANDEL_SHARED int HANDEL_API xiaRemoveAllDetChans(void);
HANDEL_SHARED void HANDEL_API xiaFreeDetSet(DetChanSetElem *head);

HANDEL_SHARED int HANDEL_API xiaGetBoardType(int detChan, char *boardType);

HANDEL_SHARED int HANDEL_API xiaGetNumFirmware(Firmware *firmware);
HANDEL_SHARED int xiaFirmComp(const void *key1, const void *key2);
HANDEL_SHARED int HANDEL_API xiaInsertSort(Firmware **head,
                                           int (*compare)(const void *key1,
                                                          const void *key2));

HANDEL_SHARED int HANDEL_API xiaMergeSort(void *data, int size,
                                          int esize, int i, int k,
                                          int (*compare)(const void *key1,
                                                         const void *key2));

HANDEL_SHARED int HANDEL_API xiaGetElemType(int detChan);
HANDEL_SHARED void HANDEL_API xiaClearTags(void);
HANDEL_SHARED DetChanElement* HANDEL_API xiaGetDetChanPtr(int detChan);

HANDEL_SHARED char* HANDEL_API xiaGetAliasFromDetChan(int detChan);
HANDEL_SHARED int HANDEL_API xiaGetModChan(int detChan);
HANDEL_SHARED int HANDEL_API xiaGetAbsoluteChannel(int detChan,
                                                   Module *module, int *chan);
HANDEL_SHARED int HANDEL_API xiaGetModDetectorChan(int detChan);
HANDEL_SHARED int HANDEL_API xiaTagAllRunActive(Module *module, boolean_t state);
HANDEL_SHARED int HANDEL_API xiaGetDefaultStrFromDetChan(int detChan,
                                                         char* defaultStr);
HANDEL_SHARED XiaDefaults* HANDEL_API xiaGetDefaultFromDetChan(int detChan);
HANDEL_SHARED int HANDEL_API xiaSetDetectorType(Detector* detector, const char* type);

HANDEL_SHARED int HANDEL_API xiaBuildXerxesConfig(void);
HANDEL_SHARED double HANDEL_API xiaGetValueFromDefaults(const char *name,
                                                        const char *alias);
HANDEL_SHARED int HANDEL_API xiaGetDSPNameFromFirmware(const char *alias,
                                                       double peakingTime, char *dspName);
HANDEL_SHARED int HANDEL_API xiaGetFippiNameFromFirmware(const char *alias,
                                                         double peakingTime,
                                                         char *fippiName);
HANDEL_SHARED int HANDEL_API xiaGetValueFromFirmware(const char *alias,
                                                     double peakingTime,
                                                     const char *name,
                                                     char *value);
HANDEL_SHARED int HANDEL_API xiaGetPSLHandlers(const char *boardType,
                                               const PSLHandlers **handlers);

HANDEL_SHARED int HANDEL_API xiaGetFirmwareSet(int detChan, Module* module,
                                               FirmwareSet** firmwareSet,
                                               CurrentFirmware** currentFirmware);


/* FDD Shims */
HANDEL_SHARED int xiaFddInitialize(void);
HANDEL_SHARED int xiaFddGetFirmware(const char *filename, char *path,
                                    const char *ftype, double pt,
                                    unsigned int nother, char **others,
                                    const char *detectoryType,
                                    char newFilename[MAXFILENAME_LEN],
                                    char rawFilename[MAXFILENAME_LEN]);

extern void (*handel_md_output)(const char *stream);
extern void * (*handel_md_alloc)(size_t bytes);
extern void (*handel_md_free)(void *ptr);
extern int (*handel_md_puts)(const char *s);
extern int (*handel_md_wait)(float *secs);
extern char * (*handel_md_fgets)(char *s, int size, FILE *stream);
extern int (*handel_md_enable_log)(void);
extern int (*handel_md_suppress_log)(void);
extern int (*handel_md_set_log_level)(int level);


#define XIA_BEFORE 0
#define XIA_AFTER  1

/* Detector-type constants */
#define XIA_DET_UNKNOWN  0x0000
#define XIA_DET_RESET    0x0001
#define XIA_DET_RCFEED   0x0002

/* Statics used in multiple source files */
extern boolean_t isHandelInit;

/* MACROS for the Linked-Lists */
#define isListEmpty(x)        (((x) == NULL) ? TRUE_ : FALSE_)
#define getListNext(x)        ((x)->next)
#define getListPrev(x)        ((x)->prev)

#endif                        /* XIA_HANDEL_H */
