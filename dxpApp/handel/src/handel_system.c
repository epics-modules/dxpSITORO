/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 *               2005-2015 XIA LLC
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


#include "xia_handel.h"
#include "xia_system.h"
#include "xia_assert.h"
#include "xia_common.h"

#include "handel_errors.h"
#include "handel_log.h"

#include "psl.h"

/**
 * Used to determine the start and end counts.
 */
static int starts;
static int ends;
static int systemState;

#define HANDEL_SYSTEM_STATE_DEAD     (0)
#define HANDEL_SYSTEM_STATE_STARTING (1)
#define HANDEL_SYSTEM_STATE_RUNNING  (2)
#define HANDEL_SYSTEM_STATE_ENDING  (2)

void (*handel_md_output)(const char *stream);
void * (*handel_md_alloc)(size_t bytes);
void (*handel_md_free)(void *ptr);
int (*handel_md_puts)(const char *s);
int (*handel_md_wait)(float *secs);
char * (*handel_md_fgets)(char *s, int size, FILE *stream);
int (*handel_md_enable_log)(void);
int (*handel_md_suppress_log)(void);
int (*handel_md_set_log_level)(int level);

HANDEL_SHARED boolean_t HANDEL_API xiaHandelSystemStarting(void)
{
  return systemState == HANDEL_SYSTEM_STATE_STARTING;
}

HANDEL_SHARED boolean_t HANDEL_API xiaHandelSystemRunning(void)
{
  return systemState == HANDEL_SYSTEM_STATE_RUNNING;
}

HANDEL_SHARED boolean_t HANDEL_API xiaHandelSystemEnding(void)
{
  return systemState == HANDEL_SYSTEM_STATE_ENDING;
}

/** Starts the system previously defined via an .ini file.
 *
 * This routine validates as much information about the system as
 * possible before it binds to Xerxes, connects to the low-level I/O
 * drivers, downloads firmware and acquisition values, and otherwise
 * prepares the system for run operation.
 */
HANDEL_EXPORT int HANDEL_API xiaStartSystem(void)
{
    int status;

    xiaLog(XIA_LOG_INFO, "xiaStartSystem",
           "System start count: %d", ++starts);

    if (systemState != HANDEL_SYSTEM_STATE_DEAD) {
        xiaLog(XIA_LOG_ERROR, XIA_BAD_VALUE, "xiaStartSystem",
               "System is not in the DEAD state. Forcing the state to STARTING.");
    }

    systemState = HANDEL_SYSTEM_STATE_STARTING;

    status = xiaValidateFirmwareSets();

    if (status != XIA_SUCCESS) {
        systemState = HANDEL_SYSTEM_STATE_DEAD;
        xiaLog(XIA_LOG_ERROR, status, "xiaStartSystem",
               "Error validating system-wide firmware sets.");
        return status;
    }

    status = xiaValidateDetector();

    if (status != XIA_SUCCESS) {
        systemState = HANDEL_SYSTEM_STATE_DEAD;
        xiaLog(XIA_LOG_ERROR, status, "xiaStartSystem",
               "Error validating system-wide detector configurations.");
        return status;
    }

    status = xiaValidateDetSets();

    if (status != XIA_SUCCESS) {
        systemState = HANDEL_SYSTEM_STATE_DEAD;
        xiaLog(XIA_LOG_ERROR, status, "xiaStartSystem",
               "Error validating detector channel sets.");
        return status;
    }

    status = xiaSetupModules();

    if (status != XIA_SUCCESS) {
        systemState = HANDEL_SYSTEM_STATE_DEAD;
        xiaLog(XIA_LOG_ERROR, status, "xiaStartSystem",
               "Error performing module setup tasks.");
        return status;
    }

    status = xiaSetupDetectors();

    if (status != XIA_SUCCESS) {
        systemState = HANDEL_SYSTEM_STATE_DEAD;
        xiaLog(XIA_LOG_ERROR, status, "xiaStartSystem",
               "Error performing detector channel setup tasks.");
        return status;
    }

    systemState = HANDEL_SYSTEM_STATE_RUNNING;

    return XIA_SUCCESS;
}


/** Ends the system cleaning up.
 *
 */
HANDEL_EXPORT int HANDEL_API xiaEndSystem(void)
{
    int status = XIA_SUCCESS;

    if (starts) {
        int modStatus;
        int detStatus;

        xiaLog(XIA_LOG_INFO, "xiaEndSystem",
               "System end count: %d", ++ends);

        if ((systemState != HANDEL_SYSTEM_STATE_STARTING) &&
            (systemState != HANDEL_SYSTEM_STATE_RUNNING)) {
            xiaLog(XIA_LOG_ERROR, XIA_BAD_VALUE, "xiaEndSystem",
                   "System is not in the STARTING or RUNNING state. Forcing the state to ENDING.");
        }

        systemState = HANDEL_SYSTEM_STATE_ENDING;

        detStatus = xiaEndDetectors();

        if (detStatus != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, detStatus, "xiaEndSystem",
                   "Error performing detector channel end tasks.");
            status = detStatus;
        }

        modStatus = xiaEndModules();

        if (modStatus != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, modStatus, "xiaEndSystem",
                   "Error performing module end tasks.");
            if (status == XIA_SUCCESS)
                status = modStatus;
        }

        systemState = HANDEL_SYSTEM_STATE_DEAD;
    }

    return status;
}



/*****************************************************************************
 *
 * This routine returns the PSL handlers for a specific board type.
 *
 * The name has changed to break previous code that depended on the old
 * function.
 *
 * This code needs to be changed to iterate over a list of registered board
 * types. The registration is an initialisation task and not part of this
 * generic part of Handel.
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetPSLHandlers(const char *boardType,
                                               const PSLHandlers **handlers)
{
    int status;

    /* XXX TODO Use a list of function pointers
     * to call these functions.
     */

    /* We need this 'if' just so the conditionally compiled board types can use
     * 'else if'.
     */
    if (handlers == NULL) {
        status = XIA_BAD_VALUE;
    }
    else if (boardType == NULL) {
        status = XIA_UNKNOWN_BOARD;

    } else if (STREQ(boardType, "falconxn")) {
        status = falconxn_PSLInit(handlers);

    } else {
        *handlers = NULL;
        status = XIA_UNKNOWN_BOARD;
    }

    if (status == XIA_UNKNOWN_BOARD) {
        xiaLog(XIA_LOG_ERROR, status, "xiaGetPSLHandlers",
               "Board type '%s' is not supported in this version of the library",
               boardType);
        return status;

    } else if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "xiaGetPSLHandler", "Error initializing PSL functions");
        return status;
    }

    return XIA_SUCCESS;
}


/**********
 * Performs non-persistent operations on the board. Mostly
 * used with the microDXP.
 **********/
HANDEL_EXPORT int HANDEL_API xiaBoardOperation(int detChan,
                                               const char *name, void *value)
{
    int status;
    int elemType;

    Module* module = NULL;

    Detector* detector = NULL;

    if (name == NULL) {
        xiaLog(XIA_LOG_ERROR, XIA_NULL_NAME, "xiaBoardOperation",
               "'name' can not be NULL");
        return XIA_NULL_NAME;
    }

    if (value == NULL) {
        xiaLog(XIA_LOG_ERROR, XIA_NULL_NAME, "xiaBoardOperation",
               "'value' can not be NULL");
        return XIA_NULL_VALUE;
    }

    elemType = xiaGetElemType(detChan);

    switch (elemType)
    {
        case SINGLE:
            status = xiaFindModuleAndDetector(detChan, &module, &detector);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaBoardOperation",
                       "Unable to locate the module for detChan %d", detChan);
                return status;
            }

            status = module->psl->boardOperation(detChan, detector, module, name, value);
            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaBoardOperation",
                       "Unable to do board operation (%s) for detChan %d", name, detChan);
                return status;
            }

            break;


        case SET:
            status = XIA_BAD_TYPE;
            xiaLog(XIA_LOG_ERROR, status, "xiaBoardOperation",
                   "This routine only supports single detChans");
            return status;

        case 999:
            status = XIA_INVALID_DETCHAN;
            xiaLog(XIA_LOG_ERROR, status, "xiaBoardOperation",
                   "detChan number is not in the list of valid values ");
            return status;
        default:
            /* Better not get here */
            status = XIA_UNKNOWN;
            xiaLog(XIA_LOG_ERROR, XIA_UNKNOWN, "xiaBoardOperation",
                   "Should not be seeing this message");
            return status;
    }

    return XIA_SUCCESS;
}
