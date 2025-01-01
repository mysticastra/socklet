#include "server.h"
#include "event.h"

#define SERVER_PORT 8081

void send_message(client_t *client_context, void *data)
{
    echo(client_context, data);
}

int main()
{
    init_event_manager(2);

    server_t *server = create_server(SERVER_PORT);

    register_event("message", send_message);

    start_server(server);

    cleanup_event_manager();

    close_server(server);
    return 0;
}
