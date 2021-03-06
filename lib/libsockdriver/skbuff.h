#ifndef _LIBSOCKDRIVER_SKBUFF_H_
#define _LIBSOCKDRIVER_SKBUFF_H_

#include <sys/types.h>
#include <lyos/list.h>

struct sock;

struct sk_buff {
    struct list_head list;
    struct sock* sock;
    unsigned int users;
    void (*destructor)(struct sk_buff* skb);

    char cb[48] __attribute__((aligned(8)));

    size_t data_len;
    char data[0];
};

struct sk_buff_head {
    struct list_head head;
    size_t qlen;
};

static inline struct sk_buff* skb_get(struct sk_buff* skb)
{
    skb->users++;
    return skb;
}

static inline void skb_queue_init(struct sk_buff_head* list)
{
    INIT_LIST_HEAD(&list->head);
    list->qlen = 0;
}

static inline int skb_queue_empty(struct sk_buff_head* list)
{
    return list_empty(&list->head);
}

static inline void skb_queue_tail(struct sk_buff_head* list,
                                  struct sk_buff* skb)
{
    list_add_tail(&skb->list, &list->head);
    list->qlen++;
}

static inline void skb_unlink(struct sk_buff* skb, struct sk_buff_head* head)
{
    list_del(&skb->list);
}

static inline struct sk_buff* skb_peek(struct sk_buff_head* list)
{
    if (list_empty(&list->head)) return NULL;
    return list_first_entry(&list->head, struct sk_buff, list);
}

static inline void skb_orphan(struct sk_buff* skb)
{
    if (skb->destructor) {
        skb->destructor(skb);
        skb->destructor = NULL;
        skb->sock = NULL;
    }
}

#endif
