# Licensing

NU//METAL SKRATCHMASTER PRO, like Dinghy under it, is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The wiring `platform.c`, the host mode `tests/host.c`, `docs/` ([Dinghy](https://github.com/zvenson/dinghy)) | zvenson |
| The app `firmware/src/dinghy.c` (NU//METAL SKRATCHMASTER PRO), `README.md`, the logo, the changes to `tools/build.py` and `tests/run_host.sh`, `tests/run_pro.sh`, the web installer page `docs/index.html`, its package reader `docs/fwsc.js` and turntable shader `docs/deck.js`, the scratch track `docs/audio/scratch-techniques.mp3` (rendered by the firmware's host build) | Copyright (C) 2026 Mark Hellar |
| The Web MIDI update protocol `docs/fm1ota.js` (after `tools/fm1_install.py`) | Copyright (C) 2026 Leo Kuroshita, Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)) |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| "HEAD" 3D model by [Colin.Greenall](https://sketchfab.com/Colin.Greenall) ([Sketchfab](https://sketchfab.com/3d-models/head-d42e5aeb26934241aa398ada9fd1f38a)), geometry and a reduced colour map in `docs/head/` | CC-BY-4.0 | `docs/head/LICENSE.txt` |
| three.js r160, loaded by the installer page from jsDelivr | MIT | not in this repository |
| SAM (Software Automatic Mouth) in `firmware/src/sam/`: [ESP8266SAM](https://github.com/earlephilhower/ESP8266SAM) by Earle F. Philhower III, from Sebastian Macke's C port ([s-macke/SAM](https://github.com/s-macke/SAM)) of SAM (1982, Don't Ask Software); `sam_libc.c` and the `shim` headers are the FM-1 glue | GPL-3.0 (ESP8266SAM); the 1982 original is abandonware with no stated licence | `firmware/src/sam/` |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## The logo

`assets/logo.svg` / `logo.png` (a record cut by two slashes) are part of NU//METAL SKRATCHMASTER, under the GPL like
the rest. Dinghy's designburg signet is not used here.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE" and "FM-1" belong to their
owners. NU//METAL SKRATCHMASTER is not made or endorsed by them.
