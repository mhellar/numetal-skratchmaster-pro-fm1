#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# 3.1 on this computer: TOYBOX (64 bars of the mutating 808 beat, then POOFS and TOY PIANO), the band alone, and SURF's
# deep cuts (the classic STAB / CUT / AHHH spots held, no drums) -> build/host/v31/*.wav
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host/v31
cc -O2 -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER="\"${APP_VER:-dev}\"" -Ifirmware/src -Ifirmware/gen -Ifirmware/src/sam/shim -o build/host/dinghy \
    firmware/src/dinghy.c firmware/src/sam/sam.c firmware/src/sam/render.c firmware/src/sam/reciter.c
rm -f build/host/v31/*
for t in toybox toybox_band surf_deep; do printf '%-12s ' $t; build/host/dinghy build/host/v31/$t tests/$t.txt | grep audio; done
