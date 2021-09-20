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

/* Include the VLD header for memory debugging options */
#ifdef __VLD_MEM_DBG__
#include <vld.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "xia_handel.h"
#include "xia_module.h"
#include "xia_system.h"
#include "xia_common.h"
#include "xia_assert.h"
#include "xia_version.h"
#include "xia_handel_structures.h"
#include "xia_file.h"

#include "handel_generic.h"
#include "handel_errors.h"
#include "handel_log.h"
#include "handeldef.h"

#include "md_shim.h"


HANDEL_STATIC int HANDEL_API xiaInitMemory(void);
HANDEL_STATIC int HANDEL_API xiaInitHandelDS(void);


/* This is currently not used right
 * now. Most libraries require some sort
 * of initialization so it is probably
 * beyond Handel right now to try and warn
 * the user if the library isn't initialized.
 */
boolean_t isHandelInit = FALSE_;


/** @brief Initializes the library and loads an .INI file
 *
 *  The functionality of this routine can be emulated by calling xiaInitHandel()
 *  followed by xiaLoadSystem("handel_ini", iniFile). Either this routine
 *  or xiaInitHandel() must be called prior to using the other routines in
 *  Handel.
 *
 *  @param iniFile Name of file (in "handel_ini" format) to be loaded
 *
 *  @return An error value indicating success (@a XIA_SUCCESS) or failure
 *  (@a XIA_XERXES, @a XIA_NOMEM or @a XIA_OPEN_FILE)
 *
 */
HANDEL_EXPORT int HANDEL_API xiaInit(const char *iniFile)
{
    int status;
    int status2;
    int nFilesOpen;

    status = xiaInitHandel();

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaInit", "Handel initialization failed");
        return status;
    }

    if (iniFile == NULL) {
        xiaLog(XIA_LOG_ERROR, XIA_BAD_NAME, "xiaInit", ".INI file name must be non-NULL");
        return XIA_BAD_NAME;
    }

    /* Verify that we currently don't have any file handles open. This is
     * not a legitimate error condition and indicates that we are not cleaning
     * up all of our handles somewhere else in the library.
     */
    nFilesOpen = xia_num_open_handles();

    if (nFilesOpen > 0) {
        xia_print_open_handles(stdout);
        FAIL();
    }

    status = xiaReadIniFile(iniFile);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaInit", "Unable to load %s", iniFile);

        /* Need to clear data structures here since we got an
         * incomplete configuration.
         */
        /* Initialize the memory of Handel.
         */
        status2 = xiaInitMemory();

        if (status2 != XIA_SUCCESS)
        {
            /* If MD routines not defined, then use printf() here since dxp_md_error()
             *  might not be assigned yet...depending on the error
             */
            if (handel_md_log != NULL)
            {
                xiaLog(XIA_LOG_ERROR, status2, "xiaInit", "Unable to Initialize memory");
                return status2;
            } else {
                printf("[ERROR] [%d] %s: %s\n",
                       status2,
                       "xiaInit",
                       "Unable to initialize Memory\n");
                return status2;
            }
        }

        return status;
    }

    return XIA_SUCCESS;
}

/** @brief Initializes the library
 *
 *  Initializes the library. Either this routine or xiaInit(char *iniFile) must
 *  be called before any other Handel routines are used.
 *
 *  @return An error value indicating success (@a XIA_SUCCESS) or failure
 *  (@a XIA_XERXES or @a XIA_NOMEM)
 *
 */
HANDEL_EXPORT int HANDEL_API xiaInitHandel(void)
{
    int status = XIA_SUCCESS;

    /* Arbitrary length here */
    char version[120];

    /* In case the user is resetting things
     * manually.
     */
    status = xiaExit();

    if (status != XIA_SUCCESS) {
        /* Error handler may not
         * be installed yet.
         */
        printf("[ERROR] [%d] %s: %s\n",
               status, "xiaInitHandel",
               "Unable to perform exit procedures");
        return status;
    }

    /* Initialize the memory of both Handel and Xerxes.
     */
    status = xiaInitMemory();

    if (status != XIA_SUCCESS)
    {
        /* If MD routines not defined, then use printf() here since dxp_md_error()
         *  might not be assined yet...depending on the error
         */
        if (handel_md_log != NULL)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaInitHandel", "Unable to Initialize memory");
            return status;
        } else {
            printf("[ERROR] [%d] %s: %s\n",
                   status,
                   "xiaInitHandel",
                   "Unable to initialize Memory\n");
            return status;
        }
    }

    xiaGetVersionInfo(NULL, NULL, NULL, version);

    xiaLog(XIA_LOG_INFO, "xiaInitHandel", "Successfully initialized Handel %s", version);

#ifdef __VLD_MEM_DBG__
    xiaLog(XIA_LOG_INFO, "xiaInitHandel", "This version of Handel was built with VLD memory leak debugging enabled.");
#endif
    
    
    return status;
}


/******************************************************************************
 *
 * Routine to initialize the library. This routine modifies the global static
 * variable isHandelInit.
 *
 ******************************************************************************/
HANDEL_STATIC int HANDEL_API xiaInitMemory()
{
    int status = XIA_SUCCESS;


    /* Make our function pointers equal to XerXes function pointers using the
     * imported utils variable
     */
    handel_md_log           = dxp_md_log;
    handel_md_output        = dxp_md_output;
    handel_md_enable_log    = dxp_md_enable_log;
    handel_md_suppress_log  = dxp_md_suppress_log;
    handel_md_set_log_level = dxp_md_set_log_level;
    handel_md_alloc         = malloc;
    handel_md_free          = free;
    handel_md_wait          = dxp_md_wait;
    handel_md_fgets         = dxp_md_fgets;

    /* Clear the HanDeL data structures */
    status = xiaInitHandelDS();
    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "xiaInitHandel", "Unable to clear Data Structures");
        return status;
    }

    /* Init the FDD lib here */
    status = xiaFddInitialize();

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "xiaInitHandel", "Error initializing FDD layer");
        return status;
    }

    isHandelInit = TRUE_;

    return status;
}


/**********
 * Responsible for performing any tasks related to
 * exiting the library.
 **********/
HANDEL_EXPORT int HANDEL_API xiaExit(void)
{
    xiaRemoveAllModules();
    return XIA_SUCCESS;
}


/**
 * Returns various components of Handel's version information.
 *
 * Returns the release, minor and major version numbers for Handel. These values
 * would typically be reassembled using a syntax such as 'maj'.'min'.'rel'.
 * The optional 'pretty' argument returns a string preformatted for writing to
 * a log or display. The 'pretty' string also contains an extra tag of
 * information indicating special build information (dev, release, etc). There is
 * currently no way to retrieve that information as a separate unit.
 */
HANDEL_EXPORT void HANDEL_API xiaGetVersionInfo(int *rel, int *min, int *maj,
                                                char *pretty)
{
    if (rel && min && maj) {
        *rel = HANDEL_RELEASE_VERSION;
        *min = HANDEL_MINOR_VERSION;
        *maj = HANDEL_MAJOR_VERSION;
    }

    if (pretty) {
      sprintf(pretty, "v%d.%d.%d (%s)",
              HANDEL_MAJOR_VERSION, HANDEL_MINOR_VERSION,
              HANDEL_RELEASE_VERSION, VERSION_STRING);
    }

}


/******************************************************************************
 *
 * Routine to initialize the Detector linked list.
 *
 ******************************************************************************/
HANDEL_STATIC int HANDEL_API xiaInitHandelDS()
{
    int status = XIA_SUCCESS;

    status = xiaInitDetectorDS();
    if(status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaInitHandelDS",
               "Unable to clear the Detector LL");
        return status;
    }

    status = xiaInitFirmwareSetDS();
    if(status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaInitHandelDS",
               "Unable to clear the FirmwareSet LL");
        return status;
    }

    status = xiaInitModuleDS();
    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaInitHandelDS",
               "Unable to clear Module LL");
        return status;
    }

    status = xiaInitDetChanDS();
    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaInitHandelDS",
               "Unable to clear DetChan LL");
        return status;
    }

    status = xiaInitXiaDefaultsDS();
    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaInitHandelDS",
               "Unable to clear Defaults LL");
        return status;
    }

    return status;
}
