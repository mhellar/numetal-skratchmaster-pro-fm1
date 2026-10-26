#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# NU//METAL PRO on this computer: build/host/techniques.wav + tone_lfo.wav, and a frame every 1/2 s of each as PNG.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host/pro
cc -O2 -Wall -Wno-unused-function -DDINGHY_HOST -DAPP_VER="\"${APP_VER:-dev}\"" -Ifirmware/src -Ifirmware/gen -Ifirmware/src/sam/shim -o build/host/dinghy \
    firmware/src/dinghy.c firmware/src/sam/sam.c firmware/src/sam/render.c firmware/src/sam/reciter.c
rm -f build/host/pro/*.ppm build/host/pro/*.png
build/host/dinghy build/host/pro/techniques tests/pro_techniques.txt 2 | grep -v screen
build/host/dinghy build/host/pro/tone_lfo tests/pro_tone_lfo.txt 4 | grep -v screen
build/host/dinghy build/host/pro/mixer tests/pro_mixer.txt 4 | grep -v screen
python3 - <<'PY'
from pathlib import Path
from PIL import Image
for p in sorted(Path("build/host/pro").glob("*.ppm")):
    Image.open(p).save(p.with_suffix(".png"))
    p.unlink()
PY
