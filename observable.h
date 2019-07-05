#ifndef OB_OBSERVABLE_H
#define OB_OBSERVABLE_H

#include <stdbool.h>

enum ob_event_type {
    OB_DATA,
    OB_ERROR,
    OB_COMPLETE,
};

struct ob_event {
    enum ob_event_type type;
    union {
        const void *data;
        const void *error;
    };
};

typedef struct ob_event ob_event_t;
typedef struct observable observable_t;
typedef struct subscription subscription_t;

struct subscription;
struct observable;

typedef void (*subscribe_func_t)(const ob_event_t *event, void *metadata);
typedef void (*destroy_func_t)(void *metadata);
typedef void (*subscribe_hook_func_t)(observable_t *ob,
                                      subscription_t *subscription);

struct observable {
    subscription_t *subscribers;
    bool is_complete;
    subscribe_hook_func_t on_subscribe;
    void *metadata;
    destroy_func_t metadata_destroy;
};

observable_t *ob_create();
void ob_destroy(observable_t *ob);

void ob_complete(observable_t *ob);
void ob_observe(const observable_t *ob, const void *data);
void ob_error(const observable_t *ob, const void *error);
void ob_relay(observable_t *ob, const ob_event_t *event);
void ob_subscribe(observable_t *ob, const subscribe_func_t func, void *metadata,
                  const destroy_func_t metadata_destroy);

typedef void *(*map_func_t)(const void *data);
typedef bool (*filter_func_t)(const void *data);

observable_t *ob_merge(observable_t *ob1, observable_t *ob2);
observable_t *ob_map(observable_t *ob, map_func_t f);
observable_t *ob_filter(observable_t *ob, filter_func_t f);
observable_t *ob_delay(observable_t *ob, unsigned int delay_ms);
observable_t *ob_range(int start, int stop);

#endif
