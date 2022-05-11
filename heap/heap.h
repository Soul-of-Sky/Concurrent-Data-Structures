#ifndef HEAP_H
#define HEAP_H

#include "util.h"
#include "spinlock.h"

typedef enum {
    EMPTY = 0,
    AVAIL,
    BUSY
}status;

struct h_node {
    status tag;
    ukey_t k;
    int tid;
    spinlock_t lock;
};

struct heap {
    int size;
    int len;
    spinlock_t lock;

    struct h_node nodes[0];
};

extern struct heap* h_create(int size);
extern void h_destroy(struct heap* h);
extern int h_push(struct heap* h, ukey_t k, int tid);
extern int h_pop(struct heap* h, ukey_t *k);
extern int h_top(struct heap* h, ukey_t *k);

#endif