/* MCA preset run, command line configurable */

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


#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#include "handel.h"
#include "handel_errors.h"
#include "md_generic.h"


static void do_run(unsigned short resume);
static int SEC_SLEEP(float *time);
static void CHECK_ERROR(int status);

#define MAXITEM_LEN 256
#define MAX_CHANNELS 4

static void usage(const char* prog)
{
    printf("%s options\n", prog);
    printf(" -f file         : Handel INI file to load\n");
    printf(" -t preset_type  : Preset run type\n");
    printf(" -v preset_value : Preset run value\n");
}

int main(int argc, char** argv)
{
    int status;

    char ini[MAXITEM_LEN] = "t_api/sandbox/xia_test_helper.ini";
    double preset_type = 0.0;
    double preset_value = 5.0;
    double mca_refresh = 0.2;
    int a;

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

            case 't':
                ++a;
                if (a >= argc) {
                    printf("error: no preset type provided\n");
                    exit (1);
                }
                preset_type = strtod(argv[a], NULL);
                break;

            case 'v':
                ++a;
                if (a >= argc) {
                    printf("error: no preset value provided\n");
                    exit (1);
                }
                preset_value = strtod(argv[a], NULL);
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

    /* Setup logging here */
    printf("Configuring the Handel log file.\n");
    xiaSetLogLevel(MD_DEBUG);
    xiaSetLogOutput("handel.log");

    printf("Loading the .ini file.\n");
    status = xiaInit(ini);
    CHECK_ERROR(status);

    /* Boot hardware */
    printf("Starting up the hardware.\n");
    status = xiaStartSystem();
    CHECK_ERROR(status);

    status = xiaSetAcquisitionValues(-1, "preset_type", &preset_type);
    CHECK_ERROR(status);

    status = xiaSetAcquisitionValues(-1, "preset_value", &preset_value);
    CHECK_ERROR(status);

    status = xiaSetAcquisitionValues(-1, "mca_refresh", &mca_refresh);
    CHECK_ERROR(status);

    /* Run until the preset ends it. */
    do_run(0);

    printf("Cleaning up Handel.\n");
    status = xiaExit();
    CHECK_ERROR(status);

    return 0;
}

static void do_run(unsigned short resume)
{
    int       status;
    uint32_t* accepted = NULL;
    uint32_t* rejected = NULL;
    int       size = 0;
    int       wait = 2;
    int       s;
    int       run_active = 0;

    /* Start MCA mode */
    printf("Start an MCA run.\n");
    status = xiaStartRun(0, resume);
    if (status != XIA_SUCCESS) {
        CHECK_ERROR(status);
    }

    status = xiaGetRunData(0, "mca_length", &size);
    if (status != XIA_SUCCESS) {
        CHECK_ERROR(status);
    }

    printf("MCA Length: %d\n", size);

    accepted = malloc(sizeof(uint32_t) * (size_t) size);
    if (!accepted) {
        printf("No memory for the accepted array\n");
        status = XIA_NOMEM;
        CHECK_ERROR(status);
    }

    /* Number of seconds to display the plot. */
    for (s = 0; ; s += wait) {
/*        float  delay = (float)wait; */
        float  delay = 0.2;
        int i;
        int itemp;
        double moduleStatistics[MAX_CHANNELS * 9];

        for (i=0; i<MAX_CHANNELS; i++) {
            run_active = 0;
            status = xiaGetRunData(i, "run_active", &itemp);
            if (status != XIA_SUCCESS) {
                free(accepted);
                CHECK_ERROR(status);
            }
            printf("\nChannel %d run_active=%d\n", i, itemp);
            if (itemp) run_active = 1;

            status = xiaGetRunData(i, "mca", accepted);
            if ((status != XIA_SUCCESS) && (status != XIA_NO_SPECTRUM)) {
                printf("Error calling xiaGetRunData(%d, mca), status=%d\n", i, status);
                free(accepted);
                CHECK_ERROR(status);
            }

/* If the call to xiaGetRunData for "module_statistics_2" comes before the call for "mca" then we get stale data */
            status = xiaGetRunData(i, "module_statistics_2", moduleStatistics);
            if (status != XIA_SUCCESS) {
                printf("Error calling xiaGetRunData(%d, module_statistics_2), status=%d\n", i, status);
                free(accepted);
                CHECK_ERROR(status);
            }

            printf("Input Count Rate: %7.2f kcps   Output Count Rate: %7.2f kcps"
                   "    Real time: %7.3f\n",
                   moduleStatistics[i*9+5]/1000, moduleStatistics[i*9+6]/1000, moduleStatistics[i*9]);

        }
        
        if (!run_active)
            break;

        SEC_SLEEP(&delay);
    }

    free(accepted);

    /* Stop MCA mode */
    if (run_active) {
        printf("Stop the MCA run.\n");
        status = xiaStopRun(0);
        if (status != XIA_SUCCESS) {
            CHECK_ERROR(status);
        }
    }
}


static int SEC_SLEEP(float *time)
{
#ifdef WIN32
    DWORD wait = (DWORD)(1000.0 * (*time));
    Sleep(wait);
#else
    unsigned long secs = (unsigned long) *time;
    struct timespec req = {
      .tv_sec = (time_t) secs,
      .tv_nsec = (time_t) ((*time - secs) * 1000000000.0)
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
 * This is just an example of how to handle error values.  A program
 * of any reasonable size should implement a more robust error
 * handling mechanism.
 */
static void CHECK_ERROR(int status)
{
    /* XIA_SUCCESS is defined in handel_errors.h */
    if (status != XIA_SUCCESS) {
        int status2;
        printf("Error encountered (exiting)! Status = %d\n", status);
        status2 = xiaExit();
        if (status2 != XIA_SUCCESS)
            printf("Handel exit failed, Status = %d\n", status2);
        exit(status);
    }
}
