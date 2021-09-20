/* Multiple MCA runs with stats and plots display. */

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
#include "handel_constants.h"
#include "md_generic.h"


static void do_run(unsigned short resume, int durationS);
static void plot_graph(uint32_t* accepted, uint32_t* rejected, int size);
static int SEC_SLEEP(float *time);
static void CHECK_ERROR(int status);

#define MAXITEM_LEN 256

#define MAX_CHANNELS 8

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

    /* Start MCA mode runs. First a short one, then a short one with
     * resume=true and a longer one with resume=false. */
    do_run(0, 1);
    do_run(1, 1);
    do_run(0, 2);

    printf("Cleaning up Handel.\n");
    status = xiaExit();
    CHECK_ERROR(status);

    return 0;
}

static void do_run(unsigned short resume, int durationS)
{
    int       status;
    uint32_t* accepted = NULL;
    uint32_t* rejected = NULL;
    int       size = 0;
    int       wait = 1;
    int       s;
    double    stats[MAX_CHANNELS * XIA_NUM_MODULE_STATISTICS];

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
    for (s = 0; s < durationS; s += wait) {
        float  delay = (float)wait;
        double icr = 0;
        double ocr = 0;
        double realtime = 0;

        SEC_SLEEP(&delay);

        status = xiaGetRunData(0, "mca", accepted);
        if (status != XIA_SUCCESS) {
            free(accepted);
            CHECK_ERROR(status);
        }

        status = xiaGetRunData(0, "input_count_rate", &icr);
        if (status != XIA_SUCCESS) {
            free(accepted);
            CHECK_ERROR(status);
        }

        status = xiaGetRunData(0, "output_count_rate", &ocr);
        if (status != XIA_SUCCESS) {
            free(accepted);
            CHECK_ERROR(status);
        }

        status = xiaGetRunData(0, "realtime", &realtime);
        if (status != XIA_SUCCESS) {
            free(accepted);
            CHECK_ERROR(status);
        }

        printf("\n       Input Count Rate: %7.2f   Output Count Rate: %7.2f    Real time: %7.3f\n", icr, ocr, realtime);

        status = xiaGetRunData(0, "module_statistics_2", stats);
        if (status != XIA_SUCCESS) {
            free(accepted);
            CHECK_ERROR(status);
        }

        printf("Module Input Count Rate: %7.2f   Output Count Rate: %7.2f    Real time: %7.3f\n", stats[5], stats[6], stats[0]);

        plot_graph(accepted, rejected, size);
    }

    free(accepted);

    /* Stop MCA mode */
    printf("Stop the MCA run.\n");
    status = xiaStopRun(0);
    if (status != XIA_SUCCESS) {
        CHECK_ERROR(status);
    }
}

static void plot_graph(uint32_t* accepted, uint32_t* rejected, int size)
{
    unsigned int a_min = UINT_MAX;
    unsigned int a_max = 0;
    unsigned int r_min = UINT_MAX;
    unsigned int r_max = 0;
    unsigned int x_max = (unsigned int) size;
    unsigned int x_unit;
    unsigned int y_max;
    unsigned int y_min;
    unsigned int y_unit;
    int s;
    int r;
    int c;

    #define COLS (70)
    #define ROWS (30)

    for (s = 0; s < size; ++s) {
        if (accepted) {
            if (accepted[s] > a_max)
                a_max = accepted[s];
            if (accepted[s] < a_min)
                a_min = accepted[s];
        }
        if (rejected) {
            if (rejected[s] > a_max)
                a_max = rejected[s];
            if (rejected[s] < a_min)
                a_min = rejected[s];
        }
    }

    x_unit = x_max / COLS;
    y_max = a_max > r_max ? a_max : r_max;
    y_min = a_min < r_min ? a_min : r_min;
    y_unit  = (y_max - y_min) / (ROWS - 1);

#define DEBUGGING_SUPPORT 0
#if DEBUGGING_SUPPORT
    printf("a_min=%u a_max=%u\n", a_min, a_max);
    printf("r_min=%u r_max=%u\n", r_min, r_max);
    printf("x_max=%u x_unit=%u\n", x_max, x_unit);
    printf("y_min=%u y_max=%u y_unit=%u\n", y_min, y_max, y_unit);
    for (s = 0; s < size; ++s) {
        if (accepted[s] != 0)
            printf("s=%03d a=%5u (%5u)\n", s, accepted[s], accepted[s] + a_min);
    }
#endif

    printf("\n");
    for (r = ROWS; r > 0; --r) {
      unsigned int y_top = ((unsigned int)r * y_unit) + y_min;
        unsigned int y_bot = y_top - y_unit;
        char         dot = ' ';
        printf(" %7d |", y_bot);
        for (s = 0; s < size; ++s) {
            if ((accepted[s] >= y_bot) && (accepted[s] <= y_top)) {
                dot = 'x';
            }
            if (s && ((s % (int)x_unit) == 0)) {
                printf("%c", dot);
                dot = ' ';
            }
        }
        printf("\n");
    }
    printf("         +");
    for (c = 0; c < COLS; ++c) {
        printf("-");
    }
    printf("\n");
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
        printf("Error encountered (exiting)! Status = %d\n", status);
        status2 = xiaExit();
        if (status2 != XIA_SUCCESS)
            printf("Handel exit failed, Status = %d\n", status2);
        exit(status);
    }
}
