#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdio.h>
#include <openssl/sha.h>

#define BUFFER_SIZE 1024

int websocket_handshake(int client_fd);
void compute_websocket_accept_key(const char *client_key, char *accept_key);

#endif