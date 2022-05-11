#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "heap.h"

static inline void h_swap(struct h_node* a, struct h_node* b) {
    struct h_node t;
    /*memcpy spinlock is dangerous, could cause unexpected dead-lock*/
    int size = sizeof(struct h_node) - sizeof(spinlock_t);

    memcpy(&t, a, size);
    memcpy(a, b, size);
    memcpy(b, &t, size);
}

extern struct heap* h_create(int size) {
    /*size to be an odd to prevent SEG_FAULT when access right child*/
    if (!(size & 1)) {
        size++;
    }
    int h_size = sizeof(struct heap) + size * sizeof(struct h_node);
    struct heap* h = (struct heap*) malloc(h_size);
    int i;
    
    h->size = size;
    h->len = 0;
    spin_lock_init(&h->lock);

    for (i = 0; i < size; i++) {
        spin_lock_init(&h->nodes[i].lock);
        h->nodes[i].tag = EMPTY;
        h->nodes[i].tid = -1;
    }

    return h;
}

extern void h_destroy(struct heap* h) {
    free(h);
}

extern int h_push(struct heap* h, ukey_t k, int tid) {
    int now, fa, prev;
    struct h_node* nodes = h->nodes;

    spin_lock(&h->lock);
    if (h->len == h->size) {
        spin_unlock(&h->lock);
        return -ENOBUFS;
    }
    now = h->len++;
    spin_lock(&nodes[now].lock);
    nodes[now].k = k;
    nodes[now].tag = BUSY;
    nodes[now].tid = tid;
    spin_unlock(&nodes[now].lock);
    spin_unlock(&h->lock);
    
    /*root_id is 0*/
    /*father: i, child: (2 * i + 1), (2 * i + 2)*/
    while(now > 0) {
        fa = (now - 1) / 2;
        spin_lock(&nodes[fa].lock);
        spin_lock(&nodes[now].lock);
        prev = now;
        if (nodes[fa].tag == AVAIL && 
        nodes[now].tag == BUSY && nodes[now].tid == tid) {
            if (k_cmp(nodes[now].k, nodes[fa].k) < 0) {
                h_swap(&nodes[now], &nodes[fa]);
                now = fa;
            } else {
                nodes[now].tag = AVAIL;
                nodes[now].tid = -1;
                spin_unlock(&nodes[fa].lock);
                spin_unlock(&nodes[prev].lock);
                return 0;
            }
        } else if (nodes[now].tag != BUSY || nodes[now].tid != tid) {
            /*node[now] has been moved uppon*/
            now = fa;
        }
        spin_unlock(&nodes[fa].lock);
        spin_unlock(&nodes[prev].lock);
    }

    if (now == 0) {
        spin_lock(&nodes[0].lock);
        if (nodes[0].tag == BUSY && nodes[0].tid == tid) {
            nodes[0].tag = AVAIL;
            nodes[now].tid = -1;
        }
        spin_unlock(&nodes[0].lock);
    }

    return 0;
}

extern int h_pop(struct heap* h, ukey_t *k) {
    int last, left, right, child, now;
    struct h_node* nodes = h->nodes;

    spin_lock(&h->lock);
    if (!h->len) {
        spin_unlock(&h->lock);
        return -ENONET;
    }
    last = --h->len;
    spin_lock(&nodes[0].lock);
    *k = nodes[0].k;
    if (last == 0) {
        spin_unlock(&h->lock);
        spin_unlock(&nodes[0].lock);
        return 0;
    }
    spin_lock(&nodes[last].lock);
    spin_unlock(&h->lock);
    nodes[0].tag = EMPTY;
    nodes[0].tid = -1;
    h_swap(&nodes[0], &nodes[last]);
    spin_unlock(&nodes[last].lock);
    assert(nodes[0].tag != EMPTY);
    
    /*node[0] is still locked*/
    now = 0;
    while(now * 2 + 2 < h->len) {
        left = now * 2 + 1;
        right = now * 2 + 2;
        spin_lock(&nodes[left].lock);
        spin_lock(&nodes[right].lock);
        if (nodes[left].tag == EMPTY) {
            spin_unlock(&nodes[left].lock);
            spin_unlock(&nodes[right].lock);
            break;
        } else if (nodes[right].tag == EMPTY || 
        k_cmp(nodes[left].k, nodes[right].k) < 0) {
            spin_unlock(&nodes[right].lock);
            child = left;
        } else {
            spin_unlock(&nodes[left].lock);
            child = right;
        }
        /*pop() will not be delayed when add() is moving a node*/
        if (k_cmp(nodes[child].k, nodes[now].k) < 0) {
            h_swap(&nodes[child], &nodes[now]);
            spin_unlock(&nodes[now].lock);
            now = child;
        } else {
            spin_unlock(&nodes[child].lock);
            break;
        }
    }
    spin_unlock(&nodes[now].lock);

    return 0;
}

extern int h_top(struct heap* h, ukey_t *k) {
    spin_lock(&h->lock);
    if (!h->len) {
        spin_unlock(&h->lock);
        return -ENONET;
    }
    *k = h->nodes[0].k;
    spin_unlock(&h->lock);
    return 0;
}
