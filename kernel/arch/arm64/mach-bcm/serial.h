/*  This file is part of Lyos.

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

#ifndef _MACH_BCM_SERIAL_H_
#define _MACH_BCM_SERIAL_H_

extern void* uart_base_addr;

/* UART registers */
#define UART01x_DR         0x00  /* Data read or written from the interface. */
#define UART01x_RSR        0x04  /* Receive status register (Read). */
#define UART01x_ECR        0x04  /* Error clear register (Write). */
#define UART010_LCRH       0x08  /* Line control register, high byte. */
#define ST_UART011_DMAWM   0x08  /* DMA watermark configure register. */
#define UART010_LCRM       0x0C  /* Line control register, middle byte. */
#define ST_UART011_TIMEOUT 0x0C  /* Timeout period register. */
#define UART010_LCRL       0x10  /* Line control register, low byte. */
#define UART010_CR         0x14  /* Control register. */
#define UART01x_FR         0x18  /* Flag register (Read only). */
#define UART010_IIR        0x1C  /* Interrupt identification register (Read). */
#define UART010_ICR        0x1C  /* Interrupt clear register (Write). */
#define ST_UART011_LCRH_RX 0x1C  /* Rx line control register. */
#define UART01x_ILPR       0x20  /* IrDA low power counter register. */
#define UART011_IBRD       0x24  /* Integer baud rate divisor register. */
#define UART011_FBRD       0x28  /* Fractional baud rate divisor register. */
#define UART011_LCRH       0x2c  /* Line control register. */
#define ST_UART011_LCRH_TX 0x2c  /* Tx Line control register. */
#define UART011_CR         0x30  /* Control register. */
#define UART011_IFLS       0x34  /* Interrupt fifo level select. */
#define UART011_IMSC       0x38  /* Interrupt mask. */
#define UART011_RIS        0x3c  /* Raw interrupt status. */
#define UART011_MIS        0x40  /* Masked interrupt status. */
#define UART011_ICR        0x44  /* Interrupt clear register. */
#define UART011_DMACR      0x48  /* DMA control register. */
#define ST_UART011_XFCR    0x50  /* XON/XOFF control register. */
#define ST_UART011_XON1    0x54  /* XON1 register. */
#define ST_UART011_XON2    0x58  /* XON2 register. */
#define ST_UART011_XOFF1   0x5C  /* XON1 register. */
#define ST_UART011_XOFF2   0x60  /* XON2 register. */
#define ST_UART011_ITCR    0x80  /* Integration test control register. */
#define ST_UART011_ITIP    0x84  /* Integration test input register. */
#define ST_UART011_ABCR    0x100 /* Autobaud control register. */
#define ST_UART011_ABIMSC  0x15C /* Autobaud interrupt mask/clear register. */

#define UART011_FR_RI   0x100
#define UART011_FR_TXFE 0x080
#define UART011_FR_RXFF 0x040
#define UART01x_FR_TXFF 0x020
#define UART01x_FR_RXFE 0x010
#define UART01x_FR_BUSY 0x008
#define UART01x_FR_DCD  0x004
#define UART01x_FR_DSR  0x002
#define UART01x_FR_CTS  0x001
#define UART01x_FR_TMSK (UART01x_FR_TXFF + UART01x_FR_BUSY)

#define UART011_CR_CTSEN      0x8000 /* CTS hardware flow control */
#define UART011_CR_RTSEN      0x4000 /* RTS hardware flow control */
#define UART011_CR_OUT2       0x2000 /* OUT2 */
#define UART011_CR_OUT1       0x1000 /* OUT1 */
#define UART011_CR_RTS        0x0800 /* RTS */
#define UART011_CR_DTR        0x0400 /* DTR */
#define UART011_CR_RXE        0x0200 /* receive enable */
#define UART011_CR_TXE        0x0100 /* transmit enable */
#define UART011_CR_LBE        0x0080 /* loopback enable */
#define UART010_CR_RTIE       0x0040
#define UART010_CR_TIE        0x0020
#define UART010_CR_RIE        0x0010
#define UART010_CR_MSIE       0x0008
#define ST_UART011_CR_OVSFACT 0x0008 /* Oversampling factor */
#define UART01x_CR_IIRLP      0x0004 /* SIR low power mode */
#define UART01x_CR_SIREN      0x0002 /* SIR enable */
#define UART01x_CR_UARTEN     0x0001 /* UART enable */

extern void uart_aux_disp_char(const char c);

#endif
