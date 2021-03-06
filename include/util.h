#ifndef UTIL_H
#define UTIL_H 

#include <stdint.h>
#include <stdio.h>

typedef uint64_t    ukey_t;
typedef size_t      uval_t;

typedef struct {
    ukey_t k;
    uval_t v;
}entry_t;

static int k_cmp(ukey_t a, ukey_t b) {
    if (a > b) return 1;
    if (a < b) return -1;
    return 0;
}

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define MAX_NUM_THREADS     32
#define MAX_N               10000000

#endif