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

#include "handel_generic.h"
#include "handeldef.h"
#include "handel_errors.h"
#include "handel_log.h"

/*
 * Define the head of the XiaDefaults LL
 */
static XiaDefaults *xiaDefaultsHead = NULL;


/*****************************************************************************
 *
 * This routine creates a new XiaDefaults entry
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaNewDefault(const char *alias)
{
    int status = XIA_SUCCESS;

    XiaDefaults *current=NULL;


    /* If HanDeL isn't initialized, go ahead and call it... */
    if (!isHandelInit)
    {
        status = xiaInitHandel();
        if (status != XIA_SUCCESS)
        {
            fprintf(stderr, "FATAL ERROR: Unable to load libraries.\n");
            exit(XIA_INITIALIZE);
        }

        xiaLog(XIA_LOG_WARNING, "xiaNewDefault",
               "HanDeL was initialized silently");
    }

    if ((strlen(alias) + 1) > MAXALIAS_LEN)
    {
        status = XIA_ALIAS_SIZE;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewDefault",
               "Alias contains too many characters");
        return status;
    }

    /* First check if this alias exists already? */
    current = xiaFindDefault(alias);
    if (current != NULL)
    {
        status = XIA_ALIAS_EXISTS;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewDefault",
               "Alias %s already in use.", alias);
        return status;
    }
    /* Check that the Head of the linked list exists */
    if (xiaDefaultsHead == NULL)
    {
        /* Create an entry that is the Head of the linked list */
        xiaDefaultsHead = (XiaDefaults *) handel_md_alloc(sizeof(XiaDefaults));
        current = xiaDefaultsHead;
    } else {
        /* Find the end of the linked list */
        current= xiaDefaultsHead;
        while (current->next != NULL)
        {
            current= current->next;
        }

        current->next = (XiaDefaults *) handel_md_alloc(sizeof(XiaDefaults));
        current= current->next;
    }

    /* Make sure memory was allocated */
    if (current == NULL)
    {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewDefault",
               "Unable to allocate memory for default %s.", alias);
        return status;
    }

    /* Do any other allocations, or initialize to NULL/0 */
    current->alias = (char *) handel_md_alloc((strlen(alias)+1)*sizeof(char));
    if (current->alias == NULL)
    {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "xiaNewDefault",
               "Unable to allocate memory for default->alias");
        return status;
    }

    strcpy(current->alias,alias);

    current->entry = NULL;
    current->next = NULL;

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine adds information about a Default Item entry
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaAddDefaultItem(const char *alias,
                                               const char *name, void *value)
{
    int status = XIA_SUCCESS;

    XiaDefaults *chosen = NULL;

    XiaDaqEntry *prev = NULL;
    XiaDaqEntry *current = NULL;

    /* Locate the XiaDefaults entry first */
    chosen = xiaFindDefault(alias);
    if (chosen == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDefaultItem",
               "Alias %s has not been created.", alias);
        return status;
    }

    /* Check that the value is not NULL. */
    if (value == NULL)
    {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDefaultItem",
               "Value can not be NULL");
        return status;
    }

    if (name == NULL)
    {
        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDefaultItem",
               "Name can not be NULL");
        return status;
    }

    /* Since its not easy to check all possible names, accept anything, an error
     * will be generated if an invalid name is used at a later time in program
     * execution.
     */
    current = chosen->entry;
    if (current != NULL)
    {
        /* First check if the default exists already. If so, just modify and
         * return.
         */
        /* Now find a match to the name. */
        while (current!=NULL)
        {
            if (STREQ(name, current->name))
            {
                break;
            }
            prev = current;
            current=current->next;
        }

        if (current != NULL)
        {
            /* Now modify the value. */
            current->data = *((double *) value);

            status = XIA_SUCCESS;
            return status;
        }
    }
    /* If we are here, then we are at the end of the defaults list, else we found
     * a match and returned already.  Time to allocate memory for a new entry.
     */
    if (chosen->entry == NULL)
    {
        chosen->entry = (XiaDaqEntry *) handel_md_alloc(sizeof(XiaDaqEntry));
        current = chosen->entry;
    } else {
        prev->next = (XiaDaqEntry *) handel_md_alloc(sizeof(XiaDaqEntry));
        current = prev->next;
    }

    if (current == NULL)
    {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDefaultItem",
               "Unable to allocate memory for DAQ entry");
        return status;
    }

    current->next = NULL;

    /* Create the name entry. */
    current->name = (char *) handel_md_alloc((strlen(name)+1)*sizeof(char));
    if (current->name == NULL)
    {
        status = XIA_NOMEM;
        xiaLog(XIA_LOG_ERROR, status, "xiaAddDefaultItem",
               "Unable to allocate memory for current->name");
        return status;
    }

    strcpy(current->name, name);

    current->data    = *((double *) value);
    current->pending = 0.0;
    current->state   = AV_STATE_UNKNOWN;

    return XIA_SUCCESS;
}

/*****************************************************************************
 *
 * This routine modifies information about a Firmware Item entry
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaModifyDefaultItem(const char *alias,
                                                  const char *name, void *value)
{
    int status = XIA_SUCCESS;

    XiaDefaults *chosen = NULL;

    XiaDaqEntry *current = NULL;

    /* Check that the name and value are not NULL */
    if (value == NULL)
    {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyDefaultItem",
               "Value can not be NULL");
        return status;
    }

    if (name == NULL)
    {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyDefaultItem",
               "Name can not be NULL");
        return status;
    }

    /* Locate the XiaDefaults entry first */
    chosen = xiaFindDefault(alias);
    if (chosen == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyDefaultItem",
               "Alias %s was not found.", alias);
        return status;
    }

    /* Now find a match to the name */
    current = chosen->entry;
    while (current!=NULL)
    {
        if (STREQ(name, current->name))
        {
            break;
        }
        current=current->next;
    }

    if (current == NULL)
    {
        status = XIA_BAD_VALUE;
        xiaLog(XIA_LOG_ERROR, status, "xiaModifyDefaultItem",
               "No entry named %s found.", name);
        return status;
    }

    /* Now modify the value */
    current->data = *((double *) value);

    return status;
}

/*****************************************************************************
 *
 * This routine retrieves the value of a XiaDefaults entry
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaGetDefaultItem(const char *alias,
                                               const char *name, void *value)
{
    int status = XIA_SUCCESS;

    XiaDefaults *chosen = NULL;

    XiaDaqEntry *current = NULL;

    /* Find the alias */
    chosen = xiaFindDefault(alias);
    if (chosen == NULL)
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetDefaultItem",
               "Alias: %s does not exist", alias);
        return status;
    }

    /* Decide which data to return by searching through the entries LL */
    current = chosen->entry;

    /* Search first and then cast as the last step. A little opposite of
     * the way that we usually do this, but these structures are also
     * organized different then usual, so...
     */
    while (current!=NULL)
    {
        if (STREQ(name, current->name))
        {
            break;
        }
        current=current->next;
    }

    if (current == NULL)
    {
        status = XIA_BAD_NAME;
        xiaLog(XIA_LOG_ERROR, status, "xiaGetDefaultItem",
               "Invalid name: %s", name);
        return status;
    }

    *((double *)value) = current->data;

    return status;
}


/*****************************************************************************
 *
 * This routine removes a XiaDefaults entry
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaRemoveDefault(const char *alias)
{
    int status = XIA_SUCCESS;

    XiaDefaults *prev    = NULL;
    XiaDefaults *current = NULL;
    XiaDefaults *next    = NULL;
    XiaDaqEntry *entry   = NULL;

    xiaLog(XIA_LOG_DEBUG, "xiaRemoveDefault",
           "Preparing to remove default w/ alias %s", alias);

    if (isListEmpty(xiaDefaultsHead))
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveDefault",
               "Alias %s does not exist", alias);
        return status;
    }

    /* First check if this alias exists already? */
    prev = NULL;
    current = xiaDefaultsHead;
    next = current->next;
    while (next != NULL)
    {
        if (STREQ(alias, current->alias))
        {
            break;
        }
        /* Move to the next element */
        prev = current;
        current = next;
        next = current->next;
    }

    /* Check if we found nothing */
    if ((next == NULL) &&
        (!STREQ(current->alias, alias)))
    {
        status = XIA_NO_ALIAS;
        xiaLog(XIA_LOG_ERROR, status, "xiaRemoveDefault",
               "Alias %s does not exist.", alias);
        return status;
    }

    /* Check if match is the head of the list */
    if (current == xiaDefaultsHead)
    {
        xiaDefaultsHead = next;

    } else {

        prev->next = next;
    }

    /* Free up the memory associated with this element */
    if (current->alias != NULL)
    {
        handel_md_free(current->alias);
    }

    /* Loop over the xiaDaqEntry information, deallocating memory */
    entry = current->entry;
    while (entry != NULL)
    {
        XiaDaqEntry* nextEntry = entry->next;

        if (entry->name != NULL)
        {
          handel_md_free(entry->name);
        }

        /* Free the XiaDaqEntry structure */
        handel_md_free(entry);

        entry = nextEntry;
    }

    /* Free the XiaDefaults structure */
    handel_md_free(current);

    return status;
}

/*****************************************************************************
 *
 * This routine removes a XiaDefaults entry
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaRemoveAllDefaults(void)
{
    while (xiaDefaultsHead != NULL)
    {
        xiaRemoveDefault(xiaDefaultsHead->alias);
    }

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine returns the entry of the Firmware linked list that matches
 * the alias.  If NULL is returned, then no match was found.
 *
 *****************************************************************************/
HANDEL_SHARED XiaDefaults* HANDEL_API xiaFindDefault(const char *alias)
{
    XiaDefaults *current = NULL;

    /* First check if this alias exists already? */
    current = xiaDefaultsHead;
    while (current != NULL)
    {
        /* If the alias matches, return a pointer to the detector */
        if (STREQ(alias, current->alias))
        {
            return current;
        }
        /* Move to the next element */
        current = current->next;
    }

    return NULL;
}


/*****************************************************************************
 *
 * This routine returns the value associated with the specified default. This
 * routine assumes that the value requested ACTUALLY exists in the default
 * referenced by alias. It does NOT return an error!
 *
 *****************************************************************************/
HANDEL_SHARED double HANDEL_API xiaGetValueFromDefaults(const char *name,
                                                        const char *alias)

{
    XiaDefaults *current = NULL;

    XiaDaqEntry *entry = NULL;

    current = xiaFindDefault(alias);
    if (current == NULL)
    {
        return 0.0;
    }

    entry = current->entry;

    while (entry != NULL)
    {
        if (STREQ(entry->name, name))
        {
            return entry->data;
        }

        entry = getListNext(entry);
    }

    return 0.0;
}


/*****************************************************************************
 *
 * This routine clears the Detfault LL
 *
 *****************************************************************************/
HANDEL_SHARED int HANDEL_API xiaInitXiaDefaultsDS(void)
{
    xiaDefaultsHead = NULL;
    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine returns a pointer to the head of XiaDefaults linked-list.
 *
 *****************************************************************************/
HANDEL_SHARED XiaDefaults* HANDEL_API xiaGetDefaultsHead(void)
{
    return xiaDefaultsHead;
}
