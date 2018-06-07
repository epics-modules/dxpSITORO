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

#ifndef XIA_SYSTEM_H
#define XIA_SYSTEM_H

#include <stdio.h>

#include "handeldef.h"
#include "handel_generic.h"
#include "xia_handel_structures.h"
#include "xia_common.h"


/*
 * Function pointers used for interaction with the PSL layer
 *
 * The following shows how to obtain values that use to be passed to the PSL
 * routines. The updated Handel implementation has changed the arguments to the
 * detector channel (detChan), the Detector structure pointer and the Module
 * pointer. With the routines listed here you can access the needed data.
 *
 * # Module Alias:
 *
 *  The module alias is the name of the module.
 *
 *    char* modAlias;
 *    modAlias = xiaGetAliasFromDetChan(detChan);
 *    if (madAlias == NULL)
 *       error(..)
 *
 * # Module Channel:
 *
 *  The module channel is the locgical detector value in the module
 *  strustture. It is also called the absolute detector number.
 *
 *    int status;
 *    int modChan;
 *    modChan = xiaGetModChan(detChan);
 *    if (modChan == 999)
 *       error(...)
 *    status = xiaGetAbsoluteChannel(detChan, module, &modChan);
 *    if (status != XIA_SUCCESS)
 *       error(...)
 *
 * # Detector Channel
 *
 *  The physical detector channel that is a mapping of the logical module
 *  channel.
 *
 *    int detector_chan;
 *    detectorChan = xiaGetModDetectorChan(detChan);
 *    if (detectorChan == 999)
 *       error(...)
 *
 * # Firmware Sets
 *
 *  The FirmwareSet are sets peaking time definitions that can be referenced
 *  within PSL functions to allow arbitrary firmware definitions for arbitrary
 *  boards. The CurrentFirmware is the valid set up for a detector (I think).
 *
 *    int status;
 *    FirmwareSet *firmwareSet = NULL;
 *    CurrentFirmware *currentFirmware = NULL;
 *    status = xiaGetFirmwareSet(detChan, module,
 *                               &firmwareSet, &currentFirmware);
 *    if (status != XIA_SUCCESS)
 *       error(...)
 *
 * # Defaults
 *
 *  The defaults are default DAQ settings.
 *
 *    int status;
 *    XiaDefaults *defaults = NULL;
 *    defaults = xiaGetDefaultFromDetChan(detChan);
 *    if (defaults == NULL)
 *       error(..)
 *
 * # Detector Type
 *
 *  To get the detector type as a string.
 *
 *    char detectorType[MAXITEM_LEN];
 *    status = xiaSetDetectorType(detector, detectorType);
 *    if (status != XIA_SUCCESS)
 *       error(...)
 */

/*
 * Note, all "int detChan, Detector* detector, Module* module" call
 * signatures will be changed to a single structure that will be passed.
 */

/*
 * Read INI file data. The detector will have been created by the
 * xiaNewDetector call and the 'number_of_channels' and the 'type' items added.
 */
typedef int (*iniRead_FP)(FILE* fp, fpos_t *start, fpos_t *end,
                          const int detchan, Detector *detector, Module *module);

/*
 * Write INI file data.
 */
typedef int (*iniWrite_FP)(FILE* fp, const char* section, const char* path,
                           void* value, const int index, Module *module);

typedef int (*loadChanData_FP)(const byte_t *data, const size_t len, const int modChan, Module *module);
typedef int (*saveChanData_FP)(const int modChan, Module *module, byte_t **data, size_t *len);

/*
 * Set up the module. A module contains detectors. Allocate any resources
 * specific to the module and reference it by the pslData field in the Module
 * structure.
 */
typedef int (*setupModule_FP)(Module *module);

/*
 * End the module. Clean up any allocated resources.
 */
typedef int (*endModule_FP)(Module *module);

/*
 * Set up a detector that is part of a module. The detector is the global
 * detChan and module channel is returned by xiaGetModChan. Allocate any
 * resources specific to the detector and reference it by the pslData field in
 * the Detector structure.
 */
typedef int (*setupDetChan_FP)(int detChan, Detector *detector, Module *module);

/*
 * End the detector. Clean up any allocated resources.
 */
typedef int (*endDetChan_FP)(int detChan, Detector *detector, Module *module);

/*
 * Perform any user set up on a detector. Typically paint the default settings.
 */
typedef int (*userSetup_FP)(int detChan, Detector *detector, Module *module);

/*
 * Perform a board operation. Currently the detector and module are
 * passed.
 */
typedef int (*boardOperation_FP)(int detChan, Detector* detector, Module* module,
                                 const char *name, void *value);

/*
 * Set the detector type.
 */
typedef int (*setDetectorTypeValue_FP)(int detChan, Detector *detector);

/*
 * Set the acquisition value. To obtain the defaults
 */
typedef int (*setAcquisitionValues_FP)(int detChan, Detector *detector, Module *module,
                                       const char *name, void *value);

/*
 * Get the acquisition value.
 */
typedef int (*getAcquisitionValues_FP)(int detChan, Detector *detector, Module *module,
                                       const char *name, void *value);


typedef int (*getDefaultAlias_FP)(char *, char **, double *);
typedef int (*freeSCAs_FP)(Module *m, int modChan);
typedef unsigned int (*getNumDefaults_FP)(void);

typedef int (*gainCalibrate_FP)(int detChan, Detector *det, int modChan,
                                Module *m, XiaDefaults *defs, double delta);
typedef int (*gainOperation_FP)(int detChan, const char *name, void *value,
                                Detector *det, int modChan,
                                Module *m, XiaDefaults *defs);
typedef int (*startRun_FP)(int detChan, unsigned short resume, XiaDefaults *defs,
                           Detector *detector, Module *m);
typedef int (*stopRun_FP)(int detChan, Detector *detector, Module *m);
typedef int (*getRunData_FP)(int detChan, const char *name, void *value,
                             XiaDefaults *defs, Detector *detector, Module *m);
typedef int (*doSpecialRun_FP)(int detChan, const char *name, void *info,
                               XiaDefaults *defaults,
                               Detector *detector, Module *module);
typedef int (*getSpecialRunData_FP)(int detChan, const char *name,
                                    void *value, XiaDefaults *defaults,
                                    Detector *detector, Module *module);
typedef boolean_t (*canRemoveName_FP)(const char *);

/* To be sorted out. */
typedef int (*set_FP)(int, const char *, void *, XiaDefaults *);



/* Structs
 *
 * Note: Changed PSLFuncs to PSLHandler to break existing code. This is an
 *       internal Handel interface that is not compatible to previous version
 *       and this change forces old code moving to this version to have
 *       the function table reviewed.
 */
struct PSLHandlers
{
    iniRead_FP              iniRead;
    loadChanData_FP         loadChanData;
    saveChanData_FP         saveChanData;
    iniWrite_FP             iniWrite;
    setupModule_FP          setupModule;
    endModule_FP            endModule;
    setupDetChan_FP         setupDetChan;
    endDetChan_FP           endDetChan;
    userSetup_FP            userSetup;
    boardOperation_FP       boardOperation;
    getDefaultAlias_FP      getDefaultAlias;
    getNumDefaults_FP       getNumDefaults;
    setDetectorTypeValue_FP setDetectorTypeValue;
    setAcquisitionValues_FP setAcquisitionValues;
    getAcquisitionValues_FP getAcquisitionValues;
    gainCalibrate_FP        gainCalibrate;
    gainOperation_FP        gainOperation;
    startRun_FP             startRun;
    stopRun_FP              stopRun;
    getRunData_FP           getRunData;
    doSpecialRun_FP         doSpecialRun;
    getSpecialRunData_FP    getSpecialRunData;
    canRemoveName_FP        canRemoveName;
    freeSCAs_FP             freeSCAs;
};

/**
 * Check the arguments to a PSL function. If your PSL layer leaves the PSL data
 * as NULL you will need your own check function.
 */
#define xiaPSLBadArgs(_m, _d, _f) \
    do { \
        if (!(_m) || !(_d) || \
            ((_m) && !(_m)->pslData) || ((_d) && !(_d)->pslData)) { \
            xiaLog(XIA_LOG_ERROR, XIA_BAD_PSL_ARGS, _f, "Module, detector or PSL data is NULL"); \
            return XIA_BAD_PSL_ARGS; \
        } \
    } ONCE

#endif /* XIA_SYSTEM_H */
