#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

#include "linked_list.h"

static int k_cmp(lkey_t a, lkey_t b) {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

static struct ll_node* malloc_node(lkey_t k, lval_t v) {
    struct ll_node* node = (struct ll_node*) malloc(sizeof(struct ll_node));

    node->next = NULL;
    node->e.k = k;
    node->e.v = v;
    rwlock_init(&node->lock);

    return node;
}

struct ll* ll_init() {
    struct ll* ll = (struct ll*) malloc(sizeof(struct ll));
    lkey_t max_k = UINT64_MAX;

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

static int validate(struct ll_node* pred, struct ll_node* curr) {
    return !IS_MARKED(pred->next) && !IS_MARKED(curr->next) && GET_NODE(pred->next) == curr;
}

int ll_insert(struct ll* ll, lkey_t k, lval_t v) {
    struct ll_node *pred, *curr, *node;

retry:    
    pred = ll->head;
    curr = GET_NODE(pred->next);

    while(k_cmp(curr->e.k, k) < 0) {
        pred = curr;
        curr = GET_NODE(pred->next);
    }

    write_lock(&pred->lock);
    write_lock(&curr->lock);

    if (validate(pred, curr)) {
        if (k_cmp(curr->e.k, k) == 0) {
            write_unlock(&pred->lock);
            write_unlock(&curr->lock);
            return -EEXIST;
        }
        node = malloc_node(k, v);
        node->next = curr;
        pred->next = node;

        write_unlock(&pred->lock);
        write_unlock(&curr->lock);
        return 0;
    } else {
        goto retry;
    }
}

int ll_lookup(struct ll* ll, lkey_t k, lval_t* v) {
    struct ll_node *pred, *curr;

retry:
    pred = ll->head;
    curr = GET_NODE(pred->next);
    
    while(k_cmp(curr->e.k, k) < 0) {
        pred = curr;
        curr = GET_NODE(pred->next);
    }

    if (k_cmp(curr->e.k, k) == 0 && !IS_MARKED(curr->next)) {
        *v = curr->e.v;
        return 0;
    } else {
        *v = 0;
        return -ENOENT;
    }
}

int ll_remove(struct ll* ll, lkey_t k) {
    struct ll_node *pred, *curr;

retry:
    pred = ll->head;
    curr = GET_NODE(pred->next);
    
    while(k_cmp(curr->e.k, k) < 0) {
        pred = curr;
        curr = GET_NODE(pred->next);
    }

    write_lock(&pred->lock);
    write_lock(&curr->lock);

    if (validate(pred, curr)) {
        if (k_cmp(curr->e.k, k) == 0) {
            curr->next = MARK_NODE(curr->next);
            pred->next = GET_NODE(curr->next);
            //TODO: FREE NODE

            write_unlock(&pred->lock);
            write_unlock(&curr->lock);
            return 0;
        } else {
            write_unlock(&pred->lock);
            write_unlock(&curr->lock);
            return -ENOENT;
        }
    } else {
        goto retry;
    }
}

int ll_range(struct ll* ll, lkey_t k, unsigned int len, lval_t* v_arr) {
    struct ll_node *curr;
    int cnt = 0;

    curr = GET_NODE(ll->head->next);
    
    while(k_cmp(curr->e.k, k) < 0) {
        curr = GET_NODE(curr->next);
    }

    while(cnt < len && curr) {
        v_arr[cnt++] = curr->e.v;
        curr = GET_NODE(curr->next);
    }

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

