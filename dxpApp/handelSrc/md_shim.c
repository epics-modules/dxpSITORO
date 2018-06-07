/*
 * Copyright (c) 2012 XIA LLC
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

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#include "xia_assert.h"
#include "xia_common.h"

#include "Dlldefs.h"
#include "handel_errors.h"

#include "md_shim.h"
#include "md_generic.h"
#include "md_threads.h"

#define INFO_LEN    400

/* Current output for the logging routines. By default, this is set to stdout
 * in dxp_md_log().
 */
static FILE *out_stream = NULL;

static boolean_t isSuppressed = FALSE_;

static int logLevel = MD_ERROR;

static handel_md_Mutex lock;

HANDEL_STATIC void dxp_md_error(const char* routine, const char* message,
                                int* error_code, const char *file, int line);
HANDEL_STATIC void dxp_md_warning(const char *routine, const char *message,
                                  const char *file, int line);
HANDEL_STATIC void dxp_md_info(const char *routine, const char *message,
                               const char *file, int line);
HANDEL_STATIC void dxp_md_debug(const char *routine, const char *message,
                                const char *file, int line);


/** @brief Routine to wait a specified time in seconds.
 */
XIA_SHARED int dxp_md_wait(float *time)
{
#ifdef WIN32
    DWORD wait = (DWORD)(1000.0 * (*time));
    Sleep(wait);
#else
    struct timespec req = {
      .tv_sec = (time_t) *time,
      .tv_nsec = (time_t) (((double) *time) * 1000000000.0)
    };
    struct timespec rem = {
      .tv_sec = 0,
      .tv_nsec = 0
    };
    while (TRUE_) {
        if (nanosleep(&req, &rem) == 0)
            break;
        req = rem;
    }
#endif
    return XIA_SUCCESS;
}


/*
 * Safe version of fgets() that can handle both UNIX and DOS
 * line-endings.
 *
 * length is the size of the buffer s. fgets will read at most
 * length-1 characters and add a null terminator.
 *
 * If the trailing two characters are '\\r' + \n', they are replaced by a
 * single '\n'.
 */
XIA_SHARED char * dxp_md_fgets(char *s, int length, FILE *stream)
{
    char *cstatus = NULL;

    size_t ret_len = 0;


    ASSERT(s != NULL);
    ASSERT(stream != NULL);
    ASSERT(length > 0);


    cstatus = fgets(s, length, stream);

    if (!cstatus) {
        if (ferror(stream)) {
            dxp_md_warning("dxp_md_fgets", "Error detected reading from "
                           "stream.", __FILE__, __LINE__);
        }

        return NULL;
    }

    ret_len = strlen(s);

    if ((ret_len > 1) &&
        (s[ret_len - 2] == '\r') &&
        (s[ret_len - 1] == '\n')) {
        s[ret_len - 2] = '\n';
        s[ret_len - 1] = '\0';
    }

    return s;
}


/*****************************************************************************
 *
 * This routine enables the logging output
 *
 *****************************************************************************/
XIA_SHARED int dxp_md_enable_log(void)
{
    isSuppressed = FALSE_;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine disables the logging output
 *
 *****************************************************************************/
XIA_SHARED int dxp_md_suppress_log(void)
{
    isSuppressed = TRUE_;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine sets the maximum level at which log messages will be
 * displayed.
 *
 *****************************************************************************/
XIA_SHARED int dxp_md_set_log_level(int level)
{

/* Validate level */
    if ((level > MD_DEBUG) || (level < MD_ERROR)) {
        /* Leave level at previous setting and return an error code */
        return XIA_LOG_LEVEL;
    }

    logLevel = level;

    return XIA_SUCCESS;
}


/*****************************************************************************
 *
 * This routine is the main logging routine. It shouldn't be called directly.
 * Use the macros provided in xerxes_generic.h.
 *
 *****************************************************************************/
XIA_SHARED void dxp_md_log(int level, const char *routine, const char *message,
                           int error, const char *file, int line)
{
    /* If logging is disabled or we aren't set
     * to log this message level then return gracefully, NOW!
     */
    if (isSuppressed || (level > logLevel)) {
        return;
    }

    /* Validate level */
    if ((level > MD_DEBUG) || (level < MD_ERROR)) {
        /* Don't log the message */
        return;
    }

    /*
     * Keep the log messaages ordered and sane in a threading environment.
     */

    handel_md_mutex_lock(&lock);

    /* Ordinarily, we'd set this in the globals section, but on Linux 'stdout'
     * isn't a constant, so it can't be used as an initializer.
     */
    if (out_stream == NULL) {
        out_stream = stdout;
    }

    switch (level) {
        case MD_ERROR:
            dxp_md_error(routine, message, &error, file, line);
            break;
        case MD_WARNING:
            dxp_md_warning(routine, message, file, line);
            break;
        case MD_INFO:
            dxp_md_info(routine, message, file, line);
            break;
        case MD_DEBUG:
            dxp_md_debug(routine, message, file, line);
            break;
        default:
            FAIL();
            break;
    }

    handel_md_mutex_unlock(&lock);
}

#ifdef WIN32
/**
 * Convert a Windows system time to time_t... for conversion to struct
 * tm to work with strftime. From
 * https://blogs.msdn.microsoft.com/joshpoley/2007/12/19/datetime-formats-and-conversions/
 */
HANDEL_STATIC void dxp_md_SystemTimeToTime_t(SYSTEMTIME *systemTime, time_t *dosTime)
{
    LARGE_INTEGER jan1970FT = {0};
    jan1970FT.QuadPart = 116444736000000000I64; // january 1st 1970

    LARGE_INTEGER utcFT = {0};
    SystemTimeToFileTime(systemTime, (FILETIME*)&utcFT);

    unsigned __int64 utcDosTime = (utcFT.QuadPart - jan1970FT.QuadPart)/10000000;

    *dosTime = (time_t)utcDosTime;
}

/**
 * Returns the current local time as a struct tm for string formatting
 * and the milliseconds on the side for extra precision.
 */
HANDEL_STATIC void dxp_md_local_time(struct tm **local, int *milli)
{
    SYSTEMTIME tod;

    GetLocalTime(&tod);

    time_t current;
    dxp_md_SystemTimeToTime_t(&tod, &current);
    *local = gmtime(&current);
    *milli = tod.wMilliseconds;
}

#else

/**
 * Returns the current local time as a struct tm for string formatting
 * and the milliseconds on the side for extra precision.
 */
HANDEL_STATIC void dxp_md_local_time(struct tm **local, int *milli)
{
    struct timeval tod;
    gettimeofday(&tod, NULL);
    *milli = (int) tod.tv_usec / 1000;
    time_t current = tod.tv_sec;
    *local = localtime(&current);
}
#endif

/**
 * Write a standard log format header.
 */
HANDEL_STATIC void dxp_md_log_header(const char* type, const char* routine,
                                     int* error_code, const char *file, int line)
{
    struct tm *localTime;
    int milli;
    char logTimeFormat[80];

    const char* basename;
    int out;

    dxp_md_local_time(&localTime, &milli);

    strftime(logTimeFormat, sizeof(logTimeFormat), "%Y-%m-%d %H:%M:%S", localTime);

    basename = strrchr(file, '/');
    if (basename != NULL) {
        ++basename;
    } else {
        basename = strrchr(file, '\\');
        if (basename != NULL)
            ++basename;
        else
            basename = file;
    }

    out = fprintf(out_stream, "%s %s,%03d %s (%s:%d)",
                  type, logTimeFormat, milli, routine, basename, line);

    fprintf(out_stream, "%*c ", 90 - out, ':');

    if (error_code)
        fprintf(out_stream, "[%3d] ", *error_code);
}

/*****************************************************************************
 *
 * Routine to handle error reporting.  Allows the user to pass errors to log
 * files or report the information in whatever fashion is desired.
 *
 *****************************************************************************/
HANDEL_STATIC void dxp_md_error(const char* routine, const char* message,
                                int* error_code, const char *file, int line)
{
    dxp_md_log_header("[ERROR]", routine, error_code, file, line);
    fprintf(out_stream, "%s\n", message);
    fflush(out_stream);
}


/*****************************************************************************
 *
 * Routine to handle reporting warnings. Messages are written to the output
 * defined in out_stream.
 *
 *****************************************************************************/
HANDEL_STATIC void dxp_md_warning(const char *routine, const char *message,
                                  const char *file, int line)
{
    dxp_md_log_header("[WARN ]", routine, NULL, file, line);
    fprintf(out_stream, "%s\n", message);
    fflush(out_stream);
}


/*****************************************************************************
 *
 * Routine to handle reporting info messages. Messages are written to the
 * output defined in out_stream.
 *
 *****************************************************************************/
HANDEL_STATIC void dxp_md_info(const char *routine, const char *message,
                               const char *file, int line)
{
    dxp_md_log_header("[INFO ]", routine, NULL, file, line);
    fprintf(out_stream, "%s\n", message);
    fflush(out_stream);
}


/*****************************************************************************
 *
 * Routine to handle reporting debug messages. Messages are written to the
 * output defined in out_stream.
 *
 *****************************************************************************/
HANDEL_STATIC void dxp_md_debug(const char *routine, const char *message,
                                const char *file, int line)
{
    dxp_md_log_header("[DEBUG]", routine, NULL, file, line);
    fprintf(out_stream, "%s\n", message);
    fflush(out_stream);
}


/** Redirects the log output to either a file or a special descriptor
 * such as stdout or stderr. Allowed values for @a filename are: a
 * path to a file, "stdout", "stderr", "" (redirects to stdout) or
 * NULL (also redirects to stdout).
 */
HANDEL_SHARED void dxp_md_output(const char *filename)
{
    char *strtmp = NULL;

    unsigned int i;

    char info_string[INFO_LEN];

    if (!handel_md_mutex_ready(&lock))
        handel_md_mutex_create(&lock);

    if (out_stream != NULL && out_stream != stdout && out_stream != stderr) {
        fclose(out_stream);
    }

    if (filename == NULL || STREQ(filename, "")) {
        out_stream = stdout;
        return;
    }

    strtmp = malloc(strlen(filename) + 1);
    ASSERT(strtmp);


    for (i = 0; i < strlen(filename); i++) {
        strtmp[i] = (char)tolower((int)filename[i]);
    }
    strtmp[strlen(filename)] = '\0';


    if (STREQ(strtmp, "stdout")) {
        out_stream = stdout;

    } else if (STREQ(strtmp, "stderr")) {
        out_stream = stderr;

    } else {
        out_stream = fopen(filename, "w");

        if (!out_stream) {
            int status = XIA_OPEN_FILE;

            /* Reset to stdout with the hope that it is redirected
             * somewhere meaningful.
             */
            out_stream = stdout;
            sprintf(info_string, "Unable to open filename '%s' for logging. "
                    "Output redirected to stdout.", filename);
            dxp_md_error("dxp_md_output", info_string, &status, __FILE__, __LINE__);
        }
    }

    free(strtmp);
}
