#include <stdio.h>
#include <unistd.h>
#include "observable.h"

void print_int(const ob_event_t *event, void *metadata) {
    (void)metadata;
    int i;
    switch (event->type) {
        case OB_DATA:
            i = (int)(event->data);
            printf("%s: %d\n", __func__, i);
            break;
        case OB_COMPLETE:
            printf("%s: complete\n", __func__);
            break;
        default:
            break;
    }
}

void *add_1(const void *data) {
    int i = (int)data;
    return (void *)(i + 1);
}

bool even(const void *data) {
    int i = (int)data;
    return i % 2 == 0;
}

bool odd(const void *data) {
    int i = (int)data;
    return i % 2 != 0;
}

int main() {
    observable_t *source = ob_range(1, 5);
    observable_t *ob = ob_delay(ob_map(source, add_1), 3000);
    // observable_t *ob = ob_map(source, add_1);
    ob_subscribe(ob_filter(ob, even), print_int, NULL, NULL);
    ob_subscribe(ob_map(ob_filter(ob, odd), add_1), print_int, NULL, NULL);
    for (int i = 0; i < 10; i++) {
        // ob_observe(source, (void *)i);
    }
    // ob_complete(source);
    usleep(4 * 1000 * 1000);
    return 0;
}

