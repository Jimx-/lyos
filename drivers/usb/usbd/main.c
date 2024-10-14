#include <lyos/types.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/driver.h>
#include <errno.h>
#include <lyos/service.h>
#include <lyos/sysutils.h>
#include <lyos/vm.h>
#include <lyos/idr.h>
#include <sys/socket.h>
#include <lyos/timer.h>

#include "worker.h"
#include "proto.h"
#include "hcd.h"

#if CONFIG_OF
void* boot_params;
#endif

static int usbd_init(void)
{
    struct sysinfo* sysinfo;

    printl("usbd: USB daemon is running\n");

#if CONFIG_OF
    get_sysinfo(&sysinfo);
    boot_params = sysinfo->boot_params;
#endif

#if CONFIG_USB_PCI
    hcd_pci_init();
#endif

    return 0;
}

void usbd_process(MESSAGE* msg)
{
    int src = msg->source;

    if (msg->type == NOTIFY_MSG) {
        switch (src) {
        case INTERRUPT:
            usb_hcd_intr(msg->INTERRUPTS);
            break;
        }
    }
}

static void usbd_post_init(void)
{
#if CONFIG_USB_PCI
    hcd_pci_scan();
#endif

#if CONFIG_USB_DWC2
    dwc2_scan();
#endif
}

int main()
{
    serv_register_init_fresh_callback(usbd_init);
    serv_init();

    worker_main_thread(usbd_post_init);

    return 0;
}
