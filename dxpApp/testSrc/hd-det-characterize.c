/*
 * This code accompanies the XIA Code and tests Handel via C.
 *
 * Copyright (c) 2005-2015 XIA LLC
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
#include <stdlib.h>
#include <ctype.h>

#include "handel.h"
#include "handel_errors.h"
#include "md_generic.h"


static void CHECK_ERROR(int status);
static int SEC_SLEEP(float *time);
static void CHECK_PULSES(const char* name, int channels);

#define MAXITEM_LEN 256

static void usage(const char* prog)
{
    printf("%s options\n", prog);
    printf(" -f file       : Handel INI file to load\n");
}


int main(int argc, char** argv)
{
    int status;
    int text_size;
    char* text;
    int percentage = 0;
    int* last_percentage;
    float interval = 0.0;
    int channels = 0;
    char module_type[MAXITEM_LEN];

    char ini[MAXITEM_LEN] = "t_api/sandbox/xia_test_helper.ini";
    char ini_save[] = "t_api/sandbox/hd-det-characterize.gen.ini";

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

    status = xiaGetModuleItem("module1", "module_type", module_type);
    CHECK_ERROR(status);

    status = xiaGetModuleItem("module1", "number_of_channels", &channels);
    CHECK_ERROR(status);

    /* Get progress text size */
    printf("Get progress text size.\n");
    status = xiaGetSpecialRunData(0, "detc-progress-text-size", &text_size);
    CHECK_ERROR(status);

    text = malloc(text_size);
    if (!text) {
        printf("No memory for progress text of length %d\n", text_size);
        status = xiaExit();
        if (status != XIA_SUCCESS)
            printf("Handel exit failed, Status = %d\n", status);
        exit(1);
    }

    last_percentage = malloc(((size_t) channels) * sizeof(int));
    if (!last_percentage) {
        free(text);
        printf("No memory for last percentages\n");
        status = xiaExit();
        if (status != XIA_SUCCESS)
            printf("Handel exit failed, Status = %d\n", status);
        exit(1);
    }

    memset(last_percentage, 0, ((size_t) channels) * sizeof(int));

    /* Start the characterization, then poll */
    printf("Characterize the detector via special run.\n");
    status = xiaDoSpecialRun(-1, "detc-start", NULL);
    CHECK_ERROR(status);

#define TIMEOUT (60.0f * 3)

    while (interval < TIMEOUT)
    {
        int running = 0;
        float waitfor = 0.050f;
        int channel;

        for (channel = 0; channel < channels; ++channel)
        {
            int channel_running = 0;
            status = xiaGetSpecialRunData(channel, "detc-running", &channel_running);
            CHECK_ERROR(status);

            if (channel_running)
                ++running;
        }

        printf("\r");

        for (channel = 0; channel < channels; ++channel)
        {
            status = xiaGetSpecialRunData(channel, "detc-percentage", &percentage);
            CHECK_ERROR(status);

            status = xiaGetSpecialRunData(channel, "detc-progress-text", &text[0]);
            CHECK_ERROR(status);
            printf("%3d%% %s%*c\r",
                   percentage, text, (int) (65 - strlen(text)), ' ');

            if (percentage != last_percentage[channel])
            {
                interval = 0;
                last_percentage[channel] = percentage;
            }
        }

        fflush(stdout);

        if (running == 0)
        {
            int successful = 0;

            printf("\n");

            for (channel = 0; channel < channels; ++channel)
            {
                int channel_successful = 0;
                status = xiaGetSpecialRunData(channel, "detc-successful", &channel_successful);
                CHECK_ERROR(status);

                printf("Characterize the detector finished: %d: %s\n",
                       channel, channel_successful ? "SUCCESSFUL" : "FAILURE");

                if (channel_successful)
                    ++successful;
            }

            if (successful == channels) {
                CHECK_PULSES("example", channels);
                CHECK_PULSES("model", channels);
                CHECK_PULSES("final", channels);
            } else {
                /* At least the FalconX returns an error getting calibration
                 * data in save system if characterization stopped as FAILED
                 * and we don't explicitly stop.
                 */
                printf("error: not all channels succeeded. Stopping detector characterization\n");
                status = xiaDoSpecialRun(-1, "detc-stop", NULL);
                CHECK_ERROR(status);

                free(last_percentage);
                free(text);
                status = xiaExit();
                CHECK_ERROR(status);
                exit(2);
            }

            break;
        }

        interval += waitfor;

        SEC_SLEEP(&waitfor);
    }

    free(last_percentage);
    free(text);

    if (interval >= TIMEOUT)
    {
        printf("error: no progress in 3 minutes. Stopping detector characterization\n");
        status = xiaDoSpecialRun(-1, "detc-stop", NULL);
        CHECK_ERROR(status);
    }

    printf("Saving the .ini file.\n");
    status = xiaSaveSystem("handel_ini", ini_save);
    CHECK_ERROR(status);

    printf("Cleaning up Handel.\n");
    status = xiaExit();
    CHECK_ERROR(status);

    return 0;
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
                           .tv_nsec = (time_t) ((*time - secs) * 1000000000.0f)
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
        printf("\nError encountered (exiting)! Status = %d\n", status);
        status2 = xiaDoSpecialRun(-1, "detc-stop", NULL);
        if (status2 != XIA_SUCCESS)
            printf("Stopping calibration failed, Status = %d\n", status2);
        status2 = xiaExit();
        if (status2 != XIA_SUCCESS)
            printf("Handel exit failed, Status = %d\n", status2);
        exit(status);
    }
}

static void CHECK_PULSES(const char* name, int channels)
{
    int     channel;
    int     status;
    int     size = 0;
    double* pulse;
    char    dataName[1024];

    for (channel = 0; channel < channels; ++channel)
    {
        sprintf(dataName, "detc-%s-pulse-size", name);
        status = xiaGetSpecialRunData(channel, dataName, &size);
        CHECK_ERROR(status);

        printf("%s pulse size: %d\n", name, size);

        pulse = malloc(sizeof(double) * (size_t) size);
        if (!pulse) {
            printf("No memory for the pulse array\n");
            status = XIA_NOMEM;
            CHECK_ERROR(status);
        }

        sprintf(dataName, "detc-%s-pulse-x", name);
        status = xiaGetSpecialRunData(channel, dataName, pulse);
        if (status != XIA_SUCCESS) {
            free(pulse);
            CHECK_ERROR(status);
        }

        sprintf(dataName, "detc-%s-pulse-y", name);
        status = xiaGetSpecialRunData(channel, dataName, pulse);
        free(pulse);
        CHECK_ERROR(status);
    }
}
