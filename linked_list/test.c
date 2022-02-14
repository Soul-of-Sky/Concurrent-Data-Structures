#include "linked_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>

#ifndef N
#define N 100 //entry per thread
#endif

#ifndef THREAD_NUM
#define THREAD_NUM 8 //number of thread
#endif

static const int rand_seed = 100;

static pkey_t data[THREAD_NUM][N];

pthread_t tids[THREAD_NUM];

//#define RANDOMIZE

//#define NDEBUG;

struct thread_argu
{
    struct linked_list *ll;
    unsigned long idx;
} ta[N * THREAD_NUM];

static void generate_data()
{
    unsigned long i, j;
    int swap_pos;
    pkey_t temp;

    temp = 0;
    for (i = 0; i <= THREAD_NUM - 1; i++)
        for (j = 0; j <= N - 1; j++)
            data[i][j] = (pkey_t)(temp++);
#ifdef RANDOMIZE
    srand(rand_seed);
    for (i = 0; i <= THREAD_NUM - 1; i++)
        for (j = 1; j <= N - 1; j++)
        {
            swap_pos = rand() % (N - i) + i;
            temp = data[i][j]; //swap
            data[i][j] = data[i][swap_pos];
            data[i][swap_pos] = temp;
        }
#endif //RANDOMIZE
    return;
}

void *thread_func_insert(void *arg)
{
    struct thread_argu *a = (struct thread_argu *)arg;
    unsigned long idx = a->idx;
    unsigned long i, j;
    struct linked_list *ll = a->ll;

    for (i = 0; i <= N - 1; i++)
        assert(ll_insert(ll, data[idx][i], data[idx][i]) == 0);

    for (i = 0; i <= N - 1; i++)
        assert(ll_lookup(ll, data[idx][i]) == 0);

    return NULL;
}

void *thread_func_del(void *arg)
{
    struct thread_argu *a = (struct thread_argu *)arg;
    unsigned long idx = a->idx;
    unsigned long i, j;
    struct linked_list *ll = a->ll;

    for (i = 0; i <= N - 1; i++)
        assert(ll_remove(ll, data[idx][i]) == 0);

    return NULL;
}

int main()
{
    long i;
    int ret;
    struct linked_list *ll;
    entry_t *scan_res = malloc(sizeof(entry_t)*(N*THREAD_NUM));
    int size;

    generate_data();

    ll = ll_init();

    for (i = 0; i <= THREAD_NUM - 1; i++)
    {
        ta[i].idx = i;
        ta[i].ll = ll;
        assert(pthread_create(tids + i, NULL, thread_func_insert, (void *)(ta + i)) == 0);
    }
    for (i = 0; i <= THREAD_NUM - 1; i++)
        assert(pthread_join(tids[i], NULL) == 0);

    size = ll_range(ll,0,799,scan_res);
    for (i = 0;i<=size-1;i++)
        printf("%d: %lu %lu\n",i,scan_res[i].key,scan_res[i].value);    

    for (i = 0; i <= THREAD_NUM - 1; i++)
        assert(pthread_create(tids + i, NULL, thread_func_del, (void *)(ta + i)) == 0);
    for (i = 0; i <= THREAD_NUM - 1; i++)
        assert(pthread_join(tids[i], NULL) == 0);

    puts("print all 2:");
    ll_print(ll);
    ll_destory(ll);
    return 0;
}
/*

*/