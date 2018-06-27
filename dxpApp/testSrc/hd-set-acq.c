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
#include <stdlib.h>

#include "handel.h"
#include "handel_errors.h"
#include "handel_constants.h"
#include "md_generic.h"

typedef struct {
    const char *name;
    double a;
    double b;
} AcqNameValues;

static const AcqNameValues falconxn_values[] =
    {
        { "analog_gain", 15.887, 1 },
        { "analog_offset", 2047, 0 },
        { "detector_polarity", 0, 1 },
        { "dc_offset", -1, 0 },
        { "number_mca_channels", 2048, 4096 },
        { "mapping_mode", 1, 0 },
        { "reset_blanking_enable", 0, 1 },
        { "reset_blanking_threshold", 1.0, -0.999 },
        { "reset_blanking_presamples", 125, 4 },
        { "reset_blanking_postsamples", 1000, 4 },
        { "detection_threshold", 0.999, 0.0 },
        { "min_pulse_pair_separation", 1023, 0 },
        { "risetime_optimization", 0, 4000 },
        { "risetime_optimization", 4, 62 },
        { "detection_filter", XIA_FILTER_LOW_ENERGY, XIA_FILTER_HIGH_RATE },
        { "decay_time", XIA_DECAY_LONG, XIA_DECAY_SHORT },
        { "preset_type", 1, 0 },
        { "preset_value", 50, 0 },
        { "scale_factor", 1, 200 },
        { "num_map_pixels", 0, 1ull << 32 },
        { "num_map_pixels_per_buffer", 0, 1024 },
        { "num_map_pixels_per_buffer", -1, 1024 },
        { "pixel_advance_mode", 0, 1 },
        { NULL, 0, 0 }
    };


static void CHECK_ERROR(int status);
static int acq_set_check(int detChan, const char* name, double value);
static void ACQ_SET_CHECK2(int detChan, const char* name, double x, double y);

#define TOLERANCE 0.001

#define MAXITEM_LEN 256

static void usage(const char* prog)
{
    printf("%s options\n", prog);
    printf(" -f file       : Handel INI file to load\n");
}

int main(int argc, char *argv[])
{
    int a;
    char  ini[MAXITEM_LEN] = "t_api/sandbox/xia_test_helper.ini";
    int status;
    char module_type[MAXITEM_LEN];
    int channels = 1;
    int channel;
    const AcqNameValues* values = falconxn_values;

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

    if (strcmp(module_type, "falconxn") == 0) {
        values = falconxn_values;
    }
    else {
        printf("Unrecognized module type: %s\n", module_type);
        xiaExit();
        exit(2);
    }

    for (channel = 0; channel < channels; ++channel) {
        int ignored = 0;

        printf(" Channel: %d\n", channel);

        const AcqNameValues* value = values;
        while (value->name) {
            ACQ_SET_CHECK2(channel, value->name, value->a, value->b);
            ++value;
        }

        /* This isn't strictly needed, but we can test the board's ability to
         * to check params consistency.
         */
        status = xiaBoardOperation(channel, "apply", &ignored);
        CHECK_ERROR(status);
    }

    printf("Cleaning up Handel.\n");
    status = xiaExit();
    CHECK_ERROR(status);

    return 0;
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

/*
 * Sets the value and checks that an immediate Get returns the value.
 */
int acq_set_check(int detChan, const char* name, double value)
{
    int status;
    double get = 0;

    status = xiaSetAcquisitionValues(detChan, name, &value);
    if (status != XIA_SUCCESS) {
        printf("  %-30s:=  FAILED (%d)\n", name, status);
        return FALSE_;
    }

    printf("  %-30s:= %14.3f\n", name, value);

    status = xiaGetAcquisitionValues(detChan, name, &get);
    if (status != XIA_SUCCESS) {
        printf("  %-30s:  FAILED (%d)\n", name, status);
        return FALSE_;
    }

    if (fabs(get - value) > TOLERANCE) {
        printf("  %-30s:  FAILED (%d), expected %.6f, actual %.6f\n",
               name, status, value, get);
        return FALSE_;
    }

    return TRUE_;
}

/*
 * Sets two different values and confirm with Gets.
 */
void ACQ_SET_CHECK2(int detChan, const char* name, double x, double y)
{
    if (acq_set_check(detChan, name, x)) {
        acq_set_check(detChan, name, y);
    }
}
