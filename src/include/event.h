#ifndef EVENT_H
#define EVENT_H

#include "client.h"

typedef struct {
    const char *event_name;
    void (*callback)(client_t *client_context, void *data);
} event_t;

extern event_t *events;
extern size_t event_count;
extern size_t event_capacity;

void init_event_manager(size_t capacity);
void register_event(const char *event_name, void (*callback)(client_t *client_context, void *data));
void emit_event(const char *event_name, client_t *client_context, void *data);
void cleanup_event_manager(void);

#endif
