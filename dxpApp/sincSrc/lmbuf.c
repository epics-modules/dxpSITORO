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

#include "lmbuf.h"


/* The initial size of the input data buffer. */
#define LMBUF_HEADER_LINE_LEN 28
#define LMBUF_INITIAL_SIZE 65536
#define LMBUF_GET_WORD(buf, offset) ( memcpy(&val_u32, ((uint8_t *)(buf)) + ((offset) * sizeof(val_u32)), sizeof(val_u32)), val_u32 )
#define LMBUF_SIGN_EXTEND_24_TO_32(v) (((v) & 0x00800000) ? (((v) & 0x7fffff) | 0xff800000) : ((v) & 0x7fffff))


/*
 * NAME:        LmBufInit
 * ACTION:      Initialises a list mode buffer.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 * RETURNS:     bool - true on success, false if out of memory.
 */

bool LmBufInit(LmBuf *lm)
{
    lm->buf = malloc(LMBUF_INITIAL_SIZE);
    if (lm->buf == NULL)
        return false;

    lm->bufSize = LMBUF_INITIAL_SIZE;

    LmBufClear(lm);

    return true;
}


/*
 * NAME:        LmBufClose
 * ACTION:      Closes an LmBuf, freeing memory.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 */

void LmBufClose(LmBuf *lm)
{
    if (lm->buf != NULL)
    {
        free(lm->buf);
        lm->buf = NULL;
    }
}


/*
 * NAME:        LmBufClose
 * ACTION:      Clears an LmBuf, emptying the contents but keeping the buffer memory ready.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 */

void LmBufClear(LmBuf *lm)
{
    lm->bufHead = 0;
    lm->bufTail = 0;
    lm->srcTailPos = 0;
}


/*
 * NAME:        LmBufCompact
 * ACTION:      Removes unused space from the start of the buffer by moving
 *              the data back to the start.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 */

static void LmBufCompact(LmBuf *lm)
{
    if (lm->bufTail == 0)
        return;

    memmove(lm->buf, &lm->buf[lm->bufTail], lm->bufHead - lm->bufTail);
    lm->bufHead -= lm->bufTail;
    lm->bufTail = 0;
}


/*
 * NAME:        HigherPowerOfTwo
 * ACTION:      Finds the next power of two which is higher than or
 *              or equal to the given number.
 * PARAMETERS:  uint32_t n - the value to use.
 * RETURNS:     uint32_t - the power of two.
 */

static uint32_t HigherPowerOfTwo(uint32_t n)
{
    n--;           // 1000 0011 --> 1000 0010
    n |= n >> 1;   // 1000 0010 | 0100 0001 = 1100 0011
    n |= n >> 2;   // 1100 0011 | 0011 0000 = 1111 0011
    n |= n >> 4;   // 1111 0011 | 0000 1111 = 1111 1111
    n |= n >> 8;   // ... (At this point all bits are 1, so further bitwise-or
    n |= n >> 16;  //      operations produce no effect.)
    n++;           // 1111 1111 --> 1 0000 0000

    return n;
}


/*
 * NAME:        LmBufExpand
 * ACTION:      Increases the size of the buffer to at least the given amount.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 * RETURNS:     bool - true on success, false if out of memory.
 */

static bool LmBufExpand(LmBuf *lm, size_t minSize)
{
    if (minSize <= lm->bufSize)
        return true; /* Nothing to do. */

    /* Use the next higher power of two as the new size. */
    size_t newSize = HigherPowerOfTwo((uint32_t)minSize);
    lm->buf = realloc(lm->buf, newSize);
    if (lm->buf == NULL)
        return false;

    lm->bufSize = newSize;

    return true;
}


/*
 * NAME:        LmBufAddData
 * ACTION:      Adds some binary data to the head of the buffer. Use
 *              this to add data read from a file or stream, then use
 *              LmBufGetNextPacket() to get packets out of the buffer.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 * RETURNS:     int - true on success, false if out of memory.
 */

bool LmBufAddData(LmBuf *lm, uint8_t *data, size_t len)
{
    /* Don't waste space at the start of the buffer. */
    LmBufCompact(lm);

    /* Make sure the buffer's big enough to hold our new data. */
    if (!LmBufExpand(lm, lm->bufHead + len))
        return false;

    /* Copy the new data in. */
    memcpy(&lm->buf[lm->bufHead], data, len);
    lm->bufHead += len;

    return true;
}


/*
 * NAME:        LmBufGetJsonHeader
 * ACTION:      Gets a JSON header from the buffer. This is usually
 *              done only at the start of a list mode data file.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 *              char **data, size_t *len - the header is put here.
 *                  Set to NULL if you don't want the header.
 *                  If non-NULL you must free() it after it's been received.
 *              bool *invalidHeader - indicates that the header is invalid or
 *                  corrupt. Will also return true in this case.
 * RETURNS:     bool - true on success or if the header was invalid,
 *                  false if the header wasn't complete.
 */

bool LmBufGetJsonHeader(LmBuf *lm, char **data, size_t *len, bool *invalidHeader)
{
    char jsonLenBuf[10];
    unsigned int jsonLenPos = 0;
    size_t jsonLen = 0;
    uint8_t ch;

    /* Initialise results to defaults. */
    *invalidHeader = false;
    if (data != NULL)
        *data = NULL;

    if (len != NULL)
        *len = 0;

    /* Verify if what's in the header. */
    if (lm->bufHead - lm->bufTail < 30)
    {
        /* Too few bytes to even bother trying. */
        return false;
    }

    if (strncmp((char *)&lm->buf[lm->bufTail], "SiToro_List_Mode\nheaderSize ", 28) != 0)
    {
        /* Bad header. */
        *invalidHeader = true;
        return true;
    }

    lm->bufTail += LMBUF_HEADER_LINE_LEN;
    lm->srcTailPos += LMBUF_HEADER_LINE_LEN;

    /* Get the header length value. */
    ch = 0;
    memset(jsonLenBuf, 0, sizeof(jsonLenBuf));

    while (lm->bufHead > lm->bufTail && jsonLenPos < sizeof(jsonLenBuf) && ch != '\n')
    {
        /* Get a character. */
        ch = lm->buf[lm->bufTail];
        lm->bufTail++;
        lm->srcTailPos++;

        if (ch != '\n')
        {
            /* Copy a digit across. */
            jsonLenBuf[jsonLenPos] = (char)ch;
            jsonLenPos++;
        }
    }

    if (ch != '\n')
    {
        if (jsonLenPos == sizeof(jsonLenBuf))
        {
            /* Got too many characters before we got a newline. Data is corrupted. */
            *invalidHeader = true;
            return true;
        }
        else
        {
            /* Out of characters before we got a newline. Incomplete packet. */
            return false;
        }
    }

    jsonLen = (size_t)atoi(jsonLenBuf);
    if (jsonLen <= 0 || jsonLen > 1000000)
    {
        /* Bad JSON size. Corrupted packet. */
        *invalidHeader = true;
        return true;
    }

    /* Do we have enough data to read the entire header now? */
    if (lm->bufHead - lm->bufTail < jsonLen)
    {
        return false;
    }

    /* Copy the header results if the caller wants them. */
    if (data != NULL)
    {
        *data = malloc(jsonLen+1);
        memset(*data, 0, jsonLen+1);
        memcpy(*data, &lm->buf[lm->bufTail], jsonLen);
    }

    if (len != NULL)
        *len = jsonLen;

    lm->bufTail += jsonLen;
    lm->srcTailPos += jsonLen;

    return true;
}


/*
 * NAME:        LmBufGetNextPacket
 * ACTION:      Gets the next available packet from the buffer. If no
 *              packet is available it returns false.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 *              LmPacket *packet - the parsed packet is written here.
 * RETURNS:     bool - true on success, false if no packet is available.
 */

bool LmBufGetNextPacket(LmBuf *lm, LmPacket *packet)
{
    size_t       bufBytes;
    uint8_t     *bufPos;
    uint8_t      packetType;
    bool         corrupted = false;
    uint32_t     word;
    uint32_t     word2;
    uint32_t     v;
    unsigned int i;
    char         errorMessage[160] = "error";
    unsigned int packetBytes = sizeof(uint32_t);
    uint32_t     val_u32;

    /* Get a valid packet type. */
    do
    {
        if (lm->bufHead - lm->bufTail < sizeof(uint32_t))
        {
            /* Not enough data for a packet. Give up. */
            return false;
        }

        /* Get the packet type. */
        word = LMBUF_GET_WORD(&lm->buf[lm->bufTail], 0);
        packetType = word >> 28;

        /* Is it an out-of-sync packet? */
        if (packetType == 0x1)
        {
            /* This is the second word of a pulse, maybe split between buffers. Resync. */
            lm->bufTail += sizeof(uint32_t);
            lm->srcTailPos += sizeof(uint32_t);
        }

    } while (packetType == 0x1);

    /* Get the updated buffer size and pos. */
    bufBytes = lm->bufHead - lm->bufTail;
    bufPos = &lm->buf[lm->bufTail];

    /* Interpret the packet. */
    switch (packetType)
    {
    case 0x0: /* Pulse event. */
        packetBytes = sizeof(uint32_t) * 2;
        if (bufBytes < packetBytes)
            return false;

        packet->typ = LmPacketTypePulse;
        packet->p.pulse.invalid = (word >> 27) & 0x1;
        packet->p.pulse.amplitude = (int32_t)LMBUF_SIGN_EXTEND_24_TO_32(word);
        word2 = LMBUF_GET_WORD(bufPos, 1);
        if ((word2 >> 28) == 0x1)
        {
            /* Packet has a time of arrival. */
            packet->p.pulse.hasTimeOfArrival = true;
            packet->p.pulse.timeOfArrival = (word2 >> 8) & 0xfff;
            packet->p.pulse.subSampleTimeOfArrival = word2 & 0xff;
        }
        else
        {
            /* Short packet - no time of arrival. */
            packetBytes = sizeof(uint32_t);
            packet->p.pulse.hasTimeOfArrival = false;
            packet->p.pulse.timeOfArrival = 0;
            packet->p.pulse.subSampleTimeOfArrival = 0;
        }

        lm->bufTail += packetBytes;
        lm->srcTailPos += packetBytes;
        break;

    case 0x8: /* Sync event. */
        packetBytes = sizeof(uint32_t);
        packet->typ = LmPacketTypeSync;
        packet->p.sync.timestamp = word & 0x00ffffff;
        lm->bufTail += packetBytes;
        lm->srcTailPos += packetBytes;
        break;

    case 0xa: /* Gated statistics data. */
        packetBytes = sizeof(uint32_t) * 11;
        if (bufBytes < packetBytes)
            return false;

        /* Check integrity. */
        for (i = 0; i < 10 && !corrupted; i++)
        {
            if ( (LMBUF_GET_WORD(bufPos, i) >> 24) != (0xa0 | i) )
            {
                corrupted = true;
                sprintf(errorMessage, "gated stats incomplete at word %d", i);
            }
        }

        if ( !corrupted && (LMBUF_GET_WORD(bufPos, 10) >> 24) != 0xaf)
        {
            corrupted = true;
            sprintf(errorMessage, "gated stats incomplete at word 10");
        }

        if (corrupted)
            break;

        /* Get fields. */
        packet->typ = LmPacketTypeGatedStats;
        packet->p.gatedStats.sampleCount = (LMBUF_GET_WORD(bufPos, 1) << 24) | (word & 0x00ffffff);
        packet->p.gatedStats.erasedSampleCount = LMBUF_GET_WORD(bufPos, 2) & 0x00ffffff;
        packet->p.gatedStats.saturatedSampleCount = LMBUF_GET_WORD(bufPos, 3) & 0x00ffffff;
        packet->p.gatedStats.estimatedIncomingPulseCount = LMBUF_GET_WORD(bufPos, 4) & 0x00ffffff;
        packet->p.gatedStats.rawIncomingPulseCount = LMBUF_GET_WORD(bufPos, 5) & 0x00ffffff;
        for (i = 0; i < 4; i++)
        {
            packet->p.gatedStats.counter[i] = LMBUF_GET_WORD(bufPos, i+6) & 0x00ffffff;
        }
        packet->p.gatedStats.timestamp = LMBUF_GET_WORD(bufPos, 10) & 0x00ffffff;

        lm->bufTail += packetBytes;
        lm->srcTailPos += packetBytes;
        break;

    case 0xb: /* Spatial statistics. */
        packetBytes = sizeof(uint32_t) * 11;
        if (bufBytes < packetBytes)
            return false;

        /* Check integrity. */
        for (i = 0; i < 10 && !corrupted; i++)
        {
            if ( (LMBUF_GET_WORD(bufPos, i) >> 24) != (0xb0 | i) )
            {
                corrupted = true;
                sprintf(errorMessage, "spatial stats incomplete at word %d", i);
            }
        }

        if ( !corrupted && (LMBUF_GET_WORD(bufPos, 10) >> 24) != 0xbf)
        {
            corrupted = true;
            sprintf(errorMessage, "spatial stats incomplete at word 10");
        }

        if (corrupted)
            break;

        /* Get fields. */
        packet->typ = LmPacketTypeSpatialStats;
        packet->p.spatialStats.sampleCount = (LMBUF_GET_WORD(bufPos, 1) << 24) | (word & 0x00ffffff);
        packet->p.spatialStats.erasedSampleCount = LMBUF_GET_WORD(bufPos, 2) & 0x00ffffff;
        packet->p.spatialStats.saturatedSampleCount = LMBUF_GET_WORD(bufPos, 3) & 0x00ffffff;
        packet->p.spatialStats.estimatedIncomingPulseCount = LMBUF_GET_WORD(bufPos, 4) & 0x00ffffff;
        packet->p.spatialStats.rawIncomingPulseCount = LMBUF_GET_WORD(bufPos, 5) & 0x00ffffff;
        for (i = 0; i < 4; i++)
        {
            packet->p.spatialStats.counter[i] = LMBUF_GET_WORD(bufPos, i+6) & 0x00ffffff;
        }
        packet->p.spatialStats.timestamp = LMBUF_GET_WORD(bufPos, 10) & 0x00ffffff;

        lm->bufTail += packetBytes;
        lm->srcTailPos += packetBytes;
        break;

    case 0xc: /* Spatial position event. */
        packetBytes = sizeof(uint32_t) * 7;
        if (bufBytes < packetBytes)
            return false;

        /* Check integrity. */
        for (i = 0; i < 6 && !corrupted; i++)
        {
            if ( (LMBUF_GET_WORD(bufPos, i) >> 24) != (0xc0 | i) )
            {
                corrupted = true;
                sprintf(errorMessage, "spatial position incomplete at word %d", i);
            }
        }

        if ( !corrupted && (LMBUF_GET_WORD(bufPos, 6) >> 24) != 0xcf)
        {
            corrupted = true;
            sprintf(errorMessage, "spatial position incomplete at word 6");
        }

        if (corrupted)
            break;

        /* Get fields. */
        packet->typ = LmPacketTypeSpatialPosition;
        for (i = 0; i < 6; i++)
        {
            uint32_t val = LMBUF_GET_WORD(bufPos, i);
            packet->p.spatialPosition.axis[i] = (int32_t)LMBUF_SIGN_EXTEND_24_TO_32(val);
        }
        packet->p.spatialPosition.timestamp = LMBUF_GET_WORD(bufPos, 6) & 0x00ffffff;

        lm->bufTail += packetBytes;
        lm->srcTailPos += packetBytes;
        break;

    case 0xd: /* Gate state event. */
        packetBytes = sizeof(uint32_t);
        packet->typ = LmPacketTypeGateState;
        packet->p.gateState.gate = (word >> 24) & 0x1;
        packet->p.gateState.timestamp = word & 0x00ffffff;
        lm->bufTail += packetBytes;
        lm->srcTailPos += packetBytes;
        break;

    case 0xe: /* Periodic statistics. */
        packetBytes = sizeof(uint32_t) * 10;
        if (bufBytes < packetBytes)
            return false;

        /* Check integrity. */
        if ( (word >> 24) != 0xe0)
            corrupted = true;

        for (i = 2; i < 10; i++)
        {
            if ( (LMBUF_GET_WORD(bufPos, i-1) >> 24) != (0xe0 | i) )
            {
                corrupted = true;
                sprintf(errorMessage, "periodic stats incomplete at word %d", i-2);
            }
        }

        if ( !corrupted && (LMBUF_GET_WORD(bufPos, 9) >> 24) != 0xef)
        {
            corrupted = true;
            sprintf(errorMessage, "periodic stats incomplete at word 9");
        }

        if (corrupted)
            break;

        /* Get fields. */
        packet->typ = LmPacketTypePeriodicStats;
        packet->p.periodicStats.sampleCount = word & 0x00ffffff;
        packet->p.periodicStats.erasedSampleCount = LMBUF_GET_WORD(bufPos, 1) & 0x00ffffff;
        packet->p.periodicStats.saturatedSampleCount = LMBUF_GET_WORD(bufPos, 2) & 0x00ffffff;
        packet->p.periodicStats.estimatedIncomingPulseCount = LMBUF_GET_WORD(bufPos, 3) & 0x00ffffff;
        packet->p.periodicStats.rawIncomingPulseCount = LMBUF_GET_WORD(bufPos, 4) & 0x00ffffff;
        for (i = 0; i < 4; i++)
        {
            packet->p.periodicStats.counter[i] = LMBUF_GET_WORD(bufPos, i+5) & 0x00ffffff;
        }
        packet->p.periodicStats.timestamp = LMBUF_GET_WORD(bufPos, 9) & 0x00ffffff;

        lm->bufTail += packetBytes;
        lm->srcTailPos += packetBytes;
        break;

    case 0xf:
        packetBytes = sizeof(uint32_t);
        v = word >> 24;
        if (v == 0xf0)
        {
            /* Analog status event. */
            packet->typ = LmPacketTypeAnalogStatus;
            packet->p.analogStatus.positiveSaturation = (word >> 19) & 0x1;
            packet->p.analogStatus.negativeSaturation = (word >> 18) & 0x1;
            lm->bufTail += packetBytes;
            lm->srcTailPos += packetBytes;
        }
        else if (v == 0xf1)
        {
            /* Internal buffer overflow. */
            packet->typ = LmPacketTypeInternalBufferOverflow;
            packet->p.internalBufferOverflow.timestamp = word & 0x00ffffff;
            lm->bufTail += packetBytes;
            lm->srcTailPos += packetBytes;
        }
        else
        {
            /* Invalid packet. */
            corrupted = true;
            sprintf(errorMessage, "invalid extended packet type 0x%02x", v);
        }
        break;

    default:
        corrupted = true;
        packetBytes = sizeof(uint32_t);
        sprintf(errorMessage, "invalid packet type 0x%x", packetType);
        break;
    }

    if (corrupted)
    {
        /* Invalid packet. */
        int errBytesWritten = 0;
        char *errPos = packet->p.error.message;
        errPos += sprintf(packet->p.error.message, "%s at offset %ld:", errorMessage, lm->srcTailPos);

        for (i = 0; i < packetBytes; i++)
        {
            if (lm->bufTail + i < lm->bufHead)
            {
                size_t errSpaceAvailable = sizeof(packet->p.error.message) - (size_t)(errPos - packet->p.error.message);
                errBytesWritten = snprintf(errPos, errSpaceAvailable, " %02x", lm->buf[lm->bufTail + i]);
                if ((size_t)errBytesWritten <= errSpaceAvailable)
                    errPos += errBytesWritten;
            }
        }

        lm->srcTailPos++;
        lm->bufTail++;
        packet->typ = LmPacketTypeError;
    }

    return true;
}


/*
 * NAME:        LmBufTranslatePacketSimple
 * ACTION:      Translates a LmPacket into textual form.
 * PARAMETERS:  char *textBuf     - where to place the resulting text.
 *              int textBufLen    - how large the text buffer is.
 *              LmPacket *packet  - the parsed packet is written here.
 *              int *packetLenPtr - if not NULL the binary length of the packet will be placed here.
 * RETURNS:     int - the number of bytes placed in the text buffer.
 */

int LmBufTranslatePacketSimple(char *textBuf, int textBufLen, LmPacket *packet, int *packetLenPtr)
{
    int packetLen = 0;
    int bytesWritten = 0;

    /* Show the packet contents. */
    switch (packet->typ)
    {
    case LmPacketTypeError:
        bytesWritten = snprintf(textBuf, (size_t)textBufLen, "error %s", packet->p.error.message);
        break;

    case LmPacketTypeSync:
        bytesWritten = snprintf(textBuf, (size_t)textBufLen, "sync %d", packet->p.sync.timestamp);
        packetLen = sizeof(uint32_t);
        break;

    case LmPacketTypePulse:
        if (packet->p.pulse.hasTimeOfArrival)
        {
            bytesWritten = snprintf(textBuf, (size_t)textBufLen,
                                    "pulse %d %d %d %d",
                                    packet->p.pulse.invalid,
                                    packet->p.pulse.amplitude,
                                    packet->p.pulse.timeOfArrival,
                                    packet->p.pulse.subSampleTimeOfArrival);
            packetLen = sizeof(uint32_t) * 2;
        }
        else
        {
            bytesWritten = snprintf(textBuf, (size_t)textBufLen, "pulse_short %d %d",
                   packet->p.pulse.invalid,
                   packet->p.pulse.amplitude);
            packetLen = sizeof(uint32_t);
        }
        break;

    case LmPacketTypeGateState:
        bytesWritten = snprintf(textBuf, (size_t)textBufLen,
               "gateState %d %d",
               packet->p.gateState.gate,
               packet->p.gateState.timestamp);
        packetLen = sizeof(uint32_t);
        break;

    case LmPacketTypeGatedStats:
        bytesWritten = snprintf(textBuf, (size_t)textBufLen,
               "gatedStats %d %d %d %d %d %d %d %d %d %d",
               packet->p.gatedStats.sampleCount,
               packet->p.gatedStats.erasedSampleCount,
               packet->p.gatedStats.saturatedSampleCount,
               packet->p.gatedStats.estimatedIncomingPulseCount,
               packet->p.gatedStats.rawIncomingPulseCount,
               packet->p.gatedStats.counter[0],
               packet->p.gatedStats.counter[1],
               packet->p.gatedStats.counter[2],
               packet->p.gatedStats.counter[3],
               packet->p.gatedStats.timestamp);
        packetLen = sizeof(uint32_t) * 9;
        break;

    case LmPacketTypeSpatialPosition:
        bytesWritten = snprintf(textBuf, (size_t)textBufLen,
               "spatialPosition %d %d %d %d %d %d %d",
               packet->p.spatialPosition.axis[0],
               packet->p.spatialPosition.axis[1],
               packet->p.spatialPosition.axis[2],
               packet->p.spatialPosition.axis[3],
               packet->p.spatialPosition.axis[4],
               packet->p.spatialPosition.axis[5],
               packet->p.spatialPosition.timestamp);
        packetLen = sizeof(uint32_t) * 7;
        break;

    case LmPacketTypeSpatialStats:
        bytesWritten = snprintf(textBuf, (size_t)textBufLen,
               "spatialStats %d %d %d %d %d %d %d %d %d %d",
               packet->p.spatialStats.sampleCount,
               packet->p.spatialStats.erasedSampleCount,
               packet->p.spatialStats.saturatedSampleCount,
               packet->p.spatialStats.estimatedIncomingPulseCount,
               packet->p.spatialStats.rawIncomingPulseCount,
               packet->p.spatialStats.counter[0],
               packet->p.spatialStats.counter[1],
               packet->p.spatialStats.counter[2],
               packet->p.spatialStats.counter[3],
               packet->p.spatialStats.timestamp);
        packetLen = sizeof(uint32_t) * 9;
        break;

    case LmPacketTypePeriodicStats:
        bytesWritten = snprintf(textBuf, (size_t)textBufLen,
               "periodicStats %d %d %d %d %d %d %d %d %d %d",
               packet->p.periodicStats.sampleCount,
               packet->p.periodicStats.erasedSampleCount,
               packet->p.periodicStats.saturatedSampleCount,
               packet->p.periodicStats.estimatedIncomingPulseCount,
               packet->p.periodicStats.rawIncomingPulseCount,
               packet->p.periodicStats.counter[0],
               packet->p.periodicStats.counter[1],
               packet->p.periodicStats.counter[2],
               packet->p.periodicStats.counter[3],
               packet->p.periodicStats.timestamp);
        packetLen = sizeof(uint32_t) * 8;
        break;

    case LmPacketTypeAnalogStatus:
        bytesWritten = snprintf(textBuf, (size_t)textBufLen, "analogStatus %d %d", packet->p.analogStatus.positiveSaturation, packet->p.analogStatus.negativeSaturation);
        packetLen = sizeof(uint32_t);
        break;

    case LmPacketTypeInternalBufferOverflow:
        bytesWritten = snprintf(textBuf, (size_t)textBufLen, "internalBufferOverflow %d", packet->p.internalBufferOverflow.timestamp);
        packetLen = sizeof(uint32_t);
        break;
    }

    if (packetLenPtr != NULL)
    {
        *packetLenPtr = packetLen;
    }

    if (bytesWritten >= textBufLen-1)
        return (int)strlen(textBuf);
    else
        return bytesWritten;
}


/*
 * NAME:        LmBufTranslatePacketComplex
 * ACTION:      Translates a LmPacket into textual form.
 * PARAMETERS:  char *textBuf           - where to place the resulting text.
 *              int textBufLen          - how large the text buffer is.
 *              LmPacket *packet        - the parsed packet is written here.
 *              bool hexDump            - true to do a hex dump of the packet.
 *              LmBuf *lmBuf            - the buffer, only used with hexDump.
 *              bool showOnlyErrors     - only produces output for errors, not normal packets.
 *              bool showTimestamps     - true to show timestamps with each packet.
 *              uint32_t *lastTimestamp - used to track the timestamp.
 * RETURNS:     int - the number of bytes placed in the text buffer.
 */

int LmBufTranslatePacketComplex(char *textBuf, int textBufLen, LmPacket *packet, bool hexDump, LmBuf *lmBuf, bool showOnlyErrors, bool showTimestamps, uint32_t *lastTimestamp)
{
    int packetLen = 0;
    int i;
    uint32_t ts;
    int totalWritten = 0;
    int bytesWritten = 0;
    char *bpos = textBuf;
    int bufLen = textBufLen;

    /* Work out the timestamp. */
    switch (packet->typ)
    {
    case LmPacketTypeSync:                   *lastTimestamp = packet->p.sync.timestamp; ts = *lastTimestamp; break;
    case LmPacketTypeGateState:              *lastTimestamp = packet->p.gateState.timestamp; ts = *lastTimestamp; break;
    case LmPacketTypeGatedStats:             *lastTimestamp = packet->p.gatedStats.timestamp; ts = *lastTimestamp; break;
    case LmPacketTypeSpatialPosition:        *lastTimestamp = packet->p.spatialPosition.timestamp; ts = *lastTimestamp; break;
    case LmPacketTypeSpatialStats:           *lastTimestamp = packet->p.spatialStats.timestamp; ts = *lastTimestamp; break;
    case LmPacketTypePeriodicStats:          *lastTimestamp = packet->p.periodicStats.timestamp; ts = *lastTimestamp; break;
    case LmPacketTypeInternalBufferOverflow: *lastTimestamp = packet->p.internalBufferOverflow.timestamp; ts = *lastTimestamp; break;
    case LmPacketTypePulse:                  ts = *lastTimestamp + packet->p.pulse.timeOfArrival; break;
    default:                                 ts = *lastTimestamp; break;
    }

    /* If we're only showing errors, skip the rest. */
    if (showOnlyErrors && packet->typ != LmPacketTypeError)
        return 0;

    /* Show the timestamp. */
    if (showTimestamps)
    {
        /* Display the most recent timestamp. */
        snprintf(bpos, (size_t)bufLen, "%d ", ts);
        bytesWritten = (int)strlen(bpos);
        bpos += bytesWritten;
        bufLen -= bytesWritten;
        totalWritten += bytesWritten;
    }

    /* Show the main part of the packet. */
    bytesWritten = LmBufTranslatePacketSimple(bpos, bufLen, packet, &packetLen);
    bpos += bytesWritten;
    bufLen -= bytesWritten;
    totalWritten += bytesWritten;

    /* Show the hex dump. */
    if (hexDump && packetLen > 0)
    {
        static size_t addr = 0;
        snprintf(bpos, (size_t)bufLen, "   %08lx: ", addr);
        bytesWritten = (int)strlen(bpos);
        bpos += bytesWritten;
        bufLen -= bytesWritten;
        totalWritten += bytesWritten;
        addr += (size_t)packetLen;

        for (i = 0; i < packetLen; i++)
        {
            int offset = (int)lmBuf->bufTail - packetLen + i;
            if (offset >= 0)
            {
                snprintf(bpos, (size_t)bufLen, " %02x", lmBuf->buf[offset]);
                bytesWritten = (int)strlen(bpos);
                bpos += bytesWritten;
                bufLen -= bytesWritten;
                totalWritten += bytesWritten;
            }
        }
    }

    return totalWritten;
}
