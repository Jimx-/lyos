#ifndef _PCI_UTILS_H_
#define _PCI_UTILS_H_

#include <libdevman/libdevman.h>

PUBLIC int pci_first_dev(int* devind, u16* vid, u16* did, device_id_t* dev_id);
PUBLIC int pci_next_dev(int* devind, u16* vid, u16* did, device_id_t* dev_id);
PUBLIC u8 pci_attr_r8(int devind, u16 port);
PUBLIC u16 pci_attr_r16(int devind, u16 port);
PUBLIC u32 pci_attr_r32(int devind, u16 port);
PUBLIC int pci_attr_w16(int devind, u16 port, u16 value);

#endif
