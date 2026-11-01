<p align="center"><img src="assets/logo.svg" width="96" alt="NU//METAL SKRATCHMASTER PRO"></p>

# NU//METAL SKRATCHMASTER PRO

**A robot turntablist with a nu metal band, for the M-VAVE FM-1. Version 3.0: INFINITE SKRATCH.**

The FM-1 becomes a deck. SAM (the 1982 Software Automatic Mouth) says 27 words onto a virtual record, and you scratch
them with the keys and the jog wheel. Or surf an endless record that is generated as you scratch it (SURF mode, new
in 3.0). Behind you a drop-tuned band plays: double-tracked 7-string guitars, a bass and a 12-piece kit locked to
twelve grooves. In **RAP MODE** the robot is the MC: four bars of verse, where it raps rhyming
lines that fit the beat, then four bars of hook, where the guitars come in and it shouts the hook in a Melvins-style
**GROWL**.

PRO adds what a turntablist expects from a real setup:

- **A real LFO**, locked to the tempo (and to MIDI clock), with 7 shapes and 6 destinations, the deck's pitch and the
  record itself among them
- **17 scratch techniques**, each one scored the way TTM notation writes a scratch (the record's motion plus the
  crossfader's clicks), and **COMBO** routines that string them together
- **HAMSTER** switch, crossfader **CURVE**, turntable motor **TORQUE**
- **TONE PLAY**: the keys become a pitch fader, for melodies on a tone, on AHHH or on any word
- **AHHH** and **FRESH**, the two classic battle-record sounds
- **A bigger sound engine**: a vocal chain, double-tracked guitars, a stereo plate reverb, a glue compressor and a
  soft limiter
- **A MIXER page** with faders and live meters
- **An 808 kit and a drop-B guitar as real samples** (new in 2.1), rendered offline in floating point
- **MIDI for drummers and visuals** (new in 2.1): play the kit from an e-kit on the TRS input, and every drum and the
  deck itself come out on USB-MIDI

<p align="center">
<img src="docs/img/scratch-score.png" width="240" alt="Scratching TRANSFORM: the technique's score on screen with a playhead">
<img src="docs/img/pro-page.png" width="240" alt="The PRO page: the LFO, square at 1/8 on the FADER">
<img src="docs/img/mixer.png" width="240" alt="The MIXER page: seven faders and meters">
</p>

<p align="center"><img src="docs/scratch.gif" width="480" alt="Scratching BREAK IT DOWN on the FM-1"></p>

> **Status: 3.0, runs on the FM-1.** Identity `FM-1_926`, the same as NU//METAL SKRATCHMASTER 1.0: it installs over it.
> Installing firmware is at your own risk; M-VAVE's official firmware can be reinstalled at any time.

## Install

- **In the browser:** open the **[web installer](https://mhellar.github.io/numetal-skratchmaster-pro-fm1/)** in Chrome
  or Edge on a computer, plug the FM-1 in with a USB data cable, press **Install**. It uses Web MIDI, and nothing
  else needs to be installed.
- **From the command line:** download `numetal-pro-3.0.fwsc` from [Releases](https://github.com/mhellar/numetal-skratchmaster-pro-fm1/releases/latest) (or [docs/firmware](docs/firmware)), then
  `python3 tools/fm1_install.py numetal-pro-3.0.fwsc`
  (needs `pip3 install mido python-rtmidi`).
- **Rescue:** hold **OCT-** while switching on. No sound, only the updater. Install anything from there.

Coming from 1.0, the first start sets everything to defaults (the saved settings grew in 2.0). 2.1 keeps 2.0's
settings. 3.0 keeps 2.x settings and sets the tuning to A once.

### What's new in 3.0: INFINITE SKRATCH

**SURF mode** (press HOME until the chip says SURF) puts an endless record on the deck. It is never stored. Every
stretch is computed from where it is on the record and from the WORLD, so scratching back over a spot plays the same
sound again, just like vinyl. The record is cut into zones of 0.74 s. Each one is a sample on a battle record with an
attack, a body and a gap of crackle, and each is made in one of seven ways:

| Material | What it is |
| --- | --- |
| **AHHH** | a vowel built from FOF grains (a pulse every pitch period, three formants ringing off it), gliding, morphing a to o |
| **CHIRP** | a tone sweeping octaves, or a LASER where the sweep repeats; sine, fat square, saw or sine + octave |
| **FM** | two operators, the index decaying, rising or wobbling: bells, screeches |
| **BYTE** | a bytebeat formula (`t*(t>>a\|t>>b)` and friends) read at the record's speed: 8-bit shrapnel |
| **DUST** | the SAM words shattered into grains, re-pitched, some backwards, two streams overlapped |
| **CHAOS** | the logistic map with r swept through its bifurcations: a tone splits into two, four, then noise |
| **HISS** | noise through a resonant filter that sweeps: the record's whistle and its *kssh* |

- **Keys** are 27 spots on the record. Tap one to play its zone, hold it to scratch there in the STYLE, and tap
  again quickly for finger strokes.
- **SURF** (EDIT + KNOB4, 0..15): a held scratch drifts along the record while it loops (0 = it stays on the spot).
- **FLOW** (EDIT + KNOB2 in SURF mode, 0..15, default 12): 0 = a battle record, separate sounds with crackly gaps;
  10 and up = no gaps, the sounds sustain and each zone crossfades into the next (up to 174 ms), so the record is one
  continuous landscape.
- **WORLD** (KNOB4, 0..99): a different record. Each world leans on three of the materials. Turning it while a
  sound plays morphs the sound under the needle.
- **The jog** surfs anywhere, far past the keys. Flick it and let go, and the record coasts through zone after zone.
- **AUTO in SURF mode = ETERNAL SKRATCH:** the robot rides the record by itself. It plays on, scratches where it is,
  and skips a wave or two, forever, never the same.
- The screen shows the record as a landscape scrolling under the needle: each zone is a mountain in its material's
  colour, the 27 key spots are marked in yellow, and a little surfer rides the needle. The big line names the
  material and the zone (`CHAOS 12`).
- REC records SURF spots into the steps like words.

**The jog** is more sensitive: about 2.5x further a click, 2.5x more again in SURF mode, a snappier hand and easier
throws.

**A harder band:** the guitar is a 7-string in **drop A** now (the default tuning; KNOB1 with EDIT still goes from
F to F). The amp is hotter, with three stages, the last one lopsided like a rectifier, a deep mid scoop, a thump at
100 Hz and a snarl at 3 kHz. Chugs are root + octave and slam shut. New grooves:

- **SEASICK:** the second guitar plays the open chords a half step sharp and slides down onto the first. Dissonant
  and woozy, and the bass clanks off the frets.
- **DIVEBOMB:** whammy dives (an octave down in a quarter second) and a **pinch-harmonic squeal** with finger vibrato.
- **BRUTAL TRUTH** (a grind blast, a chromatic tremolo riff) and **BAD DAY** (half time, seasick) replace BOUNCE and
  B-BOY.

Riffs can use the new moves in any groove: `a`..`k` = an open chord with a dive, `!` = the squeal.

**MIDI clock over TRS:** the FM-1 follows MIDI clock (and Start / Stop) on the TRS input as well as on USB, with SYNC
set to FOLLOW.

### What's new in 2.1

- **TRS MIDI IN** works: plug an e-kit (the map is set up for a Roland TD-8) or any MIDI gear into the FM-1's TRS
  jack. Channel 10 uses the General MIDI drum map, and hits that land together (kick + crash) play together.
- **MIDI out for visuals:** every drum the FM-1 plays (the band, the pads, the e-kit) goes out on USB-MIDI as a GM note
  on channel 10, and the deck sends CC 20 (scratching 0/127), CC 21 (crossfader) and CC 22 (platter angle) on channel 1.
- **The 808 kit:** kick, snare, clap, toms and a long saturated 808 bass, with the hats' and cymbals' aliasing fizz
  filtered out.
- **The guitar** is a drop-B take (plucked strings into an oversampled two-stage amp, tone stack and 4x12 cab),
  double-tracked left and right. The kit and the guitar are 16-bit samples played at TUNE pitch; DECAY 0/1 shortens
  them.
- The audio load stays about 20 % at peak with the kit hammered.

## Play

Press **PLAY**, then press **ARP** for RAP MODE. Tap a key: the robot says the word. Hold it: the word is scratched
in time, in the STYLE. Turn **SELECT** to scratch by hand; flick it and let go, and the record spins on.

Three buttons open a page while you hold them, and the knobs change meaning while they are held: **EDIT** (the
second layer, and the audio load), **LFO** (the PRO page) and **FX** (the MIXER). A quick tap still does what the
button always did.

| Key / button | Tap / hold | With **EDIT** held |
| --- | --- | --- |
| **Keys** | DJ mode: 27 words (25 = AHHH, 26 = FRESH); tap = say it, hold = a tempo-synced scratch in the STYLE (in RAP MODE the last word you tap becomes the hook). BEAT mode: keys 1-12 = the kit (hold = roll), 13-27 = the first 15 words. TONE mode: the keys are a pitch fader (see TONE PLAY). SURF mode: 27 spots on the INFINITE record (see 3.0 above) | |
| **PLAY** | start / stop | TAP tempo |
| **REC** | record into the 16 steps (quantized, see [the sequencer](#the-sequencer-record-your-own-loop)) | clear the words, keep the drums |
| **SEQ** | hold = FILL (snare roll, chugs, toms) | clear the steps |
| **ARP** | RAP MODE: verse / hook, the robot MCs | MUTATE the steps |
| **GLO** | AUTO: the robot scratches by itself (SURF mode: ETERNAL SKRATCH) | HARD: drums driven, the kick ducks the deck |
| **FX** | tap = next FX, hold = the MIXER | previous FX |
| **SEL** | hold = DROP (only the hats, then slam back in) | HAMSTER on / off |
| **ENV** | hold = FREEZE (loop a sliver) | latch FREEZE |
| **LFO** | tap = LFO on / off, hold = the PRO page | hold = CUT (the crossfader on 32nds) |
| **HOME** | DJ / BEAT / TONE / SURF | pitch back to 0 |
| **OCT- / OCT+** | the deck's pitch, a semitone | BACKSPIN / BRAKE |
| **SAVE** | save everything now | SYNC: FOLLOW / OFF |

| Encoder | Turn | **EDIT** held | **LFO** held: the PRO page | **FX** held: the MIXER |
| --- | --- | --- | --- | --- |
| **SELECT** | JOG: scratch by hand (velocity) | VELOCITY FX: the flick pumps the FX | SPACE (the plate) | VERB |
| **ALGORITHM** | STYLE (TONE mode: SOURCE) | SWING | fader CURVE | ECHO |
| **KNOB1** | the FX's amount | TUNING: drop F .. drop F (A = the 7-string, the default) | LFO RATE | VOX |
| **KNOB2** | GUITAR level | ECHO (SURF mode: FLOW) | LFO SHAPE | GUITAR |
| **KNOB3** | BPM 60..180 | KIT DECAY | LFO DEPTH | BASS |
| **KNOB4** | SAM VOICE: MC GROWL ROBOT SAM ELF E.T. STUFFY OLD LADY (SURF mode: WORLD 0..99) | DRUMS level (SURF mode: SURF drift) | LFO DEST | DRUMS |
| **PRESETS** | GROOVE: OFF, BRUTAL TRUTH, BAD DAY, JUMP, BREAKDOWN, RAP METAL, BOOM BAP, STOMP, TRAP, SLUDGE, SEASICK, DIVEBOMB | REACH | motor TORQUE | WIDTH |

**FX:** FILTER, CRUSH, MEGAPHONE, DEMON, CRUNCH, STUTTER, REVERSE, WAH, GROWL.

### The sequencer: record your own loop

The sequencer is one bar of 16 steps (16th notes) that loops under the groove. It is the row of 16 squares across
the middle of the screen: a **cyan** square says a word on that step, a **yellow** one scratches it, a **pink** dot
adds drum hits, and the bright mark is the playhead. With SWING up, the odd steps lean late like the band.

**Record a loop**

1. Press **PLAY** to start the band. The sequencer only records while it's playing.
2. Press **REC**. The REC chip, its light and a red dot in the top bar come on.
3. Play over the beat. Hits snap to the nearest step, and a hit just before a step lands on that step without
   playing twice.
   - **Tap a key:** the word (or in SURF mode, the spot) goes on that step.
   - **Hold a key:** the step becomes a scratch in the current STYLE, on the step where you pressed.
   - **BEAT mode:** keys 1-12 stack drum hits onto the step, on top of the groove's own pattern. Keys 13-27 record
     words.
4. Recording stays on and the loop keeps going round, so you can overdub as you go. A step holds one word (a new
   one replaces the old) and any number of drums.
5. Press **REC** again to stop recording. The loop keeps playing.

TONE mode isn't recorded. In RAP MODE a recorded word takes over the MC's line on that step.

**Clear and change it (hold EDIT, the shift)**

| Hold **EDIT** + | Does |
| --- | --- |
| **SEQ** | CLEARED: wipe everything, the words, the scratches and the drums |
| **REC** | NO WORDS: wipe the words and scratches, keep the drums you recorded |
| **ARP** | MUTATE: flip a few ghost drum notes, drop in a new word or a scratch, and sometimes take one out |

Clearing only touches your loop; the groove from PRESETS keeps playing. To start over: hold EDIT, press SEQ, press
REC, and play again.

**Saving:** the loop is saved with your settings once the band is stopped and the deck is quiet, or right away
with **SAVE**. It comes back the next time you switch the FM-1 on. (Plain SEQ, without EDIT, is FILL, not the
sequencer.)

### The techniques

BABY, DRAG, STAB, FORWARD, BACKWARD, CHIRP, TRANSFORM, TEAR, FLARE (1-click), 2-CLICK flare, ORBIT, CRAB, TWIDDLE,
TAZER, SCRIBBLE, HYDROPLANE, LASER, and COMBO.

Each technique is a score: key points for the record's motion (eased like a hand, linear, or accelerating) and the
stretches where the crossfader is open, 64 ticks a beat, locked to the tempo. Hold a key and the word is scratched
with it; while it plays, the screen draws the score with a playhead running over it. Turn STYLE while holding: the
next loop plays the new technique. **COMBO** plays routines, a technique a beat in phrases of four that end on a
flourish (AUTO plays whole routines).

**HAMSTER** reverses the fader in the techniques that click: a chirp keeps only the peaks, and a crab cuts out
instead of in. **CURVE** is the crossfader's cut: SHARP like a battle mixer, MEDIUM, SMOOTH. **TORQUE** is the motor:
LOW winds up like a stock turntable when a word starts, MAX starts at once. It also sets how fast a thrown record
settles and how long a BRAKE takes. Fast scratches are anti-aliased (the deck averages what it skips), so a hard tear
stays clean.

### The LFO

Tap **LFO** to switch it on; its button pulses with it. Hold **LFO** for the PRO page:

- **RATE** 4 BAR, 2 BAR, 1 BAR, 1/2, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32
- **SHAPE** SINE, TRI, SAW UP, SAW DN, SQUARE, S&H, DRIFT (a gliding S&H)
- **DEPTH** 0..15
- **DEST**
  - **FX** sweeps the current FX (FILTER and WAH glide smoothly)
  - **PITCH** from vibrato to a +-12 semitone siren
  - **FADER** a tremolo; on SQUARE it is a transformer at the rate
  - **PAN** moves the deck between left and right
  - **RECORD** the LFO's hand on the record: deep, it rocks it back and forth, a baby scratch at the rate
  - **BAND** sweeps a low-pass over the guitars and the bass

The LFO's phase is the song's position, so it sits on the grid, follows MIDI clock and restarts with PLAY.

### TONE PLAY

Press **HOME** until TONE. The keys are now a pitch fader: chromatic, with key 13 on the root of the band's tuning,
so you play in key. Hold a key and the record runs at that pitch with the fader open; let go and the fader shuts.
**ALGORITHM** picks the SOURCE: TONE (a sine), BUZZ (a saw), AHHH, FRESH, YEAH, SCREAM, or the LAST WORD you said in
DJ mode. The LFO on PITCH gives vibrato, and the LFO on FADER with SQUARE gives transformed tones.

### The sound

- **Vocals:** the post chain filters the 8-bit record's fizz, adds body under 250 Hz, then a fast compressor puts
  the voice in front. An exciter adds air, and a doubler makes it wide while staying mono-safe (the copy sits only in
  the side).
- **Guitars:** a 7-string drop-A take rendered offline (plucked strings, an oversampled three-stage scooped amp, a tone
  stack and a 4x12 cab, plus an open chord and a pinch-harmonic squeal), double-tracked hard left and right. The right side is a few cents up and about 11 ms late.
- **Drums:** an 808 kit, rendered offline as 16-bit samples.
- **Space:** a stereo plate reverb holds the vocals, snare, claps, toms and the echo, and the RAP hook gets a reverb
  throw.
- **Master:** the bus runs into a glue compressor and a soft limiter, never a hard clip.
- **Load:** hold EDIT to see the audio load; about 20 % at peak with the kit hammered.

### The MIXER

Hold **FX** and turn: VOX, GUITAR, BASS, DRUMS, VERB, ECHO and WIDTH (0 = mono: the guitars, the doubler and the
plate all in the middle). The screen shows seven fader strips with live meters.

### Velocity skratch

The keys have no velocity, so it is read from how fast you move. Flick the jog faster and each click throws the
record further. Tap the same word again quickly for finger strokes, back and forth. With EDIT held the same moves
pump the current FX instead, like a wah pedal.

### MIDI

**In** (USB or the TRS jack): channel 10 plays the kit on the General MIDI drum map (35 808, 36 kick, 37 rim, 38/40 snare, 39 clap, 42/44 hat,
46 open hat, toms 41-50 + 58, 49/55/57 crash, 51/59 ride, 52 china, 53 zap); other channels, notes 48-74 = the 27 words.
**Out** (USB): every drum played as a GM note on channel 10, and the deck on channel 1: CC 20 scratching (0/127), CC 21
the crossfader, CC 22 the platter angle, sent on change. **SYNC FOLLOW** follows an
incoming clock (USB or the TRS input), and Start / Stop / Continue drive PLAY. Send **CC 119** and it answers with
CC 119 = the audio load in %.

## Build it

You need Linux on x86-64 (or macOS with Docker, or Windows with WSL), Python 3 with Pillow, the JieLi toolchain
and three files of the JieLi AC79 SDK:

```sh
pip3 install Pillow mido python-rtmidi
tools/get_toolchain.sh                      # JieLi's clang for the chip, to ~/.jieli/toolchain
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK

tests/run_host.sh                           # on your computer: build/host/dinghy.wav + screen PNGs
tests/run_pro.sh                            # the PRO scripts: techniques, tone play + LFO, the mixer (WAV + frames)
tests/run_v3.sh                             # the 3.0 scripts: SURF, the korn grooves, TRS clock
python3 tools/render_kit.py                 # re-render the 808 kit + guitar into firmware/gen/kit_pcm.h (needs numpy, scipy)
./build.sh                                  # -> build/dinghy.fwsc
./build.sh --release 3.0                    # -> build/numetal-3.0.fwsc, "3.0" on the screen
python3 tools/fm1_install.py build/dinghy.fwsc
```

The whole app is [firmware/src/dinghy.c](firmware/src/dinghy.c). It is integer-only, because the chip has no FPU.
The platform under it is [Dinghy](https://github.com/zvenson/dinghy): [docs/API.md](docs/API.md) has every call, and
[docs/RULES.md](docs/RULES.md) has the chip's rules. The host build (`tests/host.c`) runs the same code on a computer.
Its scripts can press keys, hold buttons (`T hold MASK`) and turn encoders (`T enc ID DELTA`), and it prints the
audio code's cost per frame.

The web installer is [docs/index.html](docs/index.html), served by GitHub Pages from `docs/`. Put a new package in
`docs/firmware/` and change `FIRMWARE` at the top of the page's script.

## License

GPL-3.0-only, like everything it stands on: you may change and share it, and whoever gets the firmware gets its
source. Credits and third-party parts are in [LICENSING.md](LICENSING.md).

NU//METAL SKRATCHMASTER PRO (the app) by Mark Hellar. Built on
[Dinghy](https://github.com/zvenson/dinghy) (zvenson), on the platform of [Felucca](https://github.com/hugelton/Felucca)
(Leo Kuroshita, Hügelton Instruments) as [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) carries it. The voice is
SAM via [ESP8266SAM](https://github.com/earlephilhower/ESP8266SAM) (Earle F. Philhower III), from Sebastian Macke's
C port of SAM. The web installer's update protocol (`docs/fm1ota.js`) is Felucca's, by Leo Kuroshita.
