#ifndef INC_QUEUE_H
#define INC_QUEUE_H

#include "buf.h"
#include "string.h"

//my buffer list is ==== head -> buf1 -> buf2 -> buf3 -> ... -> bufn

static inline void
list_initialize(struct buf* head)
{
    head->blockno = 0;
    head->flags = 0;
    memset(head->data, 0, sizeof(head->data));
    head->qnext = NULL;
}

static inline void
list_push(struct buf* n,
    struct buf* head)
{
    struct buf* tmp = head;
    while (tmp->qnext != NULL)
        tmp = tmp->qnext;

    tmp->qnext = n;
    n->qnext = NULL;
}

static inline struct buf*
list_front(struct buf* head)//head != NULL
{
    return head->qnext;
}

static inline void
list_pop_front(struct buf* head)//head != NULL
{
    head->qnext = (head->qnext)->qnext;
}

static inline int
list_empty(struct buf* head)
{
    return (head->qnext == NULL);
}

#endif