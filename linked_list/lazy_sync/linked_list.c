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
    spin_lock_init(&node->lock);

    return node;
}

static void free_node(struct ll_node* node) {
    free(node);
}

struct ll* ll_create() {
    struct ll* ll = (struct ll*) malloc(sizeof(struct ll));
    ukey_t max_k = UINT64_MAX;

    ll->head = malloc_node(0, 0);
    ll->tail = malloc_node(max_k, 0);

    ll->head->next = ll->tail;

    ll->ebr = ebr_create(free_node);

    return ll;
}

void ll_destroy(struct ll* ll) {
    struct ll_node *pred, *curr;

    pred = ll->head;

    while(pred) {
        curr = GET_NODE(pred->next);
        free_node(pred);
        pred = curr;
    }

    ebr_destroy(ll->ebr);

    free(ll);
}

static int validate(struct ll_node* pred, struct ll_node* curr) {
    return !IS_MARKED(pred->next) && !IS_MARKED(curr->next) && GET_NODE(pred->next) == curr;
}

int ll_insert(struct ll* ll, ukey_t k, uval_t v, int tid) {
    struct ll_node *pred, *curr, *node;

    ebr_enter(ll->ebr, tid);
retry:    
    pred = ll->head;
    curr = GET_NODE(pred->next);

    while(k_cmp(curr->e.k, k) < 0) {
        pred = curr;
        curr = GET_NODE(pred->next);
    }

    spin_lock(&pred->lock);
    spin_lock(&curr->lock);

    if (validate(pred, curr)) {
        if (k_cmp(curr->e.k, k) == 0) {
            spin_unlock(&pred->lock);
            spin_unlock(&curr->lock);
            ebr_exit(ll->ebr, tid);
            return -EEXIST;
        } else {
            node = malloc_node(k, v);
            node->next = curr;
            pred->next = node;

            spin_unlock(&pred->lock);
            spin_unlock(&curr->lock);
            ebr_exit(ll->ebr, tid);
            return 0;
        }
    } else {
        spin_unlock(&pred->lock);
        spin_unlock(&curr->lock);
        goto retry;
    }
}

int ll_lookup(struct ll* ll, ukey_t k, uval_t* v, int tid) {
    struct ll_node *pred, *curr;

    ebr_enter(ll->ebr, tid);
retry:
    pred = ll->head;
    curr = GET_NODE(pred->next);
    
    while(k_cmp(curr->e.k, k) < 0) {
        pred = curr;
        curr = GET_NODE(pred->next);
    }

    if (k_cmp(curr->e.k, k) == 0 && !IS_MARKED(curr->next)) {
        *v = curr->e.v;
        ebr_exit(ll->ebr, tid);
        return 0;
    } else {
        *v = 0;
        ebr_exit(ll->ebr, tid);
        return -ENOENT;
    }
}

int ll_remove(struct ll* ll, ukey_t k, int tid) {
    struct ll_node *pred, *curr;

    ebr_enter(ll->ebr, tid);
retry:
    pred = ll->head;
    curr = GET_NODE(pred->next);
    
    while(k_cmp(curr->e.k, k) < 0) {
        pred = curr;
        curr = GET_NODE(pred->next);
    }

    spin_lock(&pred->lock);
    spin_lock(&curr->lock);

    if (validate(pred, curr)) {
        if (k_cmp(curr->e.k, k) == 0) {
            curr->next = MARK_NODE(curr->next);
            pred->next = GET_NODE(curr->next);
            ebr_put(ll->ebr, curr, tid);

            spin_unlock(&pred->lock);
            spin_unlock(&curr->lock);
            ebr_exit(ll->ebr, tid);
            return 0;
        } else {
            spin_unlock(&pred->lock);
            spin_unlock(&curr->lock);
            ebr_exit(ll->ebr, tid);
            return -ENOENT;
        }
    } else {
        spin_unlock(&pred->lock);
        spin_unlock(&curr->lock);
        goto retry;
    }
}

int ll_range(struct ll* ll, ukey_t k, unsigned int len, uval_t* v_arr, int tid) {
    struct ll_node *curr;
    int cnt = 0;

    ebr_enter(ll->ebr, tid);
    curr = GET_NODE(ll->head->next);
    
    while(k_cmp(curr->e.k, k) < 0) {
        curr = GET_NODE(curr->next);
    }

    while(cnt < len && curr) {
        v_arr[cnt++] = curr->e.v;
        curr = GET_NODE(curr->next);
    }
    ebr_exit(ll->ebr, tid);

    return cnt;
}

void ll_print(struct ll* ll) {
    struct ll_node *curr;
    
    curr = GET_NODE(ll->head->next);

    while(curr != ll->tail) {
        printf("<%ld, %ld> ", curr->e.k, curr->e.v);
        curr = GET_NODE(curr->next);
    }
    printf("\n");
}

