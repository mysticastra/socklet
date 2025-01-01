#include "frame.h"

void send_frame(int client_fd, const char *message)
{
    unsigned char frame[10];
    size_t message_len = strlen(message);
    int frame_len = 0;

    frame[0] = 0x81;

    if (message_len <= 125)
    {
        frame[1] = message_len;
        frame_len = 2;
    }
    else if (message_len <= 65535)
    {
        frame[1] = 126;
        frame[2] = (message_len >> 8) & 0xFF;
        frame[3] = message_len & 0xFF;
        frame_len = 4;
    }
    else
    {
        frame[1] = 127;
        frame[2] = (message_len >> 56) & 0xFF;
        frame[3] = (message_len >> 48) & 0xFF;
        frame[4] = (message_len >> 40) & 0xFF;
        frame[5] = (message_len >> 32) & 0xFF;
        frame[6] = (message_len >> 24) & 0xFF;
        frame[7] = (message_len >> 16) & 0xFF;
        frame[8] = (message_len >> 8) & 0xFF;
        frame[9] = message_len & 0xFF;
        frame_len = 10;
    }

    send(client_fd, frame, frame_len, 0);

    send(client_fd, message, message_len, 0);
}

int decode_frame(const unsigned char *input, size_t input_length, char *output, size_t *output_length)
{
    if (input_length < 2)
    {
        fprintf(stderr, "Invalid WebSocket frame: Too short.\n");
        return -1;
    }

    unsigned char fin = (input[0] >> 7) & 0x01;
    unsigned char opcode = input[0] & 0x0F;

    if (opcode == 0x8)
    {
        printf("Received close frame.\n");
        return -2;
    }

    if (opcode != 0x1 && opcode != 0x2)
    {
        fprintf(stderr, "Invalid opcode: %d\n", opcode);
        return -1;
    }

    unsigned char mask = (input[1] >> 7) & 0x01;
    uint64_t payload_length = input[1] & 0x7F;
    size_t pos = 2;

    if (payload_length == 126)
    {
        if (input_length < 4)
            return -1;
        payload_length = (input[2] << 8) | input[3];
        pos = 4;
    }
    else if (payload_length == 127)
    {
        if (input_length < 10)
            return -1;
        payload_length = 0;
        for (int i = 0; i < 8; i++)
        {
            payload_length = (payload_length << 8) | input[2 + i];
        }
        pos = 10;
    }

    if (payload_length > input_length - pos)
    {
        fprintf(stderr, "Invalid WebSocket frame: Length mismatch.\n");
        return -1;
    }

    if (!mask)
    {
        fprintf(stderr, "Invalid WebSocket frame: MASK must be set.\n");
        return -1;
    }

    unsigned char masking_key[4];
    memcpy(masking_key, input + pos, 4);
    pos += 4;

    for (uint64_t i = 0; i < payload_length; i++)
    {
        output[i] = input[pos + i] ^ masking_key[i % 4];
    }

    *output_length = payload_length;
    return 0;
}