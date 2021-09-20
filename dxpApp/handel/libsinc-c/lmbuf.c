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
#include <inttypes.h>

#include "lmbuf.h"


/* The initial size of the input data buffer. */
#define LMBUF_HEADER_LINE_LEN 28
#define LMBUF_INITIAL_SIZE 65536
#define LMBUF_CHECK_WORD_AVAILABLE(lmbuf, offset) ( (lmbuf)->bufTail + ((offset)+1) * sizeof(uint32_t) < (lmbuf)->bufHead )
#define LMBUF_RAW_GET_WORD(buf, offset) ( memcpy(&val_u32, ((uint8_t *)(buf)) + ((offset) * sizeof(val_u32)), sizeof(val_u32)), val_u32 )
#define LMBUF_LM_GET_WORD(lm, offset) ( LMBUF_RAW_GET_WORD(&(lm)->buf[(lm)->bufTail], (offset)) )
#define LMBUF_SIGN_EXTEND_24_TO_32(v) (((v) & 0x00800000) ? (((v) & 0x7fffff) | 0xff800000) : ((v) & 0x7fffff))
#define LMBUF_READ_AHEAD 5
#define LMBUF_STREAM_ALIGN_WORD 0x70717273
#define LMBUF_ERROR_MESSAGE_BUFFER_LEN 160

#define LMBUF_NEW_DECODER


/* The packet's event type marker. */
typedef enum
{
    LmEventTypePulse                  = 0x0,
    LmEventTypePulseToa               = 0x1,
    LmEventTypeStreamAlign            = 0x7,
    LmEventTypeSync                   = 0x8,
    LmEventTypeGatedStats             = 0xa,
    LmEventTypeSpatialStats           = 0xb,
    LmEventTypeSpatialPosition        = 0xc,
    LmEventTypeGateState              = 0xd,
    LmEventTypePeriodicStats          = 0xe,
    LmEventTypeInternalBufferOverflow = 0xf
} LmEventType;


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
 * NAME:        LmBufClear
 * ACTION:      Clears an LmBuf, emptying the contents but keeping the buffer memory ready.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 */

void LmBufClear(LmBuf *lm)
{
    lm->bufSize = 0;
    lm->bufHead = 0;
    lm->bufTail = 0;
    lm->srcTailPos = 0;
    lm->scanStreamAlign = false;
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
    uint8_t *mem = realloc(lm->buf, newSize);
    if (mem == NULL)
        return false;

    lm->buf = mem;
    lm->bufSize = newSize;

    return true;
}


/*
 * NAME:        LmBufAddData
 * ACTION:      Adds some binary data to the head of the buffer. Use
 *              this to add data read from a file or stream, then use
 *              LmBufGetNextPacket() to get packets out of the buffer.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 * RETURNS:     bool - true on success, false if out of memory.
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


#ifdef LMBUF_NEW_DECODER

/*
 * The concept of this new, simpler list mode decoder:
 *
 *   1) All data is assumed to be correctly byte aligned. ie. Decoding must
 *      start on a 32 bit word boundary.
 *
 *   2) If data is corrupted or misaligned we should fail quickly and then
 *      scan for an alignment word which unambiguously indicates the word
 *      and packet boundaries.
 *
 *   3) Don't make assumptions about the ordering of words in a packet
 *      in cases where the event sequence number is available. The words
 *      may arrive in any order except that the last event subtype must
 *      be 0xf. This applies to statistics packets and spatial position
 *      event packets.
 *
 * These assumptions have some effects:
 *
 *   * Much less false positives will be generated when attempting to
 *     resync after corrupted data is encountered.
 *
 *   * The code can be simpler and more robust against protocol changes.
 *
 *   * List mode data streams must contain alignment words.
 *
 *   * Don't start reading data streams on non-word boundaries if at
 *     all possible or some data will be lost.
 *
 *   * Don't split list mode data files on non-word boundaries.
 */


/*
 * NAME:        LmBufGetPulsePacket
 * ACTION:      Gets a pulse packet from the buffer. If no
 *              packet is available it returns false.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 *              LmPacket *packet - the parsed packet is written here.
 *              uint32_t word0 - the first word of the packet.
 *              bool *corrupted - set to true if corrupted data is detected.
 *              unsigned int packetBytes - set to the number of bytes in the detected packet.
 *              char *errorMessage - an error message is written here if corrupted data is found.
 * RETURNS:     bool - true on success, false if no packet is available.
 */

static bool LmBufGetPulsePacket(LmBuf *lm, LmPacket *packet, uint32_t word0, unsigned int *packetBytes)
{
    /* Do we have enough data to get a pulse packet unambiguously? */
    if (!LMBUF_CHECK_WORD_AVAILABLE(lm, 1))
    {
        /* We don't have enough data to decode this packet yet. */
        return false;
    }

    /* Get the packet data. */
    uint32_t val_u32;
    uint32_t word1 = LMBUF_LM_GET_WORD(lm, 1);
    LmEventType word1EventType = (LmEventType)(word1 >> 28);

    packet->typ = LmPacketTypePulse;
    packet->p.pulse.amplitude = (int32_t)LMBUF_SIGN_EXTEND_24_TO_32(word0);
    packet->p.pulse.invalid = (word0 & 0x08000000) != 0;

    if (word1EventType == LmEventTypePulseToa)
    {
        packet->p.pulse.hasTimeOfArrival = true;
        packet->p.pulse.timeOfArrival = (word1 >> 8) & 0xfff;
        packet->p.pulse.subSampleTimeOfArrival = word1 & 0xff;
        packet->p.pulse.inMarkedRange = ((word1 >> 20) & 0x1) != 0;
        *packetBytes = sizeof(uint32_t) * 2;
    }
    else
    {
        packet->p.pulse.hasTimeOfArrival = false;
        packet->p.pulse.timeOfArrival = 0;
        packet->p.pulse.subSampleTimeOfArrival = 0;
        packet->p.pulse.inMarkedRange = false;
        *packetBytes = sizeof(uint32_t);
    }

    return true;
}


/*
 * NAME:        LmBufGetStreamAlignPacket
 * ACTION:      Gets a stream alignment packet from the buffer. If no
 *              packet is available it returns false.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 *              LmPacket *packet - the parsed packet is written here.
 *              uint32_t word0 - the first word of the packet.
 *              bool *corrupted - set to true if corrupted data is detected.
 *              unsigned int packetBytes - set to the number of bytes in the detected packet.
 *              char *errorMessage - an error message is written here if corrupted data is found.
 * RETURNS:     bool - true on success, false if no packet is available.
 */

static bool LmBufGetStreamAlignPacket(LmPacket *packet, uint32_t word0, bool *corrupted, unsigned int *packetBytes, char *errorMessage)
{
    *packetBytes = sizeof(uint32_t);

    /* Check the stream alignment packet. */
    if (word0 != LMBUF_STREAM_ALIGN_WORD)
    {
        /* Bad stream alignment word - must must be corrupted. */
        *corrupted = true;
        snprintf(errorMessage, LMBUF_ERROR_MESSAGE_BUFFER_LEN, "invalid stream alignment data 0x%08" PRIu32 "x", word0);

        return true;
    }

    /* Make the decoded packet. */
    packet->typ = LmPacketTypeStreamAlign;
    packet->p.streamAlign.pattern = word0;

    return true;
}


/*
 * NAME:        LmBufGetGateStatePacket
 * ACTION:      Gets a gate state packet from the buffer. If no
 *              packet is available it returns false.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 *              LmPacket *packet - the parsed packet is written here.
 *              uint32_t word0 - the first word of the packet.
 *              bool *corrupted - set to true if corrupted data is detected.
 *              unsigned int packetBytes - set to the number of bytes in the detected packet.
 *              char *errorMessage - an error message is written here if corrupted data is found.
 * RETURNS:     bool - true on success, false if no packet is available.
 */

static bool LmBufGetGateStatePacket(LmPacket *packet, uint32_t word0, unsigned int *packetBytes)
{
    *packetBytes = sizeof(uint32_t);

    /* Make the decoded packet. */
    packet->typ = LmPacketTypeGateState;
    packet->p.gateState.gate = (word0 & 0x1000000) != 0;
    packet->p.gateState.timestamp = word0 & 0xffffff;

    return true;
}


/*
 * NAME:        LmBufGetSyncPacket
 * ACTION:      Gets a sync packet from the buffer. If no
 *              packet is available it returns false.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 *              LmPacket *packet - the parsed packet is written here.
 *              uint32_t word0 - the first word of the packet.
 *              bool *corrupted - set to true if corrupted data is detected.
 *              unsigned int packetBytes - set to the number of bytes in the detected packet.
 *              char *errorMessage - an error message is written here if corrupted data is found.
 * RETURNS:     bool - true on success, false if no packet is available.
 */

static bool LmBufGetSyncPacket(LmPacket *packet, uint32_t word0, unsigned int *packetBytes)
{
    *packetBytes = sizeof(uint32_t);

    /* Make the decoded packet. */
    packet->typ = LmPacketTypeSync;
    packet->p.sync.timestamp = word0 & 0x00ffffff;

    return true;
}


/*
 * NAME:        LmBufGetNumberedPacket
 * ACTION:      Gets the next available packet from the buffer. If no
 *              packet is available it returns false.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 *              LmPacket *packet - the parsed packet is written here.
 *              LmEventType eventType - the event type of the packet to decode.
 *              bool *corrupted - set to true if corrupted data is detected.
 *              unsigned int packetBytes - set to the number of bytes in the detected packet.
 *              char *errorMessage - an error message is written here if corrupted data is found.
 * RETURNS:     bool - true on success, false if no packet is available.
 */

static bool LmBufGetNumberedPacket(LmBuf *lm, LmPacket *packet, LmEventType eventType, bool *corrupted, unsigned int *packetBytes, char *errorMessage)
{
    uint32_t    words[16];
    uint32_t    val_u32;
    uint32_t    word;
    LmEventType seenEventType = eventType;
    uint32_t    subtype;
    int         offset;
    int         i;

    /* Clear the packet contents. */
    memset(words, 0, sizeof(words));

    /* Collect packet data until we've read a 0xf subtype. */
    subtype = 0;
    for (offset = 0; offset < 16 && seenEventType == eventType && subtype != 0xf; offset++)
    {
        if (!LMBUF_CHECK_WORD_AVAILABLE(lm, offset))
        {
            /* We don't have enough data to decode this packet yet. */
            return false;
        }

        /* Get the next word of data. */
        word = LMBUF_LM_GET_WORD(lm, offset);
        seenEventType = (word >> 28) & 0xf;
        if (seenEventType != eventType)
        {
            /* Mismatching event type - end the packet. */
            strcpy(errorMessage, "packet doesn't end in subtype 0xf");
            *corrupted = true;
            *packetBytes = offset * sizeof(uint32_t);
            return true;
        }

        if (seenEventType == eventType)
        {
            /* It's part of this packet. */
            subtype = (word >> 24) & 0xf;
            words[subtype] = word & 0x00ffffff;
        }
    }

    *packetBytes = offset * sizeof(uint32_t);

    if (subtype != 0xf)
    {
        /* The packet looks strange - either it's too long or it didn't
         * end in subtype 0xf. */
        *corrupted = true;
        if (seenEventType != eventType)
        {
            snprintf(errorMessage, LMBUF_ERROR_MESSAGE_BUFFER_LEN, "invalid event type 0x%x in packet type 0x%x", seenEventType, eventType);
        }
    }

    /* Construct the decoded packets of each type. */
    switch (eventType)
    {
    case LmEventTypeGatedStats:
        packet->typ = LmPacketTypeGatedStats;
        packet->p.gatedStats.sampleCount = (words[1] << 24) | words[0x0];
        packet->p.gatedStats.erasedSampleCount = words[0x2];
        packet->p.gatedStats.saturatedSampleCount = words[0x3];
        packet->p.gatedStats.rawIncomingPulseCount = (words[0x5] << 24) | words[0x4];
        for (i = 0; i < 4; i++)
        {
            packet->p.gatedStats.counter[i] = words[i+0x6];
        }

        packet->p.gatedStats.vetoSampleCount = words[0xa];
        packet->p.gatedStats.timestamp = words[0xf];
        break;

    case LmEventTypeSpatialStats:
        packet->typ = LmPacketTypeSpatialStats;
        packet->p.spatialStats.sampleCount = (words[1] << 24) | words[0x0];
        packet->p.spatialStats.erasedSampleCount = words[0x2];
        packet->p.spatialStats.saturatedSampleCount = words[0x3];
        packet->p.spatialStats.estimatedIncomingPulseCount = words[0x4];
        packet->p.spatialStats.rawIncomingPulseCount = words[0x5];
        for (i = 0; i < 4; i++)
        {
            packet->p.spatialStats.counter[i] = words[i+0x6];
        }

        packet->p.spatialStats.vetoSampleCount = words[0xa];
        packet->p.spatialStats.timestamp = words[0xf];
        break;

    case LmEventTypePeriodicStats:
        packet->typ = LmPacketTypePeriodicStats;
        packet->p.periodicStats.sampleCount = words[0x0];
        packet->p.periodicStats.erasedSampleCount = words[0x2];
        packet->p.periodicStats.saturatedSampleCount = words[0x3];
        packet->p.periodicStats.estimatedIncomingPulseCount = words[0x4];
        packet->p.periodicStats.rawIncomingPulseCount = words[0x5];
        for (i = 0; i < 4; i++)
        {
            packet->p.periodicStats.counter[i] = words[i+0x6];
        }

        packet->p.periodicStats.vetoSampleCount = words[0xa];
        packet->p.periodicStats.timestamp = words[0xf];
        break;

    case LmEventTypeSpatialPosition:
        packet->typ = LmPacketTypeSpatialPosition;
        for (i = 0; i < 6; i++)
        {
            packet->p.spatialPosition.axis[i] = (int32_t)LMBUF_SIGN_EXTEND_24_TO_32(words[i]);
        }

        packet->p.spatialPosition.timestamp = words[0xf];
        break;

    default:
        break;
    }

    return true;
}


/*
 * NAME:        LmBufScanStreamAlign
 * ACTION:      Scans the buffer for a stream align word.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 *              LmPacket *packet - the parsed packet is written here.
 * RETURNS:     bool - true if the stream align word was found, false if not.
 */

static bool LmBufScanStreamAlign(LmBuf *lm)
{
    size_t i;

    /* Scan for the first byte. */
    for (i = lm->bufTail; i < lm->bufHead - 3; i++)
    {
        if (lm->buf[i] == (LMBUF_STREAM_ALIGN_WORD & 0xff))
        {
            /* The first byte matches. Check the rest. */
            uint32_t word;
            memcpy(&word, &lm->buf[i], sizeof(word));  /* Fixes memory alignment. */
            if (word == LMBUF_STREAM_ALIGN_WORD)
            {
                lm->srcTailPos += i - lm->bufTail;
                lm->bufTail = i;
                return true;
            }
        }
    }

    /* Wasn't found. Delete the scanned data. */
    lm->srcTailPos += lm->bufHead - lm->bufTail - 3;
    lm->bufTail = lm->bufHead - 3;

    return false;
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
    uint32_t     val_u32;
    uint32_t     word;
    LmEventType  eventType;
    bool         packetFound = false;
    bool         corrupted = false;
    char         errorMessage[LMBUF_ERROR_MESSAGE_BUFFER_LEN] = "error";
    unsigned int packetBytes = 0;
    size_t       i;

    /* Check if we've got a word to read. */
    if (!LMBUF_CHECK_WORD_AVAILABLE(lm, 0))
    {
        /* We don't have enough data to decode this packet yet. */
        return false;
    }

    /* Are we in the process of scanning for a stream align marker? */
    if (lm->scanStreamAlign)
    {
        if (!LmBufScanStreamAlign(lm))
            return false;
    }

    /* Get the event type of this packet. */
    word = LMBUF_LM_GET_WORD(lm, 0);
    eventType = (LmEventType)(word >> 28);
    switch (eventType)
    {
    case LmEventTypePulse:
        packetFound = LmBufGetPulsePacket(lm, packet, word, &packetBytes);
        break;

    case LmEventTypePulseToa:
        strcpy(errorMessage, "pulse toa without pulse");
        corrupted = true;
        packetBytes = sizeof(uint32_t);
        packetFound = true;
        break;

    case LmEventTypeStreamAlign:
        packetFound = LmBufGetStreamAlignPacket(packet, word, &corrupted, &packetBytes, errorMessage);
        if (packetFound)
        {
            /* Reset scanning for a stream align packet. */
            lm->scanStreamAlign = false;
        }
        break;

    case LmEventTypeGateState:
        packetFound = LmBufGetGateStatePacket(packet, word, &packetBytes);
        break;

    case LmEventTypeSync:
        packetFound = LmBufGetSyncPacket(packet, word, &packetBytes);
        break;

    /* Event types which have numbered fields and finish with an 0xf subtype. */
    case LmEventTypeGatedStats:
    case LmEventTypeSpatialStats:
    case LmEventTypeSpatialPosition:
    case LmEventTypePeriodicStats:
        packetFound = LmBufGetNumberedPacket(lm, packet, eventType, &corrupted, &packetBytes, errorMessage);
        break;

    case LmEventTypeInternalBufferOverflow:
        packet->typ = LmPacketTypeInternalBufferOverflow;
        packet->p.internalBufferOverflow.timestamp = word & 0xffffff;
        packetBytes = sizeof(uint32_t);
        packetFound = true;
        break;

    default:
        corrupted = true;
        packetBytes = sizeof(uint32_t);
        strcpy(errorMessage, "invalid event type");
        packetFound = true;
        break;
    }

    if (!packetFound)
    {
        /* No complete packet was found. */
        return false;
    }

    if (corrupted)
    {
        /* Invalid packet. */
        int errBytesWritten = 0;
        char *errPos = packet->p.error.message;
        errPos += sprintf(packet->p.error.message, "%s at offset %zu:", errorMessage, lm->srcTailPos);

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

        packet->typ = LmPacketTypeError;
        lm->scanStreamAlign = true;
    }

    /* Do we need to adjust the number of bytes left in the buffer? */
    if (packetBytes > 0)
    {
        lm->bufTail += packetBytes;
        lm->srcTailPos += packetBytes;
    }

    return true;
}

#else /* !LMBUF_NEW_DECODER */

/*
 * NAME:        checkOptionalItems
 * ACTION:      Checks to see if an optional gate veto item is present before
 *              the timestamp in a packet.
 */

static bool checkOptionalItems(uint8_t *bufPos, int offset, uint8_t flagByte, uint8_t flagSkipMax, uint32_t *vetoValue, unsigned int *timestampOffset, bool *corrupted, unsigned int *packetBytes, size_t bufBytes)
{
    uint32_t val_u32;
    *vetoValue = 0;
    *timestampOffset = offset;

    /* If it's already corrupted don't look for this optional item. */
    if (*corrupted)
    {
        return false;
    }

    /* Check for the optional word. */
    if ( (LMBUF_RAW_GET_WORD(bufPos, offset) >> 24) != flagByte)
        return false;

    /* Optional word was found. */
    *packetBytes += sizeof(uint32_t);
    if (bufBytes < *packetBytes)
        return false;

    *vetoValue = LMBUF_RAW_GET_WORD(bufPos, offset) & 0xffffff;
    offset++;

    /* Skip past any unused items which may be added in the future. */
    flagByte++;
    while (bufBytes >= *packetBytes && flagByte < flagSkipMax && (LMBUF_RAW_GET_WORD(bufPos, offset) >> 24) == flagByte)
    {
        flagByte++;
        offset++;
        *packetBytes += sizeof(uint32_t);
    }

    *timestampOffset = offset;

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
    char         errorMessage[LMBUF_ERROR_MESSAGE_BUFFER_LEN] = "error";
    unsigned int packetBytes = sizeof(uint32_t);
    uint32_t     val_u32;
    uint32_t     vetoValue = 0;
    unsigned int timestampOffset = 0;

    /* Get a valid packet type. */
    do
    {
        if (lm->bufHead - lm->bufTail < sizeof(uint32_t))
        {
            /* Not enough data for a packet. Give up. */
            return false;
        }

        /* Get the packet type. */
        word = LMBUF_RAW_GET_WORD(&lm->buf[lm->bufTail], 0);
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
        word2 = LMBUF_RAW_GET_WORD(bufPos, 1);
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

    case 0x7: /* The first nibble of stream alignment pattern. */
        packetBytes = sizeof(uint32_t);
        packet->typ = LmPacketTypeStreamAlign;
        packet->p.streamAlign.pattern = word;

        lm->bufTail += packetBytes;
        lm->srcTailPos += packetBytes;

        /* Check if byte stream alignment is lost, which could be due to corruption in data, network congestion, etc */
        if (word != LMBUF_STREAM_ALIGN_WORD)
        {
            corrupted = true;
            sprintf(errorMessage, "Stream alignment pattern corrupted, value 0x%X", word);
        }
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
        if (bufBytes < packetBytes + sizeof(uint32_t) * LMBUF_READ_AHEAD)
            return false;

        /* Check integrity. */
        for (i = 0; i < 10 && !corrupted; i++)
        {
            if ( (LMBUF_RAW_GET_WORD(bufPos, i) >> 24) != (0xa0 | i) )
            {
                corrupted = true;
                sprintf(errorMessage, "gated stats incomplete at word %d", i);
            }
        }

        /* Optional veto sample count. */
        checkOptionalItems(bufPos, 10, 0xaa, 0xaf, &vetoValue, &timestampOffset, &corrupted, &packetBytes, bufBytes);

        /* Check for timestamp. */
        if ( !corrupted && (LMBUF_RAW_GET_WORD(bufPos, timestampOffset) >> 24) != 0xaf)
        {
            corrupted = true;
            sprintf(errorMessage, "gated stats incomplete at word 10");
        }

        if (corrupted)
            break;

        /* Get fields. */
        packet->typ = LmPacketTypeGatedStats;
        packet->p.gatedStats.sampleCount = (LMBUF_RAW_GET_WORD(bufPos, 1) << 24) | (word & 0x00ffffff);
        packet->p.gatedStats.erasedSampleCount = LMBUF_RAW_GET_WORD(bufPos, 2) & 0x00ffffff;
        packet->p.gatedStats.saturatedSampleCount = LMBUF_RAW_GET_WORD(bufPos, 3) & 0x00ffffff;
        packet->p.gatedStats.estimatedIncomingPulseCount = LMBUF_RAW_GET_WORD(bufPos, 4) & 0x00ffffff;
        packet->p.gatedStats.rawIncomingPulseCount = LMBUF_RAW_GET_WORD(bufPos, 5) & 0x00ffffff;
        for (i = 0; i < 4; i++)
        {
            packet->p.gatedStats.counter[i] = LMBUF_RAW_GET_WORD(bufPos, i+6) & 0x00ffffff;
        }

        packet->p.gatedStats.vetoSampleCount = vetoValue;
        packet->p.gatedStats.timestamp = LMBUF_RAW_GET_WORD(bufPos, timestampOffset) & 0x00ffffff;

        lm->bufTail += packetBytes;
        lm->srcTailPos += packetBytes;
        break;

    case 0xb: /* Spatial statistics. */
        packetBytes = sizeof(uint32_t) * 11;
        if (bufBytes < packetBytes + sizeof(uint32_t) * LMBUF_READ_AHEAD)
            return false;

        /* Check integrity. */
        for (i = 0; i < 10 && !corrupted; i++)
        {
            if ( (LMBUF_RAW_GET_WORD(bufPos, i) >> 24) != (0xb0 | i) )
            {
                corrupted = true;
                sprintf(errorMessage, "spatial stats incomplete at word %d", i);
            }
        }

        /* Optional veto sample count. */
        checkOptionalItems(bufPos, 10, 0xba, 0xbf, &vetoValue, &timestampOffset, &corrupted, &packetBytes, bufBytes);

        /* Check for timestamp. */
        if ( !corrupted && (LMBUF_RAW_GET_WORD(bufPos, timestampOffset) >> 24) != 0xbf)
        {
            corrupted = true;
            sprintf(errorMessage, "spatial stats incomplete at word 10");
        }

        if (corrupted)
            break;

        /* Get fields. */
        packet->typ = LmPacketTypeSpatialStats;
        packet->p.spatialStats.sampleCount = (LMBUF_RAW_GET_WORD(bufPos, 1) << 24) | (word & 0x00ffffff);
        packet->p.spatialStats.erasedSampleCount = LMBUF_RAW_GET_WORD(bufPos, 2) & 0x00ffffff;
        packet->p.spatialStats.saturatedSampleCount = LMBUF_RAW_GET_WORD(bufPos, 3) & 0x00ffffff;
        packet->p.spatialStats.estimatedIncomingPulseCount = LMBUF_RAW_GET_WORD(bufPos, 4) & 0x00ffffff;
        packet->p.spatialStats.rawIncomingPulseCount = LMBUF_RAW_GET_WORD(bufPos, 5) & 0x00ffffff;
        for (i = 0; i < 4; i++)
        {
            packet->p.spatialStats.counter[i] = LMBUF_RAW_GET_WORD(bufPos, i+6) & 0x00ffffff;
        }

        packet->p.spatialStats.vetoSampleCount = vetoValue;
        packet->p.spatialStats.timestamp = LMBUF_RAW_GET_WORD(bufPos, timestampOffset) & 0x00ffffff;

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
            if ( (LMBUF_RAW_GET_WORD(bufPos, i) >> 24) != (0xc0 | i) )
            {
                corrupted = true;
                sprintf(errorMessage, "spatial position incomplete at word %d", i);
            }
        }

        if ( !corrupted && (LMBUF_RAW_GET_WORD(bufPos, 6) >> 24) != 0xcf)
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
            uint32_t val = LMBUF_RAW_GET_WORD(bufPos, i);
            packet->p.spatialPosition.axis[i] = (int32_t)LMBUF_SIGN_EXTEND_24_TO_32(val);
        }
        packet->p.spatialPosition.timestamp = LMBUF_RAW_GET_WORD(bufPos, 6) & 0x00ffffff;

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
        if (bufBytes < packetBytes + sizeof(uint32_t) * LMBUF_READ_AHEAD)
            return false;

        /* Check integrity. */
        if ( (word >> 24) != 0xe0)
            corrupted = true;

        for (i = 2; i < 10 && !corrupted; i++)
        {
            if ( (LMBUF_RAW_GET_WORD(bufPos, i-1) >> 24) != (0xe0 | i) )
            {
                corrupted = true;
                sprintf(errorMessage, "periodic stats incomplete at word %d", i-2);
            }
        }

        /* Optional veto sample count. */
        checkOptionalItems(bufPos, 9, 0xea, 0xef, &vetoValue, &timestampOffset, &corrupted, &packetBytes, bufBytes);

        /* Check for timestamp. */
        if ( !corrupted && (LMBUF_RAW_GET_WORD(bufPos, timestampOffset) >> 24) != 0xef)
        {
            corrupted = true;
            sprintf(errorMessage, "periodic stats incomplete at word 9");
        }

        if (corrupted)
            break;

        /* Get fields. */
        packet->typ = LmPacketTypePeriodicStats;
        packet->p.periodicStats.sampleCount = word & 0x00ffffff;
        packet->p.periodicStats.erasedSampleCount = LMBUF_RAW_GET_WORD(bufPos, 1) & 0x00ffffff;
        packet->p.periodicStats.saturatedSampleCount = LMBUF_RAW_GET_WORD(bufPos, 2) & 0x00ffffff;
        packet->p.periodicStats.estimatedIncomingPulseCount = LMBUF_RAW_GET_WORD(bufPos, 3) & 0x00ffffff;
        packet->p.periodicStats.rawIncomingPulseCount = LMBUF_RAW_GET_WORD(bufPos, 4) & 0x00ffffff;
        for (i = 0; i < 4; i++)
        {
            packet->p.periodicStats.counter[i] = LMBUF_RAW_GET_WORD(bufPos, i+5) & 0x00ffffff;
        }

        packet->p.periodicStats.vetoSampleCount = vetoValue;
        packet->p.periodicStats.timestamp = LMBUF_RAW_GET_WORD(bufPos, timestampOffset) & 0x00ffffff;

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
        errPos += sprintf(packet->p.error.message, "%s at offset %zu:", errorMessage, lm->srcTailPos);

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
#endif /* !LMBUF_NEW_DECODER */


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

    case LmPacketTypeStreamAlign:
        bytesWritten = snprintf(textBuf, (size_t)textBufLen, "align %d", packet->p.streamAlign.pattern);
        packetLen = sizeof(uint32_t);
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
            bytesWritten = snprintf(textBuf, (size_t)textBufLen, "pulseShort %d %d",
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
               "gatedStats %d %d %d %d %d %d %d %d %d %d %d",
               packet->p.gatedStats.sampleCount,
               packet->p.gatedStats.erasedSampleCount,
               packet->p.gatedStats.saturatedSampleCount,
               packet->p.gatedStats.estimatedIncomingPulseCount,
               packet->p.gatedStats.rawIncomingPulseCount,
               packet->p.gatedStats.counter[0],
               packet->p.gatedStats.counter[1],
               packet->p.gatedStats.counter[2],
               packet->p.gatedStats.counter[3],
               packet->p.gatedStats.vetoSampleCount,
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
               "spatialStats %d %d %d %d %d %d %d %d %d %d %d",
               packet->p.spatialStats.sampleCount,
               packet->p.spatialStats.erasedSampleCount,
               packet->p.spatialStats.saturatedSampleCount,
               packet->p.spatialStats.estimatedIncomingPulseCount,
               packet->p.spatialStats.rawIncomingPulseCount,
               packet->p.spatialStats.counter[0],
               packet->p.spatialStats.counter[1],
               packet->p.spatialStats.counter[2],
               packet->p.spatialStats.counter[3],
               packet->p.spatialStats.vetoSampleCount,
               packet->p.spatialStats.timestamp);
        packetLen = sizeof(uint32_t) * 9;
        break;

    case LmPacketTypePeriodicStats:
        bytesWritten = snprintf(textBuf, (size_t)textBufLen,
               "periodicStats %d %d %d %d %d %d %d %d %d %d %d",
               packet->p.periodicStats.sampleCount,
               packet->p.periodicStats.erasedSampleCount,
               packet->p.periodicStats.saturatedSampleCount,
               packet->p.periodicStats.estimatedIncomingPulseCount,
               packet->p.periodicStats.rawIncomingPulseCount,
               packet->p.periodicStats.counter[0],
               packet->p.periodicStats.counter[1],
               packet->p.periodicStats.counter[2],
               packet->p.periodicStats.counter[3],
               packet->p.periodicStats.vetoSampleCount,
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
        snprintf(bpos, (size_t)bufLen, "   %08zu: ", addr);
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
