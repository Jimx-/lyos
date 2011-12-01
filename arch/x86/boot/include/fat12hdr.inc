;    This file is part of Lyos.
;
;    Lyos is free software: you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation, either version 3 of the License, or
;    (at your option) any later version.

;    Lyos is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with Lyos.  If not, see <http://www.gnu.org/licenses/>.

; Head
; ----------------------------------------------------------------------
BS_OEMName	DB 'Jimx    '	; OEM String, must be 8 bytes

BPB_BytsPerSec	DW 512		; bytes per sector
BPB_SecPerClus	DB 1		; sector per cluster
BPB_RsvdSecCnt	DW 1		; reserved sector for boot record
BPB_NumFATs	DB 2		; number of FATs
BPB_RootEntCnt	DW 224		; root entry count
BPB_TotSec16	DW 2880		; number of logical sectors
BPB_Media	DB 0xF0		; media descriptor
BPB_FATSz16	DW 9		; sectors per FAT
BPB_SecPerTrk	DW 18		; sector per track
BPB_NumHeads	DW 2		; number of heads
BPB_HiddSec	DD 0		; number of hidden sector
BPB_TotSec32	DD 0		; number of sector if wTotalSectorCount = 0 

BS_DrvNum	DB 0		; driver number for int 13
BS_Reserved1	DB 0		; reserved
BS_BootSig	DB 29h		; boot sign
BS_VolID	DD 0		; volume ID
BS_VolLab	DB 'Lyos0.2.6.3'; volume label, must be 11 bytes
BS_FileSysType	DB 'FAT12   '	; file system type, must be 8 bytes
;------------------------------------------------------------------------


; -------------------------------------------------------------------------
; constant, these defines will change when head information have been changed.
; -------------------------------------------------------------------------
FATSz			equ	9	; BPB_FATSz16
RootDirSectors		equ	14	; Root Directory sector: RootDirSectors = ((BPB_RootEntCnt * 32) + (BPB_BytsPerSec – 1)) / BPB_BytsPerSec;
SectorNoOfRootDirectory	equ	19	; number of Root Directory first sector	= BPB_RsvdSecCnt + (BPB_NumFATs * FATSz)
SectorNoOfFAT1		equ	1	; number of the first sector of FAT1	= BPB_RsvdSecCnt
DeltaSectorNo		equ	17	; DeltaSectorNo = BPB_RsvdSecCnt + (BPB_NumFATs * FATSz) - 2
					; number of File Start Sector = number of DirEntry Start Sector + number of Root Directory Sector + DeltaSectorNo
