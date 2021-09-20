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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "xia_handel.h"
#include "xia_handel_structures.h"
#include "xia_common.h"
#include "xia_assert.h"

#include "handel_generic.h"
#include "handeldef.h"
#include "handel_errors.h"
#include "handel_log.h"


/*
 * Define the head of the Firmware Sets LL
 */
static FirmwareSet *xiaFirmwareSetHead = NULL;


HANDEL_STATIC boolean_t HANDEL_API xiaIsFDFvsFirmValid(FirmwareSet *fSet);
HANDEL_STATIC boolean_t HANDEL_API xiaArePTRsValid(Firmware **firmware);
HANDEL_STATIC boolean_t HANDEL_API xiaAreFiPPIAndDSPValid(Firmware *firmware);
HANDEL_STATIC int HANDEL_API xiaSetFirmwareItem(FirmwareSet *fs, Firmware *f,
                                                const char *name, void *value);
HANDEL_STATIC boolean_t HANDEL_API xiaIsPTRRFree(Firmware *firmware,
                                                 unsigned short pttr);


/*****************************************************************************
 *
 * This routine creates a new Firmware entry
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaNewFirmware(const char *alias)
{
    int status = XIA_SUCCESS;

    FirmwareSet *current=NULL;

    /* If HanDeL isn't initialized, go ahead and call it... */
    if (!isHandelInit)
    {
        status = xiaInitHandel();
        if (status != XIA_SUCCESS)
        {
            fprintf(stderr, "FATAL ERROR: Unable to load libraries.\n");
            exit(XIA_INITIALIZE);
        }

        xiaLog(XIA_LOG_WARNING, "xiaNewFirmware",
               "HanDeL was initialized silently");
    }

    if ((strlen(alias) + 1) > MAXALIAS_LEN)
    {
        status = XIA_ALIAS_SIZE;
        xiaLog(XIA_LOG_ERROR, status, "xiaFirmwareDetector",
               "Alias contains too many characters");
        return status;
    }

    xiaLog(XIA_LOG_DEBUG, "xiaNewFirmware",
           "alias = %s", alias);

    /* First check if this alias exists already? */
    current = xiaFindFirmware(alias);

    if (current != NULL)
    {
        status = XIA_ALIAS_EXISTS;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewFirmware",
               "Alias %s already in use.", alias);
        return status;
    }

    /* Check that the Head of the linked list exists */
    if (xiaFirmwareSetHead == NULL)
    {
        /* Create an entry that is the Head of the linked list */
        xiaFirmwareSetHead = (FirmwareSet *) handel_md_alloc(sizeof(FirmwareSet));
        current = xiaFirmwareSetHead;

    } else {
        /* Find the end of the linked list */
        current= xiaFirmwareSetHead;

        while (current->next != NULL)
        {
            current= current->next;
        }
        current->next = (FirmwareSet *) handel_md_alloc(sizeof(FirmwareSet));
        current= current->next;
    }

    /* Make sure memory was allocated */
    if (current == NULL)
    {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewFirmware",
               "Unable to allocate memory for Firmware alias %s.", alias);
        return status;
    }

    /* Do any other allocations, or initialize to NULL/0 */
    current->alias = (char *) handel_md_alloc((strlen(alias) + 1) * sizeof(char));
    if (current->alias == NULL)
    {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewFirmware",
               "Unable to allocate memory for current->alias");
        return status;
    }

    strcpy(current->alias,alias);

    current->filename    = NULL;
    current->keywords    = NULL;
    current->numKeywords = 0;
    current->tmpPath     = NULL;
    current->mmu         = NULL;
    current->firmware    = NULL;
    current->next        = NULL;

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine adds information about a Firmware Item entry
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaAddFirmwareItem(const char *alias,
                                                const char *name, void *value)
{
    int status = XIA_SUCCESS;

    unsigned int i;

    char strtemp[MAXALIAS_LEN];

    FirmwareSet *chosen = NULL;

    /* Have to keep a pointer for the current Firmware ptrr */
    static Firmware *current = NULL;

    /* Locate the FirmwareSet entry first */
    chosen = xiaFindFirmware(alias);
    if (chosen == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddFirmwareItem",
               "Alias %s has not been created.", alias);
        return status;
    }

    /* convert the name to lower case */
    for (i = 0; i < (unsigned int)strlen(name); i++)
    {
        strtemp[i] = (char)tolower((int)name[i]);
    }
    strtemp[strlen(name)] = '\0';

    /* Check that the value is not NULL */
    if (value == NULL)
    {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddFirmwareItem",
               "Value for item '%s' can not be NULL", name);
        return status;
    }

    /* Switch thru the possible entries */
    if (STREQ(strtemp, "filename"))
    {
        status = xiaSetFirmwareItem(chosen, NULL, strtemp, value);

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaAddFirmwareItem",
                   "Failure to set Firmware data: %s", name);
            return status;
        }

        /* Specifying the ptrr? */
    } else if (STREQ(strtemp, "ptrr")) {
        /* Create a new firmware structure */

        if (!xiaIsPTRRFree(chosen->firmware, *((unsigned short *)value)))
        {
            status = XIA_BAD_PTRR;
            xiaLog(XIA_LOG_ERROR, status, "xiaAddFirmwareItem",
                   "PTRR %u already exists", *((unsigned short *)value));
            return status;
        }

        if (chosen->firmware == NULL)
        {
            chosen->firmware = (Firmware *) handel_md_alloc(sizeof(Firmware));
            current = chosen->firmware;
            current->prev = NULL;

        } else {

            current->next = (Firmware *) handel_md_alloc(sizeof(Firmware));
            current->next->prev = current;
            current = current->next;
        }

        if (current == NULL)
        {
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "xiaAddFirmwareItem",
                   "Unable to allocate memory for firmware");
            return status;
        }

        current->ptrr = *((unsigned short *) value);
        /* Initialize the elements */
        current->dsp        = NULL;
        current->fippi      = NULL;
        current->max_ptime  = 0.;
        current->min_ptime  = 0.;
        current->user_fippi = NULL;
        current->next       = NULL;
        current->numFilter  = 0;
        current->filterInfo = NULL;

        /* one of the FPGA values? */
    } else {

        status = xiaSetFirmwareItem(chosen, current, strtemp, value);

        if (status != XIA_SUCCESS)
        {
            status = XIA_BAD_VALUE;
            xiaLog(XIA_LOG_ERROR, status, "xiaAddFirmwareItem",
                   "Failure to set Firmware data: %s", name);
            return status;
        }
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine modifies information about a Firmware Item entry
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaModifyFirmwareItem(const char *alias,
                                                   unsigned short ptrr,
                                                   const char *name, void *value)
{
    int status = XIA_SUCCESS;

    unsigned int i;

    char strtemp[MAXALIAS_LEN];

    FirmwareSet *chosen = NULL;

    Firmware *current = NULL;

    /* Check that the value is not NULL */
    if (value == NULL)
    {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyFirmwareItem",
               "Value can not be NULL");
        return status;
    }

    /* convert the name to lower case */
    for (i = 0; i < (unsigned int)strlen(name); i++)
    {
        strtemp[i] = (char)tolower((int)name[i]);
    }
    strtemp[strlen(name)] = '\0';

    /* Locate the FirmwareSet entry first */
    chosen = xiaFindFirmware(alias);
    if (chosen == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyFirmwareItem",
               "Alias %s was not found.", alias);
        return status;
    }

    /* Check to see if the name is a ptrr-invariant name since some
     * users will probably set ptrr to NULL under these circumstances
     * which breaks the code if a ptrr check is performed
     */
    if (STREQ(name, "filename") ||
        STREQ(name, "mmu") ||
        STREQ(name, "fdd_tmp_path"))
    {
        status = xiaSetFirmwareItem(chosen, current, name, value);

        if (status != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, status, "xiaModifyFirmwareItem",
                   "Failure to set '%s' for '%s'", name, alias);
            return status;
        }

        return status;
    }


    /* Now find the ptrr only if the name being modified requires it */
    current = chosen->firmware;
    while ((current != NULL) && (current->ptrr != ptrr))
    {
        current=current->next;
    }

    if (current == NULL)
    {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyFirmwareItem",
               "ptrr (%u) not found.", ptrr);
        return status;
    }

    /* Now modify the value */
    status = xiaSetFirmwareItem(chosen, current, strtemp, value);
    if (status != XIA_SUCCESS)
    {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyFirmwareItem",
               "Failure to set Firmware data: %s", name);
        return status;
    }

    return status;
}


/*****************************************************************************
 *
 * This routine retrieves data from a Firmware Set
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaGetFirmwareItem(const char *alias,
                                                unsigned short ptrr,
                                                const char *name, void *value)
{
    int status = XIA_SUCCESS;

    unsigned int i;

    unsigned short j;

    char strtemp[MAXALIAS_LEN];

    unsigned short *filterInfo = NULL;

    FirmwareSet *chosen = NULL;

    Firmware    *current = NULL;


    /* Find Firmware corresponding to alias */
    chosen = xiaFindFirmware(alias);

    if (chosen == NULL) {

        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareItem",
               "Alias %s has not been created", alias);
        return status;
    }

    /* Convert name to lower case */
    for (i = 0; i < (unsigned int)strlen(name); i++) {
        strtemp[i] = (char)tolower((int)name[i]);
    }

    strtemp[strlen(name)] = '\0';

    /* Decide which value to return. Start with the ptrr-invariant values */
    if (STREQ(strtemp, "filename")) {

        /* Reference: BUG ID #13
         * Reference: BUG ID #69
         * The new default behavior is to return
         * a blank string in place of the filename
         * and to not error out.
         */
        if (chosen->filename == NULL) {
            xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareItem",
                   "No filename defined for firmware with alias %s",
                   chosen->alias);

            strcpy((char *)value, "");

        } else {
            strcpy((char *)value, chosen->filename);
        }

    } else if (STREQ(strtemp, "fdd_tmp_path")) {

        if (chosen->filename == NULL) {
            xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareItem",
                   "No FDD file for '%s'", chosen->alias);
            return XIA_NO_FILENAME;
        }

        if (chosen->tmpPath == NULL) {
            xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareItem",
                   "FDD temporary file path never defined for '%s'",
                   chosen->alias);
            return XIA_NO_TMP_PATH;
        }

        ASSERT((strlen(chosen->tmpPath) + 1) < MAXITEM_LEN);

        strcpy((char *)value, chosen->tmpPath);

    } else if (STREQ(strtemp, "mmu")) {

        /* Reference: BUG ID #12 */
        if (chosen->mmu == NULL) {

            status = XIA_NO_FILENAME;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareItem",
                   "No MMU file defined for firmware with alias %s",
                   chosen->alias);
            return status;
        }

        strcpy((char *)value, chosen->mmu);

    } else {
        /* Should be branching into names that require the ptrr value...if not
         * we'll still catch it at the end and everything will be fine
         */
        current = chosen->firmware;
        /* Have to check to see if current is initally NULL as well. While this
         * should be a rare case and was discovered my a malicious test that I
         * wrote, we need to protect against it.
         */
        if (current == NULL)
        {
            status = XIA_BAD_VALUE;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareItem",
                   "No ptrr(s) defined for this alias: %s", alias);
            return status;
        }

        while (current->ptrr != ptrr)
        {
            /* Check to see if we ran into the end of the list here... */
            current = current->next;

            if (current == NULL)
            {
                status = XIA_BAD_PTRR;
                xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareItem",
                       "ptrr %u is not valid for this alias", ptrr);
                return status;
            }
        }


        if (STREQ(strtemp, "min_peaking_time"))
        {
            *((double *)value) = current->min_ptime;

        } else if (STREQ(strtemp, "max_peaking_time")) {

            *((double *)value) = current->max_ptime;

        } else if (STREQ(strtemp, "fippi")) {

            strcpy((char *)value, current->fippi);

        } else if (STREQ(strtemp, "dsp")) {

            strcpy((char *)value, current->dsp);

        } else if (STREQ(strtemp, "user_fippi")) {

            strcpy((char *)value, current->user_fippi);

        } else if (STREQ(strtemp, "num_filter")) {

            /* Reference: BUG ID #8 */

            *((unsigned short *)value) = current->numFilter;

        } else if (STREQ(strtemp, "filter_info")) {

            /* Do a full copy here
             * Reference: BUG ID #8
             */
            filterInfo = (unsigned short *)value;

            for (j = 0; j < current->numFilter; j++) {

                filterInfo[j] = current->filterInfo[j];
            }

        } else {
            /* Bad name */
            status = XIA_BAD_NAME;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareItem",
                   "Invalid Name: %s", name);
            return status;
        }
        /* Bad names propogate all the way to the end so there is no reason to
         * report an error message here.
         */
    }

    return status;
}


/**********
 * This routine gets the number of firmwares in the system.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetNumFirmwareSets(unsigned int *numFirmware)
{
    unsigned int count = 0;

    FirmwareSet *current = xiaFirmwareSetHead;


    while (current != NULL) {

        count++;
        current = getListNext(current);
    }

    *numFirmware = count;

    return XIA_SUCCESS;
}


/**********
 * This routine returns a list of the firmware aliases
 * currently defined in the system. Assumes that the
 * calling routine has allocated enough memory in
 * "firmwares".
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetFirmwareSets(char *firmwares[])
{
    int i;

    FirmwareSet *current = xiaFirmwareSetHead;


    for (i = 0; current != NULL; current = getListNext(current)) {

        strcpy(firmwares[i], current->alias);
    }

    return XIA_SUCCESS;
}


/**********
 * This routine is similar to xiaGetFirmwareSets() but
 * VB can't pass string arrays to a DLL, so it only
 * returns a single alias string.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetFirmwareSets_VB(unsigned int index, char *alias)
{
    int status;

    unsigned int curIdx;

    FirmwareSet *current = xiaFirmwareSetHead;


    for (curIdx = 0; current != NULL; current = getListNext(current), curIdx++) {

        if (curIdx == index) {

            strcpy(alias, current->alias);

            return XIA_SUCCESS;
        }
    }

    status = XIA_BAD_INDEX;
    xiaLog(XIA_LOG_ERROR, status, "xiaGetFirmwareSets_VB",
           "Index = %u is out of range for the firmware set list", index);
    return status;
}


/**********
 * This routine returns the # of PTRRs defined for the
 * specified FirmwareSet alias. If a FDD file is defined
 * then an error is reported since the information
 * in the FDD doesn't translate directly into PTRRs.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetNumPTRRs(const char *alias, unsigned int *numPTRR)
{
    int status;

    unsigned int count = 0;

    FirmwareSet *chosen = NULL;

    Firmware *current = NULL;


    chosen = xiaFindFirmware(alias);

    if (chosen == NULL) {

        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetNumPTRRs",
               "Alias %s has not been created yet", alias);
        return status;
    }

    if (chosen->filename != NULL) {

        status = XIA_LOOKING_PTRR;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetNumPTRRs",
               "Looking for PTRRs and found an FDD file for alias %s", alias);
        return status;
    }

    current = chosen->firmware;

    while (current != NULL) {

        count++;

        current = getListNext(current);
    }

    *numPTRR = count;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine loops over all of the elements of the FirmwareSets LL and
 * checks that the data is valid. In the case of a misconfiguration, the
 * returned error code should indicate what part of the FirmwareSet is
 * invalid.
 *
 * The current logic is as follows:
 * 1) FirmwareSet must either define an FDF file OR a "Firmware" (LL)
 *
 * For the Firmware Elements within the FirmwareSet:
 * 1) The peaking time ranges may not overlap between different PTRRs. This
 *    creates an unacceptably ambiguous situation about which PTRR to use
 *    as the firmware definition for a given peaking time in the overlapped
 *    range.
 * 2) A (FiPPI OR User FiPPI) AND DSP must be defined for each element.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaValidateFirmwareSets(void)
{
    int status;

    FirmwareSet *current = NULL;

    current = xiaGetFirmwareSetHead();

    while (current != NULL)
    {
        if (!xiaIsFDFvsFirmValid(current))
        {
            status = XIA_FIRM_BOTH;
            xiaLog(XIA_LOG_ERROR, status, "xiaValidateFirmwareSets",
                   "Firmware alias %s contains both an FDF and Firmware definitions",
                   current->alias);
            return status;
        }

        /* If we only have an FDF file then we don't need to do anything else */
        if (current->filename != NULL)
        {
            return XIA_SUCCESS;
        }

        if (!xiaArePTRsValid(&(current->firmware)))
        {
            status = XIA_PTR_OVERLAP;
            xiaLog(XIA_LOG_ERROR, status, "xiaVAlidateFirmwareSets",
                   "Firmware definitions in alias %s have overlapping peaking times",
                   current->alias);
            return status;
        }

        if (!xiaAreFiPPIAndDSPValid(current->firmware))
        {
            status = XIA_MISSING_FIRM;
            xiaLog(XIA_LOG_ERROR, status, "xiaValidateFirmwareSets",
                   "Firmware definition(s) in alias %s is/are missing FiPPI and DSP files",
                   current->alias);
            return status;
        }

        current = getListNext(current);
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine contains the logic to check if and FDF and Firmware
 * definitions are defined in the FirmwareSet fSet.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaIsFDFvsFirmValid(FirmwareSet *fSet)
{
    if ((fSet->filename != NULL) &&
        (fSet->firmware != NULL))
    {
        return FALSE_;
    }

    if ((fSet->filename == NULL) &&
        (fSet->firmware == NULL))
    {
        return FALSE_;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine sorts the Firmware LL by min peaking time (which should
 * already be verified at the insertion point) and then checks the peaking
 * times for overlap. Assumes the firmware is non-NULL.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaArePTRsValid(Firmware **firmware)
{
    Firmware *current = NULL;
    Firmware *lookAhead = NULL;

    if (xiaInsertSort(firmware, xiaFirmComp) < 0)
    {
        return FALSE_;
    }

    current = *firmware;
    while (current != NULL)
    {
        /* The basic theory here is that since the Firmware LL is sorted based on
         * min peaking time, we can check that the max peaking time for a given element
         * does not overlap with any of the other min peaking times that are "past" it
         * in the list.
         */
        lookAhead = getListNext(current);

        while (lookAhead != NULL)
        {
            if (current->max_ptime > lookAhead->min_ptime)
            {
                return FALSE_;
            }

            lookAhead = getListNext(lookAhead);
        }

        current = getListNext(current);
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine checks that a FiPPI and DSP are defined
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaAreFiPPIAndDSPValid(Firmware *firmware)
{
    if (firmware->dsp == NULL)
    {
        return FALSE_;
    }

    if ((firmware->fippi == NULL) &&
        (firmware->user_fippi == NULL))
    {
        return FALSE_;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine modifies information about a Firmware Item entry
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaSetFirmwareItem(FirmwareSet *fs, Firmware *f,
                                                const char *name, void *value)
{
    int status = XIA_SUCCESS;

    unsigned int i;
    unsigned int len;

    char **oldKeywords = NULL;

    parameter_t *oldFilterInfo = NULL;

    /* Specify the mmu */
    if (STREQ(name, "mmu"))
    {
        /* If allocated already, then free */
        if (fs->mmu != NULL) {
            handel_md_free((void *)fs->mmu);
        }
        /* Allocate memory for the filename */
        len = (unsigned int)strlen((char *) value);
        fs->mmu = (char *) handel_md_alloc((len + 1) * sizeof(char));
        if (fs->mmu == NULL)
        {
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                   "Unable to allocate memory for fs->mmu");
            return status;
        }

        /* Copy the filename into the firmware structure */
        strcpy(fs->mmu,(char *) value);

    } else if (STREQ(name, "filename")) {
        /* If allocated already, then free. */
        if (fs->filename != NULL) {
            handel_md_free((void *)fs->filename);
        }
        len = (unsigned int)strlen((char *)value);
        fs->filename = (char *)handel_md_alloc((len + 1) * sizeof(char));
        if (fs->filename == NULL)
        {
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                   "Unable to allocate memory for fs->filename");
            return status;
        }

        strcpy(fs->filename, (char *)value);

    } else if (STREQ(name, "fdd_tmp_path")) {

        len = (unsigned int)strlen((char *)value);

        fs->tmpPath = handel_md_alloc(len + 1);

        if (!fs->tmpPath) {
            xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                   "Unable to allocated %u bytes for 'fs->tmpPath' with"
                   " alias = '%s'", len + 1, fs->alias);
            return XIA_NOMEM;
        }

        strcpy(fs->tmpPath, (char *)value);

    } else if (STREQ(name, "keyword")) {
        /* Check to see if the filename exists, because if it doesn't, then
         * there is no reason to be entering keywords. N.b. There is no _real_
         * reason that the user should have to enter the filename first...it's
         * just that it makes sense from a "logic" standpoint. This restriction
         * _may_ be eased in the future.
         */
        /* This should work, even if the number of keywords is zero */
        oldKeywords = (char **)handel_md_alloc(fs->numKeywords * sizeof(char *));

        for (i = 0; i < fs->numKeywords; i++)
        {
            oldKeywords[i] =
                (char *)handel_md_alloc((strlen(fs->keywords[i]) + 1) * sizeof(char));
            strcpy(oldKeywords[i], fs->keywords[i]);
            handel_md_free((void *)(fs->keywords[i]));
        }

        handel_md_free((void *)fs->keywords);
        fs->keywords = NULL;

        /* The original keywords array should be free (if it previously held keywords)
         */
        fs->numKeywords++;
        fs->keywords = (char **)handel_md_alloc(fs->numKeywords * sizeof(char *));
        if (fs->keywords == NULL)
        {
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                   "Unable to allocate memory for fs->keywords");
            return status;
        }

        for (i = 0; i < (fs->numKeywords - 1); i++)
        {
            fs->keywords[i] =
                (char *)handel_md_alloc((strlen(oldKeywords[i]) + 1) * sizeof(char));
            if (fs->keywords[i] == NULL)
            {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                       "Unable to allocate memory for fs->keywords[i]");
                return status;
            }

            strcpy(fs->keywords[i], oldKeywords[i]);
            handel_md_free((void *)oldKeywords[i]);
        }

        handel_md_free((void *)oldKeywords);

        len = (unsigned int)strlen((char *)value);
        fs->keywords[fs->numKeywords - 1] =
            (char *)handel_md_alloc((len + 1) * sizeof(char));
        if (fs->keywords[fs->numKeywords - 1] == NULL)
        {
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                   "Unable to allocate memory for fs->keywords[fs->numKeywords - 1]");
            return status;
        }

        strcpy(fs->keywords[fs->numKeywords - 1], (char *)value);

    } else {
        /* Check to make sure a valid Firmware structure exists */
        if (f == NULL)
        {
            status = XIA_BAD_VALUE;
            xiaLog(XIA_LOG_ERROR, status, "xiaAddFirmwareItem",
                   "PTRR not specified, no Firmware object exists");
            return status;
        }

        if (STREQ(name, "min_peaking_time"))
        {
            /* HanDeL doesn't have enough information to validate the peaking
             * time values here. It only has enough information to check that
             * they are correct relative to one another. We consider "0" to
             * signify that the min/max_peaking_times are not defined yet and,
             * therefore, we only check when one of them is changed and they
             * are both non-zero.  In the future I will try and simplify this
             * logic a little bit. For now, brute force!
             */
            f->min_ptime = *((double *)value);
            if ((f->min_ptime != 0.0) && (f->max_ptime != 0.0))
            {
                if (f->min_ptime > f->max_ptime)
                {
                    status = XIA_BAD_VALUE;
                    xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                           "Min. peaking time = %f not smaller then max. peaking time",
                           f->min_ptime);
                    return status;
                }
            }

        } else if (STREQ(name, "max_peaking_time")) {

            f->max_ptime = *((double *)value);
            if ((f->min_ptime != 0.0) && (f->max_ptime != 0.0))
            {
                if (f->max_ptime < f->min_ptime)
                {
                    status = XIA_BAD_VALUE;
                    xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                           "Max. peaking time = %f not larger then min. peaking time",
                           f->max_ptime);
                    return status;
                }
            }

        } else if (STREQ(name, "fippi")) {
            /* If allocated already, then free */
            if (f->fippi != NULL)
            {
                handel_md_free(f->fippi);
            }
            /* Allocate memory for the filename */
            len = (unsigned int)strlen((char *) value);
            f->fippi = (char *) handel_md_alloc((len + 1) * sizeof(char));
            if (f->fippi == NULL)
            {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                       "Unable to allocate memory for f->fippi");
                return status;
            }

            strcpy(f->fippi, (char *) value);

        } else if (STREQ(name, "user_fippi")) {

            if (f->user_fippi != NULL)
            {
                handel_md_free(f->user_fippi);
            }

            len = (unsigned int)strlen((char *) value);
            f->user_fippi = (char *) handel_md_alloc((len + 1) * sizeof(char));
            if (f->user_fippi == NULL)
            {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                       "Unable to allocate memory for f->user_fippi");
                return status;
            }

            strcpy(f->user_fippi, (char *) value);

        } else if (STREQ(name, "dsp")) {

            if (f->dsp != NULL)
            {
                handel_md_free(f->dsp);
            }

            /* Allocate memory for the filename */
            len = (unsigned int)strlen((char *) value);
            f->dsp = (char *) handel_md_alloc((len + 1) * sizeof(char));
            if (f->dsp == NULL)
            {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                       "Unable to allocate memory for f->dsp");
                return status;
            }

            strcpy(f->dsp, (char *) value);

        } else if (STREQ(name, "filter_info")) {

            oldFilterInfo = (parameter_t *)handel_md_alloc(f->numFilter * sizeof(parameter_t));

            for (i = 0; i < f->numFilter; i++)
            {
                oldFilterInfo[i] = f->filterInfo[i];
            }

            handel_md_free((void *)f->filterInfo);
            f->filterInfo = NULL;

            f->numFilter++;
            f->filterInfo = (parameter_t *)handel_md_alloc(f->numFilter * sizeof(parameter_t));

            for (i = 0; i < (unsigned short)(f->numFilter - 1); i++)
            {
                f->filterInfo[i] = oldFilterInfo[i];
            }
            f->filterInfo[f->numFilter - 1] = *((parameter_t *)value);

            handel_md_free((void *)oldFilterInfo);
            oldFilterInfo = NULL;

        } else {

            status = XIA_BAD_NAME;
            xiaLog(XIA_LOG_ERROR, status, "xiaSetFirmwareItem",
                   "Invalid name %s.", name);
            return status;
        }
    }

    return status;
}



/*****************************************************************************
 *
 * This routine removes a Firmware entry
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaRemoveFirmware(const char *alias)
{
    int status = XIA_SUCCESS;

    unsigned int i;

    char strtemp[MAXALIAS_LEN];

    FirmwareSet *prev = NULL;
    FirmwareSet *current = NULL;
    FirmwareSet *next = NULL;

    Firmware* firmware = NULL;

    if (isListEmpty(xiaFirmwareSetHead))
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveFirmware",
               "Alias %s does not exist", alias);
        return status;
    }

    /* Turn the alias into lower case version, and terminate with a null char */
    for (i = 0; i < (unsigned int)strlen(alias); i++)
    {
        strtemp[i] = (char)tolower((int)alias[i]);
    }
    strtemp[strlen(alias)] = '\0';

    /* First check if this alias exists already? */
    prev = NULL;
    current = xiaFirmwareSetHead;
    next = current->next;

    while (!STREQ(strtemp, current->alias) && (next != NULL))
    {
        prev = current;
        current = next;
        next = current->next;
    }

    /* Check if we found nothing */
    if ((next == NULL) && (current == NULL))
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveFirmware",
               "Alias %s does not exist.", alias);
        return status;
    }

    /* Check if match is the head of the list */
    if (current == xiaFirmwareSetHead)
    {
        xiaFirmwareSetHead = next;

    } else {

        prev->next = next;
    }

    if (current->alias != NULL)
    {
        handel_md_free(current->alias);
    }
    if (current->filename != NULL)
    {
        handel_md_free(current->filename);
    }
    if (current->mmu != NULL)
    {
        handel_md_free(current->mmu);
    }
    if (current->tmpPath != NULL)
    {
        handel_md_free(current->tmpPath);
    }

    /* Loop over the Firmware information, deallocating memory */
    firmware = current->firmware;
    while (firmware != NULL)
    {
        Firmware* nextFirmware = firmware->next;

        if (firmware->dsp != NULL)
        {
            handel_md_free((void *)firmware->dsp);
        }
        if (firmware->fippi != NULL)
        {
            handel_md_free((void *)firmware->fippi);
        }
        if (firmware->user_fippi != NULL)
        {
            handel_md_free((void *)firmware->user_fippi);
        }

        handel_md_free((void *)firmware->filterInfo);

        handel_md_free((void *)firmware);

        firmware = nextFirmware;
    }

    for (i = 0; i < current->numKeywords; i++)
    {
        handel_md_free((void *)current->keywords[i]);
    }

    handel_md_free((void *)current->keywords);

    /* Free the FirmwareSet structure */
    handel_md_free(current);

    return status;
}


/*****************************************************************************
 *
 * This routine removes all Firmware entries
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaRemoveAllFirmware(void)
{
    while (xiaFirmwareSetHead != NULL)
    {
        xiaRemoveFirmware(xiaFirmwareSetHead->alias);
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine returns the entry of the Firmware linked list that matches
 * the alias.  If NULL is returned, then no match was found.
 *
 *****************************************************************************/
HANDEL_SHARED FirmwareSet* HANDEL_API xiaFindFirmware(const char *alias)
{
    unsigned int i;

    char strtemp[MAXALIAS_LEN];

    FirmwareSet *current = NULL;


    /* Turn the alias into lower case version, and terminate with a null char */
    for (i = 0; i < (unsigned int)strlen(alias); i++)
    {
        strtemp[i] = (char)tolower((int)alias[i]);
    }
    strtemp[strlen(alias)] = '\0';

    /* First check if this alias exists already? */
    current= xiaFirmwareSetHead;
    while (current != NULL)
    {
        /* If the alias matches, return a pointer to the detector */
        if (STREQ(strtemp, current->alias))
        {
            return current;
        }

        current = current->next;
    }

    return NULL;
}


/*****************************************************************************
 *
 * Searches the Firmware LL and returns TRUE if the specified PTTR already
 * is in the list.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaIsPTRRFree(Firmware *firmware, unsigned short pttr)
{
    while (firmware != NULL)
    {
        if (firmware->ptrr == pttr)
        {
            return FALSE_;
        }

        firmware = firmware->next;
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine returns the number of firmware in a Firmware LL
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetNumFirmware(Firmware *firmware)
{
    int numFirm;

    Firmware *current = NULL;


    current = firmware;
    numFirm = 0;

    while (current != NULL)
    {
        numFirm++;
        current = current->next;
    }

    return numFirm;
}


/*****************************************************************************
 *
 * This routine compares two Firmware elements (actually, the min peaking time
 * value of each) and returns 1 if key1 > key2, 0 if key1 == key2, and -1 if
 * key1 < key2.
 *
 *****************************************************************************/
HANDEL_SHARED int xiaFirmComp(const void *key1, const void *key2)
{
    const Firmware *key1_f = (const Firmware *)key1;
    const Firmware *key2_f = (const Firmware *)key2;


    if (key1_f->min_ptime > key2_f->min_ptime)
    {
        return 1;

    } else if (key1_f->min_ptime == key2_f->min_ptime) {

        return 0;

    } else if (key1_f->min_ptime < key2_f->min_ptime) {

        return -1;
    }

    /* Compiler wants this... */
    return 0;
}


/*****************************************************************************
 *
 * This routine returns the name of the dsp code associated with the alias
 * and peakingTime.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetDSPNameFromFirmware(const char *alias,
                                                       double peakingTime,
                                                       char *dspName)
{
    FirmwareSet *current = NULL;

    Firmware *firmware = NULL;

    int status;


    current = xiaFindFirmware(alias);
    if (current == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetDSPNameFromFirmware",
               "Unable to find firmware %s", alias);
        return status;
    }

    firmware = current->firmware;
    while (firmware != NULL)
    {
        if ((peakingTime >= firmware->min_ptime) &&
            (peakingTime <= firmware->max_ptime))
        {
            strcpy(dspName, firmware->dsp);
            return XIA_SUCCESS;
        }

        firmware = getListNext(firmware);
    }

    status = XIA_BAD_VALUE;
    xiaLog(XIA_LOG_ERROR, status, "xiaGetDSPNameFromFirmware",
           "peakingTime %f does not match any of the PTRRs in %s",
           peakingTime, alias);
    return status;
}


/*****************************************************************************
 *
 * This returns the name of the FiPPI code associated with the alias and
 * peaking time.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetFippiNameFromFirmware(const char *alias,
                                                         double peakingTime,
                                                         char *fippiName)
{
    FirmwareSet *current = NULL;

    Firmware *firmware = NULL;

    int status;


    current = xiaFindFirmware(alias);
    if (current == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetFippiNameFromFirmware",
               "Unable to find firmware %s", alias);
        return status;
    }

    firmware = current->firmware;

    while (firmware != NULL)
    {
        if ((peakingTime >= firmware->min_ptime) &&
            (peakingTime <= firmware->max_ptime))
        {
            strcpy(fippiName, firmware->fippi);
            return XIA_SUCCESS;
        }

        firmware = getListNext(firmware);
    }

    status = XIA_BAD_VALUE;
    xiaLog(XIA_LOG_ERROR, status, "xiaGetFippiNameFromFirmware",
           "peakingTime %f does not match any of the PTRRs in %s",
           peakingTime, alias);
    return status;
}


/*****************************************************************************
 *
 * This routine looks to replace the get*FromFirmware() routines by using
 * a generic search based on peakingTime.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetValueFromFirmware(const char *alias,
                                                     double peakingTime,
                                                     const char *name, char *value)
{
    int status;

    FirmwareSet *current = NULL;

    Firmware *firmware = NULL;


    current = xiaFindFirmware(alias);
    if (current == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetValueFromFirmware",
               "Unable to find firmware %s", alias);
        return status;
    }


    if (STREQ(name, "mmu"))
    {
        if (current->mmu == NULL)
        {
            status = XIA_BAD_VALUE;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetValueFromFirmware",
                   "MMU is NULL");
            return status;
        }

        strcpy(value, current->mmu);
        return XIA_SUCCESS;
    }

    /* Hacky way of dealing with the
     * special uDXP FiPPI types.
     */
    if (STREQ(name, "fippi0") ||
        STREQ(name, "fippi1") ||
        STREQ(name, "fippi2"))
    {
        strcpy(value, name);
        return XIA_SUCCESS;
    }

    firmware = current->firmware;
    while (firmware != NULL)
    {
        if ((peakingTime >= firmware->min_ptime) &&
            (peakingTime <= firmware->max_ptime))
        {
            if (STREQ(name, "fippi"))
            {
                if (firmware->fippi == NULL)
                {
                    status = XIA_BAD_VALUE;
                    xiaLog(XIA_LOG_ERROR, status, "xiaGetValueFromFirmware",
                           "FiPPI is NULL");
                    return status;
                }

                strcpy(value, firmware->fippi);

                return XIA_SUCCESS;

            } else if (STREQ(name, "user_fippi")) {

                if (firmware->user_fippi == NULL)
                {
                    status = XIA_BAD_VALUE;
                    xiaLog(XIA_LOG_ERROR, status, "xiaGetValueFromFirmware",
                           "User FiPPI is NULL");
                    return status;
                }

                strcpy(value, firmware->user_fippi);

                return XIA_SUCCESS;

            } else if (STREQ(name, "dsp")) {

                if (firmware->dsp == NULL)
                {
                    status = XIA_BAD_VALUE;
                    xiaLog(XIA_LOG_ERROR, status, "xiaGetValueFromFirmware",
                           "DSP is NULL");
                    return status;
                }

                strcpy(value, firmware->dsp);

                return XIA_SUCCESS;

            }
        }

        firmware = getListNext(firmware);
    }

    status = XIA_BAD_VALUE;
    xiaLog(XIA_LOG_ERROR, status, "xiaGetValueFromFirmware",
           "Error getting %s from %s", name, alias);
    return status;
}

/*****************************************************************************
 *
 * This routine returns the Firmware set and current firmware given a module
 * and detector.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetFirmwareSet(int detChan,
                                               Module* module,
                                               FirmwareSet** firmwareSet,
                                               CurrentFirmware** currentFirmware)
{
    int status;
    int modChan = 0;
    char* firmAlias = NULL;

    if (firmwareSet)
        *firmwareSet = NULL;
    if (currentFirmware)
        *currentFirmware = NULL;

    status = xiaGetAbsoluteChannel(detChan, module, &modChan);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "xiaGetValueFromFirmware",
               "Unable to find detChan %d in module", detChan);
        return status;
    }

    /* The preampGain is passed in directly since the detector alias also has
     * the detector channel information encoded into it.
     */
    firmAlias = module->firmware[modChan];

    if (firmwareSet)
        *firmwareSet = xiaFindFirmware(firmAlias);

    if (currentFirmware)
        *currentFirmware = &module->currentFirmware[modChan];

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine clears the Firmware Set LL
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaInitFirmwareSetDS(void)
{
    xiaFirmwareSetHead = NULL;
    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * Return the detector channel list's head.
 *
 *****************************************************************************/
HANDEL_SHARED FirmwareSet * HANDEL_API xiaGetFirmwareSetHead(void)
{
    return xiaFirmwareSetHead;
}
