/*
 * This test requires user action to verify that get_connected correctly
 * returns false after the device is disconnected. Start the test, wait for
 * "sleeping" message, and turn off the FalconX. The test will proceed
 * automatically after a few seconds.
 *
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
static void CHECK_CONNECTED(int expected, int connected);

#define MAXITEM_LEN 256

static void usage(const char* prog)
{
    printf("%s options\n", prog);
    printf(" -f file       : Handel INI file to load\n");
}


int main(int argc, char** argv)
{
    int status;
    int connected;
    float sleepS;

    char ini[MAXITEM_LEN] = "t_api/sandbox/xia_test_helper.ini";
    boolean_t interactive = TRUE_;
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

            case 'h': /* headless, no wait for disconnect */
                interactive = FALSE_;
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

    /* Connected sanity check */
    printf("Checking connected status... ");
    status = xiaBoardOperation(0, "get_connected", &connected);
    CHECK_ERROR(status);
    CHECK_CONNECTED(TRUE_, connected);
    printf("ok\n");

    if (interactive) {
        sleepS = 5.0;
        printf("Sleeping %.1f seconds. Please turn off the device.\n", (double) sleepS);
        SEC_SLEEP(&sleepS);

        printf("Checking disconnected status... ");
        status = xiaBoardOperation(0, "get_connected", &connected);
        CHECK_ERROR(status);
        CHECK_CONNECTED(FALSE_, connected);
        printf("ok\n");
    }

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
    struct timespec req = {
                           .tv_sec = (time_t) (unsigned long) *time,
                           .tv_nsec = (time_t) ((*time) * 1000000000.0f)
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

static void CHECK_CONNECTED(int expected, int connected)
{
    if (connected != expected) {
        int status2;
        printf("Connected test failed (exiting)! Connected = %d, expected = %d\n",
               connected, expected);
        status2 = xiaExit();
        if (status2 != XIA_SUCCESS)
            printf("Handel exit failed, Status = %d\n", status2);
        exit(1);
    }
}
