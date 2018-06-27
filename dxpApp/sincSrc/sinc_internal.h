/********************************************************************
 ***                                                              ***
 ***                   libsinc internal header                    ***
 ***                                                              ***
 ********************************************************************/

/*
 * This header contains internal declarations not intended for
 * general use.
 */

#ifndef SINC_INTERNAL_H
#define SINC_INTERNAL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "sinc.pb-c.h"
#include "sinc.h"

// Constants.
#define SINC_COMMAND_MARKER                     0x88E7D5C6
#define SINC_RESPONSE_MARKER                    0x87D6C4B5
#define SINC_HEADER_LENGTH                      10
#define SINC_RESPONSE_CODE_PROTOBUF             3
#define SINC_RESPONSE_CODE_DATAGRAM             4
#define SINC_RESPONSE_CODE_CALIBRATION_PROGRESS 21
#define SINC_RESPONSE_CODE_CALIBRATION_FINISHED 22
#define SINC_RESPONSE_CODE_DC_OFFSET_RESULT     30
#define SINC_RESPONSE_CODE_OSCILLOSCOPE_DATA    41
#define SINC_RESPONSE_CODE_HISTOGRAM_DATA       51
#define SINC_RESPONSE_CODE_LIST_MODE_DATA       61
#define SINC_SPECTRUMSELECT_ACCEPTED            0x01
#define SINC_SPECTRUMSELECT_REJECTED            0x02

// The read buffer starts at this size but can expand.
#define SINC_READBUF_DEFAULT_SIZE 65536
#define SINC_MAX_DATAGRAM_BYTES 65536

/* Handy network write macros. These assume a little endian architecture for speed but we can substitute big endian if necessary. */
#define SINC_PROTOCOL_WRITE_UINT32(buf, val) { uint32_t v = (uint32_t)val; memcpy((buf), &v, sizeof(v)); }
#define SINC_PROTOCOL_READ_UINT16(buf) ( memcpy(&val_u16, (buf), sizeof(val_u16)), val_u16 )
#define SINC_PROTOCOL_READ_UINT32(buf) ( memcpy(&val_u32, (buf), sizeof(val_u32)), val_u32 )
#define SINC_PROTOCOL_READ_UINT64(buf) ( memcpy(&val_u64, (buf), sizeof(val_u64)), val_u64 )
#define SINC_PROTOCOL_READ_DOUBLE(buf) ( memcpy(&val_double, (buf), sizeof(val_double)), val_double )

// Prototypes from api.c.
void SincErrorInit(SincError *err);
SiToro__Sinc__ErrorCode SincErrorCode(SincError *err);
const char *SincErrorMessage(SincError *err);
void SincErrorSetMessage(SincError *err, SiToro__Sinc__ErrorCode code, const char *msg);
void SincErrorSetCode(SincError *err, SiToro__Sinc__ErrorCode code);
void SincReadErrorSetMessage(Sinc *sc, SiToro__Sinc__ErrorCode code, const char *msg);
void SincReadErrorSetCode(Sinc *sc, SiToro__Sinc__ErrorCode code);
void SincWriteErrorSetMessage(Sinc *sc, SiToro__Sinc__ErrorCode code, const char *msg);
void SincWriteErrorSetCode(Sinc *sc, SiToro__Sinc__ErrorCode code);

// Prototypes from encapsulation.c.
int  SincProtocolEncodeHeader(uint8_t *buf, int payloadLen, SiToro__Sinc__MessageType msgType);
int  SincProtocolEncodeHeaderGeneric(uint8_t *buf, int payloadLen, SiToro__Sinc__MessageType msgType, uint32_t marker);
bool SincDecodePacketEncapsulation(const SincBuffer *fromBuf, int *bytesConsumed, int *responseCode, SiToro__Sinc__MessageType *msgType, SincBuffer *msg, uint32_t marker);
void SincProtocolWriteUint32(uint8_t *buf, uint32_t val);
uint16_t SincProtocolReadUint16(const uint8_t *buf);
uint32_t SincProtocolReadUint32(const uint8_t *buf);
uint64_t SincProtocolReadUint64(const uint8_t *buf);
double SincProtocolReadDouble(const uint8_t *buf);

// Prototypes from socket.c.
int SincSocketConnect(int *fd, const char *host, int port, int timeout);
int SincSocketDisconnect(int fd);
int SincSocketWait(int fd, int timeout, bool *readOk);
int SincSocketWaitMulti(const int *fd, int numFds, int timeout, bool *readOk);
int SincSocketRead(int fd, uint8_t *buf, int bufLen, int *bytesRead);
int SincSocketWriteNonBlocking(int fd, const uint8_t *buf, int bufLen, int *bytesWritten);
int SincSocketWrite(int fd, const uint8_t *buf, int bufLen);
int SincSocketSetNonBlocking(int fd);
int SincSocketBindDatagram(int *datagramFd, int *port);
int SincSocketReadDatagram(int fd, uint8_t *buf, size_t *buflen, bool nonBlocking);

// Prototypes from readmessage.c.
bool SincReadMessage(Sinc *sc, int timeout, SincBuffer *buf, SiToro__Sinc__MessageType *msgType);
bool SincCopyCalibrationPulse(SincError *err, SincCalibrationPlot *pulse, int len, double *x, double *y);

// Prototypes from blocking.c.
bool SincWaitForMessageType(Sinc *sc, int timeout, SincBuffer *buf, SiToro__Sinc__MessageType seekMsgType);

// Prototypes from decode.c.
bool SincGetNextPacketFromBufferGeneric(SincBuffer *readBuf, uint32_t marker, SiToro__Sinc__MessageType *packetType, SincBuffer *packetBuf, int *packetFound);

// Prototypes from base64.c.
int Base64Encode(const void* data_buf, size_t dataLength, char* result, size_t resultSize);
int Base64Decode(char *in, size_t inLen, unsigned char *out, size_t *outLen);

#ifdef __cplusplus
}
#endif

#endif /* SINC_INTERNAL_H */
