#include <stdlib.h>

/**
 * base64_encode - Base64 encode
 * @src: Data to be encoded
 * @len: Length of the data to be encoded
 * @out_len: Pointer to output length variable, or %NULL if not used
 * Returns: Allocated buffer of out_len bytes of encoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer. Returned buffer is
 * nul terminated to make it easier to use as a C string. The nul terminator is
 * not included in out_len.
 */
char * base64_encode(const unsigned char *src, size_t len, size_t *out_len);

/**
 * base64_decode - Base64 decode
 * @src: Data to be decoded
 * @len: Length of the data to be decoded
 * @out_len: Pointer to output length variable
 * Returns: Allocated buffer of out_len bytes of decoded data,
 * or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer.
 */
unsigned char * base64_decode(const unsigned char *src, size_t len, size_t *out_len);

/**
 * base58_decode - Base58 decode
 * @input: Data to be decoded
 * @output: Allocated buffer of out_len bytes to store decoded data
 * @out_len: output length variable
 * Returns: decoded data length
 *
 */
int base58_decode(const char* input, unsigned char *output, size_t outsize);
