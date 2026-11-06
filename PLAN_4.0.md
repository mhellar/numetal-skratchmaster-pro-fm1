# SKRATCH 4.0: the streamline plan (11-05)

How Mark plays (from the T-8 jam videos): left hand on SELECT (the jog), right hand taps the keys,
ALGORITHM switches the techniques to mix it up. A Roland T-8 is the band, over MIDI (clock) and audio.
Everything else is extra. Goal: jog, tap, techniques, a few FX, locked to the T-8.

## Keep (and make better)
- SELECT = the jog, hand on the record, the deep SURF feel everywhere.
- Keys = 27 spots. Tap = needle drop, tap again fast = strokes, hold = the technique.
- ALGORITHM = ALL the techniques (17 + COMBO), the name big on screen; a turn while a key is held
  switches on the next beat, so it stays in time with the T-8.
- SURF / the INFINITE record and its WORLDs, plus the word record (AHHH, FRESH, the words).
- MIDI SYNC FOLLOW on by default: the T-8's clock sets the BPM and the grid.
- TOYBOX as a practice beat: PLAY toggles it, off by default, the T-8's MIDI Start does NOT turn it on.
  Auto toys (they swap every 8 bars), the mutating 808.
- AUTO (the robot scratches), BACKSPIN / BRAKE, pitch (OCT-/+), FREEZE, CUT (32nd transformer).

## New: the 4 right knobs = live FX, no picking, no EDIT
- KNOB1 FILTER (left low-pass, right high-pass, middle off), KNOB2 ECHO (synced throw),
  KNOB3 CRUSH, KNOB4 STUTTER / GATE (synced). FX button held = all FX off (dry snap).
- Small bars for the 4 FX along the bottom of the SURF screen.

## Cut
- RAP MODE, GROWL, the rhyme pairs and the hook logic.
- BEAT mode, TONE mode, the kit pads.
- The step sequencer, REC, MUTATE, FILL, DROP, SWING.
- The LFO PRO page. The mixer page. TAP tempo, BPM knob (clock or 120).
- The 9-FX list (MEGAPHONE, DEMON, CRUNCH, REVERSE, WAH, VELOCITY FX) -> the 4 knobs.
- Toy picker (PRESETS = the record / WORLD now).
- The EDIT layers: what is left goes to one SETTINGS page behind HOME:
  TORQUE, CURVE, HAMSTER, SAM VOICE, REACH, levels (deck, 808, toys, verb), sync.

## Improve the core
1. Jog: resolution and the slow-drag dead zone, so a slow drag is smooth.
2. Taps: strokes start on the sound's attack.
3. FX: zipper-free resonant filter, echo / stutter locked to the clock.
4. Screen: one main screen (SURF landscape + technique name + FX bars).

## Steps (flash after each, jam with the T-8)
1. Branch `skratch40` off 3.1 (9229aff). Delete the cut features. No behavior change to jog/taps/techniques.
2. New control map: knob FX, SETTINGS page, PLAY = practice beat, sync default.
3. Feel: jog, taps, technique switch on the beat, FX quality, screen.
4. Release 4.0 + one-page manual + a demo video; 3.1 stays online.
