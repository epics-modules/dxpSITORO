/*
 * Copyright (c) 2002-2004 X-ray Instrumentation Associates
 *               2005-2016 XIA LLC
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
 *
 *
 */


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "psl_common.h"

#include "xia_handel.h"
#include "xia_assert.h"

#include "handel_errors.h"
#include "handel_generic.h"
#include "handel_log.h"

#include "md_threads.h"

static char formatBuffer[2048];
static handel_md_Mutex lock;

/**
 * The PSL layer logging.
 */
PSL_SHARED void PSL_API pslLog(int level, const char* file, int line,
                               const char* routine, int error,
                               const char* message, ...)
{
    va_list args;

    va_start(args, message);

    if (!handel_md_mutex_ready(&lock))
        handel_md_mutex_create(&lock);

    handel_md_mutex_lock(&lock);

    /*
     * Cannot use vsnprintf on MinGW currently because is generates
     * an unresolved external.
     */
    vsprintf(formatBuffer, message, args);
    formatBuffer[sizeof(formatBuffer) - 1] = '\0';

    handel_md_log(level, routine, formatBuffer, error, file, line);

    handel_md_mutex_unlock(&lock);

    va_end(args);
}

/**
 * This routine doesn't make any assumptions about
 * the presence of an acq. value when searching
 * the list. Returns an error if the acq. value isn't
 * found. If acq. value is found then the value is stored
 * in "value".
 */
PSL_SHARED int pslGetDefault(const char *name, void *value, XiaDefaults *defaults)
{

    XiaDaqEntry *entry = NULL;

    entry = defaults->entry;

    while (entry != NULL) {

        if (STREQ(name, entry->name)) {

            *((double *)value) = entry->data;
            return XIA_SUCCESS;
        }

        entry = getListNext(entry);
    }

    pslLog(PSL_LOG_ERROR, XIA_NOT_FOUND,
           "Unable to locate acquisition value '%s'.", name);

    return XIA_NOT_FOUND;
}


/**
 * Sets the named acquisition value to the new value
 * specified in "value". If the value doesn't
 * exist in the list then an error is returned.
 */
PSL_SHARED int PSL_API pslSetDefault(const char *name, void *value,
                                     XiaDefaults *defaults)
{

    XiaDaqEntry *entry = NULL;


    entry = defaults->entry;

    while (entry != NULL) {

        if (STREQ(name, entry->name)) {

            entry->data = *((double *)value);
            return XIA_SUCCESS;
        }

        entry = getListNext(entry);
    }

    return XIA_NOT_FOUND;
}


/** @brief Removes the named default and free the associated memory
 *
 * This routine does not check if the default is required or not. Removing
 * required defaults will most certainly cause the library to crash (most likely
 * on a failed ASSERT).
 *
 */
PSL_SHARED int pslRemoveDefault(const char *name, XiaDefaults *defs)
{
    XiaDaqEntry *e    = NULL;
    XiaDaqEntry *prev = NULL;


    ASSERT(name != NULL);
    ASSERT(defs != NULL);


    for (e = defs->entry; e != NULL; prev = e, e = e->next) {

        if (STREQ(name, e->name)) {
            if (!prev) {
                defs->entry = e->next;

            } else {
                prev->next = e->next;
            }

            pslLog(PSL_LOG_DEBUG, "e = %p", (void*) e);

            free(e->name);
            free(e);

            return XIA_SUCCESS;
        }
    }

    pslLog(PSL_LOG_ERROR, XIA_NOT_FOUND,
           "Unable to find acquisition value '%s' in defaults", name);
    return XIA_NOT_FOUND;
}


/** @brief Converts a detChan to a modChan
 *
 * Converts a detChan to the modChan for the specified module. \n
 * Returns an error if the detChan isn't assigned to a channel \n
 * in that module.
 *
 * @param detChan detChan to convert
 * @param m Module to get the module channel from
 * @param modChan Returned module channel.
 *
 * @returns Error status indicating success or failure.
 *
 */
PSL_SHARED int pslGetModChan(int detChan, Module *m, int *modChan)
{
    int i;


    ASSERT(m != NULL);
    ASSERT(modChan != NULL);


    for (i = 0; i < (int) m->number_of_channels; i++) {
        if (m->channels[i] == detChan) {
            *modChan = i;
            return XIA_SUCCESS;
        }
    }

    pslLog(PSL_LOG_ERROR, XIA_INVALID_DETCHAN,
           "detChan '%d' is not assigned to module '%s'", detChan, m->alias);
    return XIA_INVALID_DETCHAN;
}


/** @brief Frees memory associated with the SCAs
 *
 * @param m Pointer to a valid module structure containing the
 *          SCAs to destroy.
 * @param modChan The module channel number of the SCAs to be
 *                destroyed.
 *
 * @return Integer indicating success or failure.
 *
 */
PSL_SHARED int PSL_API pslDestroySCAs(Module *m, int modChan)
{
    ASSERT(m != NULL);


    free(m->ch[modChan].sca_lo);
    m->ch[modChan].sca_lo = NULL;
    free(m->ch[modChan].sca_hi);
    m->ch[modChan].sca_hi = NULL;

    m->ch[modChan].n_sca = 0;

    return XIA_SUCCESS;
}

/** Set the number of SCA to a new given value
 *  Allocate or free data if needed, returns XIA_NOMEM if failed
 */
PSL_SHARED int PSL_API pslSetNumberSCAs(Module *m, XiaDefaults *defs, int modChan, int nSCA)
{
  int status;

  unsigned short *lo, *hi;
  char limit[9];


  /* If the number of SCAs shrank then we need to remove the limits
  * that are greater then the new number of SCAs.
  */
  if ((unsigned short)nSCA < m->ch[modChan].n_sca) {

    for (unsigned short i = (unsigned short)nSCA; i < m->ch[modChan].n_sca; i++) {
      pslLog(PSL_LOG_DEBUG, "Removing sca%hu_* limits for modChan %d",
            i, modChan);

      sprintf(limit, "sca%hu_lo", i);
      status = pslRemoveDefault(limit, defs);

      if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_WARNING, "Unable to remove SCA limit '%s' for "
                "modChan %d", limit, modChan);
      }

      sprintf(limit, "sca%u_hi", i);
      status = pslRemoveDefault(limit, defs);

      if (status != XIA_SUCCESS) {
        pslLog(PSL_LOG_WARNING, "Unable to remove SCA limit '%s' for "
                "modChan %d", limit, modChan);
      }
    }
  }

  /* Reset the entire array here */
  pslDestroySCAs(m, modChan);

  if (nSCA > 0) {
    size_t scaArraySize = (size_t) nSCA * sizeof(unsigned short);

    lo = (unsigned short *)malloc(scaArraySize);
    hi = (unsigned short *)malloc(scaArraySize);

    if (!lo || !hi) {
      pslLog(PSL_LOG_ERROR, XIA_NOMEM,
            "Unable to allocate %zu bytes for sca limits on modChan %d",
            scaArraySize, modChan);
      return XIA_NOMEM;
    }

    m->ch[modChan].sca_lo = lo;
    m->ch[modChan].sca_hi = hi;

    memset(m->ch[modChan].sca_lo, 0, scaArraySize);
    memset(m->ch[modChan].sca_hi, 0, scaArraySize);
  }

  m->ch[modChan].n_sca = (unsigned short)nSCA;

  return XIA_SUCCESS;
}


/** @brief Find the entry structure matching the supplied name
 *
 */
PSL_SHARED XiaDaqEntry *pslFindEntry(const char *name, XiaDefaults *defs)
{
    XiaDaqEntry *e = NULL;


    e = defs->entry;

    while (e != NULL) {
        if (STREQ(e->name, name)) {
            return e;
        }

        e = e->next;
    }

    return NULL;
}
