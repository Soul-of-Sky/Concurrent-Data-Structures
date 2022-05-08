#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

#include "linked_list.h"

static struct ll_node* malloc_node(ukey_t k, uval_t v) {
    struct ll_node* node = (struct ll_node*) malloc(sizeof(struct ll_node));

    node->next = NULL;
    node->e.k = k;
    node->e.v = v;

    return node;
}

struct ll* ll_init() {
    struct ll* ll = (struct ll*) malloc(sizeof(struct ll));
    ukey_t max_k = UINT64_MAX;

    ll->head = malloc_node(0, 0);
    ll->tail = malloc_node(max_k, 0);

    ll->head->next = ll->tail;

    return ll;
}

static void free_node(struct ll_node* node) {
    free(node);
}

void ll_destroy(struct ll* ll) {
    struct ll_node *pred, *curr;

    pred = ll->head;

    while(pred) {
        curr = GET_NODE(pred->next);
        free_node(pred);
        pred = curr;
    }

    free(ll);
}

static void find(struct ll* ll, ukey_t k, struct ll_node** pred, struct ll_node** curr) {
    struct ll_node *__pred, *__curr;
    markable_t pred_markable_v, curr_markable_v;

retry: 
    __pred = ll->head;
    __curr = GET_NODE(__pred->next);
    while(1) {
        curr_markable_v = __curr->next;
        while(IS_MARKED(curr_markable_v)) {
            pred_markable_v = __pred->next;
            if (!cmpxchg2(&__pred->next, REMOVE_MARK(pred_markable_v), REMOVE_MARK(curr_markable_v))) {
                goto retry;
            }
            __pred = __curr;
            __curr = REMOVE_MARK(curr_markable_v);

            //TODO: FREE NODE
        }
        if (k_cmp(__curr->e.k, k) >= 0) {
            *pred = __pred;
            *curr = __curr;
            return;
        }
        __pred = __curr;
        __curr = REMOVE_MARK(curr_markable_v);
    }
}

int ll_insert(struct ll* ll, ukey_t k, uval_t v) {
    struct ll_node *pred, *curr, *node;

retry:
    find(ll, k, &pred, &curr);

    if (k_cmp(curr->e.k, k) == 0) {
        return -EEXIST;
    } else {
        node = malloc_node(k, v);
        node->next = curr;
        
        if (!cmpxchg2(&pred->next, curr, node)) {
            free_node(node);
            goto retry;
        }
        return 0;
    }
}

extern int ll_insert2(struct ll* ll, struct ll_node* node) {
    struct ll_node *pred, *curr;
    ukey_t k = node->e.k;
retry:
    find(ll, k, &pred, &curr);

    if (k_cmp(curr->e.k, k) == 0) {
        return -EEXIST;
    } else {
        node->next = curr;
        
        if (!cmpxchg2(&pred->next, curr, node)) {
            goto retry;
        }
        return 0;
    }
}

int ll_lookup(struct ll* ll, ukey_t k, uval_t* v) {
    struct ll_node *curr = GET_NODE(ll->head->next);

    while(k_cmp(curr->e.k, k) < 0) {
        curr = GET_NODE(curr->next);
    }

    if (k_cmp(curr->e.k, k) == 0 && !IS_MARKED(curr->next)) {
        *v = curr->e.v;
        return 0;
    } else {
        *v = 0;
        return -ENOENT;
    }
}

int ll_remove(struct ll* ll, ukey_t k) {
    struct ll_node *pred, *curr;
    markable_t curr_markable_v;

retry:
    find(ll, k, &pred, &curr);

    if (k_cmp(curr->e.k, k) == 0) {
        curr_markable_v = curr->next;
        if (!cmpxchg2(&curr->next, REMOVE_MARK(curr_markable_v), MARK_NODE(curr_markable_v))) {
            goto retry;
        }
        cmpxchg2(&pred->next, curr, REMOVE_MARK(curr_markable_v));
        return 0;
    } else {
        return -ENOENT;
    }
}

int ll_range(struct ll* ll, ukey_t k, unsigned int len, uval_t* v_arr) {
    struct ll_node *curr;
    int cnt = 0;

    curr = GET_NODE(ll->head->next);
    
    while(k_cmp(curr->e.k, k) < 0) {
        curr = GET_NODE(curr->next);
    }

    while(cnt < len && curr) {
        if (!IS_MARKED(curr->next)) {
            v_arr[cnt++] = curr->e.v;
        }
        curr = GET_NODE(curr->next);
    }

    return cnt;
}

void ll_print(struct ll* ll) {
    struct ll_node *curr;
    
    curr = GET_NODE(ll->head->next);

    while(curr != ll->tail) {
        if (!IS_MARKED(curr->next)) {
            printf("<%ld, %ld> ", curr->e.k, curr->e.v);
            curr = GET_NODE(curr->next);
        }
    }
    printf("\n");
}

