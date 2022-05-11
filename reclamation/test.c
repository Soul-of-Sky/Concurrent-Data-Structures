#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include "ebr.h"
#include "qsbr.h"
#include "hpbr.h"

#define N 1000000
#define NUM_THREADS 8

struct item {
    int free;
    int vis;
}items[N];

static void set_free(void* addr) {
    struct item* it = (struct item*) addr;
    assert(it->free == 0);
    it->free = 1;
}


void gen_workload() {
    
}

void* ebr_test() {
    struct ebr* ebr = ebr_create(set_free);
    
}

void* qsbr_test() {

}

void* hpbr_test() {

}

int main() {
    gen_workload();
}