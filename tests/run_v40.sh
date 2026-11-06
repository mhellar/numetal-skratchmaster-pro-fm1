#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# 4.0 SKRATCH on this computer: a T-8 clocking 120 on TRS while the deck is played (taps, jog, held techniques, the knob
# FX, DRY, strokes, the practice beat, WORDS, CUT, SETTINGS, BACKSPIN, BRAKE), the practice beat alone (with taps, then bare), SURF's deep cuts
# -> build/host/v40/*.wav
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host/v40
cc -O2 -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER="\"${APP_VER:-dev}\"" -Ifirmware/src -Ifirmware/gen -Ifirmware/src/sam/shim -o build/host/dinghy \
    firmware/src/dinghy.c firmware/src/sam/sam.c firmware/src/sam/render.c firmware/src/sam/reciter.c
rm -f build/host/v40/*
for t in s40_jam s40_beat s40_drums surf_deep; do printf '%-12s ' $t; build/host/dinghy build/host/v40/$t tests/$t.txt | grep audio; done
