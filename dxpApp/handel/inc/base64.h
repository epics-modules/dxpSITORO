/*
 * Base64 codec borrowed from libsinc-c.
 */

extern int Base64Encode(const void* data_buf, size_t dataLength, char* result,
                        size_t resultSize);
extern int Base64Decode(char *in, size_t inLen, unsigned char *out, size_t *outLen);
