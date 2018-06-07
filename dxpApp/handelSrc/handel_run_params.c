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


#include <ctype.h>

#include "xia_common.h"
#include "xia_assert.h"
#include "xia_handel_structures.h"
#include "xia_handel.h"
#include "xia_system.h"

#include "handeldef.h"
#include "handel_errors.h"
#include "handel_log.h"


/*****************************************************************************
 *
 * This routine is responsible for calculating key DSP parameters from user
 * supplied values. Think of this routine as translating real worl parameters,
 * like threshold energy (in eV), into the DSP equivalant of that parameter,
 * like THRESHOLD.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaSetAcquisitionValues(int detChan,
                                                     const char *name, void *value)
{
    int status;
    int elemType;

    double savedValue;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;

    Module *module = NULL;

    Detector *detector = NULL;

    /* See Bug ID #66. Protect
     * against malformed name
     * strings.
     */
    if (name == NULL) {
        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaSetAcquisitionValues",
               "Name may not be NULL");
        return status;
    }

    elemType = xiaGetElemType(detChan);

    switch(elemType)
    {
        case SINGLE:

            status = xiaFindModuleAndDetector(detChan, &module, &detector);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaUserSetup",
                       "Unable to complete user setup for detChan %d.", detChan);
                return status;
            }

            status = module->psl->setAcquisitionValues(detChan, detector, module, name, value);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaSetAcquisitionValues",
                       "Unable to set '%s' to %0.3f for detChan %d.",
                       name, *((double *)value), detChan);
                return status;
            }

            break;

        case SET:
            detChanElem = xiaGetDetChanPtr(detChan);

            detChanSetElem = detChanElem->data.detChanSet;

            /* Save the user value, else it will be changed by the return value
             * of the setAcquisitionValues call.  Use the last return value as
             * the actual return value */
            savedValue = *((double *) value);

            while (detChanSetElem != NULL)
            {
                *((double *) value) = savedValue;
                status =
                    xiaSetAcquisitionValues((int)detChanSetElem->channel, name, value);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaSetAcquisitionValues",
                           "Error setting acquisition values for detChan %d",
                           detChan);
                    return status;
                }

                detChanSetElem = getListNext(detChanSetElem);
            }

            break;

        case 999:
            status = XIA_INVALID_DETCHAN;
            xiaLog(XIA_LOG_ERROR, status, "xiaSetAcquisitionValues",
                   "detChan number is not in the list of valid values");
            return status;
        default:
            status = XIA_UNKNOWN;
            xiaLog(XIA_LOG_ERROR, status, "xiaSetAcquisitionValues",
                   "Should not be seeing this message");
            return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine retrieves the current setting of an acquisition value by
 * either calculating it or pulling it from the defaults for some of them.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaGetAcquisitionValues(int detChan,
                                                     const char *name, void *value)
{
    int status;
    int elemType;

    Module *module = NULL;

    Detector *detector = NULL;

    elemType = xiaGetElemType(detChan);

    switch(elemType)
    {
        case SET:
            status = XIA_BAD_TYPE;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetAcquisitionValues",
                   "Unable to retrieve values for a detChan SET");
            return status;

        case SINGLE:
            status = xiaFindModuleAndDetector(detChan, &module, &detector);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaGetAcquisitionValues",
                       "Unable to locate the module for detChan %d", detChan);
                return status;
            }

            status = module->psl->getAcquisitionValues(detChan, detector, module, name, value);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaGetAcquisitionValues",
                       "Unable to get acquisition values for detChan %d", detChan);
                return status;
            }

            break;

        case 999:
            status = XIA_INVALID_DETCHAN;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetAcquisitionValues",
                   "detChan number is not in the list of valid values ");
            return status;
        default:
            status = XIA_UNKNOWN;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetAcquisitionValues",
                   "Should not be seeing this message");
            return status;
    }

    return XIA_SUCCESS;
}


/**
 * This routine removes an acquisition value from the internal defaults
 * list for a specified channel.
 */
HANDEL_EXPORT int HANDEL_API xiaRemoveAcquisitionValues(int detChan, const char *name)
{
    int status;
    int elemType;

    XiaDefaults *defaults = NULL;

    XiaDaqEntry *entry    = NULL;
    XiaDaqEntry *previous = NULL;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;

    boolean_t canRemove = FALSE_;

    Module *module = NULL;

    Detector *detector = NULL;

    elemType = xiaGetElemType(detChan);

    switch(elemType) {

        case SINGLE:

            status = xiaFindModuleAndDetector(detChan, &module, &detector);

            if (status != XIA_SUCCESS) {

                xiaLog(XIA_LOG_ERROR, status, "xiaRemoveAcquisitionValues",
                       "Error getting the module for detChan %d", detChan);
                return status;
            }

            canRemove = module->psl->canRemoveName(name);

            if (canRemove) {

                defaults = xiaGetDefaultFromDetChan(detChan);

                entry = defaults->entry;

                while (entry != NULL) {

                    if (STREQ(name, entry->name)) {

                        if (previous == NULL) {

                            defaults->entry = entry->next;

                        } else {

                            previous->next = entry->next;
                        }

                        handel_md_free((void *)entry->name);
                        handel_md_free((void *)entry);

                        break;
                    }

                    previous = entry;
                    entry = entry->next;
                }

                status = module->psl->setupDetChan(detChan, detector, module);

                if (status != XIA_SUCCESS) {
                    xiaLog(XIA_LOG_ERROR, status, "xiaRemoveAcquisitionValues",
                           "Error updating acquisition values after '%s' removed "
                           "from list for detChan %d", name, detChan);
                    return status;
                }
            } else {

                status = XIA_NO_REMOVE;
                xiaLog(XIA_LOG_ERROR, status, "xiaRemoveAcquisitionValues",
                       "Specified acquisition value %s is a required value for detChan %d",
                       name, detChan);
                return status;
            }

            break;

        case SET:

            detChanElem = xiaGetDetChanPtr(detChan);

            detChanSetElem = detChanElem->data.detChanSet;

            while (detChanSetElem != NULL) {

                status = xiaRemoveAcquisitionValues(detChanSetElem->channel, name);

                if (status != XIA_SUCCESS) {

                    xiaLog(XIA_LOG_ERROR, status, "xiaRemoveAcquisitionValues",
                           "Error removing %s from detChan %u",
                           name, detChanSetElem->channel);
                    return status;
                }

                detChanSetElem = detChanSetElem->next;
            }

            break;

        case 999:
            status = XIA_INVALID_DETCHAN;
            xiaLog(XIA_LOG_ERROR, status, "xiaRemoveAcquisitionValues",
                   "detChan number is not in the list of valid values ");
            return status;
        default:

            status = XIA_UNKNOWN;
            xiaLog(XIA_LOG_ERROR, status, "xiaRemoveAcquisitionValues",
                   "Should not be seeing this message");
            return status;
    }

    return XIA_SUCCESS;

}

HANDEL_EXPORT int HANDEL_API xiaGainOperation(int detChan,
                                              const char *name, void *value)
{
    int status;
    int modChan;

    Detector *detector = NULL;

    XiaDefaults *defaults = NULL;

    Module *module = NULL;

    status = xiaFindModuleAndDetector(detChan, &module, &detector);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "xiaGainOperation",
               "Unable to do gain operation %s for detChan %d "
               "(get module and detector failed).", name, detChan);
        return status;
    }

    defaults = xiaGetDefaultFromDetChan(detChan);
    modChan  = xiaGetModChan(detChan);

    status = module->psl->gainOperation(detChan, name, value, detector,
                                        modChan, module, defaults);
    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "xiaGainOperation",
               "Error doing gain operation %s for detChan %d", name, detChan);
        return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine adjusts the gain by modifying the preamp gain. This is the
 * routine that should be called for things like gain matching.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaGainCalibrate(int detChan, double deltaGain)
{
    int status;
    int elemType;

    int modChan;

    Detector *detector = NULL;

    XiaDefaults *defaults = NULL;

    Module *module = NULL;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;

    elemType = xiaGetElemType(detChan);

    switch(elemType)
    {
        case SINGLE:
            status = xiaFindModuleAndDetector(detChan, &module, &detector);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaGainCalibrate",
                       "Unable to set gain calibrate for detChan %d "
                       "(get module and detector failed).", detChan);
                return status;
            }

            defaults = xiaGetDefaultFromDetChan(detChan);
            modChan  = xiaGetModChan(detChan);

            status = module->psl->gainCalibrate(detChan, detector, modChan, module,
                                                defaults, deltaGain);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaGainCalibrate",
                       "Error calibrating the gain for detChan %d", detChan);
                return status;
            }

            break;

        case SET:
            detChanElem = xiaGetDetChanPtr(detChan);

            detChanSetElem = detChanElem->data.detChanSet;

            while (detChanSetElem != NULL)
            {
                status = xiaGainCalibrate((int)detChanSetElem->channel, deltaGain);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaGainCalibrate",
                           "Error calibrating the gain for detChan %d", detChan);
                    return status;
                }

                detChanSetElem = getListNext(detChanSetElem);
            }

            break;

        case 999:
            status = XIA_INVALID_DETCHAN;
            xiaLog(XIA_LOG_ERROR, status, "xiaGainCalibrate",
                   "detChan number is not in the list of valid values");
            return status;
        default:
            status = XIA_UNKNOWN;
            xiaLog(XIA_LOG_ERROR, status, "xiaGainCalibrate",
                   "Should not be seeing this message");
            return status;
    }

    return XIA_SUCCESS;
}
