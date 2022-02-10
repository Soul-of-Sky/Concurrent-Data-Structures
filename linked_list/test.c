#include "linked_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main()
{
    int n = 10, size;
    pkey_t i;
    struct linked_list *ll = ll_init();

    for (i = (pkey_t)n; i >= 1; i--)
    {
        printf(ll_insert(ll, i, i) == 0 ? "good insert\n" : "bad insert\n");
    }
    for (i = 1; i <= n + 1; i++)
    {
        printf(ll_lookup(ll, i) == 0 ? "good lookup\n" : "bad lookup\n");
    }
    //for (i = n; i >= 1; i--)
    //    printf("insert ret val %d\n", ll_remove(ll, n - i + 1));

    printf("lookup ret val %d\n", ll_lookup(ll, n));
    printf("ret val %d\n", ll_update(ll, n, 114514));
    entry_t *arr = malloc(sizeof(entry_t) * (n));
    
    printf("range:\n");
    size = ll_range(ll, 1, n / 2, arr);
    for (i=0;i<=size-1;i++)
        printf("%lu %lu\n", arr[i].key, arr[i].value);

    ll_print(ll);
    ll_destory(ll);
    return 0;
}
