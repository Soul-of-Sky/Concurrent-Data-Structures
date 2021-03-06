#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#include "ebr.h"
#include "qsbr.h"
#include "hpbr.h"

#define N               100
#define NUM_THREADS     8
#define REPEAT_TIMES    10000

/* emulate operations on a lock-free linked list */

#define OP_GET  0x0
#define OP_DEL  0x1

struct item {
    int free;
    int vis;
}items[N];

pthread_t tids[N];

__thread struct timeval t0, t1;
__thread double interval;

static inline void start_measure() {
    gettimeofday(&t0, NULL);
}

static inline double end_measure() {
    gettimeofday(&t1, NULL);
    return t1.tv_sec - t0.tv_sec + 1.0 * (t1.tv_usec - t0.tv_usec) / 1000000;
}

struct ebr* ebr;
struct qsbr* qsbr;
struct hpbr* hpbr;

static inline void set_free(void* addr) {
    struct item* it = (struct item*) addr;
    assert(cmpxchg2(&it->free, 0, 1));
}

static inline int logical_del(struct item* it) {
    return cmpxchg2(&it->vis, 1, 0);
}

static inline void access_item(struct item* it) {
    int free = ACCESS_ONCE(it->free);
    assert(!free);
}

static inline int visable(struct item* it) {
    int vis = ACCESS_ONCE(it->vis);
    return vis;
}

void gen_workload() {
    int i;

    for (i = 0; i < N; i++) {
        items[i].free = 0;
        items[i].vis = 1;
    }
}

void* ebr_test_fun(void* args) {
    long tid = (long) args;
    int idx, op, i;
    int times = N;

    ebr_thread_register(ebr, tid);

    while(times--) {
        idx = rand() % N;
        op = rand() % 2;

        ebr_enter(ebr, tid);

        /* find */
        for (i = 0; i < idx; i++) {
            if (!visable(&items[i])) {
                continue;
            }
            access_item(&items[i]);
        }

        if (visable(&items[i])) {
            switch (op) {
            case OP_GET:
                access_item(&items[i]);
                break;
            case OP_DEL:
                if (logical_del(&items[i])) {
                    ebr_put(ebr, &items[i], tid);
                }
                break;
            }
        }

        ebr_exit(ebr, tid);
    }

    ebr_thread_unregister(ebr, tid);
}

void* qsbr_test_fun(void* args) {
    long tid = (long) args;
    int idx, op, i;
    int times = N;

    qsbr_thread_register(qsbr, tid);

    while(times--) {
        idx = rand() % N;
        op = rand() % 2;

        /* find */
        for (i = 0; i < idx; i++) {
            if (!visable(&items[i])) {
                continue;
            }
            access_item(&items[i]);
        }

        if (visable(&items[i])) {
            switch (op) {
            case OP_GET:
                access_item(&items[i]);
                break;
            case OP_DEL:
                if (logical_del(&items[i])) {
                    qsbr_put(qsbr, &items[i], tid);
                }
                break;
            }
        }

        qsbr_checkpoint(qsbr, tid);
    }

    qsbr_thread_unregister(qsbr, tid);
}

void* hpbr_test_fun(void* args) {
    long tid = (long) args;
    int idx, op, i;
    int times = N;
    int first;

    hpbr_thread_register(hpbr, tid);

    while(times--) {
        idx = rand() % N;
        op = rand() % 2;

        first = 1;
        /* find */
        for (i = 0; i < idx; i++) {
            if (!visable(&items[i])) {
                continue;
            }
            if (!first) {
                hpbr_release(hpbr, 0, 0, tid);
            }
            first = 0;
            hpbr_hold(hpbr, 1, 0, &items[i], tid);
            access_item(&items[i]);
            hpbr_hold(hpbr, 0, 0, &items[i], tid);
        }

        if (visable(&items[i])) {
            if (!first) {
                hpbr_release(hpbr, 0, 0, tid);
            }
            hpbr_hold(hpbr, 1, 0, &items[i], tid);
            switch (op) {
            case OP_GET:
                access_item(&items[i]);
                hpbr_release(hpbr, 1, 0, tid);
                break;
            case OP_DEL:
                if (logical_del(&items[i])) {
                    assert(!items[i].vis);
                    hpbr_retire(hpbr, &items[i], tid);
                }
                break;
            }
        }
    }

    hpbr_thread_unregister(hpbr, tid);
}

void start_test(void* (*test_fun)(void*)) {
    long i;

    for (i = 0; i < NUM_THREADS; i++) {
        pthread_create(&tids[i], NULL, test_fun, (void*) i);
    }

    for (i = 0; i < NUM_THREADS; i++) {
        pthread_join(tids[i], NULL);
    }
}

void ebr_test() {
    int times = REPEAT_TIMES;

    printf("EBR TEST START\n");

    start_measure();
    while(times--) {
        gen_workload();

        ebr = ebr_create(set_free);

        start_test(ebr_test_fun);

        ebr_destroy(ebr);
    }
    interval = end_measure();
    printf("EBR PASSED, time elapsed %.3lf seconds\n", interval);
}

void qsbr_test() {
    int times = REPEAT_TIMES;

    printf("QSBR TEST START\n");

    while(times--) {
        gen_workload();

        qsbr = qsbr_create(set_free);

        start_test(qsbr_test_fun);

        qsbr_destroy(qsbr);
    }
    interval = end_measure();
    printf("QSBR PASSED, time elapsed %.3lf seconds\n", interval);
}

void hpbr_test() {
    int times = REPEAT_TIMES;

    printf("HPBR TEST START\n");

    while(times--) {
        gen_workload();

        hpbr = hpbr_create(set_free, 1);

        start_test(hpbr_test_fun);

        hpbr_destroy(hpbr);
    }
    
    interval = end_measure();
    printf("HPBR PASSED, time elapsed %.3lf seconds\n", interval);
}

int main() {
    ebr_test();
    qsbr_test();
    hpbr_test();
}