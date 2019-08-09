/*
 * This code accompanies the XIA Code and tests Handel via C.
 *
 * Copyright (c) 2016 XIA LLC
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


static void do_tests(void);
static void SEC_SLEEP(float *time);
static void CHECK_ERROR(int status);

#define MAXITEM_LEN 256

static void usage(const char* prog)
{
    printf("%s options\n", prog);
    printf(" -f file       : Handel INI file to load\n");
}

int main(int argc, char** argv)
{
    int status;

    char ini[MAXITEM_LEN] = "t_api/sandbox/xia_test_helper.ini";
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

    /* Start tests */
    do_tests();

    printf("Cleaning up Handel.\n");
    status = xiaExit();
    CHECK_ERROR(status);

    return 0;
}

static void do_tests(void)
{
    int       status;

    double    size;
    unsigned short maxsize;
    char limit[9];

    int       durationS = 5;
    int       wait = 1;
    int       s;

    double sca_lo, sca_hi;
    double number_mca_channels;
    int    scaWidth;

    double *sca;

    /* Can we handle non-existent acq vaules which matches sca pattern? */
    status = xiaGetAcquisitionValues(0, "sca_time_off", &size);
    if(status != XIA_NOT_FOUND) {
        CHECK_ERROR(status);
    }

    double trigger = 0.0;
    status = xiaGetAcquisitionValues(0, "sca_trigger_mode", &trigger);
    CHECK_ERROR(status);

    trigger = 3.0;
    status = xiaSetAcquisitionValues(-1, "sca_trigger_mode", &trigger);
    CHECK_ERROR(status);

    trigger = 4.0;
    status = xiaSetAcquisitionValues(-1, "sca_trigger_mode", &trigger);
    if(status != XIA_BAD_VALUE) {
        CHECK_ERROR(status);
    }

    double duration = 0.0;
    status = xiaGetAcquisitionValues(0, "sca_pulse_duration", &duration);
    CHECK_ERROR(status);

    duration = 800.0;
    status = xiaSetAcquisitionValues(-1, "sca_pulse_duration", &duration);
    CHECK_ERROR(status);

    status = xiaGetRunData(0, "max_sca_length", &maxsize);
    CHECK_ERROR(status);

    size = maxsize;
    status = xiaSetAcquisitionValues(-1, "number_of_scas", &size);
    CHECK_ERROR(status);

    size = 0;
    status = xiaSetAcquisitionValues(-1, "number_of_scas", &size);
    CHECK_ERROR(status);

    size = maxsize;
    status = xiaSetAcquisitionValues(-1, "number_of_scas", &size);
    CHECK_ERROR(status);

    status = xiaGetAcquisitionValues(0, "number_of_scas", &size);
    CHECK_ERROR(status);

    printf("max_sca_length = %hu number_of_scas = %0.0f\n",
          maxsize, size);

    status = xiaGetAcquisitionValues(0, "number_mca_channels", &number_mca_channels);
    CHECK_ERROR(status);

    scaWidth = (int)(number_mca_channels / size);

    sca_hi = -1;
    for (int i = 0; i < size; i++) {
      sca_lo = sca_hi + 1.0;
      sprintf(limit, "sca%i_lo", i);
      status = xiaSetAcquisitionValues(-1, limit, &sca_lo);
      CHECK_ERROR(status);

      sca_hi += scaWidth;
      sprintf(limit, "sca%i_hi", i);
      status = xiaSetAcquisitionValues(-1, limit, &sca_hi);
      CHECK_ERROR(status);
    }

    printf("Limits:\n");

    for (int i = 0; i < size; i++) {
      sprintf(limit, "sca%i_lo", i);
      status = xiaGetAcquisitionValues(0, limit, &sca_lo);
      CHECK_ERROR(status);

      sprintf(limit, "sca%i_hi", i);
      status = xiaGetAcquisitionValues(0, limit, &sca_hi);
      CHECK_ERROR(status);

      printf("SCA%d: [%0.0f, %0.0f]\n", i, sca_lo, sca_hi);
    }

    /* Start MCA mode */
    printf("\nStart an MCA run of %d seconds.\n", durationS);
    status = xiaStartRun(0, 0);
    CHECK_ERROR(status);

    /* Number of seconds to display the plot. */
    for (s = 0; s < durationS; s += wait) {
        float  delay = (float)wait;
        printf(".");
        SEC_SLEEP(&delay);
    }

    /* Stop MCA mode */
    printf("\nStop the MCA run.\n");
    status = xiaStopRun(0);
    CHECK_ERROR(status);

    sca = malloc((size_t)size * sizeof(double));
    if (!sca) {
        printf("No memory for SCA data\n");
        CHECK_ERROR(XIA_NOMEM);
    }

    /* Read out the SCAs */
    printf("SCA counters:\n");

    status = xiaGetRunData(0, "sca", (void *)sca);
    if (status != XIA_SUCCESS) {
        free(sca);
        CHECK_ERROR(status);
    }

    for (int i = 0; i < size; i++) {
        printf(" SCA%d = %0.0f\n", i, sca[i]);
    }

    free(sca);
}


static void SEC_SLEEP(float *time)
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
