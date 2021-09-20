/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 *               2005-2012 XIA LLC
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

#include "xia_assert.h"
#include "xia_handel.h"
#include "xia_handel_structures.h"

#include "handel_generic.h"
#include "handeldef.h"
#include "handel_errors.h"
#include "handel_log.h"


/*
 * Define the head of the Detector list
 */
static Detector *xiaDetectorHead = NULL;

/*
 * Static functions.
 */
HANDEL_STATIC boolean_t HANDEL_API xiaArePolaritiesValid(Detector *detector);
HANDEL_STATIC boolean_t HANDEL_API xiaAreGainsValid(Detector *detector);
HANDEL_STATIC boolean_t HANDEL_API xiaIsTypeValid(Detector *detector);


/** @brief Creates a new detector
 *
 * Create a new detector with the name @a alias that can be reference by other
 * routines such as xiaAddDetectorItem(char *alias, char *name, void *value),
 * xiaGetDetectorItem(char *alias, char *name, void *value),
 * xiaModifyDetectorItem(char *alias, char *name, void *value) and
 * xiaRemoveDetector(char *alias).
 *
 * @param alias Name of the new detector to be added to the system
 *
 * @return An error value indicating success (@a XIA_SUCCESS) or failure
 * (@a XIA_ALIAS_SIZE, @a XIA_ALIAS_EXISTS or @a XIA_NOMEM)
 *
 */
HANDEL_EXPORT int HANDEL_API xiaNewDetector(const char *alias)
{
    int status = XIA_SUCCESS;

    Detector *current=NULL;

    /* If HanDeL isn't initialized, go ahead and call it... */
    if (!isHandelInit)
    {
        status = xiaInitHandel();
        if (status != XIA_SUCCESS)
        {
            fprintf(stderr, "FATAL ERROR: Unable to load libraries.\n");
            exit(XIA_INITIALIZE);
        }

        xiaLog(XIA_LOG_WARNING, "xiaNewDetector", "HanDeL was initialized silently");
    }


    xiaLog(XIA_LOG_DEBUG, "xiaNewDetector",
           "Creating new detector w/ alias = %s", alias);


    if ((strlen(alias) + 1) > MAXALIAS_LEN)
    {
        status = XIA_ALIAS_SIZE;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewDetector",
               "Alias contains too many characters");
        return status;
    }

    /* First check if this alias exists already? */
    current = xiaFindDetector(alias);
    if (current != NULL)
    {
        status = XIA_ALIAS_EXISTS;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewDetector",
               "Alias %s already in use.", alias);
        return status;
    }
    /* Check that the Head of the linked list exists */
    if (xiaDetectorHead == NULL)
    {
        /* Create an entry that is the Head of the linked list */
        xiaDetectorHead = (Detector *) handel_md_alloc(sizeof(Detector));
        current = xiaDetectorHead;
    } else {
        /* Find the end of the linked list */
        current = xiaDetectorHead;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = (Detector *) handel_md_alloc(sizeof(Detector));
        current = current->next;
    }

    /* Make sure memory was allocated */
    if (current == NULL)
    {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewDetector",
               "Unable to allocate memory for Detector alias %s.", alias);
        return status;
    }

    /* Do any other allocations, or initialize to NULL/0 */
    current->alias = (char *) handel_md_alloc((strlen(alias)+1)*sizeof(char));
    if (current->alias == NULL)
    {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewDetector",
               "Unable to allocate memory for current->alias");
        return status;
    }

    strcpy(current->alias,alias);

    current->nchan     = 0;
    current->polarity  = NULL;
    current->gain      = NULL;
    current->type      = XIA_DET_UNKNOWN;
    current->typeValue = NULL;
    current->pslData   = NULL;
    current->next      = NULL;

    return XIA_SUCCESS;
}

/** @brief Adds information to a detector
 *
 * Adds information about the detector setup using the name-value
 * pairs in the detector section of Handel Name-Value Pairs.
 *
 * @param alias A valid detector alias
 *
 * @param name A valid name from the detector section of Handel
 * Name-Value Pairs.
 *
 * @param value Value to set the corresponding detector information to, cast
 * into a <em>void *</em>. See the Usage section below for an example.
 *
 * @return An error value indicating success (@a XIA_SUCCESS) or failure
 * (@a XIA_BAD_VALUE, @a XIA_NO_ALIAS or @a XIA_BAD_NAME)
 *
 * @par Usage:
 * @include xiaAddDetectorItem.c
 *
 */
HANDEL_EXPORT int HANDEL_API xiaAddDetectorItem(const char *alias,
                                                const char *name, void *value)
{
    int status = XIA_SUCCESS;

    unsigned int i, slen;

    long chan = 0;

    unsigned short j;

    char strtemp[MAXALIAS_LEN];

    Detector *chosen = NULL;

    /* Check that the value is not NULL */
    if (value == NULL)
    {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDetectorItem",
               "Value can not be NULL");
        return status;
    }

    /* Locate the Detector entry first */
    chosen = xiaFindDetector(alias);
    if (chosen == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDetectorItem",
               "Alias %s has not been created.", alias);
        return status;
    }

    /* Convert the name to lower case */
    for (i = 0; i < (unsigned int)strlen(name); i++)
    {
        strtemp[i] = (char)tolower((int)name[i]);
    }
    strtemp[strlen(name)] = '\0';

    /* Switch thru the possible entries */
    if (STREQ(strtemp, "number_of_channels"))
    {
        /* Make sure the number of channels is less than max, this is mostly a
         * check for poorly passed type of value...i.e. if value is passed as a
         * double, it will likely look like a bogus number of channels */
        if (*((unsigned short *)value) > MAXDETECTOR_CHANNELS)
        {
            status = XIA_BAD_VALUE;
            xiaLog(XIA_LOG_ERROR, status, "xiaAddDetectorItem",
                   "Number of channels too large: %u.",
                   *((unsigned short*)value));
            return status;
        }
        chosen->nchan = *((unsigned short *) value);
        chosen->polarity  = (unsigned short *)
            handel_md_alloc(chosen->nchan*sizeof(unsigned short));
        chosen->gain = (double *) handel_md_alloc(chosen->nchan*sizeof(double));
        chosen->typeValue = (double *)handel_md_alloc(sizeof(double) * chosen->nchan);

        if ((chosen->polarity  == NULL) ||
            (chosen->gain      == NULL) ||
            (chosen->typeValue == NULL))
        {
            if (chosen->polarity) {
                handel_md_free(chosen->polarity);
                chosen->polarity = NULL;
            }
            if (chosen->gain) {
                handel_md_free(chosen->gain);
                chosen->gain = NULL;
            }
            if (chosen->typeValue) {
                handel_md_free(chosen->typeValue);
                chosen->typeValue = NULL;
            }
            status = XIA_NOMEM;
            xiaLog(XIA_LOG_ERROR, status, "xiaAddDetectorItem",
                   "Unable to allocate memory for detector info");
            return status;
        }

        return XIA_SUCCESS;
    }


    /* The number of channels must be set first before we can do any of the
     * other values. Why? Because memory is allocated once the number of channels
     * is known.
     */
    if (chosen->nchan == 0) {
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDetectorItem",
               "Detector '%s' must set its number of channels before setting '%s'",
               chosen->alias, strtemp);
        return XIA_NO_CHANNELS;
    }

    if (STREQ(strtemp, "type")) {

        if (STREQ((char *)value, "reset"))
        {
            chosen->type = XIA_DET_RESET;

        } else if (STREQ((char *)value, "rc_feedback")) {

            chosen->type = XIA_DET_RCFEED;

        } else {

            status = XIA_BAD_VALUE;
            xiaLog(XIA_LOG_ERROR, status, "xiaAddDetectorItem",
                   "Error setting detector type for %s", chosen->alias);
            return status;
        }

    } else if (STREQ(strtemp, "type_value")) {

        /* This, effectively, constrains us to a "single" det type value for
         * now.  If that isn't good enough for some customers, we can upgrade
         * it at a later date to allow for each channel to be set individually.
         */
        for (j = 0; j < chosen->nchan; j++)
        {
            chosen->typeValue[j] = *((double *)value);
        }

    } else if (strncmp(strtemp, "channel", 7) == 0) {
        /* Determine the quantity to define: gain or polarity */
        slen = (unsigned int)strlen(strtemp);
        if (STREQ(strtemp + slen - 5, "_gain"))
        {
            strtemp[slen - 5] = ' ';
            chan = atol(strtemp + 7);
            /* Now store the value for the gain if the channel number is
             * value */
            if (chan >= (long)chosen->nchan)
            {
                status = XIA_BAD_VALUE;
                xiaLog(XIA_LOG_ERROR, status, "xiaAddDetectorItem",
                       "Channel number invalid for %s.", name);
                return status;
            }

            chosen->gain[chan] = *((double *) value);

        } else if (STREQ(strtemp + slen - 9, "_polarity")) {
            /* Now get the channel number, insert space, then use the standard
             * C routine */
            strtemp[slen - 9] = ' ';
            chan = atol(strtemp + 7);
            /* Now store the value for the gain if the channel number is
             * value */
            if (chan >= (long)chosen->nchan)
            {
                status = XIA_BAD_VALUE;
                xiaLog(XIA_LOG_ERROR, status, "xiaAddDetectorItem",
                       "Channel number invalid for %s.", name);
                return status;
            }

            if ((STREQ((char *) value, "pos")) ||
                (STREQ((char *) value, "+"))   ||
                (STREQ((char *) value, "positive")))
            {
                chosen->polarity[chan] = 1;

            } else if ((STREQ((char *) value, "neg")) ||
                       (STREQ((char *) value, "-"))   ||
                       (STREQ((char *) value, "negative")))
            {
                chosen->polarity[chan] = 0;

            } else {

                status = XIA_BAD_VALUE;
                xiaLog(XIA_LOG_ERROR, status, "xiaAddDetectorItem",
                       "Invalid polarity %s.", (char *) value);
                return status;
            }

        } else {

            status = XIA_BAD_NAME;
            xiaLog(XIA_LOG_ERROR, status, "xiaAddDetectorItem",
                   "Invalid name %s.", name);
            return status;
        }

    } else {

        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDetectorItem",
               "Invalid name %s.", name);
        return status;
    }

    return XIA_SUCCESS;
}


/** @brief Modify the detector information
 *
 * Modify a subset of the total detector information. The allowed name-value
 * pairs that can be modified are @a channel{n}_gain, @a channel{n}_polarity and
 * @a type_value. Note: If xiaStartSystem(void) has already been called,
 * then it must be called again after this routine.
 *
 * @param alias A valid detector alias
 *
 * @param name A valid name from the detector items listed above
 *
 * @param value Value to set the corresponding detector information to, cast
 * into a <em>void *</em>. See the Usage section below for an example.
 *
 * @return An error value indicating success (@a XIA_SUCCESS) or failure
 * (@a XIA_BAD_VALUE, @a XIA_BAD_NAME or @a XIA_NO_ALIAS)
 *
 * @par Usage:
 * @include xiaModifyDetectorItem.c
 *
 */
HANDEL_EXPORT int HANDEL_API xiaModifyDetectorItem(const char *alias,
                                                   const char *name, void *value)
{
    int status = XIA_SUCCESS;

    unsigned int i, slen;
    char strtemp[MAXALIAS_LEN];


    if (alias == NULL) {
        status = XIA_NULL_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyDetectorItem",
               "Alias can not be NULL");
        return status;
    }

    /* Check that the value is not NULL */
    if (value == NULL)
    {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyDetectorItem",
               "Value can not be NULL");
        return status;
    }

    /* convert the name to lower case */
    for (i = 0; i < (unsigned int)strlen(name); i++)
    {
        strtemp[i] = (char)tolower((int)name[i]);
    }
    strtemp[strlen(name)] = '\0';

    if (strncmp(strtemp, "channel", 7) == 0)
    {
        /* Make sure that the name is either gain or polarity, no others are
         * allowed modification. Addendum: type_value is also allowed now. See
         * BUG ID #58. */
        slen = (unsigned int)strlen(strtemp);
        if ((STREQ(strtemp + slen - 5, "_gain")) ||
            (STREQ(strtemp + slen - 9,"_polarity")))
        {
            /* Try to modify the value */
            status = xiaAddDetectorItem(alias, name, value);

            if (status != XIA_SUCCESS)
            {
                xiaLog(XIA_LOG_ERROR, status, "xiaModifyDetectorItem",
                       "Unable to modify detector value");
                return status;
            }

            return status;
        }
    }

    if (STREQ(strtemp, "type_value")) {

        status = xiaAddDetectorItem(alias, name, value);

        if (status != XIA_SUCCESS) {

            xiaLog(XIA_LOG_ERROR, status, "xiaModifyDetectorItem",
                   "Unable to modify detector value");
            return status;
        }

        return XIA_SUCCESS;
    }

    /* Not a valid name to modify */
    status = XIA_BAD_NAME;
    xiaLog(XIA_LOG_ERROR, status, "xiaModifyDetectorItem",
           "Can not modify the name:%s", name);
    return status;
}


/** @brief Retrieve current information from detector configuration
 *
 * Retrieve current information from detector configuration. All of the names
 * listed in the detector section of Handel Name-Value Pairs
 * may be retrieved using this routine.
 *
 * @param alias A valid detector alias
 *
 * @param name A valid name from the detector section Handel
 * Name-Value Pairs.
 *
 * @param value Value to store the retrieved value in cast into a
 * <em>void *</em>. See the Usage section below for an example.
 *
 * @return An error value indicating success (@a XIA_SUCCESS) or failure
 * (@a XIA_NO_ALIAS, @a XIA_BAD_VALUE or @a XIA_BAD_NAME)
 *
 * @par Usage:
 * @include xiaGetDetectorItem.c
 *
 */
HANDEL_EXPORT int HANDEL_API xiaGetDetectorItem(const char *alias,
                                                const char *name, void *value)
{
    int status = XIA_SUCCESS;
    int idx;

    unsigned int i;

    char *sidx;

    char strtemp[MAXALIAS_LEN];

    Detector *chosen = NULL;

    /* Try and find the alias */
    chosen = xiaFindDetector(alias);
    if (chosen == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetDetectorItem",
               "Alias %s has not been created.", alias);
        return status;
    }

    /* Convert name to lowercase */
    for (i = 0; i < (unsigned int)strlen(name); i++)
    {
        strtemp[i] = (char)tolower((int)name[i]);
    }
    strtemp[strlen(name)] = '\0';

    /* Decide which data should be returned */
    if (STREQ(name, "number_of_channels"))
    {

        *((unsigned short *)value) = chosen->nchan;

    } else if (STREQ(name, "type")) {

        switch (chosen->type)
        {
            case XIA_DET_RESET:
                strcpy((char *)value, "reset");
                break;

            case XIA_DET_RCFEED:
                strcpy((char *)value, "rc_feedback");
                break;

            default:
            case XIA_DET_UNKNOWN:
                status = XIA_BAD_VALUE;
                xiaLog(XIA_LOG_ERROR, status, "xiaGetDetectorItem",
                       "Detector %s currently is not assigned a valid type",
                       chosen->alias);
                return status;
        }

    } else if (STREQ(name, "type_value")) {

        /* Since all values are the same for the typeValue, this is an
         * acceptable thing to do.
         */
        *((double *)value) = chosen->typeValue[0];

    } else if (strncmp(name, "channel", 7) == 0) {
        /* Is it a gain or a polarity? */
        /* The #-may-be-greater-then-one-digit hack */
        sidx = strchr(strtemp, '_');
        sidx[0] = '\0';
        idx = atoi(&strtemp[7]);

        /* Sanity Check: This *is* a valid channel, right?? */
        if (idx >= (int)chosen->nchan)
        {
            status = XIA_BAD_VALUE;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetDetectorItem",
                   "Channel #: %d is invalid for %s", idx, name);
            return status;
        }

        if (STREQ(sidx + 1, "gain"))
        {
            *((double *)value) = chosen->gain[idx];

        } else if (STREQ(sidx + 1, "polarity")) {

            switch (chosen->polarity[idx]) {
                default:
                    /* Bad Trouble */
                    status = XIA_BAD_VALUE;
                    xiaLog(XIA_LOG_ERROR, status, "xiaGetDetectorItem",
                           "Internal polarity value inconsistient");
                    return status;
                case 0:
                    strcpy((char *)value, "neg");
                    break;
                case 1:
                    strcpy((char *)value, "pos");
                    break;
            }

        } else {

            status = XIA_BAD_NAME;
            xiaLog(XIA_LOG_ERROR, status, "xiaGetDetectorItem",
                   "Invalid name: %s", name);
            return status;
        }

    } else {

        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetDetectorItem",
               "Invalid name: %s", name);
        return status;
    }

    return status;
}


/**********
 * This routine returns the total # of detectors currently
 * defined in the system. Unfortunately, our design doesn't
 * currently have a LL-manager for the Detector LL, so we
 * have to do this the hard (and slow way). Luckily, we
 * have encapsulated all of this in our routine so that if we
 * change to a smarter LL-manager no one will be the wiser.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetNumDetectors(unsigned int *numDetectors)
{
    unsigned int count = 0;

    Detector *current = xiaDetectorHead;


    while (current != NULL) {

        count++;
        current = getListNext(current);
    }

    *numDetectors = count;

    return XIA_SUCCESS;
}


/**********
 * This routine returns a list of the detector aliases
 * that are currently defined in the system. Assumes that the user
 * has already allocated enough memory in "detectors".
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetDetectors(char *detectors[])
{
    int i;

    Detector *current = xiaDetectorHead;


    for (i = 0; current != NULL; current = getListNext(current), i++) {

        strcpy((char*) detectors[i], current->alias);
    }

    return XIA_SUCCESS;
}


/**********
 * This routine is similar to xiaGetDetectors() but only
 * returns a single detector since VB can't pass string
 * arrays to a DLL.
 **********/
HANDEL_EXPORT int HANDEL_API xiaGetDetectors_VB(unsigned int index, char *alias)
{
    int status;

    unsigned int curIdx;

    Detector *current = xiaDetectorHead;


    for (curIdx = 0; current != NULL; current = getListNext(current), curIdx++) {

        if (curIdx == index) {

            strcpy(alias, current->alias);

            return XIA_SUCCESS;
        }
    }

    status = XIA_BAD_INDEX;
    xiaLog(XIA_LOG_ERROR, status, "xiaGetDetectors_VB",
           "Index = %u is out of range for the detectors list", index);
    return status;
}


/*****************************************************************************
 *
 * This routine sets up all detectors.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaSetupDetectors(void)
{
    int status;

    DetChanElement *current = NULL;

    /* Set polarity and reset time from info in detector struct */
    current = xiaGetDetChanHead();

    while (current != NULL) {
        switch (xiaGetElemType(current->detChan)) {
            case SET:
                /* Skip SETs since all SETs are composed of SINGLES */
                break;

            case SINGLE:

                status = xiaSetupDetectorChannel(current->detChan);

                if (status != XIA_SUCCESS) {
                    xiaLog(XIA_LOG_ERROR, status, "xiaSetupDetectors",
                           "Unable to complete user setup for detChan %d.",
                           current->detChan);
                    return status;
                }

                break;

            case 999:
                xiaLog(XIA_LOG_ERROR, XIA_INVALID_DETCHAN, "xiaSetupDetectors",
                       "detChan %d is not valid.", current->detChan);
                return XIA_INVALID_DETCHAN;

            default:
                FAIL();
                break;
        }

        current = getListNext(current);
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine ends all detectors.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaEndDetectors(void)
{
    int status;

    DetChanElement *current = NULL;

    boolean_t rescan = TRUE_;

    current = xiaGetDetChanHead();

    while (rescan) {
        rescan = FALSE_;

        while (current != NULL) {
            switch (xiaGetElemType(current->detChan)) {
                case SET:
                    /* Skip SETs since all SETs are composed of SINGLES */
                    break;

                case SINGLE:

                    status = xiaEndDetectorChannel(current->detChan);

                    if (status != XIA_SUCCESS) {
                        xiaLog(XIA_LOG_ERROR, status, "xiaEndDetector",
                               "Unable to end detector for detChan %d.",
                               current->detChan);
                        return status;
                    }

                    rescan = TRUE_;
                    break;

                case 999:
                    xiaLog(XIA_LOG_ERROR, XIA_INVALID_DETCHAN, "xiaEndDetectors",
                           "detChan %d is not valid.", current->detChan);
                    return XIA_INVALID_DETCHAN;

                default:
                    FAIL();
                    break;
            }

            if (rescan)
                break;

            current = getListNext(current);
        }
    }

    return XIA_SUCCESS;
}

/** @brief Setup a detector given the detector alias.
 *
 *
 */
HANDEL_SHARED int HANDEL_API xiaSetupDetector(const char* alias)
{
    int status;

    Module* chanModule = NULL;
    Detector* chanDetector = NULL;

    DetChanElement *current = NULL;

    Detector* detector = xiaFindDetector(alias);

    if (detector == NULL) {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaSetupDetector",
               "Unable to find detector alias %s.", alias);
        return status;
    }

    current = xiaGetDetChanHead();

    while (current != NULL) {
        switch (xiaGetElemType(current->detChan)) {
            case SET:
                /* Skip SETs since all SETs are composed of SINGLES */
                break;

            case SINGLE:

                status = xiaFindModuleAndDetector(current->detChan,
                                                  &chanModule,
                                                  &chanDetector);
                if (status != XIA_SUCCESS) {
                    xiaLog(XIA_LOG_ERROR, status, "xiaSetupDetector",
                           "Unable to find the module and detector for detChan %d.",
                           current->detChan);
                    return status;
                }

                if (detector == chanDetector) {
                    status = xiaSetupDetectorChannel(current->detChan);
                    if (status != XIA_SUCCESS) {
                        xiaLog(XIA_LOG_ERROR, status, "xiaSetupDetector",
                               "Detector setup for detChan %d failed.",
                               current->detChan);
                        return status;
                    }
                }
                break;

            case 999:
                xiaLog(XIA_LOG_ERROR, XIA_INVALID_DETCHAN, "xiaSetupDetector",
                       "detChan %d is not valid.", current->detChan);
                return XIA_INVALID_DETCHAN;

            default:
                FAIL();
                break;
        }

        current = getListNext(current);
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine loops over all of the elements of the Detector LL and checks
 * that the data is valid. In the case of misconfiguration, the returned error
 * code should indicate what part of the Detector is invalid.
 *
 * The current logic is as follows:
 * 1) Check that all polarities are valid from 0...nchan - 1
 * 2) Check that all gains are within a valid range from 0...nchan - 1
 * 3) Check that the type is defined beyond XIA_DET_UNKNOWN
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaValidateDetector(void)
{
    int status;

    Detector *current = NULL;

    current = xiaGetDetectorHead();

    while (current != NULL)
    {
        if (!xiaArePolaritiesValid(current))
        {
            status = XIA_MISSING_POL;
            xiaLog(XIA_LOG_ERROR, status, "xiaValidateDetector",
                   "Missing polarity in alias %s\n", current->alias);
            return status;
        }

        if (!xiaAreGainsValid(current))
        {
            status = XIA_MISSING_GAIN;
            xiaLog(XIA_LOG_ERROR, status, "xiaValidateDetector",
                   "Missing gain in alias %s\n", current->alias);
            return status;
        }

        if (!xiaIsTypeValid(current))
        {
            status = XIA_MISSING_TYPE;
            xiaLog(XIA_LOG_ERROR, status, "xiaValidateDetector",
                   "Missing type in alias %s\n", current->alias);
            return status;
        }

        current = getListNext(current);
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine verifies that all of the polarities in detector have a valid
 * value. (Essentially, all that this *really* checks is that all of the
 * values in the polarity array are set to something. This is the case
 * because when the polarity is set via a call to xiaAddDetectorItem(), the
 * value is verified, therefore the value can only be out-of-range if it
 * hasn't been set yet.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaArePolaritiesValid(Detector *detector)
{
    unsigned int i;

    for (i = 0; i < detector->nchan; i++)
    {
        if ((detector->polarity[i] != 1) &&
            (detector->polarity[i] != 0))
        {
            return FALSE_;
        }
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine verifies that all of the gains in detector are within a valid
 * range. This routine has the same caveats as xiaArePolaritiesValid() in the
 * sense that the value should really only be out-of-range if it hasn't been
 * set yet.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaAreGainsValid(Detector *detector)
{
    unsigned int i;

    for (i = 0; i < detector->nchan; i++)
    {
        if ((detector->gain[i] < XIA_GAIN_MIN) ||
            (detector->gain[i] > XIA_GAIN_MAX))
        {
            return FALSE_;
        }
    }

    return TRUE_;
}


/*****************************************************************************
 *
 * This routine verifies that the type isn't XIA_DET_UNKNOWN (since that is
 * what it is initialized to.
 *
 *****************************************************************************/
HANDEL_STATIC boolean_t HANDEL_API xiaIsTypeValid(Detector *detector)
{
    if (detector->type == XIA_DET_UNKNOWN)
    {
        return FALSE_;
    }

    return TRUE_;
}


/** @brief Setup a detector given the detector channel.
 *
 *
 */
HANDEL_SHARED int HANDEL_API xiaSetupDetectorChannel(int detChan)
{
    int status;

    Module* module;
    Detector* detector;

    status = xiaFindModuleAndDetector(detChan, &module, &detector);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "xiaSetupDetectorChannel",
               "Detector setup failed for detChan %u (unable to find module and/or detector).",
               detChan);
        return status;
    }

    if (module->psl != NULL) {
        status = module->psl->setupDetChan(detChan, detector, module);

        if (status != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, status, "xiaSetupDetectorChannel",
                   "Detector setup failed for detChan %u.", detChan);
            return status;
        }

        status = module->psl->setDetectorTypeValue(detChan, detector);

        if (status != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, status, "xiaSetupDetectorChannel",
                   "Detector setup failed for detChan %u.", detChan);
            return status;
        }

        status = module->psl->userSetup(detChan, detector, module);

        if (status != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, status, "xiaSetupDetectorChannel",
                   "Detector setup failed for detChan %u.", detChan);
            return status;
        }
    }

    return XIA_SUCCESS;
}


/** @brief End a detector given a detector alias.
 *
 *
 */
HANDEL_SHARED int HANDEL_API xiaEndDetector(const char* alias)
{
    int status;

    Module* chanModule = NULL;
    Detector* chanDetector = NULL;

    DetChanElement *current = NULL;

    Detector* detector = xiaFindDetector(alias);

    if (detector == NULL) {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaENdDetector",
               "Unable to find detector alias %s.", alias);
        return status;
    }

    /*
     * Find all the channels on the detector and end them. The detector
     * channel list is not touched so the iterator is always valid.
     */

    current = xiaGetDetChanHead();

    while (current != NULL) {
        switch (xiaGetElemType(current->detChan)) {
            case SET:
                /* Skip SETs since all SETs are composed of SINGLES */
                break;

            case SINGLE:

                status = xiaFindModuleAndDetector(current->detChan,
                                                  &chanModule,
                                                  &chanDetector);
                if (status == XIA_SUCCESS) {
                    if (detector == chanDetector) {
                        status = xiaEndDetectorChannel(current->detChan);
                        if (status != XIA_SUCCESS) {
                            xiaLog(XIA_LOG_ERROR, status, "xiaEndDetector",
                                   "Detector end for detChan %d failed.",
                                   current->detChan);
                                return status;
                        }
                    }
                }
                break;

            case 999:
                xiaLog(XIA_LOG_ERROR, XIA_INVALID_DETCHAN, "xiaEndDetector",
                       "detChan %d is not valid.", current->detChan);
                return XIA_INVALID_DETCHAN;

            default:
                FAIL();
                break;
        }

        current = getListNext(current);
    }

    return XIA_SUCCESS;
}


/** @brief End a detector given the detector channel.
 *
 *
 */
HANDEL_SHARED int HANDEL_API xiaEndDetectorChannel(int detChan)
{
    int status;

    Module* module;
    Detector* detector;

    status = xiaFindModuleAndDetector(detChan, &module, &detector);

    if (status != XIA_SUCCESS) {
        xiaLog(XIA_LOG_ERROR, status, "xiaEndDetectorChannel",
               "Detector end failed for detChan %u (unable to find module and/or detector).",
               detChan);
        return status;
    }

    if (module->psl != NULL) {
        status = module->psl->endDetChan(detChan, detector, module);

        if (status != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, status, "xiaEndDetectorChannel",
                   "Detector end failed for detChan %u", detChan);
            return status;
        }

        if (detector->pslData != NULL) {
            xiaLog(XIA_LOG_ERROR, status, "xiaEndDetectorChannel",
                   "Detector end did not clear PSL data for detChan %u", detChan);
        }
    }

    return XIA_SUCCESS;
}

/** @brief Deletes a detector from the system
 *
 * @param detector Pointer to the detector.
 *
 * @return An error value indicating success (@a XIA_SUCCESS) or failure
 * (@a XIA_NO_ALIAS)
 *
 */
HANDEL_STATIC int HANDEL_API xiaDeleteDetector(Detector* detector)
{
    int status = XIA_SUCCESS;

    Detector *prev    = NULL;
    Detector *current = NULL;
    Detector *next    = NULL;

    if (isListEmpty(xiaDetectorHead))
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaDeleteDetector",
               "Alias %s does not exist", detector->alias);
        return status;
    }

    /* First check if this alias exists already? */
    prev = NULL;
    current = xiaDetectorHead;
    next = current->next;
    while (detector != current)
    {
        /* Move to the next element */
        prev = current;
        current = next;
        next = current->next;
    }

    /* Check if we found nothing */
    if ((next==NULL) &&
        (current==NULL))
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaDeleteDetector",
               "Detector %s does not exist.", detector->alias);
        return status;
    }

    /* Free up the memory associated with this element */
    if (current->pslData != NULL)
    {
        status = xiaEndDetector(current->alias);
        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaDeleteDetector",
                   "Detector end failure");
            return status;
        }
    }

    /* Check if match is the head of the list */
    if (current == xiaDetectorHead)
    {
        /* Move the next element into the Head position. */
        xiaDetectorHead = next;

    } else {
        /* Element is inside the list, change the pointers to skip the matching
         * element
         */
        prev->next = next;
    }

    if (current->alias != NULL)
    {
        handel_md_free(current->alias);
    }
    if (current->polarity != NULL)
    {
        handel_md_free(current->polarity);
    }
    if (current->gain != NULL)
    {
        handel_md_free(current->gain);
    }

    handel_md_free(current->typeValue);

    /* Free the Board_Info structure */
    handel_md_free(current);

    return status;
}

/** @brief Removes a detector from the system
 *
 * @param alias A valid detector alias
 *
 * @return An error value indicating success (@a XIA_SUCCESS) or failure
 * (@a XIA_NO_ALIAS)
 *
 * @par Usage:
 * @include xiaRemoveDetector.c
 *
 */
HANDEL_EXPORT int HANDEL_API xiaRemoveDetector(const char *alias)
{
    Detector* detector = xiaFindDetector(alias);

    xiaLog(XIA_LOG_INFO, "xiaRemoveDetector",
           "Removing %s", alias);

   if (detector == NULL) {
        int status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveDetector",
               "Alias %s does not exist.", alias);
        return status;

    }

    return xiaDeleteDetector(detector);
}

/** @brief Remove all detectors
 *
 * @return An error value indicating success @a XIA_SUCCESS
 */
HANDEL_EXPORT int HANDEL_API xiaRemoveAllDetectors(void)
{
    while (xiaDetectorHead != NULL)
    {
        xiaDeleteDetector(xiaDetectorHead);
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine returns the entry of the Detector linked list that matches
 * the alias.  If NULL is returned, then no match was found.
 *
 *****************************************************************************/
HANDEL_SHARED Detector* HANDEL_API xiaFindDetector(const char *alias)
{
    int i;

    char strtemp[MAXALIAS_LEN];

    Detector *current = NULL;

    /* Turn the alias into lower case version, and terminate with a null
     * char */
    for (i = 0; i < (int)strlen(alias); i++)
    {
        strtemp[i] = (char)tolower((int)alias[i]);
    }
    strtemp[strlen(alias)] = '\0';

    /* First check if this alias exists already? */
    current = xiaDetectorHead;
    while (current != NULL)
    {
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
 * This routine clears the Detector LL
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaInitDetectorDS(void)
{
    xiaDetectorHead = NULL;
    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * Return the detector link head.
 *
 *****************************************************************************/
HANDEL_SHARED Detector * HANDEL_API xiaGetDetectorHead(void)
{
    return xiaDetectorHead;
}
