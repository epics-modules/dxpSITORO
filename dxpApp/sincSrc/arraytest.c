#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "sincarray.h"

#define NUM_BOARDS 2
#define CHANNELS_PER_BOARD 24


int main()
{
    printf("sinc array test.\n");

    // Connect.
    char *hosts[NUM_BOARDS] = {"10.1.1.213", "10.1.1.214"};
    SincArray sa;
    SincArrayInit(&sa);
    if (!SincArrayConnect(&sa, (const char **)hosts, NUM_BOARDS, CHANNELS_PER_BOARD))
    {
        printf("can't connect to array: %s\n", SincArrayErrorMessage(&sa));
        exit(EXIT_FAILURE);
    }

    // Ping.
    if (!SincArrayPing(&sa, false))
    {
        printf("can't connect to array: %s\n", SincArrayErrorMessage(&sa));
        exit(EXIT_FAILURE);
    }

    // Stop data acquisition.
    if (!SincArrayStop(&sa, -1, sa.timeout, false))
    {
        printf("can't stop: %s\n", SincArrayErrorMessage(&sa));
        exit(EXIT_FAILURE);
    }

    // Monitor all channels.
    int monChans[NUM_BOARDS * CHANNELS_PER_BOARD];
    int i;
    for (i = 0; i < NUM_BOARDS * CHANNELS_PER_BOARD; i++)
    {
        monChans[i] = i;
    }

    if (!SincArrayMonitorChannels(&sa, monChans, NUM_BOARDS * CHANNELS_PER_BOARD))
    {
        printf("can't monitor channels: %s\n", SincArrayErrorMessage(&sa));
        exit(EXIT_FAILURE);
    }

    // Calibrate.
    if (!SincArrayStartCalibration(&sa, -1))
    {
        printf("can't stop: %s\n", SincArrayErrorMessage(&sa));
        exit(EXIT_FAILURE);
    }

    // Start histogram.
    if (!SincArrayStartHistogram(&sa, -1))
    {
        printf("can't start histogram: %s\n", SincArrayErrorMessage(&sa));
        exit(EXIT_FAILURE);
    }

    sleep(2);

    // Stop data acquisition.
    if (!SincArrayStop(&sa, -1, sa.timeout, false))
    {
        printf("can't stop: %s\n", SincArrayErrorMessage(&sa));
        exit(EXIT_FAILURE);
    }

    // Disconnect.
    if (!SincArrayDisconnect(&sa))
    {
        printf("can't disconnect from array: %s\n", SincArrayErrorMessage(&sa));
        exit(EXIT_FAILURE);
    }

    SincArrayCleanup(&sa);

    return 0;
}
