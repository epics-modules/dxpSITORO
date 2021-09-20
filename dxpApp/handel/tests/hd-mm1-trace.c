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


#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_CHANNELS 8

#define XMAP_MAPPING_TICKS 0.00000032

static void plot_graph(uint32_t* accepted, uint32_t* rejected, int size);

static void usage(const char* prog)
{
    printf("%s options\n", prog);
    printf(" -f file : Handel MM1 file to trace\n");
    printf(" -p      : Plot the spectrum\n");
}

static int header_read32(uint16_t* buffer)
{
    return (int) ((((uint32_t) buffer[1]) << 16) | (uint32_t) buffer[0]);
}

int main(int argc, char** argv)
{
    char* file = NULL;

    int a;

    int i;

    FILE* fp;

    struct stat sb;

    uint32_t* buffer;

    size_t size;

    size_t index = 0;

    int pixel[MAX_CHANNELS];

    int plot = 0;

    for (a = 1; a < argc; ++a) {
        if (argv[a][0] == '-') {
            switch (argv[a][1]) {
                case 'f':
                    ++a;
                    if (a >= argc) {
                        fprintf(stderr, "error: no file provided\n");
                        exit (1);
                    }
                    file = argv[a];
                    break;
                case 'p':
                    plot = 1;
                    break;
                case '?':
                case 'h':
                    usage(argv[0]);
                    exit(1);
                default:
                    fprintf(stderr, "error: invalid option: %s\n", argv[a]);
                    usage(argv[0]);
                    exit(1);
            }
        }
        else {
            fprintf(stderr, "error: invalid option: %s\n", argv[a]);
            usage(argv[0]);
            exit(1);
        }
    }

    if (!file) {
        fprintf(stderr, "error: no file\n");
        usage(argv[0]);
        exit(1);
    }

    for (i = 0; i < MAX_CHANNELS; i++)
        pixel[i] = 0;

    printf("Loading Handel MM1 trace file: %s\n", file);

    if (stat(file, &sb) < 0) {
        fprintf(stderr, "error: file stat: %s\n", strerror(errno));
        exit(1);
    }

    size = (size_t) sb.st_size;

    buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "error: no memory, file too large\n");
        exit(1);
    }

    fp = fopen(file, "rb");
    if (!fp) {
        fprintf(stderr, "error: file open: %s\n", strerror(errno));
        free(buffer);
        exit(1);
    }

    if (fread(buffer, 1, size, fp) != size) {
        fprintf(stderr, "error: file read: %s\n", strerror(errno));
        fclose(fp);
        free(buffer);
        exit(1);
    }

    fclose(fp);

    size /= sizeof(uint32_t);

    while (index < size) {
        int p;

        uint16_t* header = (uint16_t*) &buffer[index];

        int header_size;
        int mode;
        int run_number;
        int buffer_num;
        int buffer_id;
        int pixel_count;
        int start_pixel;
        int det_chan;

        if (header[0] != 0x55aa || header[1] != 0xaa55)
            break;

        header_size = header[2];
        mode = header[3];
        run_number = header[4];
        buffer_num = header_read32(&header[5]);
        buffer_id = header[7];
        pixel_count = header[8];
        start_pixel = header_read32(&header[9]);
        det_chan    = header[12];

        if (mode != 1) {
            fprintf(stderr, "error: bad mode, XMAP header @ %08x\n",
                    (int) (index * sizeof(uint32_t)));
            free(buffer);
            exit(2);
        }

        if (buffer_id < 0 || buffer_id > 1) {
            fprintf(stderr, "error: bad buffer id, XMAP header @ %08x\n",
                    (int) (index * sizeof(uint32_t)));
            free(buffer);
            exit(2);
        }

        if (det_chan >= MAX_CHANNELS) {
            fprintf(stderr, "error: detChan larger than max channels, XMAP header @ %08x\n",
                    (int) (index * sizeof(uint32_t)));
            free(buffer);
            exit(2);
        }

        /*
         * Check that pixels are ordered correctly for each detChan.
         * We may have multiple channels, each with their own buffers.
         * For example, assuming two channels and N pixels per buffer,
         * we expect this order of buffers:
         *
         * - ch 0, buf 0, pix [0,N-1]
         * - ch 1, buf 0, pix [0,N-1]
         * - ch 0, buf 1, pix [N,2N-1]
         * - ch 1, buf 1, pix [N,2N-1]
         */
        if (start_pixel != pixel[det_chan]) {
            fprintf(stderr, "error: bad start pixel, XMAP header @ %08x\n",
                    (int) (index * sizeof(uint32_t)));
            free(buffer);
            exit(2);
        }

        printf("BUFFER: [0x%08x:0x%08x] num:%4d id:%c detChan:%4d pixels:%4d pixel:%4d\n",
               (int) (index * sizeof(uint32_t)),
               (int) ((index + (size_t) header_size / 2) * sizeof(uint32_t)) - 1,
               buffer_num, buffer_id == 0 ? 'A' : 'B',
               det_chan, pixel_count, start_pixel);

        index += (size_t) header_size / 2;

        for (p = 0; p < pixel_count; ++p) {
            int px_header_size;
            int px_mode;
            int px_number;
            int px_block_size;
            int px_ch_size;
            int px_realtime;
            int px_livetime;
            int px_triggers;
            int px_output_events;

            header = (uint16_t*) &buffer[index];

            if (header[0] != 0x33cc || header[1] != 0xcc33) {
                fprintf(stderr, "error: bad tags, XMAP pixel @ %08x\n",
                        (int) (index * sizeof(uint32_t)));
                free(buffer);
                exit(2);
            }

            px_header_size = header[2];
            px_mode = header[3];
            px_number = header_read32(&header[4]);
            px_block_size = header_read32(&header[6]);
            px_ch_size = header[8];
            px_realtime = header_read32(&header[32]);
            px_livetime = header_read32(&header[34]);
            px_triggers = header_read32(&header[36]);
            px_output_events = header_read32(&header[38]);

            if (mode != 1) {
                fprintf(stderr, "error: bad mode: %d, XMAP pixel @ %08x\n",
                        mode, (int) (index * sizeof(uint32_t)));
                free(buffer);
                exit(2);
            }

            if (px_number != pixel[det_chan]) {
                fprintf(stderr, "error: bad pixel: %d, XMAP pixel @ %08x\n",
                        px_number, (int) (index * sizeof(uint32_t)));
                free(buffer);
                exit(2);
            }

            if ((px_block_size - px_header_size) != px_ch_size) {
                fprintf(stderr, "error: size mismatch: header:%6d total:%6d ch0:%6d\n",
                        px_header_size, px_block_size, px_ch_size);
                free(buffer);
                exit(2);
            }

            ++pixel[det_chan];

            printf(" PIXEL: [0x%08x:0x%08x] num:%4d size:%6d chsize:%6d realtime:%10.3f " \
                   "livetime:%10.3f triggers:%10d output-events:%10d\n",
                   (int) (index * sizeof(uint32_t)),
                   (int) ((index + (size_t)px_block_size / 2) * sizeof(uint32_t)) - 1,
                   px_number, px_block_size, px_ch_size,
                   px_realtime * XMAP_MAPPING_TICKS, px_livetime * XMAP_MAPPING_TICKS,
                   px_triggers, px_output_events);

            if (plot) {
                plot_graph(&buffer[index + ((size_t) px_header_size / 2)], NULL,
                           (px_block_size - px_header_size) / 2);
                printf("\n");
            }

            index += (size_t) px_block_size / 2;
        }
    }

    if (index != size)
        printf("BUFFER: 0x%08x bytes in file remaining\n",
               (int) ((size - (size_t) index) * sizeof(uint32_t)));

    return 0;
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
