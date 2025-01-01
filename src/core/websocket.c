#include "websocket.h"

int websocket_handshake(int client_fd)
{
    char buffer[BUFFER_SIZE], client_key[256], accept_key[256];

    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0)
    {
        perror("Failed to receive handshake request");
        return -1;
    }

    buffer[bytes_received] = '\0';

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

    strncpy(client_key, key_start, key_end - key_start);
    client_key[key_end - key_start] = '\0';

    compute_websocket_accept_key(client_key, accept_key);
    printf("Sec-WebSocket-Key: %s\n", client_key);
    printf("Sec-WebSocket-Accept: %s\n", accept_key);

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