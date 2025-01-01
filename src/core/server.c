#include "server.h"
#include "client.h"

#define RESET_COLOR "\033[0m"
#define GREEN_COLOR "\033[32m"
#define RED_COLOR "\033[31m"
#define YELLOW_COLOR "\033[33m"
#define CYAN_COLOR "\033[36m"

bool default_authetication(int client_fd)
{
    return true;
}

server_t *create_server(int port)
{
    server_t *server = (server_t *)malloc(sizeof(server_t));
    if (!server)
    {
        printf(RED_COLOR "Failed to allocate memory for server" RESET_COLOR "\n");
        exit(EXIT_FAILURE);
    }

    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_fd < 0)
    {
        printf(RED_COLOR "Socket creation failed" RESET_COLOR "\n");
        free(server);
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        printf(RED_COLOR "Failed to set socket options" RESET_COLOR "\n");
        close(server->server_fd);
        free(server);
        exit(EXIT_FAILURE);
    }

    server->address.sin_family = AF_INET;
    server->address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server->address.sin_port = htons(port);
    server->authentication_handler = default_authetication;

    if (bind(server->server_fd, (struct sockaddr *)&server->address, sizeof(server->address)) < 0)
    {
        printf(RED_COLOR "Binding failed" RESET_COLOR "\n");
        close(server->server_fd);
        free(server);
        exit(EXIT_FAILURE);
    }

    if (listen(server->server_fd, 5) < 0)
    {
        perror(RED_COLOR "Listen failed" RESET_COLOR "\n");
        close(server->server_fd);
        free(server);
        exit(EXIT_FAILURE);
    }

    server->is_running = 1;
    printf(GREEN_COLOR "Server started on port %d\n" RESET_COLOR, port);

    return server;
}

void start_server(server_t *server)
{
    int client_fd;
    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    pthread_t thread_id;

    printf(GREEN_COLOR "Waiting for connections...\n" RESET_COLOR);

    while (server->is_running)
    {
        client_fd = accept(server->server_fd, (struct sockaddr *)&client_address, &client_len);
        if (client_fd < 0)
        {
            perror(RED_COLOR "Failed to accept connection" RESET_COLOR);
            continue;
        }

        printf(CYAN_COLOR "New connection accepted: %s:%d\n" RESET_COLOR,
               inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        int *client_fd_copy = malloc(sizeof(int));
        if (client_fd_copy == NULL)
        {
            perror(RED_COLOR "Failed to allocate memory for client FD" RESET_COLOR);
            close(client_fd);
            continue;
        }
        *client_fd_copy = client_fd;

        if (server->authentication_handler(client_fd))
        {
            if (pthread_create(&thread_id, NULL, client_handler, client_fd_copy) != 0)
            {
                perror(RED_COLOR "Failed to create thread for client" RESET_COLOR);
                close(client_fd);
                free(client_fd_copy);
            }
            else
            {
                pthread_detach(thread_id);
            }
        }
        else
        {
            printf(RED_COLOR "Authentication failed\n" RESET_COLOR);
            close(client_fd);
            free(client_fd_copy);
        }
    }
}

void close_server(server_t *server)
{
    if (server)
    {
        close(server->server_fd);
        server->is_running = 0;
        printf(RED_COLOR "Server stopped.\n" RESET_COLOR);
        free(server);
    }
}