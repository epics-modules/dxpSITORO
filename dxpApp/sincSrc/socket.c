/********************************************************************
 ***                                                              ***
 ***                Socket interface for libsinc                  ***
 ***                                                              ***
 ********************************************************************/

/*
 * This module connects the protocol to the network via
 * standard network sockets.
 */

#include <ctype.h>
#include <errno.h>

#if defined (_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define MAXHOSTNAMELEN 64

#else

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifndef _WIN32
#include <sys/socket.h>
#endif
#include <fcntl.h>
#include <memory.h>
#include <unistd.h>
#include <stdio.h>
#endif

#include "sinc.h"
#include "sinc_internal.h"


/*
 * NAME:        SincSocketInit
 * ACTION:      If the socket system isn't already initialised, initialise it.
 *              This is only used on windows/winsock.
 * RETURNS:     0 on success, a SiToro__Sinc__ErrorCode otherwise.
 */

static int SincSocketInit()
{
#ifdef _WIN32
    static int winSockInitialised = false;

    if (!winSockInitialised)
    {
        WORD wVersionRequested;
        WSADATA wsaData;
        int err;

        wVersionRequested = MAKEWORD(2, 2);
        err = WSAStartup(wVersionRequested, &wsaData);
        if (err)
            return SI_TORO__SINC__ERROR_CODE__BAD_PARAMETERS;

        winSockInitialised = true;
    }
#endif

    return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
}


/*
 * NAME:        SincSocketSetNonBlocking
 * ACTION:      Set the socket into non-blocking mode.
 * PARAMETERS:  int fd - the file descriptor of the socket to change.
 * RETURNS:     0 on success, a SiToro__Sinc__ErrorCode otherwise.
 */

int SincSocketSetNonBlocking(int fd)
{
#ifdef _WIN32
   unsigned long mode = 1;
   if (ioctlsocket(fd, FIONBIO, &mode) < 0)
       return SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES;
#else
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0)
       return SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES;

   flags |= O_NONBLOCK;
   if (fcntl(fd, F_SETFL, flags) < 0)
       return SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES;
#endif

   return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
}


/*
 * NAME:        SincSocketConnect
 * ACTION:      Connect to the device.
 * PARAMETERS:  int *clientFd - on success the opened fd will be placed here.
 *              const char *host - the host to connect to.
 *              int port - the port to connect to.
 *              int timeout - in milliseconds. 0 to poll. -1 to wait forever.
 * RETURNS:     0 on success, a SiToro__Sinc__ErrorCode otherwise.
 */

int SincSocketConnect(int *clientFd, const char *host, int port, int timeout)
{
    // Make sure winsock is initialised.
    int errCode = SincSocketInit();
    if (errCode != SI_TORO__SINC__ERROR_CODE__NO_ERROR)
        return errCode;

    // Look up the host name.
    struct hostent *hostInfo = gethostbyname(host);
    if (hostInfo == NULL)
        return SI_TORO__SINC__ERROR_CODE__HOST_NOT_FOUND;

    // Create the socket.
    int fd = socket (PF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES;

    *clientFd = fd;

    // Set the socket into non-blocking mode.
    errCode = SincSocketSetNonBlocking(fd);
    if (errCode != SI_TORO__SINC__ERROR_CODE__NO_ERROR)
        return errCode;

    // Connect to the remote host.
    struct sockaddr_in inetAddr;
    memset(&inetAddr, 0, sizeof(inetAddr));
    inetAddr.sin_family = PF_INET;
    inetAddr.sin_port = htons((unsigned short)port);
    memcpy (&inetAddr.sin_addr.s_addr, hostInfo->h_addr, sizeof(struct in_addr));
    int connectResult = connect(fd, (struct sockaddr *)&inetAddr, sizeof(inetAddr));
    if (connectResult == 0)
        return SI_TORO__SINC__ERROR_CODE__NO_ERROR;  // Connected immediately.
#ifndef _WIN32
    else if (errno != EINPROGRESS)
    {
        return SI_TORO__SINC__ERROR_CODE__CONNECTION_FAILED;  // Failed to connect.
    }
#else
    else if (WSAGetLastError() != WSAEWOULDBLOCK)
    {
        return SI_TORO__SINC__ERROR_CODE__CONNECTION_FAILED;  // Failed to connect.
    }
#endif

    // Wait for connect to complete.
    fd_set readFds;
    fd_set writeFds;
    struct timeval timeoutTv;
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_SET(fd, &readFds);
    FD_SET(fd, &writeFds);

    timeoutTv.tv_sec = timeout / 1000;
    timeoutTv.tv_usec = (timeout % 1000) * 1000;

    int retry = 1;
    do
    {
        int numFds = select(fd + 1, &readFds, &writeFds, NULL, (timeout >= 0) ? &timeoutTv : NULL);
        if (numFds < 0)
        {
            if (errno != EINTR)
                return SI_TORO__SINC__ERROR_CODE__CONNECTION_FAILED;

            // Retry on EINTR.
        }
        else if (numFds == 0)
        {
            // Timed out.
            return SI_TORO__SINC__ERROR_CODE__TIMEOUT;
        }
        else
        {
            // Got something, continue.
            retry = 0;
        }

    } while (retry);

    // Did we have a connection error?
    int socketError = 0;
    socklen_t errLen = sizeof(socketError);

    if (FD_ISSET(fd, &readFds) || FD_ISSET(fd, &writeFds))
    {
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&socketError, &errLen) < 0)
            return SI_TORO__SINC__ERROR_CODE__CONNECTION_FAILED;
    }
    else
        return SI_TORO__SINC__ERROR_CODE__CONNECTION_FAILED;

    if (socketError == EHOSTUNREACH)
        return SI_TORO__SINC__ERROR_CODE__HOST_UNREACHABLE;
    else if (socketError == ETIMEDOUT)
        return SI_TORO__SINC__ERROR_CODE__TIMEOUT;
    else if (socketError != 0)
        return SI_TORO__SINC__ERROR_CODE__CONNECTION_FAILED;

    return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
}


/*
 * NAME:        SincSocketDisconnect
 * ACTION:      Disconnect from the device.
 * PARAMETERS:  int fd - the connection to close.
 * RETURNS:     0 on success, a SiToro__Sinc__ErrorCode otherwise.
 */

int SincSocketDisconnect(int fd)
{
#ifdef _WIN32
    shutdown(fd, SD_SEND);
    closesocket(fd);
#else
    shutdown(fd, SHUT_RDWR);
    if (close(fd) != 0)
        return SI_TORO__SINC__ERROR_CODE__BAD_PARAMETERS;
#endif

    return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
}


/*
 * NAME:        SincSocketRead
 * ACTION:      Read from the device. Will not block.
 * PARAMETERS:  int fd - the connection to read from.
 *              uint8_t *buf - the buffer to read to.
 *              int bufLen - the number of bytes available in the buffer.
 *              int *bytesRead - filled with the number of bytes read.
 * RETURNS:     0 on success, a SiToro__Sinc__ErrorCode otherwise.
 */

int SincSocketRead(int fd, uint8_t *buf, int bufLen, int *bytesRead)
{
#if defined (_WIN32)
    *bytesRead = recv(fd, buf, bufLen, 0);
#else
    *bytesRead = (int)read(fd, buf, (size_t)bufLen);
#endif

    if (*bytesRead < 0) {
        return SI_TORO__SINC__ERROR_CODE__READ_FAILED;
    }

#ifdef PACKET_DEBUG
    printf("SincSocketRead: ");
    int i;
    for (i = 0; i < *bytesRead; i++)
        printf("%02x ", buf[i]);
    printf("\n");
#endif

    return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
}


/*
 * NAME:        SincSocketWriteNonBlocking
 * ACTION:      Write to the device. Will not block but may not write the entire buffer.
 * PARAMETERS:  int fd - the connection to write on.
 *              const uint8_t *buf - the buffer to write.
 *              int bufLen - the number of bytes to write.
 *              int *bytesWritten - filled with the number of bytes written.
 * RETURNS:     0 on success, a SiToro__Sinc__ErrorCode otherwise.
 */

int SincSocketWriteNonBlocking(int fd, const uint8_t *buf, int bufLen, int *bytesWritten)
{
#ifdef _WIN32
    *bytesWritten = send(fd, buf, bufLen, 0);
#else
    *bytesWritten = (int)write(fd, buf, (size_t)bufLen);
#endif

#ifdef PACKET_DEBUG
    printf("SincSocketWriteNonBlocking: ");
    int i;
    for (i = 0; i < *bytesWritten; i++)
        printf("%02x ", buf[i]);
    printf("\n");
#endif

    if (*bytesWritten < 0)
        return SI_TORO__SINC__ERROR_CODE__WRITE_FAILED;

    return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
}


/*
 * NAME:        SincSocketWrite
 * ACTION:      Write to the device. Will block until all data is written.
 * PARAMETERS:  int fd - the connection to write on.
 *              const uint8_t *buf - the buffer to write.
 *              int bufLen - the number of bytes to write.
 *              int *bytesWritten - filled with the number of bytes written.
 * RETURNS:     0 on success, a SiToro__Sinc__ErrorCode otherwise.
 */

int SincSocketWrite(int fd, const uint8_t *buf, int bufLen)
{
    int numFds;
    fd_set writeFds;
    fd_set exceptFds;
    int bytesWritten;

    SincSocketInit();

    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);
    FD_SET(fd, &writeFds);
    FD_SET(fd, &exceptFds);

    while (bufLen > 0)
    {
        // Can we write on this fd?
        numFds = select(fd + 1, 0, &writeFds, &exceptFds, NULL);
        if (numFds <= 0)
        {
            if (errno != EINTR)
                return SI_TORO__SINC__ERROR_CODE__WRITE_FAILED;
        }
        else
        {
            if (FD_ISSET(fd, &writeFds))
            {
                // We can write. Do it.
                int errCode = SincSocketWriteNonBlocking(fd, buf, bufLen, &bytesWritten);
                if (errCode != SI_TORO__SINC__ERROR_CODE__NO_ERROR)
                    return errCode;

                buf += bytesWritten;
                bufLen -= bytesWritten;
            }

            if (FD_ISSET(fd, &exceptFds))
            {
                return SI_TORO__SINC__ERROR_CODE__WRITE_FAILED;
            }
        }
    }

    return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
}


/*
 * NAME:        SincSocketWait
 * ACTION:      Wait until data is available for reading from the device.
 * PARAMETERS:  int fd - the connection.
 *              int timeout - in milliseconds. 0 to poll. -1 to wait forever.
 *              int waitWrite - waits on write as well as read if non-zero.
 *              bool *readOk - set to true if data is available to read.
 * RETURNS:     0 on success, a SiToro__Sinc__ErrorCode otherwise.
 */

int SincSocketWait(int fd, int timeout, bool *readOk)
{
    return SincSocketWaitMulti(&fd, 1, timeout, readOk);
}



/*
 * NAME:        SincSocketWaitMulti
 * ACTION:      Wait until data is available for reading from the device on one
 *              of a number of sockets.
 * PARAMETERS:  int *fd - an array of fds to wait on.
 *              int numFds - the number of fds in the above array.
 *              int timeout - in milliseconds. 0 to poll. -1 to wait forever.
 *                  Will return SI_TORO__SINC__ERROR_CODE__TIMEOUT on timeout.
 *              bool *readOk - an array corresponding to the fd array. Set to true if data is available to read.
 * RETURNS:     0 on success, a SiToro__Sinc__ErrorCode otherwise.
 */

int SincSocketWaitMulti(const int *fd, int numFds, int timeout, bool *readOk)
{
    int numFdsSelected;
    int i;
    int maxFd = 0;
    fd_set readFds;
    fd_set exceptFds;
    struct timeval tv;
    struct timeval *ptv;

    // Clear the readOk results.
    for (i = 0; i < numFds; i++)
        readOk[i] = false;

    // Set up the select() params.
    FD_ZERO(&readFds);
    FD_ZERO(&exceptFds);
    for (i = 0; i < numFds; i++)
    {
        FD_SET(fd[i], &readFds);
        FD_SET(fd[i], &exceptFds);

        if (fd[i] > maxFd)
            maxFd = fd[i];
    }

    if (timeout < 0)
    {
        ptv = NULL;
    }
    else
    {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        ptv = &tv;
    }

    // Can we read from this fd?
    while (true)
    {
        numFdsSelected = select(maxFd + 1, &readFds, NULL, &exceptFds, ptv);
        if (numFdsSelected == 0)
        {
            if (timeout == 0)
                return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
            else
                return SI_TORO__SINC__ERROR_CODE__TIMEOUT;
        }
        else if (numFdsSelected < 0)
        {
            // Got an error.
            if (errno != EINTR)
            {
                return SI_TORO__SINC__ERROR_CODE__READ_FAILED;
            }

            // Go again on EINTR.
        }
        else
        {
            // Check for activity on any socket.
            for (i = 0; i < numFds; i++)
            {
                if (FD_ISSET(fd[i], &readFds))
                {
                    // We have data available to read.
                    readOk[i] = true;
                }

                if (FD_ISSET(fd[i], &exceptFds))
                {
                    return SI_TORO__SINC__ERROR_CODE__READ_FAILED;
                }
            }

            return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
        }
    }
}


/*
 * NAME:        SincSocketBindDatagram
 * ACTION:      Create a UDP socket which can receive datagrams.
 * PARAMETERS:  int *datagramFd - on success the opened fd will be placed here.
 *              int *port - set to the port to connect to.
 * RETURNS:     0 on success, a SiToro__Sinc__ErrorCode otherwise.
 */

int SincSocketBindDatagram(int *datagramFd, int *port)
{
    struct sockaddr_in addr;
    socklen_t addrLen;

    // Create the socket.
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
    {
        return SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES;
    }

    // Bind to a port.
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = 0;                 // Allow bind on any port.
    addr.sin_addr.s_addr = INADDR_ANY; // Bind on the local host.
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
#ifndef _WIN32
		close(fd);
#else
		closesocket(fd);
#endif
        return SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES;
    }

    // Find out what port we got.
    addrLen = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &addrLen) < 0)
    {
#ifndef _WIN32
		close(fd);
#else
		closesocket(fd);
#endif
		return SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES;
    }

    // Return results.
    *datagramFd = fd;
    *port = ntohs(addr.sin_port);

    return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
}


/*
 * NAME:        SincSocketReadDatagram
 * ACTION:      Read a datagram.
 * PARAMETERS:  int datagramFd - on success the opened fd will be placed here.
 *              uint8_t *buf - the buffer to place the result in.
 *              size_t *bufLen - set this to the available space. Will be set to the packet size.
 *              bool nonBlocking - set to true if non-blocking reads are desired.
 * RETURNS:     0 on success, a SiToro__Sinc__ErrorCode otherwise.
 */

int SincSocketReadDatagram(int fd, uint8_t *buf, size_t *bufLen, bool nonBlocking)
{
    int flags = 0;

#ifndef _WIN32
    if (nonBlocking)
        flags = MSG_DONTWAIT;
#endif

    int packetSize = (int)recv(fd, buf, *bufLen, flags);
    if (packetSize < 0)
    {
        *bufLen = 0;
        if (errno == EWOULDBLOCK)
            return SI_TORO__SINC__ERROR_CODE__TIMEOUT;
        else
            return SI_TORO__SINC__ERROR_CODE__READ_FAILED;
    }

    *bufLen = (size_t)packetSize;
    return SI_TORO__SINC__ERROR_CODE__NO_ERROR;
}
