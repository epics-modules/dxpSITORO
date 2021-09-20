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


#include <stdio.h>
#include <stdlib.h>

#include "handel.h"
#include "handel_errors.h"
#include "handel_generic.h"
#include "md_generic.h"

static void CHECK_ERROR(int status);
static void free_modules(char **modules, int len);

static void usage(const char* prog)
{
    printf("%s options\n", prog);
    printf(" -f file       : Handel INI file to load\n");
}


int main(int argc, char** argv)
{
    int status;

    unsigned int numModulesU;
    int numModules;
    char **modules;
    int i;

    char ini[MAX_PATH_LEN] = "t_api/sandbox/xia_test_helper.ini";
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

    printf("Querying the modules... ");
    status = xiaGetNumModules(&numModulesU);
    CHECK_ERROR(status);

    numModules = (int) numModulesU;

    modules = malloc(sizeof(char *) * numModulesU);
    if (!modules) {
        printf("No memory for module names\n");
        status = XIA_NOMEM;
        CHECK_ERROR(status);
    }

    for (i = 0; i < numModules; i++) {
        modules[i] = malloc(sizeof(char) * MAXALIAS_LEN);
        if (!modules[i]) {
            free_modules(modules, numModules);
            printf("No memory for module names\n");
            status = XIA_NOMEM;
            CHECK_ERROR(status);
        }
    }

    status = xiaGetModules(modules);
    if (status != XIA_SUCCESS) {
        free_modules(modules, numModules);
        CHECK_ERROR(status);
    }

    printf("%u configured.\n", numModules);

    /* Get info for each module */
    for (i = 0; i < numModules; i++) {
        int detChan;
        int boardChannelCount;
        unsigned int number_of_channels;
        char info[160];
        char firmware_version[32];

        /* Get the detChan of the first channel in the module. */
        status = xiaGetModuleItem(modules[i], "channel0_alias", &detChan);
        if (status != XIA_SUCCESS) {
            free_modules(modules, numModules);
            CHECK_ERROR(status);
        }

        /* Get info */
        printf("Info for module %s, detChan %d.\n",
               modules[i], detChan);
        status = xiaBoardOperation(detChan, "get_board_info", info);
        if (status != XIA_SUCCESS) {
            free_modules(modules, numModules);
            CHECK_ERROR(status);
        }

        status = xiaBoardOperation(detChan, "get_firmware_version", firmware_version);
        if (status != XIA_SUCCESS) {
            free_modules(modules, numModules);
            CHECK_ERROR(status);
        }

        /*
         * The board info is an array of characters with the following fields:
         *
         *   0(32): Product name.
         *  32(8) : Reserved.
         *  40(8) : Protocol version.
         *  48(32): Firmware version.
         *  80(32): Digital board serial number.
         * 112(32): Analog board serial number.
         *
         * Length is 144 bytes.
         */
        #define GET_INT(_a) (((_a)[0] << 24) | ((_a)[1] << 16) | ((_a)[2] << 8) | (_a)[3])

        printf("  Product name:       %s\n", &info[0]);
        printf("  Protocol version:   %d\n", GET_INT(&info[40]));
        printf("  Firmware version:   %s (%s)\n", &info[48], firmware_version);
        printf("  Digital board SN:   %s\n", &info[80]);
        printf("  Analog board SN:    %s\n", &info[112]);

        /* Check the channel count on the board. */
        status = xiaBoardOperation(detChan, "get_channel_count", &boardChannelCount);
        if (status != XIA_SUCCESS) {
            free_modules(modules, numModules);
            CHECK_ERROR(status);
        }

        /* And compare to the number defined in the Handel module,
         * which may be less.
         */
        status = xiaGetModuleItem(modules[i], "number_of_channels", &number_of_channels);
        if (status != XIA_SUCCESS) {
            free_modules(modules, numModules);
            CHECK_ERROR(status);
        }

        printf("  Channel count:      Board = %d, INI = %u\n",
               boardChannelCount, number_of_channels);
    }

    /* Clean up */
    free_modules(modules, numModules);

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
 * Clean up the allocated module aliases.
 */
static void free_modules(char **modules, int len)
{
    if (modules) {
        int i;

        for (i = 0; i < len; i++) {
            if (modules[i]) {
                free(modules[i]);
                modules[i] = NULL;
            }
        }

        free(modules);
    }
}
