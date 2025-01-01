#include "base64.h"

void base64_encode(const unsigned char *input, int length, char *output)
{
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bio = BIO_new(BIO_s_mem());
    BIO_push(b64, bio);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, input, length);
    BIO_flush(b64);

    BUF_MEM *buffer_ptr;
    BIO_get_mem_ptr(b64, &buffer_ptr);
    memcpy(output, buffer_ptr->data, buffer_ptr->length);
    output[buffer_ptr->length] = '\0';

    BIO_free_all(b64);
}