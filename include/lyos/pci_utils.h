#ifndef _PCI_UTILS_H_
#define _PCI_UTILS_H_

#include <libdevman/libdevman.h>

int pci_sendrec(int function, MESSAGE* msg);

int pci_first_dev(int* devind, u16* vid, u16* did, device_id_t* dev_id);
int pci_next_dev(int* devind, u16* vid, u16* did, device_id_t* dev_id);
u8 pci_attr_r8(int devind, u16 port);
u16 pci_attr_r16(int devind, u16 port);
u32 pci_attr_r32(int devind, u16 port);
int pci_attr_w16(int devind, u16 port, u16 value);
int pci_get_bar(int devind, u16 port, u32* base, u32* size, int* ioflag);
int pci_find_capability(int devind, int cap);
int pci_find_next_capability(int devind, int pos, int cap);

#endif
