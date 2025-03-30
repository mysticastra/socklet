#ifndef SOCKLET_H_
#define SOCKLET_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include "jsoncraftor.h"

#define BUFFER_SIZE 1024

typedef struct
{
    int client_fd;
    struct sockaddr_in client_address;
    char *extra_info;
} client_t;

typedef struct
{
    int server_fd;
    struct sockaddr_in address;
    void (*callback)(int, char *, client_t *);
    bool (*authentication_handler)(int, char *);
} server_t;

typedef struct client_data
{
    int client_fd;
    server_t *server;
} client_data_t;

typedef struct
{
    const char *event_name;
    void (*callback)(client_t *client, void *data);
} event_t;

void server_init(server_t *server, void (*callback)(int, char *, client_t *), bool (*authentication_handler)(int, char *));
void server_listen(server_t *server, int port);
void server_close(server_t *server);
void *client_handler(void *arg);
int websocket_handshake(int client_fd, char *headers_string);
void compute_websocket_accept_key(const char *client_key, char *accept_key);
void base64_encode(const unsigned char *input, int length, char *output);
void add_client(client_t *client);
void remove_client(int client_fd);
void send_frame(int client_fd, const char *message);
int decode_frame(const unsigned char *input, size_t input_length, char *output, size_t *output_length);
void register_event(const char *event_name, void (*callback)(client_t *client, void *data));
void handle_event(client_t *client, void *data);
void emit_event(const char *event_name, client_t *client, void *data);

#ifdef SOCKLET_IMPLEMENTATION

client_t **clients = NULL;
event_t **events = NULL;
int client_count = 0;
int events_count = 0;
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

void server_init(server_t *server, void (*callback)(int, char *, client_t *), bool (*authentication_handler)(int, char *))
{
    server->server_fd = 0;
    server->callback = callback;
    server->authentication_handler = authentication_handler;
}

void server_listen(server_t *server, int port)
{
    int opt = 1;
    int client_fd;
    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    pthread_t thread_id;

    if ((server->server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    server->address.sin_family = AF_INET;
    server->address.sin_addr.s_addr = INADDR_ANY;
    server->address.sin_port = htons(port);

    if (bind(server->server_fd, (struct sockaddr *)&server->address, sizeof(server->address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server->server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", port);

    while (1)
    {
        if ((client_fd = accept(server->server_fd, (struct sockaddr *)&client_address, &client_len)) < 0)
        {
            perror("accept");
            continue;
        }

        printf("New connection from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        client_data_t *client_data = malloc(sizeof(client_data_t));
        if (client_data == NULL)
        {
            perror("Failed to allocate memory for client data");
            close(client_fd);
            continue;
        }
        client_data->client_fd = client_fd;
        client_data->server = server;

        if (pthread_create(&thread_id, NULL, client_handler, client_data) != 0)
        {
            perror("Failed to create thread for client");
            close(client_fd);
            free(client_data);
        }
        else
        {
            pthread_detach(thread_id);
        }
    }
}

void server_close(server_t *server)
{
    close(server->server_fd);
}

void *client_handler(void *arg)
{
    char headers[BUFFER_SIZE];

    client_data_t *client_data = (client_data_t *)arg;

    int client_fd = client_data->client_fd;
    server_t *server = client_data->server;

    free(client_data);

    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    getpeername(client_fd, (struct sockaddr *)&client_address, &client_len);

    printf("Handling client %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

    char buffer[BUFFER_SIZE] = {0};

    if (websocket_handshake(client_fd, headers) != 0)
    {
        close(client_fd);
        return NULL;
    }

    if (server->authentication_handler(client_fd, headers))
    {
        printf("Authentication successful for %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
    }
    else
    {
        printf("Authentication failed for %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
        close(client_fd);
        return NULL;
    }

    client_t *client = malloc(sizeof(client_t));
    client->client_fd = client_fd;
    client->client_address = client_address;
    client->extra_info = NULL;

    add_client(client);

    server->callback(client_fd, headers, client);

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);

        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
            {
                printf("Client disconnected: %s:%d\n",
                       inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
            }
            else
            {
                perror("recv failed");
            }
            remove_client(client_fd);
            break;
        }
        else
        {
            char decoded[BUFFER_SIZE];
            size_t decoded_length = 0;

            if (decode_frame((unsigned char *)buffer, bytes_received, decoded, &decoded_length) == 0)
            {
                decoded[decoded_length] = '\0';
                handle_event(client, decoded);
            }
            else
            {
                printf("Failed to decode WebSocket frame.\n");
            }
        }
    }

    remove_client(client_fd);
    return NULL;
}

void add_client(client_t *client)
{
    pthread_mutex_lock(&client_lock);
    client_t **temp = realloc(clients, sizeof(client_t *) * (client_count + 1));
    if (!temp)
    {
        perror("Failed to allocate memory for clients");
        pthread_mutex_unlock(&client_lock);
        return;
    }
    clients = temp;
    clients[client_count++] = client;
    printf("Client added. Total clients: %d\n", client_count);
    pthread_mutex_unlock(&client_lock);
}

void remove_client(int client_fd)
{
    pthread_mutex_lock(&client_lock);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i]->client_fd == client_fd)
        {
            printf("Removing client %d\n", client_fd);
            close(clients[i]->client_fd);
            free(clients[i]);
            for (int j = i; j < client_count - 1; j++)
            {
                clients[j] = clients[j + 1];
            }
            client_count--;
            clients = realloc(clients, sizeof(client_t *) * client_count);
            break;
        }
    }
    printf("Total clients after removal: %d\n", client_count);
    pthread_mutex_unlock(&client_lock);
}

void register_event(const char *event_name, void (*callback)(client_t *client, void *data))
{
    event_t **temp = realloc(events, sizeof(event_t *) * (events_count + 1));
    if (!temp)
    {
        perror("Failed to allocate memory for events");
        return;
    }
    events = temp;

    event_t *event = malloc(sizeof(event_t));
    event->event_name = event_name;
    event->callback = callback;

    events[events_count++] = event;
}

void handle_event(client_t *client, void *data)
{
    char type[BUFFER_SIZE];
    char event[BUFFER_SIZE];
    char client_data[BUFFER_SIZE] = {0};
    char *error = NULL;

    JsonMap mappings[] = {
        {"type", &type, 's', 20, true, NULL},
        {"event", &event, 's', 20, true, NULL},
        {"data", &client_data, 's', 500, false, NULL}
    };

    if(parse_json(data, mappings, 3, &error))
    {
        if(strcmp(type, "socklet:dispatch") == 0)
        {
            emit_event(event, client, client_data);
        }
        else
        {
            printf("Invalid message type: %s\n", type);
        }
    }
    else
    {
        printf("Failed to parse JSON: %s\n", error);
    }
}

void emit_event(const char *event_name, client_t *client, void *data)
{
    for (int i = 0; i < events_count; i++)
    {
        if (strcmp(events[i]->event_name, event_name) == 0)
        {
            events[i]->callback(client, data);
            break;
        }
    }
}

int websocket_handshake(int client_fd, char *headers_string)
{
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0)
    {
        perror("Failed to receive handshake request");
        return -1;
    }

    buffer[bytes_received] = '\0';

    char *headers_start = buffer;
    char *headers_end = strstr(headers_start, "\r\n\r\n");

    if (!headers_end)
    {
        printf("Failed to find the end of headers.\n");
        return -1;
    }

    size_t headers_length = headers_end - headers_start;

    strncpy(headers_string, headers_start, headers_length);
    headers_string[headers_length] = '\0';

    char *key_start = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_start)
    {
        printf("Missing Sec-WebSocket-Key in request\n");
        return -1;
    }

    key_start += strlen("Sec-WebSocket-Key: ");

    char *key_end = strstr(key_start, "\r\n");
    if (!key_end)
    {
        printf("Invalid Sec-WebSocket-Key format\n");
        return -1;
    }

    char client_key[256];
    strncpy(client_key, key_start, key_end - key_start);
    client_key[key_end - key_start] = '\0';

    char accept_key[256];
    compute_websocket_accept_key(client_key, accept_key);

    snprintf(buffer, sizeof(buffer),
             "HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n\r\n",
             accept_key);

    if (send(client_fd, buffer, strlen(buffer), 0) < 0)
    {
        perror("Failed to send handshake response");
        return -1;
    }

    return 0;
}

void compute_websocket_accept_key(const char *client_key, char *accept_key)
{
    const char *GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[256];
    unsigned char sha1_hash[SHA_DIGEST_LENGTH];

    snprintf(combined, sizeof(combined), "%s%s", client_key, GUID);
    SHA1((unsigned char *)combined, strlen(combined), sha1_hash);
    base64_encode(sha1_hash, SHA_DIGEST_LENGTH, accept_key);
}

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

#endif

#endif
