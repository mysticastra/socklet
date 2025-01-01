#include "server.h"

#define SERVER_PORT 8081

int main() {
    server_t* server = create_server(SERVER_PORT);
    start_server(server);

    close_server(server);
    return 0;
}
