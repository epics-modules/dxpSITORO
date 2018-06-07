/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
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

#ifndef HANDEL_H
#define HANDEL_H

#include "xia_common.h"

#include "handeldef.h"


#define XIA_RUN_HARDWARE  0x01
#define XIA_RUN_HANDEL    0x02
#define XIA_RUN_CT        0x04

#define XIA_BEFORE        0
#define XIA_AFTER         1

/* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
extern "C" {
#endif

    HANDEL_IMPORT int HANDEL_API xiaInit(const char *iniFile);
    HANDEL_IMPORT int HANDEL_API xiaInitHandel(void);
    HANDEL_IMPORT int HANDEL_API xiaNewDetector(const char *alias);
    HANDEL_IMPORT int HANDEL_API xiaAddDetectorItem(const char *alias, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaModifyDetectorItem(const char *alias, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaGetDetectorItem(const char *alias, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaGetNumDetectors(unsigned int *numDet);
    HANDEL_IMPORT int HANDEL_API xiaGetDetectors(char *detectors[]);
    HANDEL_IMPORT int HANDEL_API xiaGetDetectors_VB(unsigned int index, char *alias);
    HANDEL_IMPORT int HANDEL_API xiaRemoveDetector(char *alias);
    HANDEL_IMPORT int HANDEL_API xiaDetectorFromDetChan(int detChan, char *alias);
    HANDEL_IMPORT int HANDEL_API xiaNewFirmware(const char *alias);
    HANDEL_IMPORT int HANDEL_API xiaAddFirmwareItem(const char *alias, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaModifyFirmwareItem(const char *alias, unsigned short decimation,
                                                       const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaGetFirmwareItem(const char *alias, unsigned short decimation,
                                                    const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaGetNumFirmwareSets(unsigned int *numFirmware);
    HANDEL_IMPORT int HANDEL_API xiaGetFirmwareSets(char *firmware[]);
    HANDEL_IMPORT int HANDEL_API xiaGetFirmwareSets_VB(unsigned int index, char *alias);
    HANDEL_IMPORT int HANDEL_API xiaGetNumPTRRs(const char *alias, unsigned int *numPTRR);
    HANDEL_IMPORT int HANDEL_API xiaRemoveFirmware(const char *alias);
    HANDEL_IMPORT int HANDEL_API xiaNewModule(const char *alias);
    HANDEL_IMPORT int HANDEL_API xiaAddModuleItem(const char *alias, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaModifyModuleItem(const char *alias, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaGetModuleItem(const char *alias, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaGetNumModules(unsigned int *numModules);
    HANDEL_IMPORT int HANDEL_API xiaGetModules(char *modules[]);
    HANDEL_IMPORT int HANDEL_API xiaGetModules_VB(unsigned int index, char *alias);
    HANDEL_IMPORT int HANDEL_API xiaRemoveModule(const char *alias);
    HANDEL_IMPORT int HANDEL_API xiaModuleFromDetChan(int detChan, char *alias);
    HANDEL_IMPORT int HANDEL_API xiaAddChannelSetElem(int detChanSet, int newChan);
    HANDEL_IMPORT int HANDEL_API xiaRemoveChannelSetElem(int detChan, int chan);
    HANDEL_IMPORT int HANDEL_API xiaRemoveChannelSet(int detChan);
    HANDEL_IMPORT int HANDEL_API xiaStartSystem(void);
    HANDEL_IMPORT int HANDEL_API xiaEndSystem(void);
    HANDEL_IMPORT int HANDEL_API xiaDownloadFirmware(int detChan, const char *type);
    HANDEL_IMPORT int HANDEL_API xiaSetAcquisitionValues(int detChan, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaGetAcquisitionValues(int detChan, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaRemoveAcquisitionValues(int detChan, const char *name);
    HANDEL_IMPORT int HANDEL_API xiaGainCalibrate(int detChan, double deltaGain);
    HANDEL_IMPORT int HANDEL_API xiaGainOperation(int detChan, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaStartRun(int detChan, unsigned short resume);
    HANDEL_IMPORT int HANDEL_API xiaStopRun(int detChan);
    HANDEL_IMPORT int HANDEL_API xiaGetRunData(int detChan, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaDoSpecialRun(int detChan, const char *name, void *info);
    HANDEL_IMPORT int HANDEL_API xiaGetSpecialRunData(int detChan, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaLoadSystem(const char *type, const char *filename);
    HANDEL_IMPORT int HANDEL_API xiaSaveSystem(const char *type, const char *filename);
    HANDEL_IMPORT int HANDEL_API xiaBoardOperation(int detChan, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaMemoryOperation(int detChan, const char *name, void *value);
    HANDEL_IMPORT int HANDEL_API xiaCommandOperation(int detChan, byte_t cmd,
                                                     unsigned int lenS, byte_t *send,
                                                     unsigned int lenR, byte_t *recv);

    HANDEL_IMPORT int HANDEL_API xiaFitGauss(long data[], int lower, int upper, float *pos,
                                             float *fwhm);
    HANDEL_IMPORT int HANDEL_API xiaFindPeak(long *data, int numBins, float thresh, int *lower,
                                             int *upper);
    HANDEL_IMPORT int HANDEL_API xiaExit(void);

    HANDEL_IMPORT int HANDEL_API xiaEnableLogOutput(void);
    HANDEL_IMPORT int HANDEL_API xiaSuppressLogOutput(void);
    HANDEL_IMPORT int HANDEL_API xiaSetLogLevel(int level);
    HANDEL_IMPORT int HANDEL_API xiaSetLogOutput(const char *fileName);

    HANDEL_IMPORT int HANDEL_API xiaSetIOPriority(int pri);

    HANDEL_IMPORT void HANDEL_API xiaGetVersionInfo(int *rel, int *min, int *maj,
                                                    char *pretty);

    HANDEL_IMPORT int HANDEL_API xiaMemStatistics(unsigned long *total,
                                                  unsigned long *current,
                                                  unsigned long *peak);
    HANDEL_EXPORT void HANDEL_API xiaMemSetCheckpoint(void);
    HANDEL_EXPORT void HANDEL_API xiaMemLeaks(char *);

#ifdef __MEM_DBG__

#include <crtdbg.h>

    HANDEL_IMPORT void xiaSetReportMode(void);
    HANDEL_IMPORT void xiaMemCheckpoint(int pass);
    HANDEL_IMPORT void xiaReport(char *message);
    HANDEL_IMPORT void xiaMemDumpAllObjectsSince(void);
    HANDEL_IMPORT void xiaDumpMemoryLeaks(void);
    HANDEL_IMPORT void xiaEndMemDbg(void);

#endif /* __MEM_DBG__ */

#ifdef _DEBUG
    HANDEL_IMPORT void HANDEL_API xiaUnitTests(unsigned short tests);
#endif

    /* If this is compiled by a C++ compiler, make it clear that these are C routines */
#ifdef __cplusplus
}
#endif

#endif                        /* Endif for HANDEL_H */
