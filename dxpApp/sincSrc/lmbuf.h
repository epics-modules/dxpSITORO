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
} LmBuf;


typedef enum
{
    LmPacketTypeError,
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
    uint32_t timestamp;
} LmTimestamp;


typedef struct
{
    bool     invalid;
    int32_t  amplitude;
    bool     hasTimeOfArrival;      /* If true, this packet has valid timeOfArrival and subSampleTimeOfArrival data. */
    uint32_t timeOfArrival;
    uint32_t subSampleTimeOfArrival;
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
bool LmBufInit(LmBuf *lm);
void LmBufClose(LmBuf *lm);
void LmBufClear(LmBuf *lm);
bool LmBufAddData(LmBuf *lm, uint8_t *data, size_t len);
bool LmBufGetJsonHeader(LmBuf *lm, char **data, size_t *len, bool *invalidHeader);
bool LmBufGetNextPacket(LmBuf *lm, LmPacket *packet);
int  LmBufTranslatePacketSimple(char *textBuf, int textBufLen, LmPacket *packet, int *packetLenPtr);
int  LmBufTranslatePacketComplex(char *textBuf, int textBufLen, LmPacket *packet, bool hexDump, LmBuf *lmBuf, bool showOnlyErrors, bool showTimestamps, uint32_t *lastTimestamp);

#ifdef __cplusplus
}
#endif

#endif /* SINC_LMBUF */
