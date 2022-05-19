#ifndef ELIMINATOR_H
#define ELIMINATOR_H

#include <sys/time.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "util.h"
#include "atomic.h"

#ifndef DISABLE_MAX
/*DEFAULT: the lower 2 bits are unavailable*/
#define MASK    (~0x3UL)
#define EMPTY   0x0
#define WAITING 0x1
#define BUSY    0x2
#else
/*the higher 2 bits are unavailable*/
#define MASK    0x3fffffffffffffff
#define EMPTY   0x0UL
#define WAITING (0x1UL << 62)
#define BUSY    (0x2UL << 62)
#endif

#define E_ARR_SIZE  32

struct exchanger {
    volatile uint64_t ev;
};

struct elimination {
    struct exchanger e_arr[E_ARR_SIZE];
};

static struct elimination* el_create() {
    struct elimination* el = calloc(1, sizeof(struct elimination));
    return el;
}

static void el_destroy(struct elimination* el) {
    free(el);
}

static int exchange(struct exchanger* ex, uint64_t v, unsigned long timeout_usecs, uint64_t* ex_v) {
    struct timeval t0, t1;
    unsigned long interval;
    uint64_t tag;
    uint64_t old_ev, new_ev;

    *ex_v = 0;

    v &= MASK;
    gettimeofday(&t0, NULL); 
    while(1) {
        gettimeofday(&t1, NULL);
        interval = (t1.tv_sec - t0.tv_sec) * 1000000UL + t1.tv_usec - t0.tv_usec;
        if (interval > timeout_usecs) {
            return -ETIMEDOUT;
        }
        old_ev = ex->ev;
        tag = old_ev & (~MASK);
        switch (tag) {
        case EMPTY:
            new_ev = v | WAITING;
            if (cmpxchg2(&ex->ev, old_ev, new_ev)) {
                while(1) {
                    gettimeofday(&t1, NULL);
                    interval = (t1.tv_sec - t0.tv_sec) * 1000000UL + t1.tv_usec - t0.tv_usec;
                    if (interval > timeout_usecs) {
                        break;
                    }
                    old_ev = ex->ev;
                    tag = old_ev & (~MASK);
                    if (tag == BUSY) {
                        /* another thread exchange it */
                        ex->ev = EMPTY;
                        *ex_v = old_ev & MASK;
                        return 0;
                    }
                }

                old_ev = new_ev;
                if (cmpxchg2(&ex->ev, old_ev, EMPTY)) {
                    /* too long to wait, let's leave */
                    return -ETIMEDOUT;
                } else {
                    /* thank god, someone exchange it at the last moment */
                    old_ev = ex->ev;
                    ex->ev = EMPTY;
                    *ex_v = old_ev & MASK;
                    return 0;
                }
            }
            break;
        case WAITING:
            /* someone is waiting for us to exchange*/
            new_ev = v | BUSY;
            if (cmpxchg2(&ex->ev, old_ev, new_ev)) {
                *ex_v = old_ev & MASK;
                return 0;
            }
            /* better luck next time */
            break;
        case BUSY:
            /* there have been two guys... */
            break;
        }
        
    }
}

static int exchang2(struct elimination* el, int fence, uint64_t v, unsigned long timeout_usecs, uint64_t* ex_v) {
    int idx = rand() % fence;
    return exchange(&el->e_arr[idx], v, timeout_usecs, ex_v);
}

#endif