#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>

#ifdef __APPLE__
#include "pthread_barrier.h"
#endif
#include "bptree.h"

#define N           1000000
#define NUM_THREAD  8

#define RAND
// #define DETAIL
// #define ASSERT

#ifdef DETAIL
#define test_print(fmt, args ...) do{printf(fmt, ##args);}while(0)
#else
#define test_print(fmt, args ...) do{}while(0)
#endif

#ifdef ASSERT
#define test_assert(arg) do{assert(arg);}while(0)
#else
#define test_assert(arg) do{}while(0)
#endif

__thread struct timeval t0, t1;
__thread uval_t v_arr[N];

pthread_barrier_t barrier;
pthread_t tids[NUM_THREAD];

ukey_t k[N];
uval_t v[N];

struct bp* bp;

static void gen_data() {
    int i;

    for (i = 0; i < N; i++) {
        k[i] = i + 1;
        v[i] = i + 1;
    }

#ifdef RAND
    ukey_t temp_k;
    uval_t temp_v;
    int idx;

    srand(time(0));
    for (i = 0; i < N; i++) {
        idx = rand() % N;
        temp_k = k[i];
        k[i] = k[idx];
        k[idx] = temp_k;
        temp_v = v[i];
        v[i] = v[idx];
        v[idx] = temp_v;
    }
#endif
}

static void start_measure() {
    gettimeofday(&t0, NULL);
}

static double end_measure() {
    gettimeofday(&t1, NULL);
    return t1.tv_sec - t0.tv_sec + (t1.tv_usec - t0.tv_usec) / 1e6;
}

static void do_insert(long id, int expect_ret) {
    int st, ed, i, ret;
    double interval;

    start_measure();

    st = 1.0 * id / NUM_THREAD * N;
    ed = 1.0 * (id + 1) / NUM_THREAD * N;

    for (i = st; i < ed; i++) {
        ret = bp_insert(bp, k[i], v[i]);
        test_assert(expect_ret == -1 || ret == expect_ret);
        // printf("INSERT %lu\n", k[i]);
        // bp_print(bp);
    }

    interval = end_measure();
    test_print("thread[%ld] end in %.3lf seconds\n", interval);
}

static void do_lookup(long id, int expect_ret) {
    int st, ed, i, ret;
    double interval;
    uval_t __v;

    start_measure();

    st = 1.0 * id / NUM_THREAD * N;
    ed = 1.0 * (id + 1) / NUM_THREAD * N;

    for (i = st; i < ed; i++) {
        ret = bp_lookup(bp, k[i], &__v);
        test_assert(expect_ret == -1 || ret == expect_ret);
    }
    
    interval = end_measure();
    test_print("thread[%ld] end in %.3lf seconds\n", interval);
}

static void do_remove(long id, int expect_ret) {
    int st, ed, i, ret;
    double interval;

    start_measure();

    st = 1.0 * id / NUM_THREAD * N;
    ed = 1.0 * (id + 1) / NUM_THREAD * N;

    for (i = st; i < ed; i++) {
        ret = bp_remove(bp, k[i]);
        test_assert(expect_ret == -1 || ret == expect_ret);
        // printf("REMOVE %lu\n", k[i]);
        // bp_print(bp);
    }

    interval = end_measure();
    test_print("thread[%ld] end in %.3lf seconds\n", interval);
}

static void do_range(long id) {
    int st, ed, i, ret;
    double interval;

    start_measure();

    ret = bp_range(bp, 0, N, v_arr);
    test_assert(ret == N);

    interval = end_measure();
    test_print("thread[%ld] end in %.3lf seconds\n", interval);
}

static void do_barrier(long id, const char* arg) {
    pthread_barrier_wait(&barrier);
    if (id == 0) {
        printf("%s finished in %.3lf seconds\n", arg, end_measure());
    }
}

void* test(void* arg) {
    long id = (long) arg;

    do_insert(id, 0);

    do_barrier(id, "INSERT");

    do_lookup(id, 0);

    do_barrier(id, "LOOKUP");

    do_range(id);

    do_barrier(id, "RANGE");

    do_remove(id, 0);

    do_barrier(id, "REMOVE");

    do_lookup(id, -ENOENT);

    do_barrier(id, "LOOKUP");
}

int main() {
    int i;

    gen_data();

    bp = bp_create(32);
    
    pthread_barrier_init(&barrier, NULL, NUM_THREAD);

    for (i = 0; i < NUM_THREAD; i++) {
        pthread_create(&tids[i], NULL, test, (void*) i);
    }

    for (i = 0; i < NUM_THREAD; i++) {
        pthread_join(tids[i], NULL);
    }

    bp_destroy(bp);

    return 0;
}