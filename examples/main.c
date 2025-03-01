#define SOCKLET_IMPLEMENTATION

#include "../socklet.h"

void callback(int client_fd, char *buffer)
{
    send_frame(client_fd, "Hello from server");
}

bool authentication_handler(int client_fd, char *headers)
{
    char *auth_header_start = strstr(headers, "Authorization: ");
    if (!auth_header_start)
    {
        send_frame(client_fd, "HTTP/1.1 401 Unauthorized\r\n\r\n");
        printf("Missing Authorization header in request\n");
        return false;
    }

    auth_header_start += strlen("Authorization: ");
    char *auth_header_end = strstr(auth_header_start, "\r\n");
    
    if (!auth_header_end)
    {
        send_frame(client_fd, "HTTP/1.1 401 Unauthorized\r\n\r\n");
        printf("Invalid Authorization header format\n");
        return false;
    }

    char auth_header[256] = {0};
    strncpy(auth_header, auth_header_start, auth_header_end - auth_header_start);
    auth_header[auth_header_end - auth_header_start] = '\0';
    
    if(strcmp(auth_header, "Bearer 123456") != 0)
    {
        send_frame(client_fd, "HTTP/1.1 401 Unauthorized\r\n\r\n");
        return false;
    }

    return true;
}

int main()
{
    server_t server;
    server_init(&server, callback, authentication_handler);
    server_listen(&server, 8081);
    server_close(&server);
    return 0;
}