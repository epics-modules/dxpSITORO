/*
 * Copyright (c) 2004 X-ray Instrumentation Associates
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
 *
 * $Id$
 *
 */


#ifndef XIA_COMMON_H
#define XIA_COMMON_H

#include <string.h>
#include <math.h>

/* Define the length of the error reporting string info_string */
#define INFO_LEN 400
/* Define the length of the line string used to read in files */
#define XIA_LINE_LEN 132

/** Typedefs **/
typedef unsigned char  byte_t;
typedef unsigned char  boolean_t;
typedef unsigned short parameter_t;
typedef unsigned short flag_t;

#ifdef LINUX
typedef int             HANDLE;
typedef unsigned char   UCHAR;
typedef unsigned short  USHORT;
typedef unsigned short* PUSHORT;
typedef unsigned int    ULONG;
typedef unsigned long*  PULONG;
#endif /* LINUX */

/** MACROS **/
#define TRUE_  (1 == 1)
#define FALSE_ (1 == 0)

#define UNUSED(x)   ((x) = (x))
#define STREQ(x, y) (strcmp((x), (y)) == 0)
#define STRNEQ(x, y) (strncmp((x), (y), strlen(y)) == 0)
#define ROUND(x)    ((x) < 0.0 ? ceil((x) - 0.5) : floor((x) + 0.5))
#define PRINT_NON_NULL(x) ((x) == NULL ? "NULL" : (x))
#define BYTE_TO_WORD(lo, hi) (unsigned short)(((unsigned short)(hi) << 8) | (lo))
#define WORD_TO_LONG(lo, hi) (unsigned long)(((unsigned long)(hi) << 16) | (lo))
#define LO_BYTE(word) (byte_t)((word) & 0xFF)
#define HI_BYTE(word) (byte_t)(((word) >> 8) & 0xFF)
#define LO_WORD(dword) ((dword) & 0xFFFF)
#define HI_WORD(dword) (((dword) >> 16) & 0xFFFF)
#define MAKE_LOWER_CASE(s, i) for ((i) = 0; (i) < strlen((s)); (i)++) (s)[i] = (char)tolower((int)((s)[i]))
#define N_ELEMS(x) (sizeof(x) / sizeof((x)[0]))

/* There is a known issue with glibc on Cygwin where ctype routines
 * that are passed an input > 0x7F return garbage.
 */
#ifdef CYGWIN32
#define CTYPE_CHAR(c) ((unsigned char)(c))
#else
#define CTYPE_CHAR(c) (c)
#endif /* CYGWIN32 */


/* These macros already exist on Linux, so they need to be protected */

#ifndef MIN
#define MIN(x, y)  ((x) < (y) ? (x) : (y))
#endif /* MIN */

#ifndef MAX
#define MAX(x, y)  ((x) > (y) ? (x) : (y))
#endif /* MAX */

/* Visual Studio projects that are nested set the __FILE__ to be the
 * path the compiler used to find the file which causes expansions
 * like ..\..\..\..\src\foo.c. This clogs up the log files. This idea
 * was suggested here: http://stackoverflow.com/a/8488201
 */
#ifdef _WIN32
#define XIA_FILE (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else /* _WIN32 */
#define XIA_FILE __FILE__
#endif /* _WIN32 */

/* Define __func__ and ssize_t for msvc */
#ifdef _MSC_VER
#define __func__ __FUNCTION__

#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

/*
 * Handle compiler pragma settings.
 */
#ifdef __clang__
#define PRAGMA_clang_0(_p)              #_p
#define PRAGMA_clang_1(_p)              PRAGMA_clang_0(clang _p)
#define PRAGMA_clang_2(_p)              PRAGMA_clang_1(_p)
#define PRAGMA_clang(_p)                _Pragma(PRAGMA_clang_2(_p))
#define PRAGMA_PUSH                     PRAGMA_clang(diagnostic push)
#define PRAGMA_POP                      PRAGMA_clang(diagnostic push)
#define PRAGMA_IGNORE_CAST_QUALIFIER    PRAGMA_clang(diagnostic ignored "-Wcast-qual")
#define PRAGMA_IGNORE_STRICT_PROTOTYPES PRAGMA_clang(diagnostic ignored "-Wstrict-prototypes")
#else
#define PRAGMA_clang(_p)
#endif

#ifdef _MSC_VER
#define PRAGMA_msvc(_p)                 __pragma(_p)
#define PRAGMA_PUSH                     PRAGMA_msvc(warning(push))
#define PRAGMA_POP                      PRAGMA_msvc(warning(pop))
#define PRAGMA_IGNORE_COND_CONST        PRAGMA_msvc(warning(disable:4127))

/* msvc /Ze allows non-constant and address of automatic variable
 * aggregate initializers but warns.
 */
#define PRAGMA_IGNORE_AGGREGATE_INIT    PRAGMA_msvc(warning(disable:4204; disable:4221))

#else
#define PRAGMA_msvc(_p)
#endif

#define PRAGMA_0(_cc) PRAGMA_ ## _cc
#define PRAGMA(_cc, _p) PRAGMA_0(_cc)(_p)

#if !defined(PRAGMA_PUSH)
#define PRAGMA_PUSH
#endif
#if !defined(PRAGMA_POP)
#define PRAGMA_POP
#endif
#if !defined(PRAGMA_IGNORE_CAST_QUALIFIER)
#define PRAGMA_IGNORE_CAST_QUALIFIER
#endif
#if !defined(PRAGMA_IGNORE_STRICT_PROTOTYPES)
#define PRAGMA_IGNORE_STRICT_PROTOTYPES
#endif
#if !defined(PRAGMA_IGNORE_COND_CONST)
#define PRAGMA_IGNORE_COND_CONST
#endif
#if !defined(PRAGMA_IGNORE_AGGREGATE_INIT)
#define PRAGMA_IGNORE_AGGREGATE_INIT
#endif


/* A generic buffer to facilitate passing data with length in one argument. */
typedef struct {
    void *data;
    size_t length;
} GenBuffer;

#endif /* XIA_COMMON_H */
