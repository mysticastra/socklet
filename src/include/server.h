#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024

typedef struct
{
    int server_fd;
    struct sockaddr_in address;
    int is_running;
    bool (*authentication_handler)(int);
} server_t;

server_t *create_server(int port);
void start_server(server_t *server);
void close_server(server_t *server);

#endif
