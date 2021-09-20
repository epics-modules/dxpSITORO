#ifndef _WIN32
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "sinc.h"
#include "sinc_internal.h"
#include "discovery.h"

#define DISCOVER_PORT 8755
#define DISCOVER_REQUEST_MESSAGE "SiToro discovery v1 "
#define DISCOVER_READBUF_DEFAULT_SIZE 4096
#define DISCOVER_MULTICAST_GROUP_ADDRESS "236.97.11.116"


/*
 * NAME:        DiscoverFindNetworkInterfaces
 * ACTION:      Makes a list of the available network interfaces.
 */

static int DiscoverFindNetworkInterfaces(Discover *d)
{
    /* XXX - change to use getifaddrs()? */
    /* Create a socket. */
    struct ifconf ifc;
    struct ifreq *ifr;
    char         *buf;
    size_t        numInterfaces;
    size_t        i;
    int           s;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES);
        return false;
    }

#define BUF_SIZE (32768)

    buf = malloc(BUF_SIZE);
    if (buf == NULL)
    {
        close(s);
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    /* Get the list of interfaces. */
    ifc.ifc_len = BUF_SIZE;
    ifc.ifc_buf = buf;
    if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
    {
        free(buf);
        close(s);
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES);
        return false;
    }

    /* Iterate through the list of interfaces. */
    d->numNetworkInterfaces = 0;
    ifr = ifc.ifc_req;
    numInterfaces = ifc.ifc_len / (int)sizeof(struct ifreq);
    d->ifAddrs = calloc(numInterfaces, sizeof(struct sockaddr_in));
    d->ifNames = calloc(numInterfaces, sizeof(char*) + IF_NAMESIZE + 1);
    d->broadcastAddrs = calloc(numInterfaces, sizeof(struct sockaddr_in));
    if (d->ifAddrs == NULL || d->ifNames == NULL || d->broadcastAddrs == NULL)
    {
        free(d->ifAddrs);
        free(d->ifNames);
        free(d->broadcastAddrs);
        free(buf);
        close(s);
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    const char* p = ((const char*) d->ifNames) + numInterfaces * sizeof(char*);

    for (i = 0; i < numInterfaces; i++)
    {
        d->ifNames[i] = p;
        p += IF_NAMESIZE + 1;
    }

    for (i = 0; i < numInterfaces; i++)
    {
        struct ifreq *item = &ifr[i];

        /* Show the device name and IP address */
        struct sockaddr_in *addr = (struct sockaddr_in *)&item->ifr_addr;

        if (addr->sin_addr.s_addr != htonl(INADDR_LOOPBACK))
        {
            int index = d->numNetworkInterfaces;
            strncpy((char*) d->ifNames[index], item->ifr_name, IF_NAMESIZE);

            struct sockaddr_in *storeifaddr = &d->ifAddrs[index];
            storeifaddr->sin_family = AF_INET;
            storeifaddr->sin_addr = addr->sin_addr;
            storeifaddr->sin_port = 0;

            if (ioctl(s, SIOCGIFBRDADDR, item) >= 0)
            {
                struct sockaddr_in *baddr = (struct sockaddr_in *)&item->ifr_broadaddr;
                struct sockaddr_in *storebaddr = &d->broadcastAddrs[index];
                storebaddr->sin_family = AF_INET;
                storebaddr->sin_addr = baddr->sin_addr;
                storebaddr->sin_port = htons(DISCOVER_PORT);
            }

            d->numNetworkInterfaces++;
        }
    }

    free(buf);
    close(s);

    return true;
}


/*
 * NAME:        DiscoverInit
 * ACTION:      Initialise the discover structure.
 */

int DiscoverInit(Discover *d)
{
    /* Initialise the structure. */
    memset(d, 0, sizeof(*d));
    d->readBufSize = DISCOVER_READBUF_DEFAULT_SIZE;
    d->readBuf = malloc((size_t)d->readBufSize);
    if (d->readBuf == NULL)
    {
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    d->fd = -1;
    d->readBufUsed = 0;
    d->errNo = SI_TORO__SINC__ERROR_CODE__NO_ERROR;
    d->errStr = NULL;

    d->numNetworkInterfaces = 0;
    d->ifAddrs = NULL;
    d->ifNames = NULL;
    d->broadcastAddrs = NULL;

    d->numValidIfaces = 0;
    d->validIfaces = NULL;

    /* Find the list of network interfaces. */
    if (!DiscoverFindNetworkInterfaces(d))
        return false;

    /* Create the multicast group address. */
    memset(&d->multicastGroupAddr, 0, sizeof(d->multicastGroupAddr));
    d->multicastGroupAddr.sin_family = AF_INET;
    d->multicastGroupAddr.sin_addr.s_addr = inet_addr(DISCOVER_MULTICAST_GROUP_ADDRESS);
    d->multicastGroupAddr.sin_port = htons(DISCOVER_PORT);

    return true;
}


/*
 * NAME:        DiscoverCleanup
 * ACTION:      Clean up the discover structure.
 * RETURNS:     true on success, false otherwise. On failure use DiscoverErrno() and
 *                  DiscoverStrError() to get the error status.
 */

void DiscoverCleanup(Discover *d)
{
    if (d->fd >= 0)
        close(d->fd);
    d->fd = -1;

    free(d->errStr);
    d->errStr = NULL;

    free(d->readBuf);
    d->readBuf = NULL;

    free(d->ifAddrs);
    d->ifAddrs = NULL;

    free(d->ifNames);
    d->ifNames = NULL;

    free(d->broadcastAddrs);
    d->broadcastAddrs = NULL;

    free(d->validIfaces);
    d->validIfaces = NULL;
}

/*
 * NAME:        DiscoverSetInterfaceList
 * ACTION:      Set the list of interfaces discovery is to operate on. Some systems
 *              can have external facing interfaces and should not be probed.
 *              Call this after DiscoverInit() and before DiscoverRequest().
 * RETURNS:     true on success, false otherwise. On failure use DiscoverErrno() and
 *                  DiscoverStrError() to get the error status.
 */

int DiscoverSetInterfaceList(Discover *d, int numValidIfaces, const char *ifaces[])
{
    size_t      size;
    size_t      i;
    const char *p;

    /*
     * A single allocation with a table of char* pointers at the start then the
     * strings themselves. We take a copy of the strings.
     */
    size = numValidIfaces * sizeof(char*);

    for (i = 0; i < numValidIfaces; ++i)
        size += strlen(ifaces[i]) + 1;

    p = calloc(1, size);
    if (p == NULL)
    {
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    d->validIfaces = (const char**) p;
    p += numValidIfaces * sizeof(char*);

    for (i = 0; i < numValidIfaces; ++i)
    {
        d->validIfaces[i] = p;
        strcpy((char*) d->validIfaces[i], ifaces[i]);
        p += strlen(d->validIfaces[i]) + 1;
    }

    d->numValidIfaces = numValidIfaces;

    return true;
}

/*
 * NAME:        DiscoverListen
 * ACTION:      Start to listen for responses from discoverable devices.
 *              Call this after DiscoverInit() and before DiscoverRequest().
 * RETURNS:     true on success, false otherwise. On failure use DiscoverErrno() and
 *                  DiscoverStrError() to get the error status.
 */

int DiscoverListen(Discover *d)
{
    /* Close if there is an open socket. */
    if (d->fd >= 0)
        close(d->fd);

    /* Create the socket. */
    d->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (d->fd < 0)
    {
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES);
        return false;
    }

    /* Enable broadcast. */
    int broadcastEnable = 1;
    int ret = setsockopt(d->fd, SOL_SOCKET, SO_BROADCAST,
                         &broadcastEnable, sizeof(broadcastEnable));
    if (ret < 0)
    {
        close(d->fd);
        d->fd = -1;
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES);
        return false;
    }

    /* Bind it to the port */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0;

    ret = bind(d->fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if (ret < 0)
    {
        close(d->fd);
        d->fd = -1;
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__CONNECTION_FAILED);
        return false;
    }

    return true;
}


/*
 * NAME:        DiscoverRequest
 * ACTION:      Broadcasts a discovery request.
 *              Call this after DiscoverInit() and DiscoverListen().
 * RETURNS:     true on success, false otherwise. On failure use DiscoverErrno() and
 *                  DiscoverStrError() to get the error status.
 */

int DiscoverRequest(Discover *d)
{
    int i;

    if (d->fd < 0)
    {
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__BAD_PARAMETERS);
        return false;
    }

    for (i = 0; i < d->numNetworkInterfaces; i++)
    {
        if (d->numValidIfaces > 0)
        {
            int v;
            for (v = 0; v < d->numValidIfaces; ++v)
            {
                if (strcmp(d->ifNames[i], d->validIfaces[v]) == 0)
                    break;
            }
            if (v >= d->numValidIfaces)
                continue;
        }

        /* Broadcast it to this interface. */
        if (d->broadcastAddrs[i].sin_family == AF_INET &&
            sendto(d->fd, DISCOVER_REQUEST_MESSAGE,
                   sizeof(DISCOVER_REQUEST_MESSAGE)-1, 0,
                   (struct sockaddr *)&d->broadcastAddrs[i], sizeof(struct sockaddr_in)) < 0)
        {
            DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__WRITE_FAILED);
            return false;
        }

        /* Multicast it. */
        if (setsockopt(d->fd, IPPROTO_IP, IP_MULTICAST_IF,
                       &d->ifAddrs[i].sin_addr, sizeof(d->ifAddrs[i].sin_addr)) < 0)
        {
            DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__WRITE_FAILED);
            return false;
        }

        if (sendto(d->fd, DISCOVER_REQUEST_MESSAGE,
                   sizeof(DISCOVER_REQUEST_MESSAGE)-1, 0,
                   (struct sockaddr *)&d->multicastGroupAddr, sizeof(struct sockaddr_in)) < 0)
        {
            DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__WRITE_FAILED);
            return false;
        }
    }

    return true;
}


/*
 * NAME:        DiscoverReadyToRead
 * ACTION:      Waits for data to become available. Set the timeout to 0 to poll.
 * PARAMETERS:  int timeout - how long to wait in ms. 0  to poll.
 *              int *dataAvailable - set to true if data is available, false otherwise.
 * RETURNS:     true on success, false otherwise. On failure use DiscoverErrno() and
 *                  DiscoverStrError() to get the error status.
 */

int DiscoverReadyToRead(Discover *d, int timeout, int *dataAvailable)
{
    int maxFd = 0;
    fd_set readFds;

    if (d->fd < 0)
    {
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__BAD_PARAMETERS);
        return false;
    }

    FD_ZERO(&readFds);
    FD_SET(d->fd, &readFds);
    maxFd = d->fd;

    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    int numFds = select(maxFd + 1, &readFds, NULL, NULL, &tv);
    if (numFds < 0)
    {
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__READ_FAILED);
        return false;
    }

    *dataAvailable = FD_ISSET(d->fd, &readFds);

    return true;
}


/*
 * NAME:        ParseResponse
 * ACTION:      Parse a response packet into its fields.
 */

static void ParseResponse(DiscoverDeviceInfo *ddi, uint8_t *buf)
{
    char line[80];
    char *bpos = (char *)buf;

    /* read the packet buffer line by line */
    while (*bpos != 0)
    {
        /* copy a line across */
        int lcount = sizeof(line) - 1;
        char *lpos;
        for (lpos = line; *bpos != '\n' && *bpos != 0 && lcount > 0; lpos++, bpos++, lcount--)
            *lpos = *bpos;

        /* skip newline */
        if (*bpos == '\n')
            bpos++;

        *lpos = 0;

        /* Get the key/value pair. */
        char *equalsPos = index(line, '=');
        if (equalsPos != NULL)
        {
            *equalsPos = 0;
            char *val = equalsPos + 1;

            if (strcmp(line, "numChannels") == 0)
            {
                ddi->numChannels = atoi(val);
            }
            else if (strcmp(line, "productName") == 0)
            {
                strncpy(ddi->productName, val, sizeof(ddi->productName)-1);
            }
            else if (strcmp(line, "productId") == 0)
            {
                ddi->productId = atoi(val);
            }
            else if (strcmp(line, "serialNumber") == 0)
            {
                strncpy(ddi->serialNumber, val, sizeof(ddi->serialNumber)-1);
            }
            else if (strcmp(line, "firmwareVersion") == 0)
            {
                strncpy(ddi->firmwareVersion, val, sizeof(ddi->firmwareVersion)-1);
            }
            else if (strcmp(line, "hostName") == 0)
            {
                strncpy(ddi->hostName, val, sizeof(ddi->hostName)-1);
            }
        }
    }
}


/*
 * NAME:        DiscoverReadResponse
 * ACTION:      Read a response packet from a device.
 * RETURNS:     int - error code or SI_TORO__SINC__ERROR_CODE__NO_ERROR if no packets were available to be read.
 */

int DiscoverReadResponse(Discover *d, DiscoverDeviceInfo *ddi)
{
    /* Check that there's data waiting. */
    int dataAvailable = false;
    int result = DiscoverReadyToRead(d, 0, &dataAvailable);
    if (result < 0)
    {
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__READ_FAILED);
        return false;
    }

    if (!dataAvailable)
    {
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__NO_ERROR);
        return false;
    }

    /* Read a packet. */
    struct sockaddr_storage fromAddr;
    socklen_t fromAddrLen = sizeof(fromAddr);
    ssize_t packetSize = recvfrom(d->fd, d->readBuf,
                                  (size_t)d->readBufSize - 1, 0,
                                  (struct sockaddr*)&fromAddr, &fromAddrLen);
    if (packetSize < 0 || packetSize == d->readBufSize-1)
    {
        DiscoverSetErrno(d, SI_TORO__SINC__ERROR_CODE__READ_FAILED);
        return false;
    }

    d->readBuf[packetSize] = 0;

    memset(ddi, 0, sizeof(*ddi));
    ddi->addr = ((struct sockaddr_in *)&fromAddr)->sin_addr;
    ParseResponse(ddi, d->readBuf);

    return true;
}


/*
 * NAME:        DiscoverSetErrStr
 * ACTION:      Sets the error string for this channel.
 * PARAMETERS:  Discover *d - the discover object.
 *              int errno - the error code.
 *              const char *str - the error message.
 */

void DiscoverSetErrStr(Discover *d, int errNo, const char *str)
{
    d->errNo = errNo;

    // Free any existing error message.
    if (d->errStr != NULL)
    {
        free(d->errStr);
    }

    d->errStr = strdup(str);
}


/*
 * NAME:        DiscoverSetErrno
 * ACTION:      Sets the error code for this channel. Will also set a default error string for this code.
 * PARAMETERS:  Discover *d - the discover object.
 *              int errno - the error code.
 */

void DiscoverSetErrno(Discover *d, int errNo)
{
    switch ((SiToro__Sinc__ErrorCode)errNo)
    {
    case SI_TORO__SINC__ERROR_CODE__NO_ERROR:                     DiscoverSetErrStr(d, errNo, "no error"); break;
    case SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY:                DiscoverSetErrStr(d, errNo, "out of memory"); break;
    case SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED:                DiscoverSetErrStr(d, errNo, "unimplemented"); break;
    case SI_TORO__SINC__ERROR_CODE__NOT_FOUND:                    DiscoverSetErrStr(d, errNo, "not found"); break;
    case SI_TORO__SINC__ERROR_CODE__BAD_PARAMETERS:               DiscoverSetErrStr(d, errNo, "bad parameters"); break;
    case SI_TORO__SINC__ERROR_CODE__HOST_NOT_FOUND:               DiscoverSetErrStr(d, errNo, "host not found"); break;
    case SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES:             DiscoverSetErrStr(d, errNo, "out of resources"); break;
    case SI_TORO__SINC__ERROR_CODE__CONNECTION_FAILED:            DiscoverSetErrStr(d, errNo, "connection failed"); break;
    case SI_TORO__SINC__ERROR_CODE__READ_FAILED:                  DiscoverSetErrStr(d, errNo, "read failed"); break;
    case SI_TORO__SINC__ERROR_CODE__WRITE_FAILED:                 DiscoverSetErrStr(d, errNo, "write failed"); break;
    case SI_TORO__SINC__ERROR_CODE__COMMAND_FAILED:               DiscoverSetErrStr(d, errNo, "command failed"); break;
    case SI_TORO__SINC__ERROR_CODE__SOCKET_CLOSED_UNEXPECTEDLY:   DiscoverSetErrStr(d, errNo, "socket closed unexpectedly"); break;
    case SI_TORO__SINC__ERROR_CODE__TIMEOUT:                      DiscoverSetErrStr(d, errNo, "timed out"); break;
    case SI_TORO__SINC__ERROR_CODE__HOST_UNREACHABLE:             DiscoverSetErrStr(d, errNo, "host unreachable"); break;
    case SI_TORO__SINC__ERROR_CODE__AUTHORIZATION_FAILED:         DiscoverSetErrStr(d, errNo, "authorization failed"); break;
    case SI_TORO__SINC__ERROR_CODE__DEVICE_ERROR:                 DiscoverSetErrStr(d, errNo, "device error"); break;
    case SI_TORO__SINC__ERROR_CODE__INVALID_REQUEST:              DiscoverSetErrStr(d, errNo, "invalid request"); break;
    case SI_TORO__SINC__ERROR_CODE__NON_GATED_HISTOGRAM_DISABLED: DiscoverSetErrStr(d, errNo, "non-gated histogram disabled"); break;
    case SI_TORO__SINC__ERROR_CODE__NOT_CONNECTED:                DiscoverSetErrStr(d, errNo, "not connected"); break;
    case SI_TORO__SINC__ERROR_CODE__MULTIPLE_THREAD_WAIT:         DiscoverSetErrStr(d, errNo, "multiple thread wait"); break;
    case _SI_TORO__SINC__ERROR_CODE_IS_INT_SIZE:                  DiscoverSetErrStr(d, errNo, "unknown error"); break;
    }
}


/*
 * NAME:        DiscoverErrno
 * ACTION:      Get the most recent error code.
 * PARAMETERS:  Discover *d - the discover object.
 */

int DiscoverErrno(Discover *d)
{
    return d->errNo;
}


/*
 * NAME:        DiscoverStrError
 * ACTION:      Get the most recent error code in alphanumeric form.
 * PARAMETERS:  Discover *d - the discover object.
 */

const char *DiscoverStrError(Discover *d)
{
    return d->errStr;
}

#endif
