/*
 * This code accompanies the XIA Code and tests Handel via C.
 *
 * Copyright (c) 2005-2016 XIA LLC
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

/*
 * Exercises the xMAP list-mode functionality by repeatedly reading
 * out the buffers as fast as possible.
 */

#ifdef WIN32
#include <windows.h>
#endif

#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "xia_common.h"

#include "handel.h"
#include "handel_errors.h"

#include "md_generic.h"


static int SEC_SLEEP(double time);
static void print_usage(void);


#define A 0
#define B 1

#define SWAP_BUFFER(x) ((x) == A ? B : A)

#define MAX_DET_CHANNELS (8)

int main(int argc, char *argv[])
{
    const char* ini = "t_api/sandbox/xia_test_helper.ini";
    const char* data_prefix = "test_mm1";

    int status;
    int ignore;

    double mode = 1.0;
    double variant = 2.0;
    double num_map_pixels = 0.0;
    double num_map_pixels_per_buffer = 16.0;
    double n_secs = 0.0;
    double n_hrs = 0.0;
    int sync = 0; /* if true no manual advance */

    time_t start;

    const char *buffer_str[2] = {
        "buffer_a",
        "buffer_b"
    };

    const char *buffer_full_str[2] = {
        "buffer_full_a",
        "buffer_full_b"
    };

    const char buffer_done_char[2] = {
        'a',
        'b'
    };

    int det_channels = 4;
    int det;

    FILE* fp[MAX_DET_CHANNELS];
    int current[MAX_DET_CHANNELS];
    int buffer_number[MAX_DET_CHANNELS];

    unsigned long bufferLength = 0;
    size_t bufferSize = 0;
    uint32_t *buffer = NULL;

    double wait_period = 0.050; /* 50 msecs */
    int quiet = 0;

    int arg = 1;

    while (arg < argc) {
        if (argv[arg][0] == '-') {
            if (strlen(argv[arg]) != 2) {
                fprintf(stderr, "error: invalid option: %s\n", argv[arg]);
                exit(1);
            }

            switch (argv[arg][1]) {
            case 'f':
                ++arg;
                if (arg >= argc) {
                    fprintf(stderr, "error: -f requires a file\n");
                    exit(1);
                }
                ini = argv[arg];
                ++arg;
                break;
            case 'D':
                ++arg;
                if (arg >= argc) {
                    fprintf(stderr, "error: -D requires a label\n");
                    exit(1);
                }
                data_prefix= argv[arg];
                ++arg;
                break;
            case 'H':
                ++arg;
                if (arg >= argc) {
                    fprintf(stderr, "error: -H requires the hours\n");
                    exit(1);
                }
                sscanf(argv[arg], "%lf", &n_hrs);
                ++arg;
                break;
            case 'S':
                ++arg;
                if (arg >= argc) {
                    fprintf(stderr, "error: -S requires the seconds\n");
                    exit(1);
                }
                sscanf(argv[arg], "%lf", &n_secs);
                ++arg;
                break;
            case 'P':
                ++arg;
                if (arg >= argc) {
                    fprintf(stderr, "error: -P requires the number of pixels\n");
                    exit(1);
                }
                sscanf(argv[arg], "%lf", &num_map_pixels);
                ++arg;
                break;
            case 'B':
                ++arg;
                if (arg >= argc) {
                    fprintf(stderr,
                            "error: -B requires the number of buffer pixels\n");
                    exit(1);
                }
                sscanf(argv[arg], "%lf", &num_map_pixels_per_buffer);
                ++arg;
                break;
            case 's':
                sync = 1;
                ++arg;
                break;
            case 'w':
                ++arg;
                if (arg >= argc) {
                    fprintf(stderr,
                            "error: -w requires the number milli-seconds\n");
                    exit(1);
                }
                sscanf(argv[arg], "%lf", &wait_period);
                ++arg;
                break;
            case 'd':
                ++arg;
                if (arg >= argc) {
                    fprintf(stderr,
                            "error: -d requires the number of detector channels\n");
                    exit(1);
                }
                sscanf(argv[arg], "%d", &det_channels);
                ++arg;
                break;
            case 'q':
                quiet = 1;
                break;
            case '?':
                print_usage();
                exit(0);
            default:
                fprintf(stderr, "error: invalid option; try -?\n");
                exit(1);
            }
        } else {
            fprintf(stderr, "error: invalid option; try -?\n");
            exit(1);
        }
    }

    if (num_map_pixels > 0) {
        if (n_secs > 0 || n_hrs > 0) {
            fprintf(stderr, "error: number of pixels and seconds or hours set\n");
            exit(1);
        }
    } else {
        if (n_secs > 0 && n_hrs > 0) {
            fprintf(stderr, "error: seconds and hours set\n");
            exit(1);
        }
        if (n_hrs > 0)
            n_secs = (double)n_hrs * 60.0 * 60.0;
    }

    if (n_secs == 0.0) {
        n_secs = 30.0;
    }

    fprintf(stdout, "MM1 Capture\n");
    fprintf(stdout, "  INI: %s\n", ini);
    fprintf(stdout, "  Data prefix: %s\n", data_prefix);

    if (num_map_pixels > 0)
        fprintf(stdout, "  Pixels: %d Pixels per buffer: %d\n",
                (int) num_map_pixels, (int) num_map_pixels_per_buffer);
    else if (n_hrs > 0)
        fprintf(stdout, "  Hours: %d Pixels per buffer: %d\n",
                (int) n_hrs, (int) num_map_pixels_per_buffer);
    else
        fprintf(stdout, "  Seconds: %d Pixels per buffer: %d\n",
                (int) n_secs, (int) num_map_pixels_per_buffer);

    if (quiet == 0.0)
        xiaSetLogLevel(MD_DEBUG);
    xiaSetLogOutput("handel.log");

    status = xiaInit(ini);

    if (status != XIA_SUCCESS) {
        fprintf(stderr, "Unable to initialize Handel using '%s'.\n", ini);
        exit(1);
    }

    status = xiaStartSystem();

    if (status != XIA_SUCCESS) {
        fprintf(stderr, "Unable to start the system.\n");
        exit(1);
    }

    status = xiaGetModuleItem("module1", "number_of_channels", &det_channels);

    if (status != XIA_SUCCESS) {
        fprintf(stderr, "Unable to get the number of channels.\n");
        exit(1);
    }

    /* Switch to the mode. */
    status = xiaSetAcquisitionValues(-1, "mapping_mode", &mode);

    if (status != XIA_SUCCESS) {
        xiaExit();

        fprintf(stderr, "Error setting 'mapping_mode' to %.1f.\n", mode);
        exit(1);
    }

    if (mode == 1.0) {
        status = xiaSetAcquisitionValues(-1, "num_map_pixels_per_buffer",
                                         &num_map_pixels_per_buffer);

        if (status != XIA_SUCCESS) {
            xiaExit();

            fprintf(stderr, "Error setting 'num_map_pixels_per_buffer' to %.1f.\n",
                    num_map_pixels_per_buffer);
            exit(1);
        }
    }
    else if (mode == 3.0) {
        status = xiaSetAcquisitionValues(-1, "list_mode_variant", &variant);

        if (status != XIA_SUCCESS) {
            xiaExit();

            fprintf(stderr, "Error setting 'list_mode_variant' to %.1f.\n",
                    variant);
            exit(1);
        }
    }

    if (num_map_pixels > 0) {
        status = xiaSetAcquisitionValues(-1, "num_map_pixels", &num_map_pixels);

        if (status != XIA_SUCCESS) {
            xiaExit();

            fprintf(stderr, "Error setting 'num_map_pixels' to %.1f.\n",
                    num_map_pixels);
            exit(1);
        }
    }

    for (det = 0; det < det_channels; ++det) {
        status = xiaBoardOperation(det, "apply", &ignore);

        if (status != XIA_SUCCESS) {
            xiaExit();

            fprintf(stderr, "Error applying the mode settings.\n");
            exit(1);
        }
    }

    status = xiaGetRunData(0, "buffer_len", &bufferLength);

    if (status != XIA_SUCCESS) {
        xiaExit();

        fprintf(stderr, "Error reading 'buffer_len'.\n");
        exit(1);
    }

    bufferSize = bufferLength * sizeof(uint32_t);
    buffer = malloc(bufferSize);

    if (!buffer) {
        xiaStopRun(-1);
        xiaExit();

        fprintf(stderr, "Unable to allocate a buffer of %zu bytes.\n",
                bufferSize);
        exit(1);
    }

    fprintf(stdout, "  Buffer length: %lu (%zu bytes).\n",
            bufferLength, bufferSize);

    memset(buffer, 0, bufferSize);

    for (det = 0; det < det_channels; ++det) {
        char name[256];

        current[det] = A;
        buffer_number[det] = 0;

        sprintf(name, "%s_d%02d.bin", data_prefix, det);

        fp[det] = fopen(name, "wb");

        if (!fp[det]) {
            int d;
            fprintf(stderr, "Unable to open '%s' for writing.\n", name);
            free(buffer);
            for (d = 0; d < det; ++d)
                fclose(fp[d]);
            exit(1);
        }
    }

    fprintf(stdout, "Starting MM1 run.\n");

    status = xiaStartRun(-1, 0);

    if (status != XIA_SUCCESS) {
        xiaExit();

        fprintf(stderr, "Error starting the list-mode run.\n");
        for (det = 0; det < det_channels; ++det)
            fclose(fp[det]);
        free(buffer);
        exit(1);
    }

    /* The algorithm here is to read the current buffer, let the
     * hardware know we are done with it, write the raw buffer to disk
     * and then read the other buffer, etc.
     */
    time(&start);

    for (;;) {
        int any_buffer_full = 0;
        int any_running = 0;
        int buffer_full[MAX_DET_CHANNELS];
        unsigned long active[MAX_DET_CHANNELS];
        unsigned long det_current_pixel = 0;
        int polls = 0;

        double now = difftime(time(NULL), start);

        do {
            if ((num_map_pixels == 0) && (now >= n_secs))
                break;

            any_running = 0;

            for (det = 0; det < det_channels; ++det) {
                int buffer_overrun = 0;

                active[det] = 0;
                buffer_full[det] = 0;

                status = xiaGetRunData(det, "run_active", &active[det]);

                if (status != XIA_SUCCESS) {
                    xiaStopRun(-1);
                    xiaExit();

                    fprintf(stderr, "Error get the run active status'.\n");
                    for (det = 0; det < det_channels; ++det)
                        fclose(fp[det]);
                    free(buffer);
                    exit(1);
                }

                if (active[det]) {
                    int dummy = 0;

                    any_running = 1;

                    if (!sync && (mode == 1.0)) {
                        status = xiaBoardOperation(det,
                                                   "mapping_pixel_next",
                                                   &dummy);
                    }
                }

                status = xiaGetRunData(det,
                                       buffer_full_str[current[det]],
                                       &buffer_full[det]);

                if (status != XIA_SUCCESS) {
                    xiaStopRun(-1);
                    xiaExit();

                    fprintf(stderr, "Error getting the status of buffer '%c'.\n",
                            buffer_done_char[current[det]]);
                    for (det = 0; det < det_channels; ++det)
                        fclose(fp[det]);
                    free(buffer);
                    exit(1);
                }

                status = xiaGetRunData(det, "buffer_overrun", &buffer_overrun);

                if (status != XIA_SUCCESS) {
                    xiaStopRun(-1);
                    xiaExit();

                    fprintf(stderr,
                            "Error getting the overrun status of buffer '%c'.\n",
                            buffer_done_char[current[det]]);
                    for (det = 0; det < det_channels; ++det)
                        fclose(fp[det]);
                    free(buffer);
                    exit(1);
                }

                if (buffer_overrun) {
                    xiaStopRun(-1);
                    xiaExit();

                    fprintf(stderr, "Buffer overrun.\n");
                    for (det = 0; det < det_channels; ++det)
                        fclose(fp[det]);
                    free(buffer);
                    exit(1);
                }

                if (buffer_full[det])
                    any_buffer_full = 1;
            }

            if (!any_buffer_full)
                SEC_SLEEP(wait_period);

            ++polls;

        } while (any_running && !any_buffer_full && (polls < (10 / wait_period)));

        if ((num_map_pixels == 0) && (now >= n_secs))
            break;

        if (!any_buffer_full) {
            xiaStopRun(-1);
            xiaExit();

            fprintf(stderr, "Timeout on buffer filling.\n");
            for (det = 0; det < det_channels; ++det)
                fclose(fp[det]);
            free(buffer);
            exit(1);
        }

        printf("%d ", (int) now);
        for (det = 0; det < det_channels; ++det)
            printf("%d:%s/%s ",
                   det,
                   active[det] ? "ACTIVE" : "ready",
                   buffer_full[det] ? "FULL" : "empty");
        printf("\n");

        for (det = 0; det < det_channels; ++det) {
            if (buffer_full[det]) {
                status = xiaGetRunData(det, buffer_str[current[det]], buffer);

                if (status != XIA_SUCCESS) {
                    xiaStopRun(-1);
                    xiaExit();

                    fprintf(stderr, "Error reading '%s'.\n",
                            buffer_str[current[det]]);
                    for (det = 0; det < det_channels; ++det)
                        fclose(fp[det]);
                    free(buffer);
                    exit(1);
                }

                char c = buffer_done_char[current[det]];
                status = xiaBoardOperation(det, "buffer_done", (void*) &c);

                if (status != XIA_SUCCESS) {
                    xiaStopRun(-1);
                    xiaExit();

                    fprintf(stderr, "Error setting buffer '%c' to done.\n",
                            buffer_done_char[current[det]]);
                    for (det = 0; det < det_channels; ++det)
                        fclose(fp[det]);
                    free(buffer);
                    exit(1);
                }

                status = xiaGetRunData(det,
                                       buffer_full_str[current[det]],
                                       &buffer_full[det]);

                if (status != XIA_SUCCESS) {
                    xiaStopRun(-1);
                    xiaExit();

                    fprintf(stderr, "Error getting the status of buffer '%c' after buffer_done.\n",
                            buffer_done_char[current[det]]);
                    for (det = 0; det < det_channels; ++det)
                        fclose(fp[det]);
                    free(buffer);
                    exit(1);
                }

                status = xiaGetRunData(det, "current_pixel", &det_current_pixel);

                if (status != XIA_SUCCESS) {
                    xiaStopRun(-1);
                    xiaExit();

                    fprintf(stderr, "Error getting the current pixel\n");
                    for (det = 0; det < det_channels; ++det)
                        fclose(fp[det]);
                    free(buffer);
                    exit(1);
                }

                fprintf(stdout, "Buffer write: det: %d buffer:%d/%c pixel:%lu full:%d\n",
                        det, buffer_number[det],
                        buffer_done_char[current[det]], det_current_pixel,
                        buffer_full[det]);

                if (fwrite(&buffer[0], sizeof(uint32_t),
                           bufferLength, fp[det]) != bufferLength) {
                    xiaStopRun(-1);
                    xiaExit();

                    fprintf(stderr, "Error writing buffer.\n");
                    for (det = 0; det < det_channels; ++det)
                        fclose(fp[det]);
                    free(buffer);
                    exit(1);
                }

                current[det] = SWAP_BUFFER(current[det]);
                buffer_number[det]++;
            }
        }

        if (!any_running) {
            /* Verify buffer full reports false after all pixels are processed. */
            if (num_map_pixels > 0) {
                for (det = 0; det < det_channels; ++det) {
                    status = xiaGetRunData(det,
                                           buffer_full_str[current[det]],
                                           &buffer_full[det]);

                    if (status != XIA_SUCCESS) {
                        xiaStopRun(-1);
                        xiaExit();

                        fprintf(stderr, "Error getting the status of buffer '%c'.\n",
                                buffer_done_char[current[det]]);

                        for (det = 0; det < det_channels; ++det)
                            fclose(fp[det]);
                        free(buffer);
                        exit(1);
                    }

                    if (buffer_full[det]) {
                        xiaStopRun(-1);
                        xiaExit();

                        fprintf(stderr, "Buffer '%c' reports full after run stop.",
                                buffer_done_char[current[det]]);

                        for (det = 0; det < det_channels; ++det)
                            fclose(fp[det]);
                        free(buffer);
                        exit(1);
                    }
                }
            }

            break;
        }
    }


    for (det = 0; det < det_channels; ++det)
        fclose(fp[det]);
    free(buffer);

    status = xiaStopRun(-1);

    if (status != XIA_SUCCESS) {
        xiaExit();

        fprintf(stderr, "Error stopping the mapping mode run.\n");
        exit(1);
    }


    xiaExit();

    return 0;
}


static int SEC_SLEEP(double time)
{
#ifdef WIN32
    DWORD wait = (DWORD)(1000.0 * (time));
    Sleep(wait);
#else
    unsigned long secs = (unsigned long) time;
    struct timespec req = {
        .tv_sec = (time_t) secs,
        .tv_nsec = (time_t) ((time - secs) * 1000000000.0)
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


static void print_usage(void)
{
    fprintf(stdout,
            "hd-mm1 [optios]\n" \
            "options and arguments: \n" \
            " -?           : help\n" \
            " -f file      : INI file\n" \
            " -D label     : data prefix label\n" \
            " -H hours     : hours to run the capture\n" \
            " -S seconds   : seconds to run the capture\n" \
            " -P pixels    : pixels to capture\n" \
            " -B pixels    : pixels per buffer\n" \
            " -s           : external sync, no manual pixel advance\n" \
            " -w msecs     : wait period in milli-seconds\n" \
            " -d detectors : number of detector channels\n" \
            " -q           : quiet, no Handel debug output\n" \
            "Where:\n" \
            " Pixels to capture overrides hours which overrides seconds.\n" \
            " Wait time in milli-seconds defines the polling rate.\n");
    return;
}
