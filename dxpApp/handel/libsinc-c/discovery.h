/********************************************************************
 ***                                                              ***
 ***              libsinc discover subsystem header               ***
 ***                                                              ***
 ********************************************************************/

/*
 * The "discover" subsystem interrogates the network for a list
 * of available cards.
 */

#ifndef SINC_DISCOVER_H
#define SINC_DISCOVER_H

#ifndef _WIN32

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <netinet/in.h>


/* The discovery client object. */
typedef struct
{
    int                 fd;                  // The socket file descriptor.
    uint8_t            *readBuf;             // The read data buffer.
    int                 readBufSize;         // The size of the read data buffer.
    int                 readBufUsed;         // How much of the read data buffer is used.

    int                 numNetworkInterfaces;// The number of network interfaces present.
    struct sockaddr_in *ifAddrs;             // An array of the addresses of the network interfaces.
    struct sockaddr_in *broadcastAddrs;      // An array of the broadcast addresses of the network interfaces.
    struct sockaddr_in  multicastGroupAddr;  // The address to send multicasts to.

    int                 errNo;               // The most recent error code.
    char               *errStr;              // The most recent error string.
} Discover;


/* Information we've found about a device. */
typedef struct
{
    struct in_addr addr;
    int            numChannels;
    char           productName[80];
    int            productId;
    char           serialNumber[80];
    char           firmwareVersion[80];
    char           hostName[80];
} DiscoverDeviceInfo;


/* Public functions. */
int DiscoverInit(Discover *d);
void DiscoverCleanup(Discover *d);
int DiscoverRequest(Discover *d);
int DiscoverReadyToRead(Discover *d, int timeout, int *dataAvailable);
int DiscoverReadResponse(Discover *d, DiscoverDeviceInfo *ddi);

/* Internal functions. */
int DiscoverListen(Discover *d);
int DiscoverReadyToReadMulti(Discover *d, int timeout, int *dataAvailable, int *onInterface);


/*
 * NAME:        DiscoverErrno
 * ACTION:      Get the most recent error code.
 * PARAMETERS:  Discover *d - the discover object.
 */

int DiscoverErrno(Discover *d);


/*
 * NAME:        DiscoverStrError
 * ACTION:      Get the most recent error code in alphanumeric form.
 * PARAMETERS:  Discover *d - the discover object.
 */

const char *DiscoverStrError(Discover *d);


/* Internal functions. */
void DiscoverSetErrno(Discover *d, int errNo);
void DiscoverSetErrStr(Discover *d, int errNo, const char *str);

#ifdef __cplusplus
}
#endif

#endif /* !_WIN32 */

#endif /* SINC_DISCOVER_H */
