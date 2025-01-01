#ifndef BASE64_H
#define BASE64_H

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

void base64_encode(const unsigned char *input, int length, char *output);
void base64_decode(const unsigned char *input, int length, char *output);

#endif