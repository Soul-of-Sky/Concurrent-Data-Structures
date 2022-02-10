#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include "linked_list.h"

static int key_cmp(pkey_t key_a, pkey_t key_b)
{
	if (key_a < key_b)
		return -1;
	else if (key_a == key_b)
		return 0;
	else
		return 1;
}

const pkey_t TAIL_KEY = ~0; //key of sentinel node head and tail
const pkey_t HEAD_KEY = 0;

struct ll_node *init_ll_node(pkey_t key, pval_t value, struct ll_node *next)
{
	struct ll_node *ret = malloc(sizeof(struct ll_node));
	node_key(ret) = key;
	node_val(ret) = value;
	ret->next = next;
	spin_lock_init(&(ret->lock)); //ignoring return val of spin_lock_init();

	return ret;
}

void free_ll_node(struct ll_node *node)
{
	//free(node->e);
	free(node);
	return;
}

void print_entry(entry_t entry)
{
	prinf("%lu %lu\n", entry.key, entry.value);
	return;
}

struct linked_list *ll_init()
{
	struct linked_list *ll;
	ll = malloc(sizeof(struct linked_list));

	ll->tail = init_ll_node(TAIL_KEY, 0, NULL);
	ll->head = init_ll_node(HEAD_KEY, 0, ll->tail);

	return ll;
}

static int validate(struct linked_list *ll, struct ll_node *pred, struct ll_node *curr)
{
	struct ll_node *node = ll->head;
	while (key_cmp(node_key(node), node_key(pred)) <= 0)
	{
		if (node == pred)
			return pred->next == curr ? 0 : -1;
		node = node->next;
	}
	return -1;
}

int ll_insert(struct linked_list *ll, pkey_t key, pval_t val)
{
	struct ll_node *pred,
		*curr,
		*tmp;
	int ret;

	while (1)
	{
		pred = ll->head;
		curr = ll->head->next;
		while (key_cmp(node_key(curr), key) < 0) //loop until curr->key >= key
		{
			pred = curr;
			curr = curr->next;
		}

		spin_lock(&(pred->lock)); //lock before validate
		spin_lock(&(curr->lock));
		if (validate(ll, pred, curr) == 0)
		{
			if (key_cmp(node_key(curr), key) == 0)
				ret = -EEXIST;
			else
			{
				pred->next = init_ll_node(key, val, curr);
				ret = 0;
			}
			spin_unlock(&(curr->lock)); //always unlock
			spin_unlock(&(pred->lock));
			return ret;
		}
	} //end of
} //end of ll_insert()

int ll_update(struct linked_list *ll, pkey_t key, pval_t val)
{
	struct ll_node *pred,
		*curr;
	int ret;

	while (1)
	{
		pred = ll->head;
		curr = pred->next;
		while (key_cmp(node_key(curr), key) < 0) //loop until curr->key >= key
		{
			pred = curr;
			curr = curr->next;
		}

		spin_lock(&(curr->lock));
		spin_lock(&(pred->lock));
		if (validate(ll, pred, curr) == 0)
		{
			if (key_cmp(node_key(curr), key) == 0)
			{
				node_val(curr) = val;
				ret = 0;
			}
			else
				ret = -ENOENT;

			spin_unlock(&(curr->lock));
			spin_unlock(&(pred->lock));
			return ret;
		}
	} //end of while (1)
} // end of ll_update;

int ll_lookup(struct linked_list *ll, pkey_t key)
{
	struct ll_node *pred,
		*curr;
	int ret;

	while (1)
	{
		pred = ll->head;
		curr = pred->next;
		while (key_cmp(node_key(curr), key) <= 0)
		{
			pred = curr;
			curr = curr->next;
		}

		spin_lock(&(curr->lock));
		spin_lock(&(pred->lock));
		if (validate(ll, pred, curr) == 0)
		{
			ret = key_cmp(node_key(curr), key) == 0 ? 0 : -ENOENT;

			spin_unlock(&(curr->lock));
			spin_unlock(&(pred->lock));
			return ret;
		}
	} //end of while (1)
} // end of ll_lookup()

int ll_remove(struct linked_list *ll, pkey_t key)
{
	struct ll_node *pred,
		*curr;
	int ret;
	while (1)
	{
		pred = ll->head;
		curr = pred->next;
		while (key_cmp(node_key(curr), key) <= 0)
		{
			pred = curr;
			curr = curr->next;
		}

		spin_lock(&(curr->lock));
		spin_lock(&(pred->lock));
		if (validate(ll, pred, curr) == 0)
		{
			if (key_cmp(node_key(curr), key) == 0)
			{
				pred->next = curr->next;
				free_ll_node(curr);
				ret = 0;
			}
			else
				ret = -ENOENT;

			spin_unlock(&(curr->lock));
			spin_unlock(&(pred->lock));
			return ret;
		}

	} //end of while (1)
} // end of ll_remove()

/**
 * @brief putting entries into res_arr where key is in [low, high]
 */
int ll_range(struct linked_list *ll, pkey_t low, pkey_t high, entry_t *res_arr)
{
	int size = 0;
	struct ll_node *curr = ll->head->next;
	while (key_cmp(node_key(curr), low) < 0)
		curr = curr->next;
	while (key_cmp(node_key(curr), high) <= 0)
	{
		res_arr[size++] = node_val(curr);
		curr = curr->next;
	}

	return size;
} //end of ll_range()

void ll_print(struct linked_list *ll)
{
	struct ll_node *curr = ll->head->next;
	while (key_cmp(node_key(curr), TAIL_KEY) < 0)
	{
		print_entry(curr->e);
		curr = curr->next;
	}
	return;
}

void ll_destory(struct linked_list *ll)
{
	struct ll_node *curr = ll->head, *tmp;
	while (curr != NULL)
	{
		tmp = curr->next;
		free_ll_node(curr);
		curr = tmp;
	}
	free(ll);
	return;
}