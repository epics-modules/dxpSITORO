/*
 * Copyright (c) 2009-2016 XIA LLC
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


#ifndef PSL_COMMON_H
#define PSL_COMMON_H

#include "psldef.h"

#include "xia_handel_structures.h"
#include "xia_common.h"

#include "md_shim.h"
#include "md_generic.h"

#if __GNUC__
#define PSL_PRINTF(_s, _f) __attribute__ ((format (printf, _s, _f)))
#else
#define PSL_PRINTF(_s, _f)
#endif

/* PSL Logging level */
#define PSL_LOG_ERROR   MD_ERROR, XIA_FILE, __LINE__, __func__
#define PSL_LOG_WARNING MD_WARNING, XIA_FILE, __LINE__, __func__, 0
#define PSL_LOG_INFO    MD_INFO, XIA_FILE, __LINE__, __func__, 0
#define PSL_LOG_DEBUG   MD_DEBUG, XIA_FILE, __LINE__, __func__, 0

/* Macro for block wrapper in Macros to disable the "conditional
 * expression is constant" warning
 */
#ifdef _WIN32
#  define ONCE __pragma( warning(push) ) \
               __pragma( warning(disable:4127) ) \
               while( 0 ) \
               __pragma( warning(pop) )
#  define CHECK_CONST(_expr)  __pragma( warning(push) ) \
                              __pragma( warning(disable:4127) ) \
                              _expr \
                              __pragma( warning(pop) )
#else
#  define ONCE while( 0 )
#  define CHECK_CONST(_expr) _expr
#endif

/* Shared routines */
PSL_SHARED void PSL_API pslLog(int level, const char* file, int line,
                               const char* routine, int error,
                               const char* message, ...) PSL_PRINTF(6, 7);
PSL_SHARED int PSL_API pslGetDefault(const char *name, void *value,
                                     XiaDefaults *defaults);
PSL_SHARED int PSL_API pslSetDefault(const char *name, void *value,
                                     XiaDefaults *defaults);
PSL_SHARED int pslGetModChan(int detChan, Module *m, int *modChan);
PSL_SHARED int PSL_API pslDestroySCAs(Module *m, int modChan);
PSL_SHARED int PSL_API pslSetNumberSCAs(Module *m, XiaDefaults *defs, int modChan, int nSCA);
PSL_SHARED XiaDaqEntry * pslFindEntry(const char *name, XiaDefaults *defs);
PSL_SHARED int pslInvalidate(const char *name, XiaDefaults *defs);
PSL_SHARED void pslDumpDefaults(XiaDefaults *defs);
PSL_SHARED double pslU64ToDouble(unsigned long *u64);
PSL_SHARED int pslRemoveDefault(const char *name, XiaDefaults *defs);
PSL_SHARED boolean_t pslIsUpperCase(const char *s);
#endif /* PSL_COMMON_H */
