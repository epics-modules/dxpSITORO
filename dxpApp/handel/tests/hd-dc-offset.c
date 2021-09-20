/*
 * This code accompanies the XIA Code and tests Handel via C.
 *
 * Copyright (c) 2005-2013 XIA LLC
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


#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "handel.h"
#include "handel_errors.h"
#include "handel_generic.h"
#include "md_generic.h"


static void CHECK_ERROR(int status);
static int SLEEP(float seconds);
static void INThandler(int sig);
static void usage(char *prog);
static void start(char *ini_file, char *log_name);
static void clean(void);
static int get_number_channels();

boolean_t stop = FALSE_;

int main(int argc, char *argv[])
{
    int  status;
    int  a;
    char ini[256] = "t_api/sandbox/xia_test_helper.ini";
    int  channels;
    int  i, j;
    int  iterations = 1;

    for (a = 1; a < argc; ++a) {
        if (argv[a][0] == '-') {
            switch (argv[a][1]) {
            case 'f':
                ++a;
                if (a >= argc) {
                    printf("error: no file provided\n");
                    exit (1);
                }
                strncpy(ini, argv[a], sizeof(ini) / sizeof(ini[0]));
                break;
            case 'i':
                ++a;
                if (a >= argc) {
                    printf("error: number of iterations provided\n");
                    exit (1);
                }
                iterations = atoi(argv[a]);
                break;
            default:
                printf("error: invalid option: %s\n", argv[a]);
                usage(argv[0]);
                exit(1);
            }
        }
        else {
            printf("error: invalid option: %s\n", argv[a]);
            usage(argv[0]);
            exit(1);
        }
    }

    signal(SIGINT, INThandler);
    start(ini, "handel.log");
    channels = get_number_channels();
    printf("DC Offset (iteration\\channel) \n i\\c");

    for (i = 0; i < channels; i++) {
        /* Zero the DC offset so we see it change. */
        double dc_offset = 0.0;
        status = xiaSetAcquisitionValues(i, "dc_offset", &dc_offset);
        CHECK_ERROR(status);
        /* Print a header for channels */
        printf("%7d  ", i);
    }

    for (j = 0; j < iterations; j++) {
        printf("\n%3d  ", j);
        for (i = 0; i < channels; i++) {
            int *ignore;
            status = xiaDoSpecialRun(i, "calc_dc_offset", &ignore);
            CHECK_ERROR(status);
        }

        for (i = 0; i < channels; i++) {
            double dc_offset = 0.0;
            fflush(stdout);
            status = xiaGetSpecialRunData(i, "dc_offset", &dc_offset);
            CHECK_ERROR(status);
            printf("%.6f,", dc_offset);
        }

        if (stop) break;
    }

    clean();
    return 0;
}

static void INThandler(int sig)
{
    UNUSED(sig);
    stop = TRUE_;
}

static void start(char *ini_file, char *log_name)
{
    int status;

    printf("Configuring Handel log file %s\n", log_name);
    xiaSetLogLevel(MD_DEBUG);
    xiaSetLogOutput(log_name);

    printf("Loading ini file %s\n", ini_file);
    status = xiaInit(ini_file);
    CHECK_ERROR(status);

    /* Boot hardware */
    status = xiaStartSystem();
    CHECK_ERROR(status);
}

static void clean(void)
{
    printf("\nCleaning up Handel\n");
    xiaExit();

    printf("Closing Handel log file\n");
    xiaCloseLog();
}


/*
 * This is just an example of how to handle error values.  A program
 * of any reasonable size should implement a more robust error
 * handling mechanism.
 */
static void CHECK_ERROR(int status)
{
    /* XIA_SUCCESS is defined in handel_errors.h */
    if (status != XIA_SUCCESS) {
        printf("\nError encountered! Status = %d %s\n", status, xiaGetErrorText(status));
        clean();
        exit(status);
    }
}


static void usage(char *prog)
{
    printf("%s options\n", prog);
    printf("Options and arguments:\n");
    printf(" -f file        : Handel INI file to load\n");
    printf(" -i iterations  : Number of iterations to do dc offset\n");

    return;
}

static int SLEEP(float seconds)
{
#if _WIN32
    DWORD wait = (DWORD)(1000.0 * seconds);
    Sleep(wait);
#else
    unsigned long secs = (unsigned long) seconds;
    struct timespec req = {
        .tv_sec = secs,
        .tv_nsec = ((seconds - secs) * 1000000000.0)
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

static int get_number_channels()
{
    int status;
    unsigned int m;

    char module[MAXALIAS_LEN];
    int channel_per_module = 0;

    unsigned int number_modules =  0;
    unsigned int number_channels  = 0;

    status = xiaGetNumModules(&number_modules);
    CHECK_ERROR(status);

    for (m = 0; m < number_modules; m++) {
        status = xiaGetModules_VB(m, module);
        CHECK_ERROR(status);

        status = xiaGetModuleItem(module, "number_of_channels", &channel_per_module);
        CHECK_ERROR(status);

        number_channels += channel_per_module;
    }

    return number_channels;
}

