/********************************************************************
 ***                                                              ***
 ***                   libsinc api implementation                 ***
 ***                                                              ***
 ********************************************************************/

/*
 * This module provides the caller API for libsinc.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "sinc.h"
#include "sinc_internal.h"


/*
 * NAME:        SincInit
 * ACTION:      Initialises a Sinc channel.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincInit(Sinc *sc)
{
    uint8_t pad[1];
    SincBuffer tmpBuffer = SINC_BUFFER_INIT(pad);

    memset(sc, 0, sizeof(*sc));
    sc->connected = false;

    // Force the buffer to use dynamically allocated memory.
    sc->readBuf = tmpBuffer;
    sc->readBuf.alloced = SINC_READBUF_DEFAULT_SIZE;
    sc->readBuf.must_free_data = true;
    sc->readBuf.data = malloc(SINC_READBUF_DEFAULT_SIZE);

    if (sc->readBuf.data == NULL)
    {
        SincReadErrorSetCode(sc, SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY);
        return false;
    }

    SincErrorInit(&sc->readErr);
    SincErrorInit(&sc->writeErr);
    sc->err = &sc->readErr;
    sc->timeout = -1;
    sc->datagramXfer = false;
    sc->datagramPort = 0;
    sc->datagramFd = -1;
    sc->datagramIsOpen = false;

    return true;
}


/*
 * NAME:        SincCleanup
 * ACTION:      Closes and frees data used by the channel (but doesn't free the channel structure itself).
 * PARAMETERS:  Sinc *sc         - the channel to clean up.
 */

void SincCleanup(Sinc *sc)
{
    if (sc->connected)
    {
        SincDisconnect(sc);
        sc->connected = false;
    }

    if (sc->readBuf.data != NULL)
    {
        SINC_BUFFER_CLEAR(&sc->readBuf);
        sc->readBuf.data = NULL;
    }
}


/*
 * NAME:        SincSetTimeout
 * ACTION:      Sets a timeout for following commands.
 * PARAMETERS:  int timeoutMs - timeout in milliseconds. -1 for no timeout.
 */

void SincSetTimeout(Sinc *sc, int timeoutMs)
{
    sc->timeout = timeoutMs;
}


/*
 * NAME:        SincConnect
 * ACTION:      Connects a Sinc channel to a device on a given host and port.
 * PARAMETERS:  Sinc *sc         - the channel to connect.
 *              const char *host - the host to connect to.
 *              int port         - the port to connect to.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincConnect(Sinc *sc, const char *host, int port)
{
    int err = SincSocketConnect(&sc->fd, host, port, sc->timeout);
    if (err != 0)
    {
        SincReadErrorSetCode(sc, (SiToro__Sinc__ErrorCode)err);
        return false;
    }

    sc->connected = true;

    return true;
}


/*
 * NAME:        SincDisconnect
 * ACTION:      Disconnects a Sinc channel from whatever it's connected to.
 * PARAMETERS:  Sinc *sc         - the channel to disconnect.
 * RETURNS:     true on success, false otherwise. On failure use SincErrno() and
 *                  SincStrError() to get the error status.
 */

bool SincDisconnect(Sinc *sc)
{
    int success = SincSocketDisconnect(sc->fd) == SI_TORO__SINC__ERROR_CODE__NO_ERROR;
    if (success)
    {
        sc->fd = -1;
        sc->connected = false;
    }

    return success;
}


/*
 * NAME:        SincIsConnected
 * ACTION:      Returns the connected/disconnected state of the channel.
 * PARAMETERS:  Sinc *sc         - the sinc connection.
 * RETURNS:     int - true if connected, false otherwise.
 */

int SincIsConnected(Sinc *sc)
{
    return sc->connected;
}


/*
 * NAME:        SincCurrentErrorCode / SincReadErrorCode / SincWriteErrorCode
 * ACTION:      Get the most recent error code. SincCurrentErrorCode() gets the most
 *              recent error code. SincReadErrorCode() gets the most recent read error.
 *              SincWriteErrorCode() gets the most recent write error.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 */

SiToro__Sinc__ErrorCode SincCurrentErrorCode(Sinc *sc)
{
    return SincErrorCode(sc->err);
}

SiToro__Sinc__ErrorCode SincReadErrorCode(Sinc *sc)
{
    return SincErrorCode(&sc->readErr);
}

SiToro__Sinc__ErrorCode SincWriteErrorCode(Sinc *sc)
{
    return SincErrorCode(&sc->writeErr);
}


/*
 * NAME:        SincCurrentErrorMessage / SincReadErrorMessage / SincWriteErrorMessage
 * ACTION:      Get the most recent error message in alphanumeric form. SincCurrentErrorMessage()
 *              gets the most recent error code. SincReadErrorMessage() gets the most recent read error.
 *              SincWriteErrorMessage() gets the most recent write error.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 */

const char *SincCurrentErrorMessage(Sinc *sc)
{
    return SincErrorMessage(sc->err);
}

const char *SincReadErrorMessage(Sinc *sc)
{
    return SincErrorMessage(&sc->readErr);
}

const char *SincWriteErrorMessage(Sinc *sc)
{
    return SincErrorMessage(&sc->writeErr);
}


/*
 * NAME:        SincErrorInit
 * ACTION:      Initialise the error structure.
 * PARAMETERS:  SincError *err - the error structure.
 */

void SincErrorInit(SincError *err)
{
    err->code = SI_TORO__SINC__ERROR_CODE__NO_ERROR;
    strcpy(err->msg, "");
}


/*
 * NAME:        SincErrorCode
 * ACTION:      Get the most recent error code.
 * PARAMETERS:  SincError *err - the error structure.
 */

SiToro__Sinc__ErrorCode SincErrorCode(SincError *err)
{
    return err->code;
}


/*
 * NAME:        SincErrorMessage
 * ACTION:      Get the most recent error details in alphanumeric form.
 * PARAMETERS:  SincError *err - the error structure.
 */

const char *SincErrorMessage(SincError *err)
{
    return err->msg;
}


/*
 * NAME:        SincErrorSetMessage
 * ACTION:      Sets the error string for this error.
 * PARAMETERS:  SincError *err - the error structure.
 *              SiToro__Sinc__ErrorCode code - the error code.
 *              const char *msg - the error message.
 */

void SincErrorSetMessage(SincError *err, SiToro__Sinc__ErrorCode code, const char *msg)
{
    err->code = code;

    // Copy and truncate error message with guaranteed null termination.
    memset(err->msg, 0, sizeof(err->msg));
    strncpy(err->msg, msg, SINC_ERROR_SIZE_MAX);
}


/*
 * NAME:        SincErrorSetCode
 * ACTION:      Sets the error code for this channel. Will also set a default error string for this code.
 * PARAMETERS:  SincError *err - the error structure.
 *              int errno - the error code.
 */

void SincErrorSetCode(SincError *err, SiToro__Sinc__ErrorCode code)
{
    switch (code)
    {
    case SI_TORO__SINC__ERROR_CODE__NO_ERROR:                     SincErrorSetMessage(err, code, "no error"); break;
    case SI_TORO__SINC__ERROR_CODE__OUT_OF_MEMORY:                SincErrorSetMessage(err, code, "out of memory"); break;
    case SI_TORO__SINC__ERROR_CODE__UNIMPLEMENTED:                SincErrorSetMessage(err, code, "unimplemented"); break;
    case SI_TORO__SINC__ERROR_CODE__NOT_FOUND:                    SincErrorSetMessage(err, code, "not found"); break;
    case SI_TORO__SINC__ERROR_CODE__BAD_PARAMETERS:               SincErrorSetMessage(err, code, "bad parameters"); break;
    case SI_TORO__SINC__ERROR_CODE__HOST_NOT_FOUND:               SincErrorSetMessage(err, code, "host not found"); break;
    case SI_TORO__SINC__ERROR_CODE__OUT_OF_RESOURCES:             SincErrorSetMessage(err, code, "out of resources"); break;
    case SI_TORO__SINC__ERROR_CODE__CONNECTION_FAILED:            SincErrorSetMessage(err, code, "connection failed"); break;
    case SI_TORO__SINC__ERROR_CODE__READ_FAILED:                  SincErrorSetMessage(err, code, "read failed"); break;
    case SI_TORO__SINC__ERROR_CODE__WRITE_FAILED:                 SincErrorSetMessage(err, code, "write failed"); break;
    case SI_TORO__SINC__ERROR_CODE__COMMAND_FAILED:               SincErrorSetMessage(err, code, "command failed"); break;
    case SI_TORO__SINC__ERROR_CODE__SOCKET_CLOSED_UNEXPECTEDLY:   SincErrorSetMessage(err, code, "socket closed unexpectedly"); break;
    case SI_TORO__SINC__ERROR_CODE__TIMEOUT:                      SincErrorSetMessage(err, code, "timed out"); break;
    case SI_TORO__SINC__ERROR_CODE__HOST_UNREACHABLE:             SincErrorSetMessage(err, code, "host unreachable"); break;
    case SI_TORO__SINC__ERROR_CODE__AUTHORIZATION_FAILED:         SincErrorSetMessage(err, code, "authorization failed"); break;
    case SI_TORO__SINC__ERROR_CODE__DEVICE_ERROR:                 SincErrorSetMessage(err, code, "device error"); break;
    case SI_TORO__SINC__ERROR_CODE__INVALID_REQUEST:              SincErrorSetMessage(err, code, "invalid request"); break;
    case SI_TORO__SINC__ERROR_CODE__NON_GATED_HISTOGRAM_DISABLED: SincErrorSetMessage(err, code, "non-gated histogram disabled"); break;
    case _SI_TORO__SINC__ERROR_CODE_IS_INT_SIZE:                  SincErrorSetMessage(err, code, "unknown error"); break;
    }
}


/*
 * NAME:        SincReadErrorSetMessage / SincReadErrorSetCode /
 *              SincWriteErrorSetMessage / SincWriteErrorSetCode
 * ACTION:      Sets an error state to either the read or write error code.
 */

void SincReadErrorSetMessage(Sinc *sc, SiToro__Sinc__ErrorCode code, const char *msg)
{
    SincErrorSetMessage(&sc->readErr, code, msg);
    sc->err = &sc->readErr;
}

void SincReadErrorSetCode(Sinc *sc, SiToro__Sinc__ErrorCode code)
{
    SincErrorSetCode(&sc->readErr, code);
    sc->err = &sc->readErr;
}

void SincWriteErrorSetMessage(Sinc *sc, SiToro__Sinc__ErrorCode code, const char *msg)
{
    SincErrorSetMessage(&sc->writeErr, code, msg);
    sc->err = &sc->writeErr;
}

void SincWriteErrorSetCode(Sinc *sc, SiToro__Sinc__ErrorCode code)
{
    SincErrorSetCode(&sc->writeErr, code);
    sc->err = &sc->writeErr;
}

