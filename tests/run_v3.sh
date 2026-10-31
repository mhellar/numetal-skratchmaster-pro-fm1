#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# NU//METAL 3.0 on this computer: SURF (the INFINITE record) and the korn guitar -> build/host/v3/*.wav + frames,
# then the 2.x scenes again (no frames) to hear nothing else moved.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host/v3
cc -O2 -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER="\"${APP_VER:-dev}\"" -Ifirmware/src -Ifirmware/gen -Ifirmware/src/sam/shim -o build/host/dinghy \
    firmware/src/dinghy.c firmware/src/sam/sam.c firmware/src/sam/render.c firmware/src/sam/reciter.c
rm -f build/host/v3/*
for t in surf korn; do printf '%-15s ' $t; build/host/dinghy build/host/v3/$t tests/$t.txt 2 | grep audio; done
for t in pro_techniques pro_kit td8_drums pro_mixer numetal_rap; do printf '%-15s ' $t; build/host/dinghy build/host/v3/$t tests/$t.txt | grep audio; done
