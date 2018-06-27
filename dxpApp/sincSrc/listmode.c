/********************************************************************
 ***                                                              ***
 ***                   libsinc list mode buffer                   ***
 ***                                                              ***
 ********************************************************************/

/*
 * The list mode buffer allows list mode data to be read from any
 * source and list mode packets to be extracted.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>

#include "sinc_internal.h"
#include "lmbuf.h"


/*
 * NAME:        SincListModeEncodeHeader
 * ACTION:      Encodes a list mode header.
 * PARAMETERS:  Sinc *sc - the sinc connection.
 *              int channelId - which channel to use. -1 to use the default channel for this port.
 *              uint8_t **headerData - memory will be allocated for the result and returned here.
 *                  You must free() this data.
 *              size_t *headerLen - the length of the result data will be returned here.
 * RETURNS:     true on success, false otherwise. On failure use SincCurrentErrorCode() and
 *                  SincCurrentErrorMessage() to get the error status.
 */

bool SincListModeEncodeHeader(Sinc *sc, int channelId, uint8_t **headerData, size_t *headerLen)
{
    char         lineBuf[256];
    LmBuf        json;
    char *       linePos;
    char *       lineEnd;
    time_t       timer;
    char         timeBuf[40];
    char         sizeBuf[40];
    struct tm*   tm_info;
    size_t       headerLenPos = 0;
    size_t       jsonStartPos = 0;
    unsigned int i;

    /* We're using the LmBuf here as a dynamically-sized buffer for the JSON output. */
    LmBufInit(&json);

    /* Get the channel state. */
    SiToro__Sinc__ListParamDetailsResponse *resp = NULL;
    if (!SincListParamDetails(sc, channelId, (char *)"", &resp))
        return false;

    /* Get the current time in ISO format. */
    time(&timer);
    tm_info = localtime(&timer);
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", tm_info);

    /* Output the beginning of the header. */
    strcpy(lineBuf, "SiToro_List_Mode\nheaderSize ");
    LmBufAddData(&json, (uint8_t *)lineBuf, strlen(lineBuf));
    headerLenPos = json.bufHead;
    strcpy(lineBuf, "0     \n"); /* Leaving enough space to rewrite this later. */
    LmBufAddData(&json, (uint8_t *)lineBuf, strlen(lineBuf));
    jsonStartPos = json.bufHead;

    /* Output the start of the JSON. */
    memset(lineBuf, 0, sizeof(lineBuf));
    linePos = lineBuf;
    lineEnd = &lineBuf[sizeof(lineBuf)-1];

    linePos += sprintf(linePos, "{\n");
    linePos += sprintf(linePos, "  \"_fileType\": \"SiToro List Mode\",\n");
    linePos += sprintf(linePos, "  \"file.timeStamp\": \"%s\",\n", timeBuf);
    linePos += sprintf(linePos, "  \"file.count\": 0,      \n");
    LmBufAddData(&json, (uint8_t *)lineBuf, (size_t)(linePos - lineBuf));

    /* Add each of the channel parameters. */
    for (i = 0; i < resp->n_paramdetails; i++)
    {
        linePos = lineBuf;

        SiToro__Sinc__ParamDetails *pd = resp->paramdetails[i];
        SiToro__Sinc__KeyValue *kv = pd->kv;

        /* Output the key. */
        linePos += snprintf(linePos, (size_t)(lineEnd - linePos), "  \"%s\": ", pd->kv->key);

        /* Output the value. */
        if (kv->has_intval)
        {
            linePos += snprintf(linePos, (size_t)(lineEnd - linePos), "%" PRId64, kv->intval);
        }
        else if (kv->has_floatval)
        {
            linePos += snprintf(linePos, (size_t)(lineEnd - linePos), "%.9f", kv->floatval);
        }
        else if (kv->has_boolval)
        {
            if (kv->boolval)
                linePos += snprintf(linePos, (size_t)(lineEnd - linePos), "true");
            else
                linePos += snprintf(linePos, (size_t)(lineEnd - linePos), "false");
        }
        else if (kv->strval)
        {
            linePos += snprintf(linePos, (size_t)(lineEnd - linePos), "\"%s\"", kv->strval);
        }
        else if (kv->optionval)
        {
            linePos += snprintf(linePos, (size_t)(lineEnd - linePos), "\"%s\"", kv->optionval);
        }
        else
        {
            linePos += snprintf(linePos, (size_t)(lineEnd - linePos), "0");
        }

        /* Add the end of line. */
        if (linePos < lineEnd)
        {
            linePos += snprintf(linePos, (size_t)(lineEnd - linePos), "%s\n", (i < resp->n_paramdetails-1) ? "," : "");
        }

        /* Add this line to the buffer. */
        if (linePos < lineEnd)
        {
            LmBufAddData(&json, (uint8_t *)lineBuf, (size_t)(linePos - lineBuf));
        }
    }

    /* End the JSON. */
    LmBufAddData(&json, (uint8_t *)"}\n", 2);

    /* Rewrite the total header length. */
    sprintf(sizeBuf, "%d", (int)(json.bufHead - jsonStartPos));
    strncpy((char *)&json.buf[headerLenPos], sizeBuf, strlen(sizeBuf));

    /* Hand the buffer over to the caller. */
    *headerData = json.buf;
    *headerLen = json.bufHead;

    return true;
}
