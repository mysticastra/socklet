#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define RESET_COLOR "\033[0m"
#define GREEN_COLOR "\033[32m"
#define RED_COLOR "\033[31m"
#define YELLOW_COLOR "\033[33m"
#define CYAN_COLOR "\033[36m"

typedef struct {
    int client_fd;
    struct sockaddr_in client_address;
    char extra_info[BUFFER_SIZE];
} client_t;

void *client_handler(void *arg);
void add_client(client_t *client);
void add_extra_info_to_client(client_t *client, char *extra_info);
void remove_client(int client_fd);
void echo(client_t *client, char *data);

#endif
