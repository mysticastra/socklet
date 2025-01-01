#include "event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

event_t *events = NULL;
size_t event_count = 0;
size_t event_capacity = 0;

void init_event_manager(size_t capacity) {
    if (capacity > 0) {
        events = malloc(sizeof(event_t) * capacity);
        if (events == NULL) {
            printf("Failed to allocate memory for events\n");
            exit(EXIT_FAILURE);
        }
        event_capacity = capacity;
    }
    event_count = 0;
}

void register_event(const char *event_name, void (*callback)(client_t *client_context, void *data)) {
    if (event_count >= event_capacity) {
        event_capacity = event_capacity == 0 ? 1 : event_capacity * 2;
        events = realloc(events, sizeof(event_t) * event_capacity);
        if (events == NULL) {
            printf("Failed to reallocate memory for events\n");
            exit(EXIT_FAILURE);
        }
    }

    events[event_count].event_name = strdup(event_name);
    events[event_count].callback = callback;
    event_count++;
}

void emit_event(const char *event_name, client_t *client_context, void *data) {
    printf("Emitting event: %s\n", event_name);
    for (size_t i = 0; i < event_count; i++) {
        if (strcmp(events[i].event_name, event_name) == 0) {
            events[i].callback(client_context, data);
            return;
        }
    }
    printf("No handler registered for event: %s\n", event_name);
}

void cleanup_event_manager(void) {
    for (size_t i = 0; i < event_count; i++) {
        free((char *)events[i].event_name);
    }
    free(events);
    events = NULL;
    event_count = 0;
    event_capacity = 0;
}
