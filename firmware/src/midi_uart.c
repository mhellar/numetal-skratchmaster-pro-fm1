/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MIDI IN on the TRS jack: PH8 -> input channel 1 -> UART1 RX, 31250 baud,
 * RX DMA into a 128-byte ring, polled from the TIMER5 ISR (no UART IRQ).
 * From Felucca 1.4.1 (GHOULBOX), cut down for this platform: messages go into midi_in_q next to USB, as
 * USB-MIDI packets (CIN = status >> 4), so midi_get() reads both ports the same; channel messages carry cable 1
 * so the app can tell the jack from USB (midi_trs).
 * Clock/Start/Continue/Stop pass; other realtime, system common and SysEx are ignored. */
#include "../hal/fm1_uart.h"

#define UM_RING 128u
static volatile uint8_t um_ring[UM_RING] __attribute__((aligned(16)));
static struct {
    uint32_t rd, pend;
    uint8_t st, need, got, d0, sysex;
} um;

static void uart_midi_init(void)                   /* before the timer starts: PORTH RMW */
{
    fm1_uart1_midi_init(um_ring, UM_RING);
}

static void um_put(uint32_t pkt)
{
    if (mi_w - mi_r < MQ) {
        midi_in_q[mi_w % MQ] = pkt;
        RING_PUBLISH();
        mi_w++;
    }
}

static void um_byte(uint32_t b)
{
    if (b >= 0xF8u) {
        if (b == 0xF8u || b == 0xFAu || b == 0xFBu || b == 0xFCu)
            um_put(0xFu | b << 8);
        return;                                    /* realtime keeps running status */
    }
    if (b & 0x80u) {
        um.sysex = b == 0xF0u;
        um.st = b < 0xF0u ? (uint8_t)b : 0;        /* system common/SysEx cancel running status */
        um.need = (b & 0xE0u) == 0xC0u ? 1u : 2u;  /* Cx Dx: 1 data byte, else 2 */
        um.got = 0;
        return;
    }
    if (um.sysex || !um.st)
        return;
    if (um.need == 2u && !um.got) {
        um.d0 = (uint8_t)b;
        um.got = 1;
        return;
    }
    {
        uint32_t d1 = um.need == 2u ? um.d0 : b, d2 = um.need == 2u ? b : 0u;
        um.got = 0;
        um_put(0x10u | (uint32_t)(um.st >> 4) | (uint32_t)um.st << 8 | d1 << 16 | d2 << 24);   /* cable 1: the jack */
    }
}

static void uart_midi_poll(void)                   /* TIMER5 ISR, same context as usb_poll */
{
    um.pend += fm1_uart1_rx_take();
    if (um.pend > UM_RING) {                       /* fell behind: drop the oldest, wait for a new status */
        um.rd = (um.rd + um.pend - UM_RING) & (UM_RING - 1u);
        um.pend = UM_RING;
        um.st = um.got = um.sysex = 0;
    }
    while (um.pend) {
        um_byte(um_ring[um.rd]);
        um.rd = (um.rd + 1u) & (UM_RING - 1u);
        um.pend--;
    }
}
