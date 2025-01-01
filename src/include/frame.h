#ifndef FRAME_H
#define FRAME_H

#include<stdio.h>
#include <stdint.h>

void send_frame(int client_fd, const char *message);
int decode_frame(const unsigned char *input, size_t input_length, char *output, size_t *output_length);

#endif