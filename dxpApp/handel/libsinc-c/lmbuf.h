/********************************************************************
 ***                                                              ***
 ***                   libsinc list mode buffer                   ***
 ***                                                              ***
 ********************************************************************/

/*
 * The list mode buffer allows list mode data to be read from any
 * source and list mode packets to be extracted.
 */

#ifndef SINC_LMBUF_H
#define SINC_LMBUF_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint8_t *buf;          /* The list mode data buffer. */
    size_t   bufSize;      /* The size of the list mode buffer. */
    size_t   bufHead;      /* Placed just beyond the highest used value in the buffer. */
    size_t   bufTail;      /* The lowest used value in the buffer. */
    size_t   srcTailPos;   /* Where we're up reading in the entire source data, not just the buffer. */
    bool     scanStreamAlign;   /* Currently scanning for a "resync" flag of 0x70717273. */
} LmBuf;


typedef enum
{
    LmPacketTypeError,
    LmPacketTypeStreamAlign,
    LmPacketTypeSync,
    LmPacketTypePulse,
    LmPacketTypeGateState,
    LmPacketTypeGatedStats,
    LmPacketTypeSpatialPosition,
    LmPacketTypeSpatialStats,
    LmPacketTypePeriodicStats,
    LmPacketTypeAnalogStatus,
    LmPacketTypeInternalBufferOverflow
} LmPacketType;


typedef struct
{
    char    message[256];
} LmError;


typedef struct
{
    uint32_t pattern;
} LmStreamAlignPattern;


typedef struct
{
    uint32_t timestamp;
} LmTimestamp;


typedef struct
{
    bool     invalid;
    int32_t  amplitude;
    bool     hasTimeOfArrival;      /* If true, this packet has valid timeOfArrival and subSampleTimeOfArrival data. */
    uint32_t timeOfArrival;
    uint32_t subSampleTimeOfArrival;
    bool     inMarkedRange;
} LmPulse;


typedef struct
{
    bool     gate;
    uint32_t timestamp;
} LmGateState;


typedef struct
{
    uint32_t sampleCount;
    uint32_t erasedSampleCount;
    uint32_t saturatedSampleCount;
    uint32_t estimatedIncomingPulseCount;
    uint32_t rawIncomingPulseCount;
    uint32_t counter[4];
    uint32_t vetoSampleCount;
    uint32_t timestamp;
} LmStats;


typedef struct
{
    int32_t  axis[6];
    uint32_t timestamp;
} LmSpatialPosition;


typedef struct
{
    bool positiveSaturation;
    bool negativeSaturation;
} LmAnalogStatus;


typedef struct
{
    LmPacketType typ;
    union LmPacket_
    {
        LmError           error;
        LmStreamAlignPattern    streamAlign;
        LmTimestamp       sync;
        LmPulse           pulse;
        LmGateState       gateState;
        LmStats           gatedStats;
        LmSpatialPosition spatialPosition;
        LmStats           spatialStats;
        LmStats           periodicStats;
        LmAnalogStatus    analogStatus;
        LmTimestamp       internalBufferOverflow;
    } p;
} LmPacket;


/* Prototypes. */

/*
 * NAME:        LmBufInit
 * ACTION:      Initialises a list mode buffer.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 * RETURNS:     bool - true on success, false if out of memory.
 */

bool LmBufInit(LmBuf *lm);


/*
 * NAME:        LmBufClose
 * ACTION:      Closes an LmBuf, freeing memory.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 */

void LmBufClose(LmBuf *lm);


/*
 * NAME:        LmBufClear
 * ACTION:      Clears an LmBuf, emptying the contents but keeping the buffer memory ready.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 */

void LmBufClear(LmBuf *lm);


/*
 * NAME:        LmBufAddData
 * ACTION:      Adds some binary data to the head of the buffer. Use
 *              this to add data read from a file or stream, then use
 *              LmBufGetNextPacket() to get packets out of the buffer.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 * RETURNS:     bool - true on success, false if out of memory.
 */

bool LmBufAddData(LmBuf *lm, uint8_t *data, size_t len);


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

bool LmBufGetJsonHeader(LmBuf *lm, char **data, size_t *len, bool *invalidHeader);


/*
 * NAME:        LmBufGetNextPacket
 * ACTION:      Gets the next available packet from the buffer. If no
 *              packet is available it returns false.
 * PARAMETERS:  LmBuf *lm - the list mode buffer.
 *              LmPacket *packet - the parsed packet is written here.
 * RETURNS:     bool - true on success, false if no packet is available.
 */

bool LmBufGetNextPacket(LmBuf *lm, LmPacket *packet);


/*
 * NAME:        LmBufTranslatePacketSimple
 * ACTION:      Translates a LmPacket into textual form.
 * PARAMETERS:  char *textBuf     - where to place the resulting text.
 *              int textBufLen    - how large the text buffer is.
 *              LmPacket *packet  - the parsed packet is written here.
 *              int *packetLenPtr - if not NULL the binary length of the packet will be placed here.
 * RETURNS:     int - the number of bytes placed in the text buffer.
 */

int  LmBufTranslatePacketSimple(char *textBuf, int textBufLen, LmPacket *packet, int *packetLenPtr);


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

int  LmBufTranslatePacketComplex(char *textBuf, int textBufLen, LmPacket *packet, bool hexDump, LmBuf *lmBuf, bool showOnlyErrors, bool showTimestamps, uint32_t *lastTimestamp);

#ifdef __cplusplus
}
#endif

#endif /* SINC_LMBUF */
