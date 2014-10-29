#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "list.h"

typedef struct _node_t {
    struct _node_t *next;
    void *item;
    lkd_list_free_fn *free_fn;
} node_t;

struct lkdList {
    node_t *head;
    node_t *tail;
    size_t size;
    int autofree;
};

lkdList *lkd_list_create()
{
    lkdList *self = (lkdList *) malloc(sizeof(*self));
    self->head = NULL;
    self->tail = NULL;
    self->autofree = 0;
    self->size = 0;
    return self;
}

void lkd_list_release(lkdList **self_p)
{
    if (!self_p) {
        return;
    }

    if (*self_p) {
        lkdList *self = *self_p;
        node_t *node = (*self_p)->head;
        while (node) {
            node_t *next = node->next;
            if (node->free_fn)
                (node->free_fn) (node->item);
            else if (self->autofree)
                free (node->item);
            free(node);
            node = next;
        }
        free(self);
        *self_p = NULL;
    }
}
/*
 * Return the item at the head of list. If the list is empty, return NULL.
 */
void *lkd_list_head(lkdList *self)
{
    if (!self) {
        return NULL;
    }
    return self->head ? self->head->item : NULL;
}

/*
 * Return the item at the tail of list. If the list is empty, return NULL.
 */
void *lkd_list_tail(lkdList *self)
{
    if (!self) {
        return NULL;
    }
    return self->tail ? self->tail->item : NULL;
}

static node_t *_lkd_list_node_create()
{
    node_t *node = (node_t *) malloc(sizeof(node_t));
    if (!node) {
        return NULL;
    }

    node->item = NULL;
    node->next = NULL;
    node->free_fn = NULL;

    return node;
}

/*
 * Append an item to the end of the list, return 0 if OK
 * or -1 if this failed for some reason (out of memory).
 */
int lkd_list_append(lkdList *self, void *item)
{
    if (!item) {
        return LIST_ERR;
    }

    node_t *node = _lkd_list_node_create();
    if (!node) {
        return LIST_ERR;
    }
    /* If necessary, take duplicate of (string) item */
    if (self->autofree) {
        item = strdup((char *) item);
    }

    node->item = item;
    if (self->tail) {
        self->tail->next = node;
    } else {
        self->head = node;
    }

    self->tail = node;
    node->next = NULL;

    self->size++;
    return LIST_OK;
}

/*
 * Push an item to the start of the list, return 0 if OK
 * or -1 if this failed for some reason (out of memory).
 */
int lkd_list_push(lkdList *self, void *item)
{
    if (!item) {
        return LIST_ERR;
    }

    node_t *node = (node_t *) malloc(sizeof(node_t));
    if (!node) {
        return LIST_ERR;
    }
    /* If necessary, take duplicate of (string) item */
    if (self->autofree) {
        item = strdup((char *) item);
    }

    node->item = item;
    node->next = self->head;
    self->head = node;
    if (self->tail == NULL) {
        self->tail = node;
    }

    self->size++;
    return LIST_OK;
}

/*
 * Remove item from the beginning of the list, return NULL if none.
 */
void *lkd_list_pop(lkdList *self)
{
    node_t *node = self->head;
    void *item = NULL;
    if (node) {
        item = node->item;
        self->head = node->next;
        if (self->tail == node) {
            self->tail = NULL;
        }
        free(node);
        self->size--;
    }
    return item;
}

/*
 * Remove the item from the list, if present. Safe to call on items
 * that are not in the list. Return 0 if removed one and -1 if items
 * not found.
 */
int lkd_list_remove(lkdList *self, void *item)
{
    node_t *node, *prev = NULL;

    /* First find the specific node from list */
    for (node = self->head; node != NULL; node = node->next) {
        if (node->item == item) {
            break;
        }
        prev = node;
    }
    if (node) {
        if (prev) {
            prev->next = node->next;
        } else {
            self->head = node->next;
        }

        if (node->next == NULL) {
            self->tail = prev;
        }

        if (node->free_fn) {
            (node->free_fn) (node->item);
        } else if (self->autofree) {
            free(node->item);
        }

        free(node);
        self->size--;

        return LIST_OK;
    } else {
        return LIST_ERR;
    }
}

/* Set a free function for the specified list item. When the item is
 * destroyed, the free function, if any, is called on that item.
 * Use this when list items are dynamically allocated to ensure that
 * you don't have memory leaks. You can pass 'free' or NULL as a free_fn.
 * Return the item, or NULL if there is no such item.
 */
void *lkd_list_set_freefn(lkdList *self, void *item, lkd_list_free_fn *fn,
        int at_tail)
{
    node_t *node = (at_tail ? self->tail : self->head);
    while (node) {
        if (node->item == item) {
            node->free_fn = fn;
            return item;
        }
        node = node->next;
    }
    return NULL;
}

/*
 * Make a copy of list. If the list has autofree set, the copied list will
 * duplicate all items, which must be strings. Otherwise, the list will hold
 * pointers back to the items in the original list.
 */
lkdList *lkd_list_dup (lkdList *self)
{
    if (!self) {
        return NULL;
    }

    lkdList *copy = lkd_list_create();
    copy->autofree = self->autofree;

    if (copy) {
        node_t *node;
        for (node = self->head; node; node = node->next) {
            if (lkd_list_append(copy, node->item) == LIST_ERR) {
                lkd_list_release(&copy);
                break;
            }
        }
    }
    return copy;
}

/* Only used in test function */
static void _lkd_list_free (void *data)
{
    lkdList *self = (lkdList *) data;
    lkd_list_release(&self);
}

size_t lkd_list_size(lkdList *self)
{
    return self->size;
}

void lkd_list_autofree(lkdList *self)
{
    if (!self) {
        return;
    }
    self->autofree = 1;
}

#ifdef LIST_TEST_MAIN
int main(int argc, int *argv[])
{
    (void) argc;
    (void) argv;

    printf("lkd_list_test starts...\n");

    lkdList *list = lkd_list_create();
    assert(list);
    assert(lkd_list_size(list) == 0);

    char *apple = "apple";
    char *banana = "banana";
    char *cherry = "cherry";

    lkd_list_append(list, apple);
    assert(lkd_list_size(list) == 1);
    lkd_list_append(list, banana);
    assert(lkd_list_size(list) == 2);
    lkd_list_append(list, cherry);
    assert(lkd_list_size(list) == 3);

    assert(lkd_list_head(list) == apple);
    assert(lkd_list_tail(list) == cherry);

    lkd_list_remove(list, cherry);
    assert(lkd_list_size(list) == 2);
    assert(lkd_list_tail(list) == banana);
    lkd_list_remove(list, apple);
    assert(lkd_list_size(list) == 1);
    assert(lkd_list_head(list) == banana);
    lkd_list_remove(list, banana);
    assert(lkd_list_size(list) == 0);
    assert(lkd_list_tail(list) == NULL);
    assert(lkd_list_head(list) == NULL);

    lkd_list_push(list, banana);
    assert(lkd_list_size(list) == 1);
    assert(lkd_list_head(list) == banana);
    lkd_list_push(list, apple);
    assert(lkd_list_size(list) == 2);
    assert(lkd_list_head(list) == apple);
    lkd_list_append(list, cherry);
    assert(lkd_list_size(list) == 3);
    assert(lkd_list_head(list) == apple);

    lkdList *sub_list = lkd_list_dup(list);
    assert(sub_list);
    assert(lkd_list_size(sub_list) == 3);

    char *item;
    item = (char *) lkd_list_pop(list);
    assert(item == apple);
    item = (char *) lkd_list_pop(list);
    assert(item == banana);
    item = (char *) lkd_list_pop(list);
    assert(item == cherry);
    assert(lkd_list_size(list) == 0);
    (void) item;

    assert(lkd_list_size(sub_list) == 3);
    lkd_list_push(list, sub_list);
    lkdList *sub_list_2 = lkd_list_dup(sub_list);
    lkd_list_append(list, sub_list_2);
    assert(lkd_list_set_freefn(list, sub_list, &_lkd_list_free, 0) == sub_list);
    assert(lkd_list_set_freefn(list, sub_list_2, &_lkd_list_free, 0) == sub_list_2);
    (void) _lkd_list_free;

    /* Destructor should be safe to call twice */
    lkd_list_release(&list);
    lkd_list_release(&list);
    assert(list == NULL);

    /* end */
    printf("done...OK\n");
}
#endif

