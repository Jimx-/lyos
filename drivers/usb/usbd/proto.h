#ifndef _USBD_PROTO_H_
#define _USBD_PROTO_H_

void usbd_process(MESSAGE* msg);

#if CONFIG_USB_DWC2
void dwc2_scan(void);
#endif

#endif
