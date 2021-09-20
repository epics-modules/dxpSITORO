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

#include "xia_assert.h"
#include "xia_handel.h"
#include "xia_handel_structures.h"
#include "xia_common.h"

#include "handel_errors.h"
#include "handel_log.h"

/*
 * This is the head of the DetectorChannel LL
 */
static DetChanElement *xiaDetChanHead = NULL;


HANDEL_STATIC DetChanSetElem* HANDEL_API xiaGetDetSetTail(DetChanSetElem *head);
HANDEL_STATIC int HANDEL_API xiaAddToExistingSet(int detChan, int newChan);


/*****************************************************************************
 *
 * This routine searches through the DetChanElement linked-list and returns
 * TRUE_ if the specified detChan # ISN'T used yet; FALSE_ otherwise.
 *
 *****************************************************************************/
HANDEL_SHARED boolean_t HANDEL_API xiaIsDetChanFree(int detChan)
{
    DetChanElement *current;

    current = xiaDetChanHead;

    if (isListEmpty(current))
    {
        return TRUE_;
    }

    while (current != NULL)
    {
        if (current->detChan == detChan)
        {
            return FALSE_;
        }

        current = current->next;
    }

    return TRUE_;
}

/*****************************************************************************
 *
 * This routine adds a new (valid) DetChanElement. IT ASSUMES THAT THE DETCHAN
 * VALUE HAS ALREADY BEEN VALIDATED, PREFERABLY BY CALLING xiaDetChanFree().
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaAddDetChan(int type, int detChan, void *data)
{
    int status;
    size_t len;

    DetChanSetElem *newSetElem    = NULL;
    DetChanSetElem *tail          = NULL;
    DetChanSetElem *masterTail    = NULL;

    DetChanElement *current       = NULL;
    DetChanElement *newDetChan    = NULL;
    DetChanElement *masterDetChan = NULL;

    /* The general strategy here is to walk to the end of the list and insert a
     * new element. This includes allocating memory and such.
     */

    if (data == NULL)
    {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDetChan",
               "detChan data is NULL");
        return status;
    }

    if ((type != SINGLE) &&
        (type != SET))
    {
        status = XIA_BAD_TYPE;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDetChan",
               "Specified DetChanElement type is invalid");
        return status;
    }

    newDetChan = (DetChanElement *)handel_md_alloc(sizeof(DetChanElement));
    if (newDetChan == NULL)
    {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDetChan",
               "Not enough memory to create detChan %u", detChan);
        return status;
    }

    newDetChan->type = type;
    newDetChan->detChan = detChan;
    newDetChan->isTagged = FALSE_;
    newDetChan->next = NULL;

    current = xiaDetChanHead;

    if (isListEmpty(current))
    {
        xiaDetChanHead = newDetChan;
        current = xiaDetChanHead;

    } else {

        while (current->next != NULL)
        {
            current = current->next;
        }

        current->next = newDetChan;
    }


    switch (type)
    {
        case SINGLE:
            len = strlen((char *)data);
            newDetChan->data.modAlias =(char *)handel_md_alloc((len + 1) * sizeof(char));
            if (newDetChan->data.modAlias == NULL)
            {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "xiaAddDetChan",
                       "Not enough memory to create newDetChan->data.modAlias");
                return status;
            }

            strcpy(newDetChan->data.modAlias, (char *)data);

            /* Now add it to the -1 list:
             *
             * Does -1 already exist?
             */
            if (xiaIsDetChanFree(-1)) {

                xiaLog(XIA_LOG_INFO, "xiaAddDetChan", "Creating master detChan");

                masterDetChan = (DetChanElement *)handel_md_alloc(sizeof(DetChanElement));

                if (masterDetChan == NULL) {

                    status = XIA_NOMEM;
                    xiaLog(XIA_LOG_ERROR, status, "xiaAddDetChan",
                           "Not enough memory to create the master detChan list");
                    return status;
                }

                masterDetChan->type            = SET;
                masterDetChan->next            = NULL;
                masterDetChan->isTagged        = FALSE_;
                masterDetChan->data.detChanSet = NULL;
                masterDetChan->detChan         = -1;

                /* List cannot be empty thanks to check above */
                while (current->next != NULL) {

                    current = current->next;
                }

                current->next = masterDetChan;

                xiaLog(XIA_LOG_DEBUG, "xiaAddDetChan",
                       "(masterDetChan) current->next = %p", (void *)current->next);

            } else {

                masterDetChan = xiaGetDetChanPtr(-1);
            }

            xiaLog(XIA_LOG_DEBUG, "xiaAddDetChan",
                   "masterDetChan = %p", (void *)masterDetChan);

            newSetElem = (DetChanSetElem *)handel_md_alloc(sizeof(DetChanSetElem));

            if (newSetElem == NULL) {

                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "xiaAddDetChan",
                       "Not enough memory to add channel to master detChan list");
                return status;
            }

            newSetElem->next = NULL;
            newSetElem->channel = detChan;

            masterTail = xiaGetDetSetTail(masterDetChan->data.detChanSet);

            xiaLog(XIA_LOG_DEBUG, "xiaAddDetChan",
                   "masterTail = %p", (void *)masterTail);

            if (masterTail == NULL) {

                masterDetChan->data.detChanSet = newSetElem;

            } else {

                masterTail->next = newSetElem;
            }

            break;

        case SET:
            newDetChan->data.detChanSet = NULL;

            newSetElem = (DetChanSetElem *)handel_md_alloc(sizeof(DetChanSetElem));
            if (newSetElem == NULL)
            {
                status = XIA_NOMEM;
                xiaLog(XIA_LOG_ERROR, status, "xiaAddDetChan",
                       "Not enough memory to create detChan set");
                return status;
            }

            newSetElem->next = NULL;
            newSetElem->channel = *((int *)data);

            tail = xiaGetDetSetTail(newDetChan->data.detChanSet);

            /* May be the first element */
            if (tail == NULL)
            {
                newDetChan->data.detChanSet = newSetElem;

            } else {

                tail->next = newSetElem;
            }

            break;

        default:
            /* Should NEVER get here...but that's no excuse for not putting
             * the default case in.
             */
            status = XIA_BAD_TYPE;
            xiaLog(XIA_LOG_ERROR, status, "xiaAddDetChan",
                   "Specified DetChanElement type is invalid. Should not be seeing this!");
            return status;
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine removes an element from the DetChanElement LL. The detChan
 * value doesn't even need to be valid since (worst-case) the routine will
 * search the whole list and return an error if it doesn't find it.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaRemoveDetChan(int detChan)
{
    int status;

    DetChanElement *current;
    DetChanElement *prev;
    DetChanElement *next;

    current = xiaDetChanHead;

    if (isListEmpty(current))
    {
        status = XIA_INVALID_DETCHAN;
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveDetChan",
               "Specified detChan %u doesn't exist", detChan);
        return status;
    }

    prev = NULL;
    next = current->next;

    while ((detChan != current->detChan) &&
           (next != NULL))
    {
        prev = current;
        current = next;
        next = current->next;
    }

    if ((next == NULL) &&
        (detChan != current->detChan))
    {
        status = XIA_INVALID_DETCHAN;
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveDetChan",
               "Specified detChan %u doesn't exist", detChan);
        return status;
    }

    if (current == xiaDetChanHead)
    {
        xiaDetChanHead = current->next;

    } else {

        prev->next = next;
    }

    switch (current->type)
    {
        case SINGLE:
            handel_md_free((void *)current->data.modAlias);
            break;

        case SET:
            xiaFreeDetSet(current->data.detChanSet);
            break;

        default:
            status = XIA_BAD_TYPE;
            xiaLog(XIA_LOG_ERROR, status, "xiaRemoveDetChan",
                   "Invalid type. Should not be seeing this!");
            return status;
    }

    handel_md_free((void *)current);

    if (xiaDetChanHead &&
        (xiaDetChanHead->detChan == -1) && (xiaDetChanHead->next == NULL)) {
        status = xiaRemoveDetChan(-1);
      if (status != XIA_SUCCESS)
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveDetChan",
               "Removing master detector channel");
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine removes all DetChanElement LL.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaRemoveAllDetChans(void)
{
    while (xiaDetChanHead != NULL)
    {
        xiaRemoveDetChan(xiaDetChanHead->detChan);
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine checks a detChanSet for infinite loops. Assumes that head is
 * a set.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaValidateDetSet(DetChanElement *head)
{
    int status = XIA_SUCCESS;

    DetChanElement *current = NULL;

    DetChanSetElem *element = NULL;

    /* We only want to tag detChans that are sets since multiple sets can
     * include references to SINGLE detChans.
     */
    head->isTagged = TRUE_;

    element = head->data.detChanSet;

    while (element != NULL)
    {
        current = xiaGetDetChanPtr(element->channel);

        switch (current->type)
        {
            case SINGLE:
                status = XIA_SUCCESS;
                break;

            case SET:
                if (current->isTagged) {
                    xiaLog(XIA_LOG_ERROR, XIA_INFINITE_LOOP, "xiaValidateDetSet",
                           "Infinite loop detected involving detChan %d", current->detChan);
                    return XIA_INFINITE_LOOP;
                }
                status = xiaValidateDetSet(current);
                break;

            default:
                status = XIA_UNKNOWN;
                break;
        }

        if (status != XIA_SUCCESS)
        {
            xiaLog(XIA_LOG_ERROR, status, "xiaValidateDetSet",
                   "Error validating detChans");
            return status;
        }

        element = getListNext(element);
    }

    return status;
}


/*****************************************************************************
 *
 * This routine checks detChanSets to make sure they are valid.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaValidateDetSets(void)
{
    int status = XIA_SUCCESS;

    DetChanElement *current = xiaGetDetChanHead();

    if (!current) {
        xiaLog(XIA_LOG_ERROR, XIA_NO_DETCHANS, "xiaValidateDetSet",
               "No detChans are defined.");
        return XIA_NO_DETCHANS;
    }

    while (current != NULL) {
        switch (xiaGetElemType(current->detChan)) {
            case SET:
                xiaClearTags();
                status = xiaValidateDetSet(current);
                break;

            case SINGLE:
                status = XIA_SUCCESS;
                break;

            case 999:
                status = XIA_INVALID_DETCHAN;
                xiaLog(XIA_LOG_ERROR, status, "xiaValidateDetSets",
                       "detChan %d has an invalid type.", current->detChan);
                /* Let the check below do the "return" for us. */
                break;

            default:
                FAIL();
                break;
        }

        if (status != XIA_SUCCESS) {
            xiaLog(XIA_LOG_ERROR, status, "xiaValidateDetSets",
                   "Error validating detChan %d.", current->detChan);
            return status;
        }

        current = getListNext(current);
    }

    return status;
}


/*****************************************************************************
 *
 * This routine searches through a DetChanSetElem linked-list, starting at
 * head, for the end of the list, which is returned. Returns NULL if there are
 * any problems.
 *
 *****************************************************************************/
HANDEL_STATIC DetChanSetElem* HANDEL_API xiaGetDetSetTail(DetChanSetElem *head)
{
    DetChanSetElem *current;

    current = head;

    if (isListEmpty(current))
    {
        return NULL;
    }

    while (current->next != NULL)
    {
        current = current->next;
    }

    return current;
}

/*****************************************************************************
 *
 * This routine frees a DetChanSetElem linked-list by walking through the list
 * and freeing elements in it's wake.
 *
 *****************************************************************************/
HANDEL_SHARED void HANDEL_API xiaFreeDetSet(DetChanSetElem *head)
{
    DetChanSetElem *current;

    current = head;

    while (current != NULL)
    {
        head = current;
        current = current->next;
        handel_md_free((void *)head);
    }
}


/*****************************************************************************
 *
 * This routine adds a detChan, called newChan, to a detChanSet. The
 * detChanSet will be silently created if it doesn't exist yet. newChan must
 * be an existing detChanSet or detChan associated with a module.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaAddChannelSetElem(int detChanSet, int newChan)
{
    int status;

    if (newChan < 0) {

        status = XIA_INVALID_DETCHAN;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddChannelSetElem",
               "detChan to be added is < 0 or corrupted");
        return status;
    }

    if (xiaIsDetChanFree(newChan))
    {
        status = XIA_INVALID_DETCHAN;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddChannelSet",
               "detChan to be added doesn't exist yet");
        return status;
    }

    if (xiaIsDetChanFree(detChanSet))
    {
         /* Since calling this function will init. and allocate a previously
          * non-existent list. Unfortunately, xiaAddDetChan() only works when
          * detChan doesn't already exist. Need a different static for the case
          * where we simply want to add a value to an existing SET.
          */
        status = xiaAddDetChan(SET, detChanSet, (void *)&newChan);

    } else {

        status = xiaAddToExistingSet(detChanSet, newChan);
    }

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaAddChannelSet",
               "Error adding value to detChan set");
        return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine adds a detChan to an existing detChanSet. Assumes that
 * detChan is valid and exists. Also assumes that newChan exists as a SINGLE
 * or a SET.
 *
 *****************************************************************************/
HANDEL_STATIC int HANDEL_API xiaAddToExistingSet(int detChan, int newChan)
{
    int status;

    DetChanElement *current;

    DetChanSetElem *newDetChanElem;
    DetChanSetElem *tail;


    newDetChanElem = (DetChanSetElem *)handel_md_alloc(sizeof(DetChanSetElem));
    if (newDetChanElem == NULL)
    {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddToExistingSet",
               "Not enough memory to create new detChan element");
        return status;
    }

    newDetChanElem->channel = newChan;
    newDetChanElem->next = NULL;


    current = xiaDetChanHead;

    /* If you're wondering why there isn't more error checking here, see the
     * comments for the routine...
     */
    while (current->detChan != detChan)
    {
        current = current->next;
    }

    tail = xiaGetDetSetTail(current->data.detChanSet);
    tail->next = newDetChanElem;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine removes a detChan from a detChanSet. Assume that chan exists
 * within the detChanSet...
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaRemoveChannelSetElem(int detChan, int chan)
{
    int status;

    DetChanElement *currentDetChan;

    DetChanSetElem *prev;
    DetChanSetElem *current;


    if (xiaIsDetChanFree(detChan) ||
        xiaIsDetChanFree(chan))
    {
        status = XIA_INVALID_DETCHAN;
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveChannelSetElem",
               "Invalid detChan to remove");
        return status;
    }

    currentDetChan = xiaDetChanHead;

    /* No need to check for NULL since we already verified that detChan exists
       somewhere. */
    while (currentDetChan->detChan != detChan)
    {
        currentDetChan = currentDetChan->next;
    }

    current = currentDetChan->data.detChanSet;
    prev = NULL;

    while (current->channel != chan)
    {
        prev = current;
        current = current->next;
    }

    if (current == currentDetChan->data.detChanSet)
    {
        currentDetChan->data.detChanSet = current->next;

    } else {

        prev->next = current->next;
    }

    handel_md_free((void *)current);

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine removes an entire detChanSet. Essentially a wrapper.
 *
 *****************************************************************************/
HANDEL_EXPORT int HANDEL_API xiaRemoveChannelSet(int detChan)
{
    int status;
    int type;

    type = xiaGetElemType(detChan);
    if (type != SET)
    {
        status = XIA_WRONG_TYPE;
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveChannelSet",
               "detChan %u is not a detChan set", detChan);
        return status;
    }

    status = xiaRemoveDetChan(detChan);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveChannelSet",
               "Error removing detChan: %u", detChan);
        return status;
    }

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine returns the value in the type field of the specified detChan.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetElemType(int detChan)
{
    DetChanElement *current = NULL;

    current = xiaDetChanHead;

    while (current != NULL)
    {
        if (current->detChan == detChan)
        {
            return current->type;
        }

        current = current->next;
    }

    /* This isn't an error code, this is just an "invalid" type */
    return 999;
}

/*****************************************************************************
 *
 * This routine returns a string representing the module->board_type field.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetBoardType(int detChan, char *boardType)
{
    char *modAlias = NULL;

    int status;

    modAlias = xiaGetAliasFromDetChan(detChan);

    if (modAlias == NULL)
    {
        status = XIA_INVALID_DETCHAN;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetBoardType",
               "detChan %d is not connected to a valid module", detChan);
        return status;
    }

    status = xiaGetModuleItem(modAlias, "module_type", (void *) boardType);

    if (status != XIA_SUCCESS)
    {
        xiaLog(XIA_LOG_ERROR, status, "xiaGetBoardType",
               "Error getting board_type from module");
        return status;
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine returns the module alias associated with a given detChan. If
 * the detChan doesn't exist or is a SET then NULL is returned.
 *
 *****************************************************************************/
HANDEL_SHARED char* HANDEL_API xiaGetAliasFromDetChan(int detChan)
{
    DetChanElement *current = NULL;

    if (xiaIsDetChanFree(detChan))
    {
        return NULL;
    }

    current = xiaDetChanHead;

    while ((current->next != NULL) &&
           (current->detChan != detChan))
    {
        current = current->next;
    }

    if ((current->next == NULL) &&
        (current->detChan != detChan))
    {
        return NULL;
    }

    if (current->type == SET)
    {
        return NULL;
    }

    return current->data.modAlias;
}


/*****************************************************************************
 *
 * This routine clears the isTagged fields from all of the detChan elements.
 *
 *****************************************************************************/
HANDEL_SHARED void HANDEL_API xiaClearTags(void)
{
    DetChanElement *current = NULL;

    current = xiaDetChanHead;

    while (current != NULL)
    {
        current->isTagged = FALSE_;
        current = getListNext(current);
    }
}


/*****************************************************************************
 *
 * This routine returns a pointer to the detChan element denoted by the
 * detChan value.
 *
 *****************************************************************************/
HANDEL_SHARED DetChanElement* HANDEL_API xiaGetDetChanPtr(int detChan)
{
    DetChanElement *current = NULL;

    current = xiaDetChanHead;

    while (current != NULL)
    {
        if (current->detChan == detChan)
        {
            return current;
        }

        current = getListNext(current);
    }

    return NULL;
}


/*****************************************************************************
 *
 * This routine returns default string used to find a detector's defaults.
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetDefaultStrFromDetChan(int detChan,
                                                         char* defaultStr)
{
    int modChan;

    char tmpStr[MAXITEM_LEN];

    char *alias = NULL;

    modChan = xiaGetModChan(detChan);
    alias = xiaGetAliasFromDetChan(detChan);
    sprintf(tmpStr, "default_chan%u", modChan);

    return xiaGetModuleItem(alias, tmpStr, (void *)defaultStr);
}


/*****************************************************************************
 *
 * This routine returns a pointer to the XiaDefaults item associated with
 * the specified detChan. This is pretty much a "convienence" routine.
 *
 *****************************************************************************/
HANDEL_SHARED XiaDefaults* HANDEL_API xiaGetDefaultFromDetChan(int detChan)
{
    int status;

    char defaultStr[MAXALIAS_LEN];

    status = xiaGetDefaultStrFromDetChan(detChan, defaultStr);

    if (status != XIA_SUCCESS) {
        return NULL;
    }

    return xiaFindDefault(defaultStr);
}


/*****************************************************************************
 *
 * This routine clears the Detector Channel LL
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaInitDetChanDS(void)
{
    xiaDetChanHead = NULL;
    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * Return the detector channel list's head.
 *
 *****************************************************************************/
HANDEL_SHARED DetChanElement * HANDEL_API xiaGetDetChanHead(void)
{
    return xiaDetChanHead;
}
