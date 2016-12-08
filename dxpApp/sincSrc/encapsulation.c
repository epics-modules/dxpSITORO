/********************************************************************
 ***                                                              ***
 ***              libsinc protocol implementation                 ***
 ***                                                              ***
 ********************************************************************/

/*
 * This module encodes and decodes data communicated with the Hydra
 * card.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#if defined (_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "sinc.h"
#include "sinc_internal.h"


/* SINC protocol constants */
#define SINC_COMMAND_TYPE_PROTOBUF           3
#define SINC_MAX_PACKET_SIZE                 (256 * 1024 * 1024)


/*
 * NAME:        SincMemMem
 * ACTION:      Searches for a binary sequence in a binary buffer. Works like
 *              memmem() but is more widely compatible.
 * PARAMETERS:  const void *l, size_t l_len - the buffer to search in.
 *              const void *s, size_t s_len - the sequence to search for.
 * RETURNS:     void * - the memory location of the match or NULL if not found.
 */

static void *SincMemMem(const void *l, size_t l_len, const void *s, size_t s_len)
{
    register char *cur, *last;
    const char *cl = (const char *)l;
    const char *cs = (const char *)s;

    /* we need something to compare */
    if (l_len == 0 || s_len == 0)
        return NULL;

    /* "s" must be smaller or equal to "l" */
    if (l_len < s_len)
        return NULL;

    /* special case where s_len == 1 */
    if (s_len == 1)
        return memchr(l, (int)*cs, l_len);

    /* the last position where its possible to find "s" in "l" */
    last = (char *)cl + l_len - s_len;

    for (cur = (char *)cl; cur <= last; cur++)
    {
        if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
            return cur;
    }

    return NULL;
}


/*
 * NAME:        SincProtocolEncodeHeader
 * ACTION:      Writes the header which precedes each protobuf-encoded packet.
 * PARAMETERS:  uint8_t *buf                      - the buffer to write to. 10 bytes must be available.
 *              int payloadLen                    - the length of the protobuf-encoded packet to follow.
 *              SiToro__Sinc__MessageType msgType - the message type.
 * RETURNS:     int - number of bytes written, always 10.
 */

int SincProtocolEncodeHeader(uint8_t *buf, int payloadLen, SiToro__Sinc__MessageType msgType)
{
    return SincProtocolEncodeHeaderGeneric(buf, payloadLen, msgType, SINC_COMMAND_MARKER);
}


/*
 * NAME:        SincProtocolEncodeHeaderGeneric
 * ACTION:      Writes the header which precedes each protobuf-encoded packet.
 * PARAMETERS:  uint8_t *buf                      - the buffer to write to. 10 bytes must be available.
 *              int payloadLen                    - the length of the protobuf-encoded packet to follow.
 *              SiToro__Sinc__MessageType msgType - the message type.
 *              uint32_t marker                   - which packet marker we're looking for. Either SINC_RESPONSE_MARKER
 *                                                  or SINC_COMMAND_MARKER. Usually SINC_RESPONSE_MARKER to receive
 *                                                  responses from a server.
 * RETURNS:     int - number of bytes written, always 10.
 */

int SincProtocolEncodeHeaderGeneric(uint8_t *buf, int payloadLen, SiToro__Sinc__MessageType msgType, uint32_t marker)
{
    SINC_PROTOCOL_WRITE_UINT32(&buf[0], marker);
    SINC_PROTOCOL_WRITE_UINT32(&buf[4], payloadLen+2);
    buf[8] = SINC_COMMAND_TYPE_PROTOBUF;
    buf[9] = (uint8_t)msgType;

    return SINC_HEADER_LENGTH; /* Packet headers are ten bytes long. */
}


/*
 * NAME:        SincDecodePacketEncapsulation
 * ACTION:      Examines a provided buffer for an available packet and decodes one
 *              if it's available and complete. Interprets the SINC protocol encapsulation
 *              in the process.
 * PARAMETERS:  const SincBuffer *fromBuf - the buffer to read from.
 *              int *bytesConsumed        - the number of bytes consumed, 0 if no usable data was found. Note that
 *                  data may be consumed even if no packet was found since corrupt data may
 *                  be skipped.
 *              int *responseCode         - this is SINC_RESPONSE_CODE_PROTOBUF for most messages, or one of
 *                  SINC_RESPONSE_CODE_OSCILLOSCOPE_DATA, SINC_RESPONSE_CODE_HISTOGRAM_DATA or
 *                  SINC_RESPONSE_CODE_LIST_MODE_DATA.
 *              SiToro__Sinc__MessageType *msgType - the protobuf message type is placed here.
 *              SincBuffer *msg           - where to put the de-encapsulated result message.
 *                   If NULL it won't make a buffer or consume data (this is useful for peeking ahead).
 *              uint32_t marker           - which packet marker we're looking for. Either SINC_RESPONSE_MARKER
 *                                          or SINC_COMMAND_MARKER. Usually SINC_RESPONSE_MARKER to receive
 *                                          responses from a server.
 * RETURNS:     bool                      - true if a packet was received, false otherwise.
 */

bool SincDecodePacketEncapsulation(const SincBuffer *fromBuf, int *bytesConsumed, int *responseCode, SiToro__Sinc__MessageType *msgType, SincBuffer *msg, uint32_t marker)
{
    uint8_t      commandMarker[4];
    size_t       bytesSkipped;
    unsigned int sincShortHeaderLength = SINC_HEADER_LENGTH - 2;
    size_t       contentLen;
    uint8_t     *buf = fromBuf->data;
    unsigned int bufLen = (unsigned int)fromBuf->len;
    uint32_t     val_u32;

    // By default assume we won't find a packet. We can change our mind later.
    *msgType = 0;
    *bytesConsumed = 0;

    // Empty the buffer.
    if (msg != NULL)
        msg->len = 0;

    // Create the magic response marker pattern.
    SINC_PROTOCOL_WRITE_UINT32(commandMarker, marker);

    // Keep doing this until we find a packet or run out of data.
    while (bufLen >= sincShortHeaderLength)
    {
        // Scan for the magic response marker pattern in the buffer.
        uint8_t *packetStart = SincMemMem(buf, bufLen, &commandMarker, sizeof(commandMarker));
        if (packetStart == NULL)
        {
            if (bufLen > 4)
            {
                // Throw away all but the last few bytes so we can use it to look for a command marker.
                *bytesConsumed = (int)bufLen - 4;
                //printf("dropped entire buffer %d bytes\n", bufLen - 4);
            }

            return false;
        }

        // Do we have a complete header available?
        bytesSkipped = (size_t)(packetStart - buf);
        if (bufLen - bytesSkipped < sincShortHeaderLength)
            return false;

        if (bytesSkipped > 0)
        {
            //printf("dropped partial buffer %ld bytes\n", bytesSkipped);
        }

        // Got a real packet header - skip up to the command marker.
        *bytesConsumed += bytesSkipped;
        buf += bytesSkipped;
        bufLen -= bytesSkipped;

        // Read the payload length.
        uint32_t payloadLen = SINC_PROTOCOL_READ_UINT32(&buf[4]);
        size_t payloadBufAvailable = bufLen - sincShortHeaderLength;

        // Does the payload length look valid?
        if (payloadLen <= SINC_MAX_PACKET_SIZE && payloadLen > 0)
        {
            // Looks ok but do we have a complete packet available?
            if (payloadLen > payloadBufAvailable)
            {
                // Not enough data to complete the packet - give up for now.
                return false;
            }

            // Handle it in different ways depending on the response code.
            size_t packetLen = payloadLen + sincShortHeaderLength;
            *responseCode = buf[8];
            const uint8_t *payloadBuf;

            if (*responseCode == SINC_RESPONSE_CODE_PROTOBUF)
            {
                // Return a protocol buffers message.
                payloadBuf = &buf[SINC_HEADER_LENGTH];
#ifdef PROTOCOL_VERBOSE_DEBUG
                printf("decoding a packet of %d bytes\n", payloadLen);
                printf("SincDecodePacketEncapsulation() content: ");
                int i;
                for (i = 0; i < payloadLen-2; i++)
                {
                    printf("%02x ", payloadBuf[i]);
                }
                printf("\n");
#endif

                contentLen = payloadLen - 2;
                if (msg != NULL)
                {
                    ProtobufCBuffer *cBuf = &msg->base;
                    cBuf->append(cBuf, contentLen, payloadBuf);
                }

                *msgType = (SiToro__Sinc__MessageType)buf[9];
                *bytesConsumed += packetLen;

                return true;
            }

            // Remove the bytes we've used.
            buf += packetLen;
            bufLen -= packetLen;
            *bytesConsumed += packetLen;
#ifdef PROTOCOL_VERBOSE_DEBUG
            printf("skipped strange packet type %d of %ld bytes\n", *responseCode, packetLen);
#endif
        }
        else
        {
            // The packet doesn't look valid - skip past the command marker and keep scanning.
            *bytesConsumed += sizeof(commandMarker);
            buf += sizeof(commandMarker);
            bufLen -= sizeof(commandMarker);
#ifdef PROTOCOL_VERBOSE_DEBUG
            printf("dropped %ld bytes\n", sizeof(commandMarker));
#endif
        }
    }

    // Not enough data for a packet.
    return false;
}

