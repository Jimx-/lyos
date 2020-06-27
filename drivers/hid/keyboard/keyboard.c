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

#include <lyos/type.h>
#include <lyos/ipc.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <lyos/const.h>
#include <string.h>
#include <lyos/fs.h>
#include <lyos/proc.h>
#include <lyos/global.h>
#include <lyos/proto.h>
#include <lyos/interrupt.h>
#include <lyos/portio.h>
#include <lyos/service.h>
#include <lyos/input.h>
#include <libinputdriver/libinputdriver.h>

#include "keyboard.h"

#define KEYMAP_SIZE 512

static const unsigned short set2_keycode[KEYMAP_SIZE] = {
    0,   67,  65,  63,  61,  59,  60,  88,  0,   68,  66,  64,  62,  15,  41,
    117, 0,   56,  42,  93,  29,  16,  2,   0,   0,   0,   44,  31,  30,  17,
    3,   0,   0,   46,  45,  32,  18,  5,   4,   95,  0,   57,  47,  33,  20,
    19,  6,   183, 0,   49,  48,  35,  34,  21,  7,   184, 0,   0,   50,  36,
    22,  8,   9,   185, 0,   51,  37,  23,  24,  11,  10,  0,   0,   52,  53,
    38,  39,  25,  12,  0,   0,   89,  40,  0,   26,  13,  0,   0,   58,  54,
    28,  27,  0,   43,  0,   85,  0,   86,  91,  90,  92,  0,   14,  94,  0,
    79,  124, 75,  71,  121, 0,   0,   82,  83,  80,  76,  77,  72,  1,   69,
    87,  78,  81,  74,  55,  73,  70,  99,

    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   217, 100, 255, 0,   97,  165, 0,   0,   156, 0,   0,   0,   0,   0,
    0,   125, 173, 114, 0,   113, 0,   0,   0,   126, 128, 0,   0,   140, 0,
    0,   0,   127, 159, 0,   115, 0,   164, 0,   0,   116, 158, 0,   172, 166,
    0,   0,   0,   142, 157, 0,   0,   0,   0,   0,   0,   0,   155, 0,   98,
    0,   0,   163, 0,   0,   226, 0,   0,   0,   0,   0,   0,   0,   0,   255,
    96,  0,   0,   0,   143, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    107, 0,   105, 102, 0,   0,   112, 110, 111, 108, 112, 106, 103, 0,   119,
    0,   118, 109, 0,   99,  104, 119, 0,   0,   0,   0,   65,  99,
};

static const unsigned short set1_scancode[128] = {
    0,   118, 22,  30,  38,  37,  46,  54,  61,  62,  70,  69,  78,  85,  102,
    13,  21,  29,  36,  45,  44,  53,  60,  67,  68,  77,  84,  91,  90,  20,
    28,  27,  35,  43,  52,  51,  59,  66,  75,  76,  82,  14,  18,  93,  26,
    34,  33,  42,  50,  49,  58,  65,  73,  74,  89,  124, 17,  41,  88,  5,
    6,   4,   12,  3,   11,  2,   10,  1,   9,   119, 126, 108, 117, 125, 123,
    107, 115, 116, 121, 105, 114, 122, 112, 113, 127, 96,  97,  120, 7,   15,
    23,  31,  39,  47,  55,  63,  71,  79,  86,  94,  8,   16,  24,  32,  40,
    48,  56,  64,  72,  80,  87,  111, 19,  25,  57,  81,  83,  92,  95,  98,
    99,  100, 101, 103, 104, 106, 109, 110};

#define KEYCODE_NULL 255

irq_id_t kb_irq_set;
static int kb_hook_id;

static unsigned short keycode_table[KEYMAP_SIZE];

static int emul = 0;
static int release = FALSE;

static int init_keyboard();
static void keyboard_interrupt(unsigned long irq_set);
static void set_leds();
static void kb_wait();
static void kb_ack();

static struct inputdriver keyboard_driver = {
    .input_interrupt = keyboard_interrupt,
};

/*****************************************************************************
 *                               main
 *****************************************************************************/
/**
 * The main loop.
 *
 *****************************************************************************/
int main()
{
    serv_register_init_fresh_callback(init_keyboard);
    serv_init();

    return inputdriver_start(&keyboard_driver);
}

/*****************************************************************************
 *                                keyboard_handler
 *****************************************************************************/
/**
 * <Ring 0> Handles the interrupts generated by the keyboard controller.
 *
 * @param irq The IRQ corresponding to the keyboard, unused here.
 *****************************************************************************/
static void keyboard_interrupt(unsigned long irq_set)
{
    u8 scancode;
    u16 keycode;
    s32 value;

    portio_inb(KB_DATA, &scancode);

    inputdriver_send_event(EV_MSC, MSC_RAW, scancode);

    /* extract the release bit from scancode */
    if (emul || (scancode != 0xE0 && scancode != 0xE1)) {
        release = scancode >> 7;
        scancode &= 0x7f;
    }

    switch (scancode) {
    case 0xE0:
        emul = 1;
        return;
    }

    scancode = (scancode & 0x7f) | ((scancode & 0x80) << 1);
    if (emul == 1) scancode |= 0x80;

    if (emul && --emul) {
        return;
    }

    keycode = keycode_table[scancode];

    if (keycode != KEYCODE_NULL) {
        inputdriver_send_event(EV_MSC, MSC_SCAN, scancode);
    }

    switch (keycode) {
    case KEYCODE_NULL:
        break;
    default:
        if (release) {
            value = 0;
        } else {
            value = 1;
        }

        inputdriver_send_event(EV_KEY, keycode, value);
        break;
    }

    release = FALSE;
}

/*****************************************************************************
 *                                init_keyboard
 *****************************************************************************/
/**
 * <Ring 1> Initialize some variables and set keyboard interrupt handler.
 *
 *****************************************************************************/
static int init_keyboard()
{
    int i;
    unsigned short scancode;

    /* init keycode table */
    for (i = 0; i < 128; i++) {
        scancode = set1_scancode[i];
        keycode_table[i] = set2_keycode[scancode];
        keycode_table[i | 0x80] = set2_keycode[scancode | 0x80];
    }

    set_leds();

    kb_irq_set = 1 << KEYBOARD_IRQ;
    kb_hook_id = KEYBOARD_IRQ;

    irq_setpolicy(KEYBOARD_IRQ, IRQ_REENABLE, &kb_hook_id);
    irq_enable(&kb_hook_id);

    return 0;
}

/*****************************************************************************
 *                                kb_wait
 *****************************************************************************/
/**
 * Wait until the input buffer of 8042 is empty.
 *
 *****************************************************************************/
static void kb_wait()
{
    u8 kb_stat;

    do {
        portio_inb(KB_CMD, &kb_stat);
    } while (kb_stat & 0x02);
}

/*****************************************************************************
 *                                kb_ack
 *****************************************************************************/
/**
 * Read from the keyboard controller until a KB_ACK is received.
 *
 *****************************************************************************/
static void kb_ack()
{
    u8 kb_read;

    do {
        portio_inb(KB_DATA, &kb_read);
    } while (kb_read != KB_ACK);
}

/*****************************************************************************
 *                                set_leds
 *****************************************************************************/
/**
 * Set the leds according to: caps_lock, num_lock & scroll_lock.
 *
 *****************************************************************************/
static void set_leds()
{
    u8 leds = 0;
    // u8 leds = (caps_lock << 2) | (num_lock << 1) | scroll_lock;

    kb_wait();
    portio_outb(KB_DATA, LED_CODE);
    kb_ack();

    kb_wait();
    portio_outb(KB_DATA, leds);
    kb_ack();
}
