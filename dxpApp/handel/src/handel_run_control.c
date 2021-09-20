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


#include <stdio.h>

#include "handeldef.h"
#include "xia_handel_structures.h"
#include "xia_handel.h"
#include "xia_system.h"
#include "xia_assert.h"

#include "handel_errors.h"
#include "handel_log.h"


/*****************************************************************************
 *
 * This routine starts a run on the specified detChan by calling the
 * appropriate XerXes routine through the PSL.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaStartRun(int detChan, unsigned short resume)
{
    int status;
    int elemType;

    int chan = 0;

    XiaDefaults *defaults = NULL;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;

    Module *module = NULL;
    Detector *detector = NULL;

    elemType = xiaGetElemType(detChan);

    switch(elemType)
    {
        case SINGLE:
            /* Check to see if this board is a multichannel board and,
             * if so, is a run already active on that channel set via.
             * the run broadcast...
             */
            status = xiaFindModuleAndDetector(detChan, &module, &detector);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaStartRun",
                       "Unable to start run for detChan %d (get module failed).",
                       detChan);
                return status;
            }

            if (module->isMultiChannel)
            {
                status = xiaGetAbsoluteChannel(detChan, module, &chan);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaStartRun",
                           "detChan = %d not found in module '%s'",
                           detChan, module->alias);
                    return status;
                }

                if (module->state->runActive[chan])
                {
                    xiaLog(XIA_LOG_INFO, "xiaStartRun",
                           "detChan %d is part of a multichannel module whose"
                           " run was already started", detChan);
                    break;
                }
            }


            defaults = xiaGetDefaultFromDetChan(detChan);

            status = module->psl->startRun(detChan, resume, defaults, detector, module);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaStartRun",
                       "Unable to start run for detChan %d", detChan);
                return status;
            }

            /* Tag all of the channels if this is a multichannel
             * module.
             */
            if (module->isMultiChannel)
            {
                status = xiaTagAllRunActive(module, TRUE_);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaStartRun",
                           "Error setting channel state information: runActive");
                    return status;
                }
            }

            break;

        case SET:
            detChanElem = xiaGetDetChanPtr(detChan);

            detChanSetElem = detChanElem->data.detChanSet;

            while (detChanSetElem != NULL)
            {
                status = xiaStartRun((int)detChanSetElem->channel, resume);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaStartRun",
                           "Error starting run for detChan %d", detChan);
                    return status;
                }

                detChanSetElem = getListNext(detChanSetElem);
            }

            break;

        case 999:
            status = XIA_INVALID_DETCHAN;
            xiaLog(XIA_LOG_ERROR, status, "xiaStartRun",
                   "detChan number is not in the list of valid values ");
            return status;
        default:
            status = XIA_UNKNOWN;
            xiaLog(XIA_LOG_ERROR, status, "xiaStartRun",
                   "Should not be seeing this message");
            return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine stops a run on detChan. In some cases, the hardware will have
 * no choice but to stop a run on all channels associated with the module
 * pointed to by detChan.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaStopRun(int detChan)
{
    int status;
    int elemType;

    int chan = 0;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;

    Module *module = NULL;
    Detector *detector = NULL;

    elemType = xiaGetElemType(detChan);

    switch(elemType)
    {
        case SINGLE:
            status = xiaFindModuleAndDetector(detChan, &module, &detector);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaStopRun",
                       "Unable to stop run for detChan %d (get module failed).",
                       detChan);
                return status;
            }

            if (module->isMultiChannel)
            {
                status = xiaGetAbsoluteChannel(detChan, module, &chan);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaStopRun",
                           "detChan = %d not found in module '%s'",
                           detChan, module->alias);
                    return status;
                }

                if (!module->state->runActive[chan])
                {
                    xiaLog(XIA_LOG_INFO, "xiaStopRun",
                           "detChan %d is part of a multichannel module whose"
                           " run was already stopped", detChan);
                    break;
                }
            }

            status = module->psl->stopRun(detChan, detector, module);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaStopRun",
                       "Unable to stop run for detChan %d", detChan);
                return status;
            }

            if (module->isMultiChannel)
            {
                status = xiaTagAllRunActive(module, FALSE_);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaStopRun",
                           "Error setting channel state information: runActive");
                    return status;
                }
            }

            break;

        case SET:
            detChanElem = xiaGetDetChanPtr(detChan);

            detChanSetElem = detChanElem->data.detChanSet;

            while (detChanSetElem != NULL)
            {
                /* Recursive loop here to stop run for all channels */
                status = xiaStopRun((int)detChanSetElem->channel);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaStopRun",
                           "Error stoping run for detChan %d", detChan);
                    return status;
                }

                detChanSetElem = getListNext(detChanSetElem);
            }

            break;

        case 999:
            status = XIA_INVALID_DETCHAN;
            xiaLog(XIA_LOG_ERROR, status, "xiaStopRun",
                   "detChan number is not in the list of valid values ");
            return status;
        default:
            status = XIA_UNKNOWN;
            xiaLog(XIA_LOG_ERROR, status, "xiaStopRun",
                   "Should not be seeing this message");
            return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine gets the type of data specified by name. User is expected to
 * allocate the proper amount of memory for situations where value is an
 * array.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaGetRunData(int detChan,
                                           const char *name, void *value)
{
    int status;
    int elemType;

    XiaDefaults *defaults = NULL;

    Module *module = NULL;
    Detector *detector = NULL;

    elemType = xiaGetElemType(detChan);

    switch(elemType)
    {
        case SINGLE:
            status = xiaFindModuleAndDetector(detChan, &module, &detector);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaGetRunData",
                       "Unable to get run data for detChan %d (get module failed).",
                       detChan);
                return status;
            }

            defaults = xiaGetDefaultFromDetChan(detChan);

            status = module->psl->getRunData(detChan, name, value, defaults, detector, module);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaGetRunData",
                       "Unable get run data %s for detChan %d", name, detChan);
                return status;
            }

            break;

        case SET:
            /* Don't allow SETs since there is no way to handle the potential of multi-
             * dimensional data.
             */
            status = XIA_BAD_TYPE;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetRunData",
                   "Unable to get run data for a detChan SET");
            return status;

        case 999:
            status = XIA_INVALID_DETCHAN;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetRunData",
                   "detChan number is not in the list of valid values");
            return status;
        default:
            status = XIA_UNKNOWN;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetRunData",
                   "Should not be seeing this message");
            return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine calls the PSL layer to execute a special run. Extremely
 * dependent on module type.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaDoSpecialRun(int detChan,
                                             const char *name, void *info)
{
    int status;
    int elemType;

    XiaDefaults *defaults;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;

    Module *module = NULL;
    Detector *detector = NULL;


    elemType = xiaGetElemType(detChan);

    switch(elemType)
    {
        case SINGLE:
            status = xiaFindModuleAndDetector(detChan, &module, &detector);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaDoSpecialRun",
                       "Unable to do special run for detChan %d (get module failed).",
                       detChan);
                return status;
            }

            defaults = xiaGetDefaultFromDetChan(detChan);

            status = module->psl->doSpecialRun(detChan, name, info, defaults, detector, module);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaDoSpecialRun",
                       "Unable to perform special run for detChan %d", detChan);
                return status;
            }

            break;

        case SET:
            detChanElem = xiaGetDetChanPtr(detChan);

            detChanSetElem = detChanElem->data.detChanSet;

            while (detChanSetElem != NULL)
            {
                status = xiaDoSpecialRun((int)detChanSetElem->channel, name, info);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaDoSpecialRun",
                           "Error performing special run for detChan %d", detChan);
                    return status;
                }

                detChanSetElem = getListNext(detChanSetElem);
            }

            break;

        case 999:
            status = XIA_INVALID_DETCHAN;
            xiaLog(XIA_LOG_ERROR, status, "xiaDoSpecialRun",
                   "detChan number is not in the list of valid values ");
            return status;
        default:
            status = XIA_UNKNOWN;
            xiaLog(XIA_LOG_ERROR, status, "xiaDoSpecialRun",
                   "Should not be seeing this message");
            return status;
    }

    return XIA_SUCCESS;
}


HANDEL_EXPORT int HANDEL_API xiaGetSpecialRunData(int detChan,
                                                  const char *name, void *value)
{
    int status;
    int elemType;

    XiaDefaults *defaults;

    DetChanElement *detChanElem = NULL;

    DetChanSetElem *detChanSetElem = NULL;

    Module *module = NULL;
    Detector *detector = NULL;


    elemType = xiaGetElemType(detChan);

    switch(elemType)
    {
        case SINGLE:
            status = xiaFindModuleAndDetector(detChan, &module, &detector);

            if (status != XIA_SUCCESS) {
                xiaLog(XIA_LOG_ERROR, status, "xiaGetSpecialRunData",
                       "Unable to get special run data for detChan %d (get module failed).",
                       detChan);
                return status;
            }

            defaults = xiaGetDefaultFromDetChan(detChan);

            status = module->psl->getSpecialRunData(detChan, name, value, defaults,
                                                    detector, module);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaGetSpecialRunData",
                       "Unable to get special run data for detChan %d", detChan);
                return status;
            }

            break;

        case SET:
            detChanElem = xiaGetDetChanPtr(detChan);

            detChanSetElem = detChanElem->data.detChanSet;

            while (detChanSetElem != NULL)
            {
                status = xiaGetSpecialRunData((int)detChanSetElem->channel, name, value);

                if (status != XIA_SUCCESS)
                {
                    xiaLog(XIA_LOG_ERROR, status, "xiaGetSpecialRunData",
                           "Error getting special run data for detChan %d", detChan);
                    return status;
                }

                detChanSetElem = getListNext(detChanSetElem);
            }

            break;

        case 999:
            status = XIA_INVALID_DETCHAN;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetSpecialRunData",
                   "detChan number is not in the list of valid values ");
            return status;
        default:
            status = XIA_UNKNOWN;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetSpecialRunData",
                   "Should not be seeing this message");
            return status;
    }

    return XIA_SUCCESS;
}
