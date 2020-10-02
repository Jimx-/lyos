#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "libsockdriver.h"

int skb_alloc(struct sock* sock, size_t data_len, struct sk_buff** skbp)
{
    struct sk_buff* skb;

    skb = malloc(sizeof(struct sk_buff) + data_len);
    if (!skb) return ENOMEM;

    memset(skb, 0, sizeof(struct sk_buff));
    skb->sock = sock;
    skb->data_len = data_len;
    skb->users = 1;

    *skbp = skb;
    return 0;
}

int skb_unref(struct sk_buff* skb)
{
    if (!skb) return FALSE;
    if (skb->users > 1) {
        skb->users--;
        return FALSE;
    }

    return TRUE;
}

void skb_free(struct sk_buff* skb)
{
    if (skb_unref(skb)) free(skb);
}

int skb_copy_from(struct sk_buff* skb, size_t off,
                  const struct sockdriver_data* data, size_t data_off,
                  size_t len)
{
    return sockdriver_copyin(data, data_off, &skb->data[off], len);
}

int skb_copy_to(struct sk_buff* skb, size_t off,
                const struct sockdriver_data* data, size_t data_off, size_t len)
{
    return sockdriver_copyout(data, data_off, &skb->data[off], len);
}

int skb_copy_from_iter(struct sk_buff* skb, size_t off,
                       struct iov_grant_iter* iter, size_t len)
{
    if (iov_grant_iter_copy_from(iter, &skb->data[off], len) != len)
        return EFAULT;
    return 0;
}

int skb_copy_to_iter(struct sk_buff* skb, size_t off,
                     struct iov_grant_iter* iter, size_t len)
{
    if (iov_grant_iter_copy_to(iter, &skb->data[off], len) != len)
        return EFAULT;
    return 0;
}
