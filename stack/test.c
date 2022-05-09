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
#include "stack.h"

#define N           10000000
/*NUM_THREAD HAS TO BE AN ODD*/
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

struct stack* s;

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

static void do_barrier(long id, const char* arg) {
    pthread_barrier_wait(&barrier);
    if (id == 0) {
        printf("%s finished in %.3lf seconds\n", arg, end_measure());
    }
}

static void do_push(long id, int expect_ret) {
    int st, ed, i, ret;
    double interval;

    start_measure();

    st = 1.0 * id / NUM_THREAD * N;
    ed = 1.0 * (id + 1) / NUM_THREAD * N;

    for (i = st; i < ed; i++) {
        s_push(s, v[i]);
        test_assert(expect_ret == -1 || ret == expect_ret);
    }

    interval = end_measure();
    test_print("thread[%ld] end in %.3lf seconds\n", interval);
}

static void do_pop(long id, int expect_ret) {
    int st, ed, i, ret;
    double interval;
    uval_t v;

    start_measure();

    st = 1.0 * id / NUM_THREAD * N;
    ed = 1.0 * (id + 1) / NUM_THREAD * N;

    for (i = st; i < ed; i++) {
        s_pop(s, &v);
        test_assert(expect_ret == -1 || ret == expect_ret);
    }

    interval = end_measure();
    test_print("thread[%ld] end in %.3lf seconds\n", interval);
}

void* push_fun(void* arg) {
    long id = (long) arg;

    do_push(id, -1);
    
    do_barrier(id, "PUSH & POP");
}

void* pop_fun(void* arg) {
    long id = (long) arg;

    do_pop(id, -1);

    do_barrier(id, "PUSH & POP");
}

int main() {
    int i;

    gen_data();

    s = s_init();
    
    pthread_barrier_init(&barrier, NULL, NUM_THREAD);

    for (i = 0; i < NUM_THREAD / 2; i++) {
        pthread_create(&tids[i], NULL, push_fun, (void*) i);
        pthread_create(&tids[NUM_THREAD / 2 + i], NULL, pop_fun, (void*) (NUM_THREAD / 2 + i));
    }

    for (i = 0; i < NUM_THREAD; i++) {
        pthread_join(tids[i], NULL);
    }

    s_destroy(s);

    return 0;
}