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
#include <ctype.h>

#include "handel.h"
#include "handel_errors.h"
#include "md_generic.h"


static void plot_graph(unsigned int* adc_trace, int size, int scale);
static void CHECK_ERROR(int status);

static void usage(const char* prog)
{
    printf("%s options\n", prog);
    printf("Options and arguments:\n");
    printf(" -g gain       : Set the gain\n");
    printf(" -s step       : Set the step size\n");
    printf(" -i iterations : The number of traces to capture\n");
    printf(" -c count      : The number of samples\n");
    printf(" -f file       : Handel INI file to load\n");
}

int main(int argc, char *argv[])
{
    int           status;
    double        size = 8 * 1024;
    double        gain_value = 1.0;
    double        gain_step = 0.1;
    int           iters = 10;
    int           gain = 0;
    int           scale = 0;
    int           p;
    int           a;
    char          ini[256] = "t_api/sandbox/xia_test_helper.ini";

    for (a = 1; a < argc; ++a) {
        if (argv[a][0] == '-') {
            switch (argv[a][1]) {
                case 'g':
                    ++a;
                    if (a >= argc) {
                        printf("error: no gain value provided\n");
                        exit (1);
                    }
                    gain_value = strtod(argv[a], NULL);
                    gain = 1;
                    break;

                case 's':
                    ++a;
                    if (a >= argc) {
                        printf("error: no gain value provided\n");
                        exit (1);
                    }
                    gain_step = strtod(argv[a], NULL);
                    break;

                case 'i':
                    ++a;
                    if (a >= argc) {
                        printf("error: no iterations value provided\n");
                        exit (1);
                    }
                    iters = atoi(argv[a]);
                    break;

                case 'S':
                    scale = 1;
                    break;

                case 'c':
                    ++a;
                    if (a >= argc) {
                        printf("error: no count provided\n");
                        exit (1);
                    }
                    size = atoi(argv[a]);
                    break;
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

    printf("Loading the .ini file %s.\n", ini);
    status = xiaInit(ini);
    CHECK_ERROR(status);

    /* Boot hardware */
    printf("Starting up the hardware.\n");
    status = xiaStartSystem();
    CHECK_ERROR(status);

    /* Set the gain if the user has asked for this. */
    if (gain) {
        printf("Set gain: %.0f.\n", gain_value);
        status = xiaSetAcquisitionValues(0, "gain", &gain_value);
        if (status != XIA_SUCCESS) {
            CHECK_ERROR(status);
        }
    }

    /* Use special run which returns, then poll */
    printf("Start ADC Trace run.\n");
    /* Number of plots to make. */
    for (p = 0; p < iters; ++p) {
        unsigned int* adc_trace;
        unsigned long adc_trace_length;

        status = xiaDoSpecialRun(0, "adc_trace", &size);
        if (status != XIA_SUCCESS) {
            CHECK_ERROR(status);
        }

        /* Size may be coerced by doing the special run, so allocate
         * according to the new value.
         */
        adc_trace = malloc(sizeof(unsigned int) * (size_t)size);
        if (!adc_trace) {
            printf("No memory for ADC trace data\n");
            status = XIA_NOMEM;
            CHECK_ERROR(status);
        }

        /* Verify run data matches the info. */
        status = xiaGetSpecialRunData(0, "adc_trace_length", &adc_trace_length);
        if (status != XIA_SUCCESS ||
            adc_trace_length != (unsigned long)size) {
            printf("adc_trace_length %lu should match trace info %f.\n",
                   adc_trace_length, size);
            free(adc_trace);
            CHECK_ERROR(status);
        }

        status = xiaGetSpecialRunData(0, "adc_trace", adc_trace);
        if (status != XIA_SUCCESS) {
            free(adc_trace);
            CHECK_ERROR(status);
        }

        plot_graph(adc_trace, (int)size, scale);

        free(adc_trace);
    }


    printf("Cleaning up Handel.\n");
    status = xiaExit();
    CHECK_ERROR(status);

    return 0;
}

static void plot_graph(unsigned int* adc_trace, int size, int scale)
{
    unsigned int r_min = UINT_MAX;
    unsigned int r_max = 0;
    unsigned int r_unit = 0;
    unsigned int x_unit;
    int s;
    int r;
    int c;

    #define COLS (70)
    #define ROWS (40)

    if (scale) {
        for (s = 0; s < size; ++s) {
            if (adc_trace[s] > r_max)
                r_max = adc_trace[s];
            if (adc_trace[s] < r_min)
                r_min = adc_trace[s];
        }
    }
    else {
        r_min = 0;
        r_max = 0x10000;
    }

    x_unit = (unsigned int) size / COLS;
    r_unit  = (r_max - r_min) / (ROWS - 1);

#ifdef DEBUGGING_SUPPORT
    printf("r_min=%u r_max=%u r_unit=%u\n", r_min, r_max, r_unit);
    for (s = 0; s < size; ++s) {
        printf("s=%03d r=%5u (%5u)\n", s, adc_trace[s], adc_trace[s] + r_min);
    }
#endif

    printf("\n");
    for (r = ROWS; r >= 0; --r) {
        unsigned int r_top = ((unsigned int)r * r_unit) + r_min;
        unsigned int r_bot = r_top - r_unit;
        char         dot = ' ';
        printf(" %7d |", r_top);
        for (s = 0; s < size; ++s) {
            if ((adc_trace[s] >= r_bot) && (adc_trace[s] < r_top)) {
                dot = 'x';
            }
            if (s && ((s % (int) x_unit) == 0)) {
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
