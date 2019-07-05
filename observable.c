#include "observable.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct subscription {
    const subscribe_func_t func;
    void *metadata;
    const destroy_func_t metadata_destroy;
    subscription_t *next;
};

observable_t *ob_create() { return calloc(1, sizeof(observable_t)); }

void ob_destroy(observable_t *ob) {
    subscription_t *subscriber = ob->subscribers;
    while (subscriber != NULL) {
        if (subscriber->metadata_destroy != NULL)
            subscriber->metadata_destroy(subscriber->metadata);

        subscription_t *temp = subscriber;
        subscriber = subscriber->next;
        free(temp);
    }
    if (ob->metadata_destroy != NULL) ob->metadata_destroy(ob->metadata);
    free(ob);
}

static void ob_broadcast(const observable_t *ob, const ob_event_t *event) {
    const subscription_t *subscriber = ob->subscribers;
    while (subscriber != NULL) {
        subscriber->func(event, subscriber->metadata);
        subscriber = subscriber->next;
    }
}

static ob_event_t OB_COMPLETE_EVENT = {
    .type = OB_COMPLETE,
};

void ob_observe(const observable_t *ob, const void *data) {
    const ob_event_t event = {
        .type = OB_DATA,
        .data = data,
    };
    ob_broadcast(ob, &event);
}

void ob_error(const observable_t *ob, const void *error) {
    const ob_event_t event = {
        .type = OB_ERROR,
        .error = error,
    };
    ob_broadcast(ob, &event);
}

void ob_complete(observable_t *ob) {
    ob->is_complete = true;
    ob_broadcast(ob, &OB_COMPLETE_EVENT);
}

void ob_relay(observable_t *ob, const ob_event_t *event) {
    switch (event->type) {
        case OB_DATA:
            ob_observe(ob, event->data);
            break;
        case OB_ERROR:
            ob_error(ob, event->data);
            break;
        case OB_COMPLETE:
            ob_complete(ob);
            break;
    }
}

void ob_subscribe(observable_t *ob, const subscribe_func_t func, void *metadata,
                  const destroy_func_t metadata_destroy) {
    subscription_t *subscription = calloc(1, sizeof(*subscription));
    *subscription = (subscription_t){
        .func = func,
        .metadata = metadata,
        .metadata_destroy = metadata_destroy,
        .next = ob->subscribers,
    };
    ob->subscribers = subscription;

    if (ob->on_subscribe != NULL) ob->on_subscribe(ob, subscription);

    if (ob->is_complete) subscription->func(&OB_COMPLETE_EVENT, metadata);
}

struct merge_data {
    observable_t *out;
    int complete_count;
};
static void merge_helper(const ob_event_t *event, void *metadata) {
    struct merge_data *data = metadata;
    switch (event->type) {
        case OB_COMPLETE:
            data->complete_count++;
            if (data->complete_count == 2) ob_complete(data->out);
            break;
        default:
            ob_relay(data->out, event);
            break;
    }
}
observable_t *ob_merge(observable_t *ob1, observable_t *ob2) {
    observable_t *result = ob_create();
    struct merge_data *data = calloc(1, sizeof(*data));
    data->out = result;
    ob_subscribe(ob1, merge_helper, data, free);
    ob_subscribe(ob2, merge_helper, data, free);
    return result;
}

struct map_data {
    const map_func_t func;
    observable_t *out;
};
static void map_helper(const ob_event_t *event, void *metadata) {
    struct map_data *data = metadata;
    switch (event->type) {
        case OB_DATA:
            ob_observe(data->out, data->func(event->data));
            break;
        default:
            ob_relay(data->out, event);
    }
}
observable_t *ob_map(observable_t *ob, const map_func_t f) {
    observable_t *result = ob_create();
    struct map_data *data = calloc(1, sizeof(*data));
    *data = (struct map_data){
        .func = f,
        .out = result,
    };
    ob_subscribe(ob, map_helper, data, free);
    return result;
}

struct filter_data {
    filter_func_t func;
    observable_t *out;
};
static void filter_helper(const ob_event_t *event, void *metadata) {
    struct filter_data *data = metadata;
    switch (event->type) {
        case OB_DATA:
            if (data->func(event->data)) ob_observe(data->out, event->data);
            break;
        default:
            ob_relay(data->out, event);
    }
}
observable_t *ob_filter(observable_t *ob, const filter_func_t f) {
    observable_t *result = ob_create();
    struct filter_data *data = calloc(1, sizeof(*data));
    *data = (struct filter_data){
        .func = f,
        .out = result,
    };
    ob_subscribe(ob, filter_helper, data, free);
    return result;
}

struct delay_data {
    unsigned int delay_ms;
    observable_t *out;
    ob_event_t *event;
};
static void *delay_helper_2(void *data) {
    struct delay_data *delay_data = data;
    usleep(delay_data->delay_ms * 1000);
    ob_relay(delay_data->out, delay_data->event);
    free(delay_data->event);
    free(delay_data);
    return 0;
}
static void delay_helper_1(const ob_event_t *event, void *metadata) {
    struct delay_data *delay_data = calloc(1, sizeof(*delay_data));
    memcpy(delay_data, metadata, sizeof(*delay_data));
    delay_data->event = calloc(1, sizeof(*event));
    memcpy(delay_data->event, event, sizeof(*event));
    pthread_t thread;
    pthread_create(&thread, NULL, delay_helper_2, delay_data);
}
observable_t *ob_delay(observable_t *ob, unsigned int delay_ms) {
    observable_t *result = ob_create();
    struct delay_data *data = calloc(1, sizeof(*data));
    *data = (struct delay_data){
        .delay_ms = delay_ms,
        .out = result,
    };
    ob_subscribe(ob, delay_helper_1, data, free);
    return result;
}

struct range_data {
    int start;
    int stop;
};
static void range_on_subscribe(observable_t *ob, subscription_t *subscriber) {
    struct range_data *data = ob->metadata;
    for (int i = data->start; i <= data->stop; i++) {
        const ob_event_t event = {
            .type = OB_DATA,
            .data = (void *)i,
        };
        subscriber->func(&event, subscriber->metadata);
    }
    subscriber->func(&OB_COMPLETE_EVENT, subscriber->metadata);
}
observable_t *ob_range(int start, int stop) {
    observable_t *result = ob_create();
    struct range_data *data = calloc(1, sizeof(*data));
    *data = (struct range_data){
        .start = start,
        .stop = stop,
    };
    result->metadata = data;
    result->metadata_destroy = free;
    result->on_subscribe = range_on_subscribe;
    return result;
}
