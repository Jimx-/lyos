/*  
    (c)Copyright 2011 Jimx
    
    This file is part of Lyos.

    Lyos is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lyos is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lyos.  If not, see <http://www.gnu.org/licenses/>. */
    
#include <lyos/type.h>
#include <lyos/ipc.h>
#include "sys/types.h"
#include "stdio.h"
#include "unistd.h"
#include "lyos/config.h"
#include "lyos/const.h"
#include "string.h"
#include "lyos/proc.h"
#include "lyos/global.h"
#include "lyos/proto.h"
#include <lyos/service.h>

#include <pci.h>
#include "pci.h"

/* device table from Minix */
struct pci_device pci_device_table[] =
{
	{ 0x1000, 0x0001, "NCR 53C810" },
	{ 0x1000, 0x000F, "NCR 53C875" },
	{ 0x1002, 0x4752, "ATI Rage XL PCI" },
	{ 0x100B, 0xD001, "Nat. Semi. 87410" },
	{ 0x1013, 0x00B8, "Cirrus Logic GD 5446" },
	{ 0x1013, 0x6003, "Cirrus Logic CS4614/22/24 CrystalClear" },
	{ 0x1022, 0x1100, "K8 HyperTransport Tech. Conf." },
	{ 0x1022, 0x1101, "K8 [Athlon64/Opteron] Address Map" },
	{ 0x1022, 0x1102, "K8 [Athlon64/Opteron] DRAM Controller" },
	{ 0x1022, 0x1103, "K8 [Athlon64/Opteron] Misc. Control" },
	{ 0x1022, 0x2000, "AMD Lance/PCI" },
	{ 0x1022, 0x700C, "AMD-762 CPU to PCI Bridge (SMP chipset)" },
	{ 0x1022, 0x700D, "AMD-762 CPU to PCI Bridge (AGP 4x)" },
	{ 0x1022, 0x7410, "AMD-766 PCI to ISA/LPC Bridge" },
	{ 0x1022, 0x7411, "AMD-766 EIDE Controller" },
	{ 0x102B, 0x051B, "Matrox MGA 2164W [Millennium II]" },
	{ 0x102B, 0x0525, "Matrox MGA G400 AGP" },
	{ 0x1039, 0x0008, "SiS 85C503/5513" },
	{ 0x1039, 0x0200, "SiS 5597/5598 VGA" },
	{ 0x1039, 0x0406, "SiS 85C501/2" },
	{ 0x1039, 0x5597, "SiS 5582" },
	{ 0x104C, 0xAC1C, "TI PCI1225" },
	{ 0x105A, 0x0D30, "Promise Technology 20265" },
	{ 0x10B7, 0x9058, "3Com 3c905B-Combo" },
	{ 0x10B7, 0x9805, "3Com 3c980-TX Python-T" },
	{ 0x10B9, 0x1533, "ALI M1533 ISA-bridge [Aladdin IV]" },
	{ 0x10B9, 0x1541, "ALI M1541" },
	{ 0x10B9, 0x5229, "ALI M5229 (IDE)" },
	{ 0x10B9, 0x5243, "ALI M5243" },
	{ 0x10B9, 0x7101, "ALI M7101 PMU" },
	{ 0x10C8, 0x0005, "Neomagic NM2200 Magic Graph 256AV" },
	{ 0x10C8, 0x8005, "Neomagic NM2200 Magic Graph 256AV Audio" },
	{ 0x10DE, 0x0020, "nVidia Riva TnT [NV04]" },
	{ 0x10DE, 0x0110, "nVidia GeForce2 MX [NV11]" },
	{ 0x10EC, 0x8029, "Realtek RTL8029" },
	{ 0x10EC, 0x8129, "Realtek RTL8129" },
	{ 0x10EC, 0x8139, "Realtek RTL8139" },
	{ 0x10EC, 0x8167, "Realtek RTL8169/8110 Family Gigabit NIC" },
	{ 0x10EC, 0x8169, "Realtek RTL8169" },
	{ 0x1106, 0x0305, "VIA VT8363/8365 [KT133/KM133]" },
	{ 0x1106, 0x0571, "VIA IDE controller" },
	{ 0x1106, 0x0686, "VIA VT82C686 (Apollo South Bridge)" },
	{ 0x1106, 0x1204, "K8M800 Host Bridge" },
	{ 0x1106, 0x2204, "K8M800 Host Bridge" },
	{ 0x1106, 0x3038, "VT83C572 PCI USB Controller" },
	{ 0x1106, 0x3057, "VT82C686A ACPI Power Management Controller" },
	{ 0x1106, 0x3058, "VIA AC97 Audio Controller" },
	{ 0x1106, 0x3059, "VIA AC97 Audio Controller" },
	{ 0x1106, 0x3065, "VT6102 [Rhine-II]" },
	{ 0x1106, 0x3074, "VIA VT8233" },
	{ 0x1106, 0x3099, "VIA VT8367 [KT266]" },
	{ 0x1106, 0x3104, "VIA USB 2.0" },
	{ 0x1106, 0x3108, "VIA S3 Unichrome Pro VGA Adapter" },
	{ 0x1106, 0x3149, "VIA VT6420 SATA RAID Controller" },
	{ 0x1106, 0x3204, "K8M800 Host Bridge" },
	{ 0x1106, 0x3227, "VT8237 ISA bridge" },
	{ 0x1106, 0x4204, "K8M800 Host Bridge" },
	{ 0x1106, 0x8305, "VIA VT8365 [KM133 AGP]" },
	{ 0x1106, 0xB099, "VIA VT8367 [KT266 AGP]" },
	{ 0x1106, 0xB188, "VT8237 PCI bridge" },
	{ 0x110A, 0x0005, "Siemens Nixdorf Tulip Cntlr., Power Management" },
	{ 0x1186, 0x1300, "D-Link RTL8139" },
	{ 0x1186, 0x4300, "D-Link Gigabit adapter" },
	{ 0x1234, 0x1111, "Bochs VBE Extension" },
	{ 0x1259, 0xc107, "Allied Telesyn International Gigabit Ethernet Adapter" },
	{ 0x125D, 0x1969, "ESS ES1969 Solo-1 Audiodrive" },
	{ 0x1274, 0x1371, "Ensoniq ES1371 [AudioPCI-97]" },
	{ 0x1274, 0x5000, "Ensoniq ES1370" },
	{ 0x1274, 0x5880, "Ensoniq CT5880 [AudioPCI]" },
	{ 0x1385, 0x8169, "Netgear Gigabit Ethernet Adapter" },
	{ 0x16ec, 0x0116, "US Robotics Realtek 8169S chip" },
	{ 0x1737, 0x1032, "Linksys Instant Gigabit Desktop Network Interface" },
	{ 0x1969, 0x2048, "Atheros L2 Fast Ethernet Controller" },
	{ 0x5333, 0x8811, "S3 86c764/765 [Trio32/64/64V+]" },
	{ 0x5333, 0x883d, "S3 Virge/VX" },
	{ 0x5333, 0x88d0, "S3 Vision 964 vers 0" },
	{ 0x5333, 0x8a01, "S3 Virge/DX or /GX" },
	{ 0x8086, 0x1004, "Intel 82543GC Gigabit Ethernet Controller" },
	{ 0x8086, 0x100E, "Intel PRO/1000 MT Desktop Adapter" },
 	{ 0x8086, 0x1029, "Intel EtherExpressPro100 ID1029" },
 	{ 0x8086, 0x1030, "Intel Corporation 82559 InBusiness 10/100" },
	{ 0x8086, 0x1031, "Intel Corporation 82801CAM PRO/100 VE" },
	{ 0x8086, 0x1032, "Intel Corporation 82801CAM PRO/100 VE" },
 	{ 0x8086, 0x103d, "Intel Corporation 82801DB PRO/100 VE (MOB)" },
 	{ 0x8086, 0x1064, "Intel Corporation 82562 PRO/100 VE" },
	{ 0x8086, 0x107C, "Intel PRO/1000 GT Desktop Adapter" },
	{ 0x8086, 0x10CD, "Intel PRO/1000 Gigabit Network Connection" },
	{ 0x8086, 0x10D3, "Intel 82574L Gigabit Network Connection" },
	{ 0x8086, 0x105E, "Intel 82571EB Gigabit Ethernet Controller" },
 	{ 0x8086, 0x1209, "Intel EtherExpressPro100 82559ER" },
 	{ 0x8086, 0x1229, "Intel EtherExpressPro100 82557/8/9" },
	{ 0x8086, 0x122D, "Intel 82437FX" },
	{ 0x8086, 0x122E, "Intel 82371FB (PIIX)" },
	{ 0x8086, 0x1230, "Intel 82371FB (IDE)" },
	{ 0x8086, 0x1237, "Intel 82441FX (440FX)" },
	{ 0x8086, 0x1250, "Intel 82439HX" },
	{ 0x8086, 0x1A30, "Intel 82845B/A MCH" },
	{ 0x8086, 0x1A31, "Intel 82845B/A PCI Bridge to AGP port" },
	{ 0x8086, 0x2440, "Intel 82801B PCI to ISA bridge" },
 	{ 0x8086, 0x2449, "Intel EtherExpressPro100 82562EM" },
 	{ 0x8086, 0x244e, "Intel 82801 PCI Bridge" },
 	{ 0x8086, 0x2560, "Intel 82845G/GL[Brookdale-G]/GE/PE" },
 	{ 0x8086, 0x2561, "Intel 82845G/GL/GE/PE Host-to-AGP Bridge" },
	{ 0x8086, 0x7000, "Intel 82371SB" },
	{ 0x8086, 0x7010, "Intel 82371SB (IDE)" },
	{ 0x8086, 0x7020, "Intel 82371SB (USB)" },
 	{ 0x8086, 0x7030, "Intel 82437VX" },
 	{ 0x8086, 0x7100, "Intel 82371AB" }, 
	{ 0x8086, 0x7100, "Intel 82371AB" },
	{ 0x8086, 0x7110, "Intel 82371AB (PIIX4)" },
	{ 0x8086, 0x7111, "Intel 82371AB (IDE)" },
	{ 0x8086, 0x7112, "Intel 82371AB (USB)" },
	{ 0x8086, 0x7113, "Intel 82371AB (Power)" },
 	{ 0x8086, 0x7124, "Intel 82801AA" },
	{ 0x8086, 0x7190, "Intel 82443BX" },
	{ 0x8086, 0x7191, "Intel 82443BX (AGP bridge)" },
	{ 0x8086, 0x7192, "Intel 82443BX (Host-to-PCI bridge)" },
	{ 0x80ee, 0xcafe, "Oracle VirtualBox backdoor device" },
	{ 0x9004, 0x8178, "Adaptec AHA-2940U/2940UW Ultra/Ultra-Wide SCSI Ctrlr" },
	{ 0x9005, 0x0080, "Adaptec AIC-7892A Ultra160/m PCI SCSI Controller" },
	{ 0x0000, 0x0000, NULL }
};

PUBLIC char * pci_dev_name(int vendor, int device_id)
{
	int i;
	for (i = 0; pci_device_table[i].name; i++) {
		if ((vendor == pci_device_table[i].vendor) && 
			(device_id == pci_device_table[i].device_id)) {
			return pci_device_table[i].name;
		}
	}

	return NULL;
}
