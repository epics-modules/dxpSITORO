/*
 * This code accompanies the XIA Code and tests Handel via C.
 *
 * Copyright (c) 2005-2012 XIA LLC
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


static void plot_graph(const char* title, double* x, double* y, int size);
static void CHECK_ERROR(int status);

static struct {
    const char* title;
    const char* size;
    const char* x;
    const char* y;
} pulses[3] =
{
    { "Example Pulse",
      "detc-example-pulse-size",
      "detc-example-pulse-x",
      "detc-example-pulse-y" },
    { "Model Pulse",
      "detc-model-pulse-size",
      "detc-model-pulse-x",
      "detc-model-pulse-y" },
    { "Final Pulse",
      "detc-final-pulse-size",
      "detc-final-pulse-x",
      "detc-final-pulse-y" }
};

#define MAXITEM_LEN 256

static void usage(const char* prog)
{
    printf("%s options\n", prog);
    printf(" -f file       : Handel INI file to load\n");
}

int main(int argc, char** argv)
{
    int status;
    int p;

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

    /* Get the defined pulse. */
    for (p = 0; p < 3; ++p) {
        int     size = 0;
        double* x;
        double* y;

        printf("Detector characterization: %s.\n", pulses[p].title);

        status = xiaGetSpecialRunData(0, pulses[p].size, &size);
        CHECK_ERROR(status);

        x = malloc(sizeof(double) * (size_t) size);
        if (!x) {
            printf("No memory for example pulse X array\n");
            status = XIA_NOMEM;
            CHECK_ERROR(status);
        }

        status = xiaGetSpecialRunData(0, pulses[p].x, x);
        if (status != XIA_SUCCESS) {
            free(x);
            CHECK_ERROR(status);
        }

        y = malloc(sizeof(double) * (size_t) size);
        if (!x) {
            free(x);
            printf("No memory for example pulse Y array\n");
            status = XIA_NOMEM;
            CHECK_ERROR(status);
        }

        status = xiaGetSpecialRunData(0, pulses[p].y, y);
        if (status != XIA_SUCCESS) {
            free(x);
            free(y);
            CHECK_ERROR(status);
        }

        plot_graph(pulses[p].title, x, y, size);

        free(x);
        free(y);
    }

    printf("Cleaning up Handel.\n");
    status = xiaExit();
    CHECK_ERROR(status);

    return 0;
}

static void plot_graph(const char* title, double* x, double* y, int size)
{
    double x_min = 0;
    double x_max = 0;
    double x_unit = 0;
    double y_min = 0;
    double y_max = 0;
    double y_unit = 0;
    int    s;
    int    r;
    int    c;

    #define COLS (70)
    #define ROWS (30)

    for (s = 0; s < size; ++s) {
        if (x[s] < x_min)
            x_min = x[s];
        if (x[s] > x_max)
            x_max = x[s];
        if (y[s] < y_min)
            y_min = y[s];
        if (y[s] > y_max)
            y_max = y[s];
    }

    x_unit = (x_max - x_min) / COLS;
    y_unit = (y_max - y_min) / ROWS;

#ifdef DEBUGGING_SUPPORT
    printf("x_min=%5.3f x_max=%5.3f x_unit=%5.3f y_min=%5.3f y_max=%5.3f y_unit=%5.3f\n",
           x_min, x_max, x_unit, y_min, y_max, y_unit);

    for (s = 0; s < size; ++s) {
        printf("s=%03d x=%5.3f (%5.3f) y=%5.3f (%5.3f)\n",
               s, x[s], x[s] + x_min, y[s], y[s] - y_min);
    }
#endif

    printf("\n %s\n\n", title);
    for (r = ROWS; r > 0; --r) {
        double r_top = (r * y_unit) + y_min;
        double r_bot = r_top - y_unit;
        printf(" %7.3f |", r_bot);
        for (s = 0, c = 0; c < COLS; ++c) {
            double x_top = (((c + 1) * x_unit) + x_min);
            char   dot = ' ';
            while ((s < size) && (x[s] < x_top)) {
                if ((y[s] >= r_bot) && (y[s] < r_top)) {
                    dot = '*';
                }
                ++s;
            }
            printf("%c", dot);
        }
        printf("\n");
    }
    printf("         +");
    for (c = 0; c < COLS; ++c) {
        printf("-");
    }
    printf("\n");
    printf("          %7.3f", x_min);
    for (c = 0; c < ((COLS / 2) - (7 + 4)); ++c) {
        printf(" ");
    }
    printf("%7.3f", ((COLS / 2) * x_unit) + x_min);
    for (c = 0; c < ((COLS / 2) - (3 + 7)); ++c) {
        printf(" ");
    }
    printf("%7.3f\n", x_max);
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
