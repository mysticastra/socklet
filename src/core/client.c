#include "client.h"
#include "websocket.h"
#include "base64.h"
#include "frame.h"
#include "event.h"

client_t **clients = NULL;
int client_count = 0;
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

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

void *client_handler(void *arg)
{
    int client_fd = *((int *)arg);
    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    char buffer[BUFFER_SIZE];

    getpeername(client_fd, (struct sockaddr *)&client_address, &client_len);
    printf("Client connected: %s:%d\n",
           inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

    if (websocket_handshake(client_fd) != 0)
    {
        close(client_fd);
        free(arg);
        return NULL;
    }

    client_t *client = malloc(sizeof(client_t));
    client->client_fd = client_fd;
    client->client_address = client_address;
    add_client(client);

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

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
            break;
        }

        if (bytes_received > 0)
        {
            char decoded[BUFFER_SIZE];
            size_t decoded_length = 0;

            if (decode_frame((unsigned char *)buffer, bytes_received, decoded, &decoded_length) == 0)
            {
                decoded[decoded_length] = '\0';
                printf("Decoded data: %s\n", decoded);
                char event_name[50];
                char data[BUFFER_SIZE];

                if (sscanf(decoded, "42[\"%49[^\"]\",\"%[^\"]\"]", event_name, data) == 2)
                {
                    printf("Event: %s, Data: %s\n", event_name, data);
                    emit_event(event_name, client, data);
                }
            }
            else
            {
                printf("Failed to decode WebSocket frame.\n");
            }
        }
    }

    remove_client(client_fd);
    free(arg);
    close(client_fd);
    return NULL;
}

void echo(client_t *client, char *data)
{
    if (data == NULL)
    {
        printf("Error: Data is NULL.\n");
        return;
    }
    char message[BUFFER_SIZE];
    strncpy(message, data, sizeof(message) - 1);
    message[sizeof(message) - 1] = '\0';
    send_frame(client->client_fd, message);
}