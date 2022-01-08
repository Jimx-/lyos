#include <lyos/list.h>
#include <netinet/in.h>

#include "inet.h"
#include "ifdevice.h"

static DEF_LIST(ifdev_list);

void ifdev_add(struct if_device* ifdev, const char* name, unsigned int mtu)
{
    strlcpy(ifdev->name, name, sizeof(ifdev->name));

    ifdev->mtu = mtu;

    list_add(&ifdev->list, &ifdev_list);
}

 