/* SPDX-License-Identifier: GPL-3.0-only */
/* NU//METAL SKRATCHMASTER PRO (2.0) FM-1: a fork of CYBER//SKRATCH for the M-VAVE FM-1 on Dinghy. The robot DJ joins a
 * nu metal band: drop-tuned chug guitars and a bass (the PUNK amp: detuned saws, high-pass, fuzz, a two-pole cab) play a
 * riff locked to each groove, SAM shouts a nu metal lexicon onto the turntable deck, and RAP MODE turns the robot into
 * the MC: four bars of VERSE, four bars of HOOK. Integer-only: the chip has no FPU.
 * PRO: a real LFO (tempo-synced, 7 shapes, 6 destinations), 17 turntablist techniques scored like TTM notation
 * (record motion + crossfader clicks, 64 ticks a beat) and COMBO routines, a HAMSTER switch, a fader CURVE, motor
 * TORQUE, TONE PLAY (the keys as a pitch fader on a tone or a word), anti-aliased fast scratches, AHH and FRESH.
 *
 *   KEYS       DJ mode: 27 words, tap = say it, HOLD = tempo-synced scratch in the STYLE while held
 *              (in RAP MODE the last word you tap becomes the HOOK)
 *              BEAT mode: keys 0..11 = the kit (hold = roll), keys 12..26 = the first 15 words
 *              TONE mode: the keys are the pitch fader, chromatic, the middle C key = the tuning's root: hold = the
 *              record runs at that pitch with the fader open, let go = the fader shuts (tone play)
 *
 *   button     tap / hold                              EDIT held + button
 *   PLAY       start / stop                            TAP tempo
 *   REC        record into the 16 steps (quantized)    clear the words, keep the drums
 *   SEQ        hold = FILL (snare roll, chugs, toms)   clear the steps
 *   ARP        RAP MODE: verse / hook, the robot MCs   MUTATE the steps
 *   GLO        AUTO: the robot scratches by itself     HARD: drums driven, kick ducks the deck
 *   FX         tap = next FX; hold = the MIXER         previous FX
 *   SEL        hold = DROP (only the hats; slam back)  HAMSTER on / off
 *   ENV        hold = FREEZE (loop a sliver)           latch FREEZE
 *   LFO        tap = LFO on / off; hold = the PRO page hold = CUT (crossfader on 32nds)
 *   HOME       DJ / BEAT / TONE                        pitch back to 0
 *   OCT- OCT+  the deck's pitch, a semitone            BACKSPIN / BRAKE (the turntable's power cut)
 *   SAVE       save everything now                     SYNC: FOLLOW / OFF
 *   EDIT       hold = SHIFT: the screen shows the second layer and the audio LOAD
 *
 *   encoder    turn                     EDIT held + turn          LFO held + turn (the PRO page)
 *   SELECT     JOG: scratch by hand     VELOCITY FX (a WAH pedal) SPACE: the plate's return
 *   ALGORITHM  STYLE (TONE: the SOURCE) SWING                     fader CURVE: SHARP MEDIUM SMOOTH
 *   KNOB1      the FX's amount          TUNING: drop F .. drop F  LFO RATE: 4 BAR .. 1/32, triplets
 *   KNOB2      GUITAR (0 = off)         ECHO                      LFO SHAPE: SINE TRI SAW UP SAW DN SQUARE S&H DRIFT
 *   KNOB3      BPM 60..180              KIT DECAY                 LFO DEPTH
 *   KNOB4      SAM VOICE                DRUMS level               LFO DEST: FX PITCH FADER PAN RECORD BAND
 *   PRESETS    GROOVE (and its riff)    REACH                     motor TORQUE: LOW MID HIGH MAX
 *   FX: FILTER CRUSH MEGAPHONE DEMON CRUNCH STUTTER REVERSE WAH GROWL.
 *   STYLES: BABY DRAG STAB FORWARD BACKWARD CHIRP TRANSFORM TEAR FLARE 2-CLICK ORBIT CRAB TWIDDLE TAZER SCRIBBLE
 *   HYDROPLANE LASER COMBO (routines: a technique a beat, phrased in fours, ending on a flourish).
 *   GROOVES: OFF BOUNCE B-BOY JUMP BREAKDOWN RAP METAL BOOM BAP STOMP TRAP SLUDGE.
 *   GROWL: the sludge throat, Melvins style: the word slowed down 5..12 semitones, an octave under it (the octaver's
 *   flip-flop), vocal fry (chopped by jittery pulses at 50..110 Hz), driven and darkened. In RAP MODE the hook is
 *   always growled. The GROWL voice (KNOB4) is SAM at its lowest and slowest.
 *
 *   THE LFO is locked to the song's grid (and so to MIDI clock): its phase is the song position, so it restarts with
 *   PLAY. FX sweeps the FX amount (FILTER and WAH glide), PITCH is vibrato .. a siren (+-12), FADER is a tremolo
 *   (SQUARE = a transformer at the rate), PAN moves the deck left and right, RECORD is the LFO's hand on the record
 *   (deep: it rocks back and forth, a baby scratch at the rate), BAND sweeps a low-pass over the guitars and bass.
 *   HAMSTER reverses the fader in the techniques that click: a chirp becomes the peaks, a crab cuts out instead of in.
 *
 *   THE MIXER (FX held + turn): KNOB1 VOX, KNOB2 GUITAR, KNOB3 BASS, KNOB4 DRUMS, SELECT VERB, ALGORITHM ECHO,
 *   PRESETS WIDTH (0 = mono: the guitars, the doubler and the plate in the middle); faders and meters on the screen.
 *   THE SOUND: the vocal is de-imaged (the 11 kHz record's mirror fizz), given body under 250 Hz, compressed 4:1 fast,
 *   excited (air), and doubled (~11 ms later, drifting, as the side: wide, gone in mono). The guitars are two takes,
 *   L and R, the second a few cents off and ~5 ms late. A plate (two diffusers, a damped 4-line FDN at 22050) takes
 *   the vocal, the snare, claps, rims, toms, the echo and a little guitar; RAP MODE's hook gets a throw. The bus: a
 *   3:1 glue compressor, then a soft limit (unity when quiet, round to +3.5 dB over), never a hard clip.
 *
 *   VELOCITY SKRATCH (the keys have no velocity: it is read from how fast you move)
 *     JOG     the faster the flick, the further each click throws the record; let go mid-flick and it spins
 *             on at that speed and drifts back to normal at the motor's TORQUE
 *     TAPS    tap the same word again quickly: finger strokes, back and forth, one a tap
 *     EDIT +  the same moves pump the current FX instead; it falls back when you stop
 *   USB-MIDI   channel 10 notes 36..47 = the kit; other channels notes 48..74 = the 27 words; CC 119 in = CC 119
 *              back with the audio load %
 *   MIDI SYNC  FOLLOW: an incoming clock (24 ppqn, USB or the TRS input) sets the tempo and the grid, Start / Stop /
 *              Continue drive PLAY; without one the FM-1 runs on its own BPM. OFF: ignored. It never sends clock.
 *
 * SAM: C core from earlephilhower/ESP8266SAM (GPL v3). Built on Dinghy / Felucca / SLOOP (GPL v3). */
#define APP_NAME "NUMETAL"
#include "platform.c"
#include "sam/SamData.h"
#include "sam/sam.h"
#include "sam/reciter.h"

#define POOL __attribute__((section(".pool")))
#define CLIP(x) ((x) > 32767 ? 32767 : (x) < -32767 ? -32767 : (x))

static int32_t sine(uint32_t ph)                /* a full cycle over 2^32 -> -32767..32767 (a refined parabola, ~0.1 %) */
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}
static int32_t soft(int32_t x)                  /* clamp, then a cubic: round at the top */
{
    int32_t x3;
    x = CLIP(x);
    x3 = (((x * x) >> 15) * x) >> 15;
    return (3 * x - x3) >> 1;
}

/* =========================================================================== the word bank (SAM) */
#define NWORDS 24                               /* pad words: 0..19 ten rhyme pairs (2k, 2k+1), 20..23 the ad-libs */
#define NRHYME 20
#define ADLIB 20
#define NKEYW 27                                /* + the name, AHH and FRESH: one a key */
#define NAME_WORD 24
#define AHH_WORD 25
#define FRESH_WORD 26
#define TONE_WORD 27                            /* two records that are not words: the battle record's tones */
#define BUZZ_WORD 28
#define NBANK 29
#define TONE_LOOP 3795u                         /* 85 cycles of B3 (246.94 Hz) at 11025 Hz, a seamless loop */
#define TONE_BYTES (2u * 3u * TONE_LOOP)        /* both tones, three loops each, at the start of the pool */
#define WFS 11025u                              /* the bank is kept at 11025 Hz: 8 bit, ~26 s */
#define WPOOL 286720u
static int8_t wpool[WPOOL] POOL;
static struct { uint32_t off, len, loop; } bank[NBANK];   /* loop: the tones loop that many samples, words 0 = all */
static volatile uint32_t bank_busy = 1;

static const char *const SAY[NBANK] = {         /* spelled for SAM */
    "RAGE", "STAGE", "BREAK", "SHAKE", "JUMP", "PUMP",
    "DROP", "STOP", "FIRE", "WIRE", "LOUD", "CROWD",
    "HEAVY", "READY", "SCREAM", "MACHINE", "WRECK", "MIKE CHECK",
    "SOUND", "UNDERGROUND", "BREAK IT DOWN", "LETS GO", "COME ON", "YEAH",
    "NEW METAL SKRATCH MASTER", "/AA6AA5AA4AA3AA2", "FRESH"};   /* '/' = phonemes, no reciter */
static const char *const SHOW[NBANK] = {        /* on screen */
    "RAGE", "STAGE", "BREAK", "SHAKE", "JUMP", "PUMP",
    "DROP", "STOP", "FIRE", "WIRE", "LOUD", "CROWD",
    "HEAVY", "READY", "SCREAM", "MACHINE", "WRECK", "MIC CHECK",
    "SOUND", "UNDERGROUND", "BREAK IT DOWN", "LET'S GO", "COME ON", "YEAH",
    "NU METAL SKRATCHMASTER", "AHHH", "FRESH", "TONE", "BUZZ"};

#define NVOICES 8
static const struct { const char *name; uint8_t speed, pitch, throat, mouth; } VOICES[NVOICES] = {
    {"MC", 62, 58, 150, 175}, {"GROWL", 96, 150, 80, 80}, {"ROBOT", 92, 60, 190, 190}, {"SAM", 72, 64, 128, 128}, {"ELF", 72, 64, 110, 160},
    {"E.T.", 100, 64, 150, 200}, {"STUFFY", 82, 72, 110, 105}, {"OLD LADY", 82, 32, 145, 145}};

SamData *samdata;
static SamData sam_mem;
static uint32_t sam_w, sam_half, sam_acc;
static void sam_out(void *u, unsigned char b)   /* SAM speaks at 22050 Hz: two samples make one */
{
    (void)u;
    if (!sam_half) { sam_acc = b; sam_half = 1; return; }
    sam_half = 0;
    if (sam_w < WPOOL) wpool[sam_w++] = (int8_t)(((int32_t)(sam_acc + b) - 256) >> 1);
}

static void draw_progress(uint32_t done, uint32_t all);

static void make_tones(void)                    /* TONE: a sine with a little 2nd; BUZZ: a band-limited saw */
{
    const uint32_t inc = (uint32_t)((85ull << 32) / TONE_LOOP);
    uint32_t i, h, ph = 0;
    for (i = 0; i < TONE_LOOP; i++, ph += inc) {
        int32_t saw = 0;
        for (h = 1; h <= 16u; h++) saw += sine(ph * h) / (int32_t)h;
        wpool[i] = (int8_t)((sine(ph) * 70 + sine(ph * 2u) * 14) >> 15);
        wpool[3u * TONE_LOOP + i] = (int8_t)((saw * 50) >> 15);
    }
    for (i = TONE_LOOP; i < 3u * TONE_LOOP; i++) { wpool[i] = wpool[i - TONE_LOOP]; wpool[3u * TONE_LOOP + i] = wpool[3u * TONE_LOOP + i - TONE_LOOP]; }
    bank[TONE_WORD].off = 0; bank[BUZZ_WORD].off = 3u * TONE_LOOP;
    bank[TONE_WORD].len = bank[BUZZ_WORD].len = 3u * TONE_LOOP;
    bank[TONE_WORD].loop = bank[BUZZ_WORD].loop = TONE_LOOP;
}

static void render_bank(uint32_t vi)
{
    uint32_t i, k;
    bank_busy = 1;
    samdata = &sam_mem;
    sam_w = TONE_BYTES;
    for (k = 0; k < NKEYW; k++) {
        char in[256];
        const char *s = SAY[k];
        uint32_t a, b, start = sam_w, n = 0;
        platform_poll();
        draw_progress(k, NKEYW);
        bank[k].off = start;
        bank[k].len = 0;
        if (WPOOL - sam_w < WFS / 2u) continue;          /* full: the rest stays silent */
        while (s[n] && n < 200u) { in[n] = s[n]; n++; }
        in[n++] = '[';
        in[n] = 0;
        for (i = 0; i < sizeof sam_mem; i++) ((uint8_t *)&sam_mem)[i] = 0;
        EnableSingmode(0);
        SetSpeed(VOICES[vi].speed); SetPitch(VOICES[vi].pitch);
        SetThroat(VOICES[vi].throat); SetMouth(VOICES[vi].mouth);
        sam_half = 0;
        if (s[0] == '/') {                                 /* phonemes as they are: a long held vowel */
            for (n = 0; s[n + 1u] && n < 200u; n++) in[n] = s[n + 1u];
            in[n++] = (char)155;
            in[n] = 0;
        } else if (!TextToPhonemes(in)) { sam_w = start; continue; }
        SetInput(in);
        SAMMain(sam_out, 0);
        a = start; b = sam_w;                              /* trim the silence at both ends */
        while (a < b && (wpool[a] > -3 && wpool[a] < 3)) a++;
        while (b > a && (wpool[b - 1] > -3 && wpool[b - 1] < 3)) b--;
        bank[k].off = a;
        bank[k].len = b > a + 8u ? b - a : 0;
    }
    draw_progress(NKEYW, NKEYW);
    bank_busy = 0;
}

/* =========================================================================== shared state (main writes, ISR reads) */
enum { FX_FILTER, FX_CRUSH, FX_MEGA, FX_DEMON, FX_CRUNCH, FX_STUTTER, FX_REVERSE, FX_WAH, FX_GROWL, FX_COUNT };
static const char *const FX_NAME[FX_COUNT] = {"FILTER", "CRUSH", "MEGAPHONE", "DEMON", "CRUNCH", "STUTTER", "REVERSE", "WAH", "GROWL"};

/* the techniques, scored like TTM (turntablist transcription): the record's motion as key points (tick, where in the
 * REACH 0..100, how it gets there: e = eased like a hand, l = linear, i = accelerating) and the crossfader's open
 * stretches (from tick, to tick). 64 ticks a beat; a technique is 1 or 2 beats and loops while the key is held. */
enum { SC_BABY, SC_DRAG, SC_STAB, SC_FWD, SC_BACK, SC_CHIRP, SC_TRANS, SC_TEAR, SC_FLARE, SC_FLARE2, SC_ORBIT,
       SC_CRAB, SC_TWIDDLE, SC_TAZER, SC_SCRIBBLE, SC_HYDRO, SC_LASER, SC_COMBO, SC_COUNT };
#define NTECH SC_COMBO
static const char *const SC_NAME[SC_COUNT] = {"BABY", "DRAG", "STAB", "FORWARD", "BACKWARD", "CHIRP", "TRANSFORM",
    "TEAR", "FLARE", "2-CLICK", "ORBIT", "CRAB", "TWIDDLE", "TAZER", "SCRIBBLE", "HYDROPLANE", "LASER", "COMBO"};
#define KP_END 255
static const uint8_t KP_BABY[] = {0, 0, 'e', 16, 100, 'e', 32, 0, 'e', 48, 100, 'e', 64, 0, 'e', KP_END};
static const uint8_t KP_SLOW[] = {0, 0, 'e', 32, 100, 'e', 64, 0, 'e', KP_END};
static const uint8_t KP_DRAG[] = {0, 0, 'e', 64, 100, 'e', 128, 0, 'e', KP_END};
static const uint8_t KP_STAB[] = {0, 0, 'e', 5, 70, 'l', 16, 70, 'e', 28, 0, 'e', 32, 0, 'e', 37, 70, 'l', 48, 70, 'e',
                                  60, 0, 'e', 64, 0, 'e', KP_END};
static const uint8_t KP_TEAR[] = {0, 0, 'e', 22, 100, 'e', 34, 50, 'e', 42, 50, 'e', 56, 0, 'e', 64, 0, 'e', KP_END};
static const uint8_t KP_HYDRO[] = {0, 0, 'l', 54, 90, 'l', 56, 90, 'e', 63, 0, 'e', 64, 0, 'e', KP_END};
static const uint8_t KP_LASER[] = {0, 0, 'e', 14, 100, 'i', 16, 100, 'e', 30, 0, 'e', 32, 0, 'e', 46, 100, 'i', 48, 100, 'e',
                                   62, 0, 'e', 64, 0, 'e', KP_END};
#define OPEN_ALL ((const uint8_t *)0)
static const uint8_t FD_FWD[] = {0, 16, 32, 48, KP_END};
static const uint8_t FD_BACK[] = {16, 32, 48, 64, KP_END};
static const uint8_t FD_STAB[] = {0, 6, 32, 38, KP_END};
static const uint8_t FD_CHIRP[] = {0, 12, 20, 44, 52, 64, KP_END};
static const uint8_t FD_TRANS[] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, KP_END};
static const uint8_t FD_FLARE[] = {0, 14, 18, 46, 50, 64, KP_END};
static const uint8_t FD_FLARE2[] = {0, 9, 12, 20, 23, 41, 44, 52, 55, 64, KP_END};
static const uint8_t FD_ORBIT[] = {0, 7, 9, 23, 25, 39, 41, 55, 57, 64, KP_END};
static const uint8_t FD_CRAB[] = {2, 4, 5, 7, 8, 10, 11, 13, 18, 20, 21, 23, 24, 26, 27, 29,
                                  34, 36, 37, 39, 40, 42, 43, 45, 50, 52, 53, 55, 56, 58, 59, 61, KP_END};
static const uint8_t FD_TWID[] = {2, 5, 7, 10, 18, 21, 23, 26, 34, 37, 39, 42, 50, 53, 55, 58, KP_END};
static const uint8_t FD_TAZER[] = {1, 3, 4, 6, 7, 9, 23, 25, 26, 28, 29, 31, 43, 45, 46, 48, 49, 51, KP_END};
static const uint8_t FD_HYDRO[] = {0, 55, KP_END};
static const uint8_t FD_LASER[] = {0, 15, 32, 47, KP_END};
typedef struct { const uint8_t *kp, *fd; uint8_t beats, wig_n, wig_a; } tech_t;   /* wiggle: cycles a beat, % of the reach */
static const tech_t TECH[NTECH] = {
    {KP_BABY, OPEN_ALL, 1, 0, 0},  {KP_DRAG, OPEN_ALL, 2, 0, 0},  {KP_STAB, FD_STAB, 1, 0, 0},
    {KP_BABY, FD_FWD, 1, 0, 0},    {KP_BABY, FD_BACK, 1, 0, 0},   {KP_BABY, FD_CHIRP, 1, 0, 0},
    {KP_SLOW, FD_TRANS, 1, 0, 0},  {KP_TEAR, OPEN_ALL, 1, 0, 0},  {KP_SLOW, FD_FLARE, 1, 0, 0},
    {KP_SLOW, FD_FLARE2, 1, 0, 0}, {KP_BABY, FD_ORBIT, 1, 0, 0},  {KP_BABY, FD_CRAB, 1, 0, 0},
    {KP_BABY, FD_TWID, 1, 0, 0},   {KP_TEAR, FD_TAZER, 1, 0, 0},  {KP_SLOW, OPEN_ALL, 1, 16, 9},
    {KP_HYDRO, FD_HYDRO, 1, 24, 4}, {KP_LASER, FD_LASER, 1, 0, 0},
};
static uint32_t tech_fd[NTECH][4];              /* the fader as bits, a tick each (128); built at boot */
static uint8_t tech_clicks[NTECH];              /* 1: the fader moves (HAMSTER reverses it), 0: open throughout */
#define NPHRASE 8
static const uint8_t PHRASE[NPHRASE][4] = {     /* COMBO: routines, four techniques, the last one the flourish */
    {SC_BABY, SC_CHIRP, SC_FLARE, SC_CRAB},     {SC_STAB, SC_STAB, SC_TRANS, SC_TEAR},
    {SC_CHIRP, SC_FLARE2, SC_ORBIT, SC_TAZER},  {SC_FWD, SC_BACK, SC_SCRIBBLE, SC_FLARE},
    {SC_TEAR, SC_CHIRP, SC_TWIDDLE, SC_LASER},  {SC_DRAG, SC_ORBIT, SC_CRAB, SC_CRAB},
    {SC_HYDRO, SC_FWD, SC_FLARE2, SC_TWIDDLE},  {SC_BABY, SC_TRANS, SC_CHIRP, SC_TAZER}};

/* the LFO: locked to the grid, its phase = the song position (G, 16ths Q16) times K */
#define NLFO_RATE 11
static const char *const LFO_RATE_NAME[NLFO_RATE] = {"4 BAR", "2 BAR", "1 BAR", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const uint32_t LFO_K[NLFO_RATE] = {1024u, 2048u, 4096u, 8192u, 16384u, 24576u, 32768u, 49152u, 65536u, 98304u, 131072u};
enum { LS_SINE, LS_TRI, LS_UP, LS_DOWN, LS_SQUARE, LS_SH, LS_DRIFT, LS_COUNT };
static const char *const LFO_SHAPE_NAME[LS_COUNT] = {"SINE", "TRI", "SAW UP", "SAW DN", "SQUARE", "S&H", "DRIFT"};
enum { LD_FX, LD_PITCH, LD_FADER, LD_PAN, LD_RECORD, LD_BAND, LD_COUNT };
static const char *const LFO_DEST_NAME[LD_COUNT] = {"FX", "PITCH", "FADER", "PAN", "RECORD", "BAND"};
static const uint16_t LFO_SEMI_Q8[16] = {0, 26, 51, 90, 128, 192, 256, 384, 512, 768, 1024, 1280, 1792, 2304, 3072, 3072};
static const char *const CURVE_NAME[3] = {"SHARP", "MEDIUM", "SMOOTH"};
static const uint8_t CURVE_SH[3] = {2, 4, 7};    /* the fader's slew, per sample: 0.1 ms .. 3 ms */
static const char *const TORQUE_NAME[4] = {"LOW", "MID", "HIGH", "MAX"};
static const uint8_t TORQUE_SH[4] = {9, 7, 5, 0};   /* the motor pulling the record to speed, per block (0 = at once) */
static const uint8_t BRAKE_STEPS[4] = {16, 12, 9, 6};

static volatile uint32_t p_amtq = 7u << 8;               /* the FX amount, Q8 (LFO and velocity glide between steps) */
static volatile uint32_t p_lfo_on, p_lfo_rate = 6, p_lfo_shape, p_lfo_depth = 8, p_lfo_dest;
static volatile uint32_t p_curve, p_torque = 3, p_hamster, p_space = 7;   /* SPACE: the plate's return */
static volatile uint32_t load_v;
static volatile uint32_t p_blvl = 11, p_width = 15;      /* MIXER: the bass's level, the stereo width (15 = full) */
static volatile uint32_t mt_v[4];                         /* ISR -> main: the meters' peaks, VOX GUITAR BASS DRUMS */                         /* ISR -> main: the audio load %, peak held */
static volatile int32_t lfo_v_v;                         /* ISR -> main: the LFO now, +-32767 */
static volatile uint32_t lfo_ph_v;
static volatile uint32_t dk_tech_v, dk_ph_v, dk_scr_v;   /* ISR -> main: the technique being played, where in it */
static volatile uint32_t p_fx, p_amt = 7, p_echo = 4, p_etime = 2, p_style, p_reach = 3, p_play, p_auto, p_hard = 1;
static volatile uint32_t p_steplen = 7190, p_swing, p_tune = 6, p_decay = 2, p_dlvl = 12, p_vlvl = 12;
static volatile int32_t p_rate = 16384;                 /* the deck's speed, Q16 word samples per frame (1/4 = 11025 into 44100) */
static volatile uint32_t p_groove = 1, p_roll, p_restart;
static volatile uint32_t p_cut, p_freeze, p_drop, p_fill;   /* held performance moves */
static volatile uint32_t p_glvl = 11, p_rap, p_hook = ADLIB;   /* the band's level, RAP MODE, the hook's word */
static volatile uint32_t rap_part_v;                       /* ISR -> main: 0 off, 1 + bar in the verse, 5 + bar in the hook */
static volatile int8_t rap_line[16];                       /* the verse's bar: the word on each step, -1 none */
static volatile uint32_t p_spin, p_brake;                  /* one-shots: counters, the ISR acts when they move */
static volatile uint32_t p_ext, p_ext_ticks, p_ext_start;  /* MIDI clock in: following, ticks since Start, Starts */
static void out_msg(uint32_t st, uint32_t d1, uint32_t d2) { midi_send(st, d1, d2); }   /* main loop only */
enum { CMD_SAY, CMD_SCR, CMD_LET, CMD_STROKE, CMD_TONE, CMD_TONE_OFF };
static volatile uint32_t cmd_n, cmd_kind, cmd_arg;      /* main: arg, kind, then n++ */
static volatile int32_t cmd_span;                       /* CMD_STROKE: how far, word samples (+ forward, - back) */
static volatile uint32_t cmd_dur;                       /* CMD_STROKE: how long, frames */
static volatile int32_t p_jog_gain = 140;               /* word samples a jog click moves: grows with the flick's speed */
static volatile int32_t p_throw_rate;                   /* the record's speed when the jog lets go of it, Q16 */
static volatile uint32_t p_throw;                       /* counter: a throw */
static volatile uint32_t drum_n, drum_mask;             /* main: live drum hits (bits), then n++ */
static volatile int32_t jog_total;                      /* main: detents turned, ever */
static volatile uint32_t jog_held;                      /* main: 1 while the jog is being turned */

/* ISR -> main */
static volatile int32_t dk_word_v = -1;
static volatile uint32_t dk_speed_v;                    /* |deck speed| in 1/16 of normal, for the meter */
static volatile uint32_t dk_pos_v, dk_on_v, dk_rot_v, dk_fader_v, seq_step_v, seq_frac_v, drum_hit_v, fx_state_v;
enum { ST_SPIN = 1, ST_BRAKE = 2, ST_MUTED = 4, ST_WAIT = 8 };

/* the sequencer: 16 steps */
#define SEQ_SCR 1u
static volatile int8_t seq_word[16];
static volatile uint8_t seq_flag[16], seq_skip[16];
static volatile uint16_t seq_drum[16];

/* =========================================================================== drums */
enum { W_KICK, W_SNR, W_CLAP, W_HAT, W_CRASH, W_ZAP };
typedef struct { const char *name; uint8_t wave, psh, dsh; uint32_t inc0, inc1; int16_t gain, noise, drive; } perc_t;
#define NDRUM 12
static const perc_t PERC[NDRUM] = {
    /* name     wave    psh dsh  inc0       inc1      gain   noise  drive(Q4) */
    {"KICK",  W_KICK,  9, 11, 18504394u, 5064361u, 31000, 14000, 35},
    {"SNARE", W_SNR,   9, 10, 20452225u, 16069606u, 22000, 26000, 32},
    {"CLAP",  W_CLAP,  0, 10, 0, 0,                24000, 0,     26},
    {"HAT",   W_HAT,   0, 8,  0, 0,                16000, 0,     0},
    {"OPEN",  W_HAT,   0, 11, 0, 0,                14000, 0,     0},
    {"RIM",   W_SNR,   7, 8,  79861070u, 62330591u, 20000, 11000, 26},
    {"TOM-L", W_KICK,  10, 11, 17530479u, 10713070u, 25000, 6000, 22},
    {"TOM-H", W_KICK,  10, 11, 29217465u, 18504394u, 23000, 6000, 22},
    {"808",   W_KICK,  10, 13, 10713070u, 4869577u, 30000, 4000,  27},
    {"CHINA", W_CRASH, 0, 11, 0, 0,                16000, 0,     0},
    {"ZAP",   W_ZAP,   9, 10, 253218027u, 6817408u, 18000, 0,     0},
    {"CRASH", W_CRASH, 0, 13, 0, 0,                13000, 0,     0},
};
enum { D_KICK, D_SNARE, D_CLAP, D_HAT, D_OPEN, D_RIM, D_TOML, D_TOMH, D_808, D_CHINA, D_ZAP, D_CRASH };
#define HATS ((1u << D_HAT) | (1u << D_OPEN))
static const uint32_t HAT_INC[6] = {31991176u, 47433580u, 57593466u, 81450500u, 84146298u, 124661182u};
static const uint32_t CRASH_INC[6] = {19994485u, 29645987u, 35995916u, 50906562u, 52591436u, 77913239u};

/* the riff: '.' nothing, '0'..'9' a palm-muted chug that many semitones over the tuning's root, 'A'..'K' an open
 * power chord (A = the root, B = a half step up, G = the tritone, H = the fifth). */
typedef struct { const char *name; const char *lane[4]; uint8_t drum[4]; const char *riff; } groove_t;
#define NGROOVES 10
static const groove_t GROOVES[NGROOVES] = {
    {"OFF",       {"................", "................", "................", "................"}, {D_KICK, D_SNARE, D_HAT, D_OPEN},
                  "................"},
    {"BOUNCE",    {"x..x..x...x..x..", "....x.......x...", "x.x.x.x.x.x.x.x.", "..............x."}, {D_KICK, D_SNARE, D_HAT, D_OPEN},
                  "0..0..B...0..G.."},
    {"B-BOY",     {"x.....x.x.......", "....x.......x...", "x.x.x.x.x.x.x.x.", "......x........."}, {D_KICK, D_SNARE, D_HAT, D_OPEN},
                  "A.....0.0.....1."},
    {"JUMP",      {"x.xxx.xxx.xxx.xx", "....x.......x...", "x...x...x...x...", "x..............."}, {D_KICK, D_SNARE, D_HAT, D_CRASH},
                  "0.000.000.000.BB"},
    {"BREAKDOWN", {"x..x......x..x..", "........x.......", "x...x...x...x...", "x..............."}, {D_KICK, D_SNARE, D_CHINA, D_CRASH},
                  "A..0......A..1.."},
    {"RAP METAL", {"x.x...x.x.x...x.", "....x.......x...", "x...x...x...x...", "x..............."}, {D_KICK, D_SNARE, D_OPEN, D_CRASH},
                  "0.0...0.0.0...H."},
    {"BOOM BAP",  {"x.......x.x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "..............x."}, {D_KICK, D_SNARE, D_HAT, D_OPEN},
                  "................"},
    {"STOMP",     {"x...x...x...x...", "....x.......x...", "..x...x...x...x.", "....x.......x..."}, {D_KICK, D_SNARE, D_OPEN, D_CLAP},
                  "0.0.0.0.0.0.D.F."},
    {"TRAP",      {"x......x..x.....", "........x.......", "x.x.xxx.x.x.xxxx", "x......x..x....."}, {D_KICK, D_CLAP, D_HAT, D_808},
                  "A......0..0....."},
    {"SLUDGE",    {"x.......x.....x.", "........x.......", "x...x...x...x...", "x..............."}, {D_KICK, D_SNARE, D_OPEN, D_CRASH},
                  "A.......B.....G."},
};
#define NFLOWS 7
static const char *const FLOWS[NFLOWS] = {      /* RAP MODE: where the words start in a bar */
    "x.x.x.x.x.x.x.x.", "x..x..x.x..x..x.", "..x.x.x...x.x.x.", "x.xxx.x.x.xxx.x.",
    "x...x.x.x...x.x.", "..xxx.x...xxx.x.", "x.x.x..xx.x.x..."};
static uint16_t groove_mask[NGROOVES][16];

/* =========================================================================== the audio ISR */
enum { DK_OFF, DK_PLAY, DK_SCR, DK_HAND, DK_LET, DK_SPIN, DK_FRZ, DK_THROW, DK_STROKE, DK_REST, DK_TONE };
static struct {
    int32_t word, pos, rate, hand, frz;         /* pos, hand, frz: Q16 word samples */
    int32_t st_from, st_to;                     /* a finger stroke: from, to (Q16) */
    uint32_t st_t, st_dur;                      /* frames into it, its length */
    uint32_t mode, ph, once, stut, stut_n;
    int32_t fader;
    uint32_t growl;                             /* RAP MODE's hook: growled whatever the FX */
    uint32_t tech, phrase, pstep;               /* the technique a scratch plays; COMBO: the routine, where in it */
    int32_t tsemi;                              /* TONE: the key's semitone */
    uint32_t tgate;                             /* TONE: the fader, open while a key is held */
} dk = {-1, 0, 0, 0, 0, 0, 0, 0, 0, DK_OFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static struct { int32_t env, nenv; uint32_t ph, inc, inc1, step, t; int32_t z1, z2; } dr[NDRUM];
static int16_t echo_buf[12288] POOL;            /* at 22050 Hz: 0.55 s */
static uint32_t nrng = 0x13579BDFu;
static uint32_t noise(void) { nrng = nrng * 1664525u + 1013904223u; return nrng; }
static const int32_t PITCH[25] = {16384, 17358, 18390, 19484, 20643, 21870, 23170, 24548, 26008, 27554, 29193, 30929,
    32768, 34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858, 65536};
static uint32_t mulq15(uint32_t x, uint32_t q) { return (x >> 15) * q + (((x & 0x7FFFu) * q) >> 15); }   /* x * q / 32768, no 64 bit */
static int32_t pitch_rate(int32_t rate, int32_t semi_q8)  /* rate * 2^(semi/12), semi in Q8, any octave */
{
    int32_t sh = 0, i, f, m;
    while (semi_q8 > (12 << 8)) { semi_q8 -= 12 << 8; sh++; }
    while (semi_q8 < -(12 << 8)) { semi_q8 += 12 << 8; sh--; }
    i = (semi_q8 >> 8) + 12; f = semi_q8 & 255;
    m = i >= 24 ? PITCH[24] : PITCH[i] + (((PITCH[i + 1] - PITCH[i]) * f) >> 8);
    rate = rate < 0 ? -(int32_t)mulq15((uint32_t)-rate, (uint32_t)m) : (int32_t)mulq15((uint32_t)rate, (uint32_t)m);
    return sh >= 0 ? rate << sh : rate >> -sh;
}

#define FRZ_WIN (1400 << 16)                    /* the freeze loop: 127 ms of the word */

static const uint32_t INC_C9[12] = {            /* MIDI 120..131 (C9..B9); lower octaves shift right */
    815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
static uint32_t inc_of(uint32_t note) { if (note > 131u) note = 131u; return INC_C9[note % 12u] >> (10u - note / 12u); }
#define SAW(ph) ((int32_t)(ph) >> 16)           /* -32768..32767 */
static struct {                                 /* the guitars, double-tracked: L = root fifth octave, R = the same a few cents off */
    uint32_t inc[6], ph[6], next[3];            /* 0 2 3 = L, 1 4 5 = R; next: R's notes, once its pick lands */
    int32_t env, envr, mute, lag;               /* R plays ~5 ms behind L: a second take */
} gt;
static struct { uint32_t inc, ph, sph; int32_t env, mute, lp; } bs;          /* the bass: a saw and a sub sine */

static void band_hit(uint32_t rc, uint32_t guitar)   /* a riff step: rc '0'..'9' chug, 'A'.. open; the verse: bass only */
{
    const uint32_t open = rc >= 'A', note = 29u + p_tune + (open ? rc - 'A' : rc - '0');   /* tune 6 = B1, drop B */
    const uint32_t root = inc_of(note);
    if (guitar) {
        gt.inc[0] = root;
        gt.inc[2] = inc_of(note + 7u) - (root >> 10);
        gt.inc[3] = inc_of(note + 12u);
        gt.next[0] = root + (root >> 9);        /* two guitars, a few cents apart */
        gt.next[1] = inc_of(note + 7u) + (root >> 10);
        gt.next[2] = inc_of(note + 12u) - (root >> 10);
        gt.env = 32767 << 8;
        gt.lag = 220;
        gt.mute = !open;
    }
    bs.inc = root;
    bs.env = 32767 << 8;
    bs.mute = !open;
}

static int32_t deck_speed(int32_t rate0, uint32_t fx, uint32_t amt)   /* DEMON and GROWL slow the record down */
{
    uint32_t down = dk.growl ? 9u : 0u;
    if (fx == FX_DEMON) down = amt * 4u / 5u;
    if (fx == FX_GROWL) down = 5u + amt * 7u / 15u;
    return down ? (int32_t)(((uint32_t)rate0 * (uint32_t)PITCH[12u - down]) >> 15) : rate0;
}

static struct { int32_t det, env, lp; uint32_t up, ff, t, per; } gr;
static int32_t growl(int32_t x, uint32_t amt)   /* the sludge throat: an octave under, vocal fry, driven, dark */
{
    int32_t y;
    gr.det += ((x - gr.det) * 2600) >> 15;      /* the cycles, heard below ~600 Hz */
    if (!gr.up && gr.det > 300) { gr.up = 1; gr.ff ^= 1u; }
    else if (gr.up && gr.det < -300) gr.up = 0;
    y = x + (((gr.ff ? x : -x) * (int32_t)(8u + amt)) >> 4);   /* every other cycle flipped: the octave below */
    if (++gr.t >= gr.per) { gr.t = 0; gr.per = 400u + (noise() >> 23); gr.env = 32767; }   /* fry: 48..110 Hz, jittery */
    gr.env -= gr.env >> 7;
    y = ((y >> 2) * (10000 + ((gr.env * (int32_t)(10u + amt)) >> 4))) >> 15;
    y = soft(y * 3);
    gr.lp += ((y - gr.lp) * 9000) >> 15;        /* the chest */
    return gr.lp;
}

static void rap_flow(uint32_t bar, uint32_t steplen)   /* RAP MODE: a bar of words that fit the gaps, a rhyme at the end */
{
    static uint32_t rhyme;
    const char *f = FLOWS[(noise() >> 8) % NFLOWS];
    uint32_t st, last = 16u, prev = 99u;
    for (st = 0; st < 16u; st++) { rap_line[st] = -1; if (f[st] == 'x') last = st; }
    if (!(bar & 1u)) rhyme = (noise() >> 8) % (NRHYME / 2u) * 2u + (noise() >> 31);
    for (st = 0; st < 16u; st++) {
        uint32_t nx = st + 1u, budget, k, best = 0, best_sc = ~0u;
        if (f[st] != 'x') continue;
        if (st == last) { rap_line[st] = (int8_t)(bar & 1u ? rhyme ^ 1u : rhyme); continue; }
        while (nx < 16u && f[nx] != 'x') nx++;
        budget = (nx - st) * steplen * 5u / 16u;                /* word samples: the gap, and a little over */
        for (k = 0; k < 6u; k++) {                              /* the best fit of six */
            const uint32_t w = (noise() >> 8) % (NRHYME + 4u), len = bank[w].len;
            const uint32_t sc = len > budget ? (len - budget) * 2u : (budget - len) / 2u;
            if (len && w != prev && w != rhyme && sc < best_sc) { best_sc = sc; best = w; }
        }
        rap_line[st] = (int8_t)best;
        prev = best;
    }
}


static void deck_start(int32_t w, uint32_t kind, uint32_t once)
{
    if (w < 0 || w >= NBANK || !bank[w].len || bank_busy) return;
    dk.word = w;
    dk.growl = 0;
    dk.stut = 0; dk.stut_n = 0;
    if (kind == CMD_SCR) {
        dk.mode = DK_SCR; dk.ph = 0; dk.once = once; dk.pos = 0;
        if (p_style == SC_COMBO) { dk.phrase = (noise() >> 8) % NPHRASE; dk.pstep = 0; dk.tech = PHRASE[dk.phrase][0]; }
        else dk.tech = p_style % NTECH;
    } else {
        if (dk.mode == DK_OFF || dk.mode == DK_REST || dk.mode == DK_LET) dk.rate = TORQUE_SH[p_torque] ? 0 : dk.rate;   /* the motor winds up */
        dk.mode = DK_PLAY;
        dk.pos = p_fx == FX_REVERSE ? (int32_t)((bank[w].len - 2u) << 16) : 0;
    }
    dk_word_v = w;
}

static void drum_hit(uint32_t d)
{
    const perc_t *p = &PERC[d];
    const uint32_t tune = (uint32_t)PITCH[p_tune + 6u];        /* -6..+6 semitones */
    dr[d].env = 32767 << 8;
    dr[d].nenv = 32767 << 8;
    dr[d].inc = mulq15(p->inc0, tune);
    dr[d].inc1 = mulq15(p->inc1, tune);
    dr[d].step = tune >> 7;                                     /* the metal's clock, Q8 */
    if (d == D_CHINA) dr[d].step = dr[d].step * 3u / 2u;        /* the china: the crash's squares, higher and trashier */
    dr[d].ph = 0;
    dr[d].t = 0;
    if (d == D_HAT) dr[D_OPEN].env = 0;         /* the closed hat chokes the open one */
    drum_hit_v |= 1u << d;
}

static uint32_t bump(uint32_t ph16)             /* 0 .. 32767 .. 0 over a cycle (1 - cos) / 2 */
{
    return (uint32_t)((32767 - sine((ph16 << 16) + 0x40000000u)) >> 1);
}

static int32_t tech_pos(uint32_t t, uint32_t ph)   /* where technique t has the record at ph (Q16 beats): 0..32767 */
{
    const uint8_t *k = TECH[t].kp;
    const uint32_t tq = ph >> 6;                /* ticks, Q4 */
    int32_t p = 0;
    while (k[3] != KP_END && (uint32_t)k[3] << 4 <= tq) k += 3;
    if (k[3] == KP_END) p = k[1] * 327;
    else {
        const int32_t t0 = (int32_t)k[0] << 4, t1 = (int32_t)k[3] << 4, p0 = k[1] * 327, p1 = k[4] * 327;
        int32_t u = t1 > t0 ? (int32_t)(((int32_t)tq - t0) * 32767 / (t1 - t0)) : 32767;
        if (k[5] == 'e') u = (int32_t)bump((uint32_t)u);   /* half a (1 - cos) / 2: a hand's turn */
        else if (k[5] == 'i') u = (u * u) >> 15;
        p = p0 + (((p1 - p0) * u) >> 15);
    }
    if (TECH[t].wig_n) {                        /* SCRIBBLE, HYDROPLANE: the tensed arm, the finger's judder */
        const uint32_t w = (ph * TECH[t].wig_n) & 0xFFFFu;
        const int32_t tri = w < 32768u ? (int32_t)w - 16384 : 49151 - (int32_t)w;
        p += (tri * TECH[t].wig_a) / 50;
    }
    return p < 0 ? 0 : p > 32767 ? 32767 : p;
}
static uint32_t tech_open(uint32_t t, uint32_t ph)  /* the fader at ph: 1 open (HAMSTER reverses the clicks) */
{
    const uint32_t tick = (ph >> 10) & 127u, o = tech_fd[t][tick >> 5] >> (tick & 31u) & 1u;
    return tech_clicks[t] && p_hamster ? o ^ 1u : o;
}

#define WET_DRUMS ((1u << D_SNARE) | (1u << D_CLAP) | (1u << D_RIM) | (1u << D_TOML) | (1u << D_TOMH) | (1u << D_ZAP))
static const uint32_t RV_LEN[4] = {1063u, 1277u, 1489u, 1759u};   /* the plate's lines at 22050 Hz: 48..80 ms */
static int16_t rv_line[4][1760], ap_a[211], ap_b[337];   /* in RAM: the pool is full of words */
static int16_t dbl[1024];                                /* the doubler, 23 ms at 44100 */

static void app_audio(int32_t *out, uint32_t n)
{
    static uint32_t G, gacc, stepn, restarted, cmd_seen, drum_seen, crush_h, ew, rot, half_done;
    static uint32_t spin_seen, brake_seen, braking, brake_g0, muted, slam, was_held, fill_n, frz_was;
    static uint32_t ext_seen, ext_start_seen, ext_wait;
    static int32_t jog_seen, f1, r1, r2, crush_v, cz, duck, echo_y, dcz, wl, wb, ghp, gl1, gl2;
    static uint32_t lfo_last, sh_new, sh_old, last_mode;
    static int32_t lfo_g = 32767, pan_s, blp, band_s = 32767, mot;
    static int32_t dbuf[64], vbuf[64], gbl[64], gbr[64], bbuf[64], wbuf[64];   /* drums, deck, guitars L R, bass, the wet drums */
    uint32_t pk[4] = {0, 0, 0, 0};
    static int32_t va1, va2, vlo, vex, venv, vg = 32767, mg = 32767, menv, ghpr, gl1r, gl2r, blpr, rvl, rvr;
    static uint32_t dbw, dph;
#ifndef DINGHY_HOST
    const uint32_t t_in = fm1_ticks();
#endif
    uint32_t off, amt = p_amt, amtq = p_amtq, i;
    const uint32_t fsh = CURVE_SH[p_curve % 3u], lfo_on = p_lfo_on, ldest = p_lfo_dest, ldepth = p_lfo_depth & 15u;
    const uint32_t fx = p_fx, play = p_play, hard = p_hard, steplen = p_steplen, rap = p_rap;
    const uint32_t freeze = p_freeze, drop = p_drop, fill = p_fill, held = drop | fill;
    const int32_t rate_base = p_rate;
    int32_t rate0 = rate_base;
    const int32_t dl_gain = (int32_t)p_dlvl * 2731, vl_gain = (int32_t)p_vlvl * 2731;   /* 12 = unity */

    /* the clock: G = where the song is, in 16ths (Q16). Steps fire as G passes them (odd ones later with swing). */
    if (p_restart != restarted) { restarted = p_restart; G = 0; gacc = 0; stepn = 0; braking = 0; muted = 0; }
    if (p_ext_start != ext_start_seen) {        /* MIDI Start: back to the top, wait for the first tick */
        ext_start_seen = p_ext_start; G = 0; gacc = 0; stepn = 0; braking = 0; muted = 0; ext_wait = 1; ext_seen = 0;
    }
    if (p_ext && p_ext_ticks != ext_seen) {     /* following: pull the grid onto the incoming ticks */
        const uint32_t t = p_ext_ticks - 1u, target = (t / 6u << 16) + (t % 6u) * 65536u / 6u;
        int32_t err;
        ext_seen = p_ext_ticks;
        if (ext_wait) { ext_wait = 0; G = target; }
        err = (int32_t)(target - G);
        if (err > (2 << 16) || err < -(2 << 16)) { G = target; stepn = (G >> 16) + ((G & 0xFFFFu) != 0u); }
        else G += (uint32_t)(err / 8);          /* gently: the main loop's timing is a little loose */
    }
    if (!p_ext) ext_wait = 0;

    if (cmd_n != cmd_seen) {
        cmd_seen = cmd_n;
        if (cmd_kind == CMD_LET) { if (dk.mode == DK_SCR) dk.mode = DK_LET; }
        else if (cmd_kind == CMD_STROKE) {      /* a finger stroke from wherever the record is */
            const int32_t w = (int32_t)cmd_arg;
            if (w >= 0 && w < NBANK && bank[w].len && !bank_busy) {
                const int32_t len = (int32_t)bank[w].len << 16;
                if (dk.word != w) { dk.word = w; dk.pos = 0; dk_word_v = w; }
                dk.st_from = dk.pos < 0 ? 0 : dk.pos > len ? len : dk.pos;
                dk.st_to = dk.st_from + cmd_span * 65536;
                if (dk.st_to < 0) dk.st_to = 0;
                if (dk.st_to > len) dk.st_to = len;
                dk.st_t = 0;
                dk.st_dur = cmd_dur < 256u ? 256u : cmd_dur;
                dk.mode = DK_STROKE;
            }
        }
        else if (cmd_kind == CMD_TONE) {        /* TONE: the record runs at the key's pitch, the fader opens */
            const int32_t w = (int32_t)(cmd_arg & 255u);
            if (w < NBANK && bank[w].len && !bank_busy) {
                if (dk.word != w) { dk.word = w; dk.pos = 0; dk_word_v = w; }
                if (dk.mode != DK_TONE) { dk.mode = DK_TONE; dk.rate = 0; }
                dk.tsemi = (int32_t)(cmd_arg >> 8) - 64;
                dk.tgate = 1; dk.st_t = 0; dk.growl = 0;
            }
        }
        else if (cmd_kind == CMD_TONE_OFF) dk.tgate = 0;
        else deck_start((int32_t)cmd_arg, cmd_kind, 0);
    }
    if (drum_n != drum_seen) {
        uint32_t m = drum_mask, d;
        drum_seen = drum_n;
        for (d = 0; d < NDRUM; d++) if (m >> d & 1u) drum_hit(d);
    }
    if (was_held && !held) slam = 1;            /* DROP or FILL let go: slam back in on the next step */
    was_held = held;
    if (dk.word < 0 && dk_word_v >= 0) dk.word = dk_word_v;

    /* BACKSPIN: the record thrown backwards, the beat cut until the next bar */
    if (p_spin != spin_seen) {
        spin_seen = p_spin;
        if (dk.word >= 0 && !bank_busy) {
            int32_t len = (int32_t)bank[dk.word].len << 16;
            if (dk.mode == DK_OFF || dk.pos >= len || dk.pos <= 0) dk.pos = len - (1 << 16);
            dk.mode = DK_SPIN;
            dk.rate = -14 * rate0;
        }
        if (play) muted = 1;
    }
    /* BRAKE: the turntable's power cut, a bar slowing down to nothing, then back on the one */
    if (p_brake != brake_seen) { brake_seen = p_brake; if (play && !p_ext) { braking = 256; brake_g0 = G; } }

    /* FREEZE: loop a sliver of the word */
    if (freeze && !frz_was && dk.word >= 0 && !bank_busy) {
        int32_t len = (int32_t)bank[dk.word].len << 16;
        dk.frz = dk.mode == DK_OFF ? len / 3 : dk.pos;
        if (dk.frz < 0) dk.frz = 0;
        if (dk.frz > len - FRZ_WIN) dk.frz = len > FRZ_WIN ? len - FRZ_WIN : 0;
        dk.pos = dk.frz;
        dk.mode = DK_FRZ;
    } else if (!freeze && frz_was && dk.mode == DK_FRZ)
        dk.mode = DK_PLAY;                      /* let go: it plays on */
    frz_was = freeze;

    if (jog_total != jog_seen) {                /* the jog: grab the record */
        int32_t d = jog_total - jog_seen;
        jog_seen = jog_total;
        if (dk.word >= 0 && !bank_busy) {
            int32_t len = (int32_t)bank[dk.word].len << 16;
            if (dk.mode != DK_HAND) { dk.hand = dk.pos < 0 ? 0 : dk.pos > len ? len : dk.pos; dk.mode = DK_HAND; }
            dk.hand += d * (p_jog_gain << 16);   /* velocity: a fast flick throws it further */
            if (dk.hand < 0) dk.hand = 0;
            if (dk.hand > len) dk.hand = len;
        }
    } else if (dk.mode == DK_HAND && !jog_held)
        dk.mode = DK_PLAY;                      /* let go: the record plays on */
    {
        static uint32_t throw_seen;
        if (p_throw != throw_seen) {            /* let go mid-flick: it keeps spinning at that speed */
            throw_seen = p_throw;
            if (dk.word >= 0 && (dk.mode == DK_HAND || dk.mode == DK_PLAY || dk.mode == DK_THROW)) {
                dk.mode = DK_THROW;
                dk.rate = p_throw_rate;
            }
        }
    }

    for (off = 0; off < n; off += 64u) {
        uint32_t m = n - off < 64u ? n - off : 64u, d, band_c = 32767;
        int32_t gate = 1, ft, lfo_v = 0, lfo_gt = 32767, pan = 0, rec_add = 0;

        /* ---- the LFO, once per block: its phase is the song's position, so it sits on the grid */
        if (lfo_on) {
            const uint32_t ph = G * LFO_K[p_lfo_rate % NLFO_RATE], t = ph >> 16;
            if (ph < lfo_last) { sh_old = sh_new; sh_new = noise(); }   /* a new cycle */
            lfo_last = ph;
            switch (p_lfo_shape) {
            case LS_TRI: lfo_v = t < 16384u ? (int32_t)t * 2 : t < 49152u ? 32768 - ((int32_t)t - 16384) * 2 : ((int32_t)t - 65536) * 2; break;
            case LS_UP: lfo_v = (int32_t)t - 32768; break;
            case LS_DOWN: lfo_v = 32767 - (int32_t)t; break;
            case LS_SQUARE: lfo_v = t < 32768u ? 32767 : -32767; break;
            case LS_SH: lfo_v = (int32_t)(sh_new >> 16) - 32768; break;
            case LS_DRIFT: {                    /* the S&H, glided: a drunk hand */
                const int32_t a = (int32_t)(sh_old >> 17) - 16384, b = (int32_t)(sh_new >> 17) - 16384;
                lfo_v = (a + (((b - a) * (int32_t)bump(t >> 1)) >> 15)) * 2;
                break;
            }
            default: lfo_v = sine(ph); break;
            }
            lfo_v = CLIP(lfo_v);
            lfo_ph_v = ph;
            if (ldest == LD_FX) {
                int32_t q = (int32_t)amtq + ((lfo_v * (int32_t)ldepth) >> 8);
                amtq = (uint32_t)(q < 0 ? 0 : q > 15 * 256 ? 15 * 256 : q);
                amt = amtq >> 8;
            } else if (ldest == LD_FADER || ldest == LD_BAND) {
                const int32_t u = (lfo_v + 32767) >> 1, ue = 32767 - (int32_t)ldepth * (32767 - u) / 15;
                if (ldest == LD_FADER) lfo_gt = ue;
                else band_c = 250u + (uint32_t)((ue * ue) >> 15);
            } else if (ldest == LD_PAN) pan = lfo_v * (int32_t)ldepth / 15;
        }
        lfo_v_v = lfo_v;

        /* ---- the clock, once per block */
        rate0 = rate_base;
        {
            uint32_t ginc, due = 0;
            gacc += m << 16;
            ginc = gacc / steplen;
            gacc -= ginc * steplen;
            if (braking) {                                      /* BRAKE: slower and slower over 9 steps */
                const uint32_t gone = (G - brake_g0) >> 8, bst = BRAKE_STEPS[p_torque & 3u];
                braking = gone >= bst * 240u ? 0u : 256u - gone / bst;
                if (!braking) muted = 1;
                ginc = ginc * braking >> 8;
                rate0 = rate0 * (int32_t)braking >> 8;
            }
            if (ext_wait) ginc = 0;
            if (!rap) { rap_part_v = 0; for (d = 0; d < 16u; d++) rap_line[d] = -1; }
            G += ginc;
            dk.ph += ginc >> 2;                                 /* scratches move in beats */
            while (due < 2u) {
                const uint32_t s = stepn & 15u, at = (stepn << 16) + (s & 1u ? p_swing * 65536u / 40u : 0u);
                uint32_t g;
                if ((int32_t)(G - at) < 0) break;
                due++;
                g = groove_mask[p_groove % NGROOVES][s] | seq_drum[s];
                half_done = 0;
                if (muted && s == 0u) { muted = 0; slam = 1; }
                if (play && !muted) {
                    const uint32_t bar = stepn >> 4 & 7u, verse = rap && bar < 4u;
                    uint32_t rc = (uint32_t)GROOVES[p_groove % NGROOVES].riff[s];
                    if (rap && s == 0u && (bar == 0u || bar == 4u)) slam = 1;   /* the verse and the hook come in on a crash */
                    if (drop) { g &= HATS; rc = '.'; }              /* DROP: only the hats stay */
                    if (fill) {                                     /* FILL: a snare roll, chugs, toms at the end */
                        g = (g & HATS) | 1u << D_SNARE;
                        if (fill_n >= 6u && fill_n & 1u) g |= 1u << (fill_n & 2u ? D_TOMH : D_TOML);
                        rc = '0';
                        fill_n++;
                    } else fill_n = 0;
                    if (slam && !held) { g |= 1u << D_CRASH | 1u << D_KICK; rc = 'A'; slam = 0; }
                    for (d = 0; d < NDRUM; d++) if (g >> d & 1u) drum_hit(d);
                    if (rc != '.' && p_groove % NGROOVES) band_hit(rc, !verse);
                    if (seq_word[s] >= 0 && !seq_skip[s] && dk.mode != DK_FRZ && dk.mode != DK_SPIN)
                        deck_start(seq_word[s], seq_flag[s] & SEQ_SCR ? CMD_SCR : CMD_SAY, 1);
                    if (rap) {                                      /* RAP MODE: the robot MCs */
                        rap_part_v = 1u + bar;
                        if (verse) {
                            if (s == 0u) rap_flow(bar, steplen);
                            if (rap_line[s] >= 0 && seq_word[s] < 0 && dk.mode != DK_FRZ && dk.mode != DK_SPIN && dk.mode != DK_HAND)
                                deck_start(rap_line[s], CMD_SAY, 0);
                        } else {
                            if (s == 0u) for (d = 0; d < 16u; d++) rap_line[d] = -1;
                            if (dk.mode != DK_FRZ && dk.mode != DK_SPIN && dk.mode != DK_HAND) {
                                if (s == 0u || (s == 8u && bar & 1u)) { deck_start((int32_t)p_hook, CMD_SAY, 0); dk.growl = 1; }
                                else if (s == 8u) deck_start(ADLIB + 1 + (int32_t)((noise() >> 8) % 3u), CMD_SAY, 0);
                                else if (s == 12u && (bar == 7u || (p_auto && noise() >> 30 == 0u)))
                                    deck_start((int32_t)p_hook, CMD_SCR, 1);   /* the scratch back into the verse */
                            }
                        }
                    } else if (p_auto && dk.mode != DK_FRZ) {       /* the robot DJ */
                        uint32_t r = noise() >> 24;
                        if ((s == 0 && r < 180) || (s == 8 && r < 100) || (s == 6 && r < 30))
                            deck_start((int32_t)(noise() >> 8) % NWORDS, CMD_SAY, 0);
                        else if ((s == 12 && r < 90) || (s == 4 && r < 40))
                            deck_start(dk.word >= 0 ? dk.word : 0, CMD_SCR, 1);
                    }
                }
                seq_skip[s] = 0;
                for (d = 0; d < NDRUM; d++) if (p_roll >> d & 1u) drum_hit(d);
                if (fx == FX_STUTTER && dk.mode == DK_PLAY && dk.stut_n < 6u) {   /* stutter: back to the slice's start */
                    uint32_t every = amt < 6u ? 1u : amt < 11u ? 2u : 4u;
                    if (!(s % every)) {
                        if (!dk.stut_n) dk.stut = (uint32_t)dk.pos;
                        else dk.pos = (int32_t)dk.stut;
                        dk.stut_n++;
                    }
                }
                seq_step_v = stepn++;
            }
            {
                const uint32_t into = G - ((stepn - 1u) << 16);
                seq_frac_v = into >= 65536u ? 255u : into >> 8;
            }
            if (fill && play && !muted && fill_n > 8u && !half_done && seq_frac_v >= 128u) {
                half_done = 1;                                  /* the second half of a fill: 32nds */
                drum_hit(D_SNARE);
            }
        }

        if (lfo_on && ldest == LD_PITCH) rate0 = pitch_rate(rate0, (lfo_v * (int32_t)LFO_SEMI_Q8[ldepth]) >> 15);
        if (lfo_on && ldest == LD_RECORD) rec_add = (((lfo_v * (rate0 >> 2)) >> 15) * (int32_t)ldepth * 16) / 15;

        /* ---- the deck: where the record goes this block */
        if (dk.mode == DK_PLAY && last_mode != DK_PLAY) mot = dk.rate;   /* the motor takes the record as it finds it */
        last_mode = dk.mode;
        if (dk.word >= 0 && !bank_busy) {
            int32_t len = (int32_t)bank[dk.word].len << 16;
            switch (dk.mode) {
            case DK_PLAY: {                                     /* the motor pulls it to speed at its TORQUE */
                const int32_t tr = fx == FX_REVERSE ? -rate0 : deck_speed(rate0, fx, amt);
                const uint32_t ts = TORQUE_SH[p_torque & 3u];
                mot = ts ? mot + ((tr - mot) >> ts) + (tr > mot) - (tr < mot) : tr;
                dk.rate = mot + rec_add;
                if (dk.pos >= len || dk.pos < 0) dk.mode = DK_OFF;
                break;
            }
            case DK_FRZ: dk.rate = deck_speed(rate0, fx, amt) + rec_add; break;
            case DK_SCR: {                                      /* a technique off its score */
                int32_t target;
                if (dk.ph >= (uint32_t)TECH[dk.tech].beats << 16) {
                    dk.ph -= (uint32_t)TECH[dk.tech].beats << 16;
                    if (p_style == SC_COMBO) {                  /* COMBO: the routine's next move */
                        if (dk.once && dk.pstep == 3u) { dk.mode = DK_LET; break; }
                        dk.pstep = (dk.pstep + 1u) & 3u;
                        if (!dk.pstep) dk.phrase = (noise() >> 8) % NPHRASE;
                        dk.tech = PHRASE[dk.phrase][dk.pstep];
                    } else {
                        if (dk.once) { dk.mode = DK_LET; break; }
                        dk.tech = p_style % NTECH;              /* STYLE turned while held: the next loop is the new one */
                    }
                    if (dk.ph >= (uint32_t)TECH[dk.tech].beats << 16) dk.ph = 0;
                }
                gate = (int32_t)tech_open(dk.tech, dk.ph);
                target = (tech_pos(dk.tech, dk.ph) * (int32_t)(2600u + p_reach * 800u)) << 1;   /* Q16 */
                if (target > len) target = len;
                dk.rate = (target - dk.pos) / (int32_t)m;
                break;
            }
            case DK_TONE: {                                     /* tone play: the pitch fader glides, the fader is the key */
                const int32_t tr = pitch_rate(rate0, dk.tsemi * 256) + rec_add;
                dk.rate += (tr - dk.rate) >> 2;
                gate = (int32_t)dk.tgate;
                if (dk.tgate) dk.st_t = 0;
                else if (++dk.st_t > 1400u) dk.mode = DK_OFF;  /* two seconds of silence: put away */
                break;
            }
            case DK_HAND:
                dk.rate = ((dk.hand - dk.pos) >> 2) / (int32_t)m;
                if (dk.rate > 16 * rate0) dk.rate = 16 * rate0;
                if (dk.rate < -16 * rate0) dk.rate = -16 * rate0;
                break;
            case DK_SPIN:                                       /* the throw slows down */
                dk.rate -= dk.rate >> 7;
                if (dk.pos <= 0 || dk.rate > -(rate0 >> 3)) { dk.mode = DK_LET; gate = 0; }
                break;
            case DK_THROW:                                      /* thrown: drifts back to the motor's speed */
                dk.rate += (rate0 - dk.rate) >> (8u - (p_torque & 3u));
                if (dk.rate - rate0 < (rate0 >> 4) && rate0 - dk.rate < (rate0 >> 4)) dk.mode = DK_PLAY;
                if (dk.pos >= len || (dk.pos <= 0 && dk.rate < 0)) dk.mode = DK_LET;
                break;
            case DK_STROKE: {                                   /* a finger stroke, eased in and out */
                int32_t target;
                dk.st_t += m;
                if (dk.st_t >= dk.st_dur) { dk.st_t = dk.st_dur; dk.mode = DK_REST; }
                target = dk.st_from + ((dk.st_to - dk.st_from) >> 15) * (int32_t)bump((dk.st_t << 15) / dk.st_dur);
                dk.rate = (target - dk.pos) / (int32_t)m;
                break;
            }
            case DK_REST:                                       /* the finger holds the record: silence, then it is put away */
                dk.rate = 0; gate = 0; dk.st_t += m;
                if (dk.st_t > dk.st_dur + 66150u) dk.mode = DK_OFF;
                break;
            case DK_LET: gate = 0; if (dk.fader < 64) dk.mode = DK_OFF; break;
            default: dk.rate = 0; gate = 0; break;
            }
        } else gate = 0;
        if (dk.mode == DK_OFF) gate = 0;
        if (p_cut && seq_frac_v & 128u) gate = 0;           /* the transformer: shut every other 32nd */
        ft = gate ? 32767 : 0;

        /* ---- the deck: the sound */
        for (i = 0; i < m; i++) vbuf[i] = 0;
        if (dk.word >= 0 && !bank_busy && (dk.fader || ft)) {
            const int8_t *w = wpool + bank[dk.word].off;
            const int32_t len = (int32_t)bank[dk.word].len - 1, frz = dk.mode == DK_FRZ, tone = dk.mode == DK_TONE;
            const int32_t loopl = (int32_t)(bank[dk.word].loop ? bank[dk.word].loop : (uint32_t)len) << 16;
            int32_t pos = dk.pos, rate = dk.rate, fd = dk.fader, lg = lfo_g;
            const int32_t fast = rate > 65536 || rate < -65536;   /* over a word sample a frame: average what it passed */
            for (i = 0; i < m; i++) {
                int32_t ip, s = 0;
                if (frz && pos >= dk.frz + FRZ_WIN) pos -= FRZ_WIN;
                if (tone) { if (pos >= loopl) pos -= loopl; else if (pos < 0) pos += loopl; }
                ip = pos >> 16;
                fd += (ft - fd) >> fsh;
                lg += (lfo_gt - lg) >> fsh;
                if (ip >= 0 && ip < len) {
                    if (fast) {                                 /* a box over the skipped samples: no aliasing whine */
                        int32_t lo = (pos - rate) >> 16, hi = ip, acc = 0, k;
                        if (lo > hi) { k = lo; lo = hi; hi = k; }
                        if (lo < 0) lo = 0;
                        if (hi - lo > 24) lo = hi - 24;
                        for (k = lo; k <= hi; k++) acc += w[k];
                        s = (acc << 8) / (hi - lo + 1);
                    } else {
                        int32_t a = w[ip], b = w[ip + 1];
                        s = (a << 8) + (((b - a) * (int32_t)((uint32_t)(pos & 0xFFFF) >> 1)) >> 7);
                    }
                }
                if (frz) {                                      /* soft edges on the loop */
                    int32_t e1 = (pos - dk.frz) >> 16, e2 = (dk.frz + FRZ_WIN - pos) >> 16, e = e1 < e2 ? e1 : e2;
                    if (e < 160) s = (s * (e < 0 ? 0 : e)) / 160;
                } else if (tone && !bank[dk.word].loop) {       /* a word looped: soft edges too */
                    int32_t e1 = pos >> 16, e2 = (loopl - pos) >> 16, e = e1 < e2 ? e1 : e2;
                    if (e < 160) s = (s * (e < 0 ? 0 : e)) / 160;
                }
                s = (s * fd) >> 15;
                s = (s * lg) >> 15;
                dcz += (s - dcz) >> 10;                         /* a still record is silence, not a step */
                vbuf[i] = s - dcz;
                pos += rate;
            }
            dk.pos = pos;
            dk.fader = fd;
            lfo_g = lg;
        } else {                                                /* shut, the record still moves */
            dk.fader = 0; dk.pos += dk.rate * (int32_t)m; lfo_g = lfo_gt;
            if (dk.mode == DK_TONE && dk.word >= 0) {
                const int32_t loopl = (int32_t)(bank[dk.word].loop ? bank[dk.word].loop : bank[dk.word].len - 1u) << 16;
                while (dk.pos >= loopl) dk.pos -= loopl;
                while (dk.pos < 0) dk.pos += loopl;
            }
        }
        rot += (uint32_t)(dk.mode != DK_OFF ? dk.rate : play ? rate0 : 0) * 3u * m;

        /* ---- the drums */
        for (i = 0; i < m; i++) dbuf[i] = wbuf[i] = 0;
        for (d = 0; d < NDRUM; d++) {
            uint32_t wet;
            const perc_t *p = &PERC[d];
            int32_t env = dr[d].env, nenv = dr[d].nenv, z1 = dr[d].z1, z2 = dr[d].z2, dsh;
            uint32_t ph = dr[d].ph, inc = dr[d].inc, t = dr[d].t;
            const uint32_t inc1 = dr[d].inc1, step = dr[d].step;
            if (env < 512) continue;
            dsh = (int32_t)p->dsh + (int32_t)p_decay - 2;
            wet = (1u << d) & WET_DRUMS;
            for (i = 0; i < m; i++) {
                int32_t s, nz = (int32_t)noise() >> 16, e;
                switch (p->wave) {
                case W_KICK:
                    inc -= (inc - inc1) >> p->psh;
                    ph += inc;
                    s = sine(ph) + ((((nz * (nenv >> 8)) >> 15) * p->noise) >> 15);
                    nenv -= nenv >> 7;
                    break;
                case W_SNR:
                    inc -= (inc - inc1) >> p->psh;
                    ph += inc;
                    z1 += (nz - z1) >> 1;
                    s = (sine(ph) * (32767 - p->noise) + z1 * p->noise) >> 15;
                    break;
                case W_CLAP:                                     /* three quick slaps, then the tail */
                    if (t == 420u || t == 840u || t == 1260u) env = 32767 << 8;
                    z1 += (nz - z1) >> 2;
                    s = nz - z1;                                 /* the hiss above ~2 kHz */
                    break;
                case W_HAT: case W_CRASH: {                      /* the 808's six squares, high-passed */
                    const uint32_t *f = p->wave == W_HAT ? HAT_INC : CRASH_INC;
                    uint32_t k, tt;
                    ph += step;
                    tt = (ph >> 8) & 0xFFFFFu;
                    s = 0;
                    for (k = 0; k < 6u; k++) s += ((int32_t)((f[k] * tt) >> 31) ? 4096 : -4096);
                    e = s - z1; z1 = s;
                    s = e - z2; z2 = e;                          /* twice: only the sizzle stays */
                    s = (s + (nz >> 2)) >> 1;
                    break;
                }
                default:                                         /* W_ZAP */
                    inc -= (inc - inc1) >> p->psh;
                    ph += inc;
                    s = sine(ph);
                    break;
                }
                env -= env >> (p->wave == W_CLAP && t < 1300u ? 8 : dsh);
                t++;
                s = (s * (env >> 8)) >> 15;
                s = (s * p->gain) >> 15;
                if (p->drive) s = soft((s * p->drive) >> 4);
                dbuf[i] += s;
                if (wet) wbuf[i] += s;
            }
            dr[d].env = env; dr[d].nenv = nenv; dr[d].ph = ph; dr[d].inc = inc; dr[d].t = t; dr[d].z1 = z1; dr[d].z2 = z2;
        }
        if (dr[D_KICK].env > (30000 << 8) || dr[D_808].env > (30000 << 8)) duck = 32767;

        /* ---- the band: two guitar takes (L, R), each high-pass, fuzz, a two-pole cab; the bass in the middle */
        for (i = 0; i < m; i++) gbl[i] = gbr[i] = bbuf[i] = 0;
        if (gt.lag > 0 && (gt.lag -= (int32_t)m) <= 0) {      /* the second take's pick lands */
            gt.inc[1] = gt.next[0]; gt.inc[4] = gt.next[1]; gt.inc[5] = gt.next[2];
            gt.envr = gt.env;
        }
        if (gt.env > 4096 || gt.envr > 4096 || bs.env > 4096) {
            const uint32_t bk = braking ? braking : 256u;       /* BRAKE: the band winds down with the turntable */
            const uint32_t i0 = (gt.inc[0] >> 8) * bk, i1 = (gt.inc[1] >> 8) * bk, i2 = (gt.inc[2] >> 8) * bk;
            const uint32_t i3 = (gt.inc[3] >> 8) * bk, i4 = (gt.inc[4] >> 8) * bk, i5 = (gt.inc[5] >> 8) * bk;
            const uint32_t ib = (bs.inc >> 8) * bk;
            const int32_t ta = gt.mute ? 5200 : 9500, gsh = gt.mute ? 11 : 15, bsh = bs.mute ? 11 : 14;
            for (i = 0; i < m; i++) {
                int32_t xl = 0, xr = 0, b2;
                if (gt.env > 4096) {                            /* L: three saws, high-pass, fuzz, a two-pole cab */
                    gt.ph[0] += i0; gt.ph[2] += i2; gt.ph[3] += i3;
                    xl = ((SAW(gt.ph[0]) + SAW(gt.ph[2]) + (SAW(gt.ph[3]) >> 1)) * 13) >> 5;
                    ghp += ((xl - ghp) * 600) >> 15;            /* ~130 Hz: tight, no mud */
                    xl = soft((xl - ghp) * 5);                  /* the fuzz */
                    gl1 += ((xl - gl1) * ta) >> 15;             /* the cab; a palm mute is darker */
                    gl2 += ((gl1 - gl2) * ta) >> 15;
                    xl = (gl2 * (gt.env >> 8)) >> 15;
                    gt.env -= gt.env >> gsh;
                }
                if (gt.envr > 4096) {                           /* R: the second take */
                    gt.ph[1] += i1; gt.ph[4] += i4; gt.ph[5] += i5;
                    xr = ((SAW(gt.ph[1]) + SAW(gt.ph[4]) + (SAW(gt.ph[5]) >> 1)) * 13) >> 5;
                    ghpr += ((xr - ghpr) * 600) >> 15;
                    xr = soft((xr - ghpr) * 5);
                    gl1r += ((xr - gl1r) * ta) >> 15;
                    gl2r += ((gl1r - gl2r) * ta) >> 15;
                    xr = (gl2r * (gt.envr >> 8)) >> 15;
                    gt.envr -= gt.envr >> gsh;
                }
                bs.ph += ib; bs.sph += ib >> 1;
                bs.lp += ((SAW(bs.ph) - bs.lp) * 2600) >> 15;
                b2 = bs.lp + (sine(bs.sph) >> 1);               /* + the octave below, for headphones */
                b2 = (b2 * (bs.env >> 8)) >> 15;
                bs.env -= bs.env >> bsh;
                bbuf[i] = (b2 * 3) >> 2;                        /* the bass in the middle */
                gbl[i] = xl;
                gbr[i] = xr;
            }
        }

        /* the compressors' gains for this block, from the envelopes so far */
        {
            const int32_t VT = 7000, MT = 16000;               /* the vocal: 4:1 over -13 dB; the bus: 3:1 over -6 dB */
            int32_t vgt = 32767, mgt = 32767;
            if (venv > VT) vgt = ((VT + ((venv - VT) >> 2)) * 16384 / venv) << 1;
            if (menv > MT) mgt = ((MT + (menv - MT) / 3) * 16384 / menv) << 1;
            vg += (vgt - vg) >> 1;
            mg += (mgt - mg) >> 2;
        }

        /* ---- FX on the deck, the echo, the mix */
        for (i = 0; i < m; i++) {
            int32_t x = vbuf[i], o, dd, gl, gr, side, rl = rvl, rr = rvr;
            va1 += ((x - va1) * 21000) >> 15;                   /* de-image: the 11 kHz record's mirror fizz off */
            va2 += ((va1 - va2) * 21000) >> 15;
            vlo += ((va2 - vlo) * 1150) >> 15;                  /* body: + ~3 dB under 250 Hz */
            x = va2 + ((vlo * 3) >> 3);
            if (dk.growl && fx != FX_GROWL) x = growl(x, 9u);
            switch (fx) {
            case FX_FILTER:                                      /* 0..6 low-pass, 7..8 open, 9..15 high-pass */
                if (amtq < 7u * 256u) { f1 += ((x - f1) * (int32_t)(1500u + ((amtq * 525u) >> 5))) >> 15; x = f1; }
                else if (amtq > 8u * 256u) { f1 += ((x - f1) * (int32_t)(((amtq - 2048u) * 1800u) >> 8)) >> 15; x -= f1; }
                break;
            case FX_CRUSH: {
                const uint32_t keep = 12u - (amt * 9u >> 4), hold = 1u + (amt >> 2);
                if (++crush_h >= hold) { crush_h = 0; crush_v = x & ~(int32_t)((1u << (16u - keep)) - 1u); }
                x = crush_v;
                break;
            }
            case FX_MEGA:
                r1 += ((x - r1) * 2270) >> 15; x -= r1;
                r2 += ((x - r2) * 10900) >> 15;
                x = soft((r2 * (int32_t)(4u + amt)) >> 1) + ((int32_t)noise() >> 22);
                break;
            case FX_DEMON:                                       /* the deck slowed (above), and dirtied */
                x = soft(x + (x >> 1));
                break;
            case FX_GROWL: x = growl(x, amt); break;
            case FX_WAH: {                                       /* a resonant band-pass: the amount is the pedal */
                const int32_t fq = 1600 + (int32_t)((amtq * 680u) >> 8), hp = (x >> 2) - wl - ((wb * 8000) >> 15);
                wb += (fq * hp) >> 15;
                wl += (fq * wb) >> 15;
                x = soft(wb);
                break;
            }
            case FX_CRUNCH: {
                static const int32_t DRV[16] = {64, 80, 96, 128, 160, 192, 256, 320, 384, 512, 640, 768, 1024, 1280, 1536, 2048};
                x = soft((x * DRV[amt & 15u]) >> 6);
                cz += ((x - cz) * 16000) >> 15;
                x = (cz * 3) >> 2;
                break;
            }
            default: break;
            }
            {                                                   /* the vocal comp (fast, upfront), then the exciter (air) */
                const int32_t a = x < 0 ? -x : x;
                venv += a > venv ? (a - venv) >> 4 : -(venv >> 11);
                x = ((x * vg) >> 15) * 11 >> 3;
                vex += ((x - vex) * 11500) >> 15;
                x += (soft((x - vex) * 3) * 5) >> 4;
            }
            x = (x * vl_gain) >> 15;
            if (hard && duck) { x = (x * (32767 - (duck * 5 >> 3))) >> 15; duck -= (duck >> 11) + 1; if (duck < 0) duck = 0; }
            else duck = 0;

            /* echo at 22050 Hz, the feedback darkened */
            if (!(i & 1u)) {
                uint32_t dl = steplen * (p_etime + 1u) / 2u, rd;
                if (dl > 12287u) dl = 12287u;
                rd = (ew + 12288u - dl) % 12288u;
                echo_y = echo_buf[rd];
                {
                    static int32_t lpf;
                    int32_t in = ((x * (int32_t)p_echo) >> 4) + ((echo_y * (int32_t)(p_echo * 1500u)) >> 15);
                    lpf += ((in - lpf) * 20000) >> 15;
                    echo_buf[ew] = (int16_t)CLIP(lpf);
                }
                ew = (ew + 1u) % 12288u;
                {                                               /* THE PLATE: two diffusers, a 4-line FDN, damped */
                    static uint32_t ai, bi, li[4];
                    static int32_t dmp[4];
                    const int32_t vs = dk.growl ? 15000 : 9000;  /* RAP MODE's hook: a reverb throw */
                    int32_t in = ((x * vs) >> 15) + ((wbuf[i] * 9000) >> 15) + ((echo_y * 8000) >> 15)
                               + (((gbl[i] + gbr[i]) * 1500) >> 15), y, v, d0, d1, d2, d3, s1, s2, s3, s4;
                    uint32_t k;
                    y = ap_a[ai]; v = in - ((y * 19660) >> 15); ap_a[ai] = (int16_t)CLIP(v); in = y + ((v * 19660) >> 15);
                    if (++ai >= sizeof ap_a / 2u) ai = 0;
                    y = ap_b[bi]; v = in - ((y * 19660) >> 15); ap_b[bi] = (int16_t)CLIP(v); in = y + ((v * 19660) >> 15);
                    if (++bi >= sizeof ap_b / 2u) bi = 0;
                    for (k = 0; k < 4u; k++) dmp[k] += ((rv_line[k][li[k]] - dmp[k]) * 22000) >> 15;
                    d0 = dmp[0]; d1 = dmp[1]; d2 = dmp[2]; d3 = dmp[3];
                    s1 = d0 + d1; s2 = d0 - d1; s3 = d2 + d3; s4 = d2 - d3;   /* Hadamard: every line feeds every line */
                    rv_line[0][li[0]] = (int16_t)CLIP(in + ((((s1 + s3) >> 1) * 24000) >> 15));
                    rv_line[1][li[1]] = (int16_t)CLIP(in + ((((s2 + s4) >> 1) * 24000) >> 15));
                    rv_line[2][li[2]] = (int16_t)CLIP(in + ((((s1 - s3) >> 1) * 24000) >> 15));
                    rv_line[3][li[3]] = (int16_t)CLIP(in + ((((s2 - s4) >> 1) * 24000) >> 15));
                    for (k = 0; k < 4u; k++) if (++li[k] >= RV_LEN[k]) li[k] = 0;
                    rvl = ((d0 + (d2 >> 1)) * (int32_t)(p_space * 1400u)) >> 15;
                    rvr = ((d1 + (d3 >> 1)) * (int32_t)(p_space * 1400u)) >> 15;
                }
            }

            dd = (dbuf[i] * dl_gain) >> 15;
            if (hard) dd = soft(dd + (dd >> 1));                  /* HARD: the kit into the red */
            {                                                   /* the doubler: the vocal ~11 ms later, drifting, as the side */
                const uint32_t dq = 128000u + (uint32_t)((sine(dph) + 32768) >> 4) * 46u / 8u;   /* Q8 samples: 500 .. 590 */
                const uint32_t r0 = (dbw - (dq >> 8)) & 1023u, r1 = (r0 - 1u) & 1023u;
                const int32_t a = dbl[r0], b = dbl[r1];
                dbl[dbw] = (int16_t)CLIP(x);
                dbw = (dbw + 1u) & 1023u;
                dph += 58434u;                                  /* 0.6 Hz */
                side = ((a + (((b - a) * (int32_t)(dq & 255u)) >> 8)) * 8200) >> 15;
            }
            {                                                   /* MIXER: guitars and bass, then the width */
                const int32_t bl = (bbuf[i] * (int32_t)(p_blvl * 1300u)) >> 15, gw = (int32_t)p_glvl * 1300;
                gl = (gbl[i] * gw) >> 15; gr = (gbr[i] * gw) >> 15;
                if (p_width < 15u) {                            /* the guitars and the plate toward the middle */
                    const int32_t mid = (gl + gr) >> 1, sd = (((gl - gr) >> 1) * (int32_t)p_width) / 15;
                    const int32_t rm = (rvl + rvr) >> 1, rs = (((rvl - rvr) >> 1) * (int32_t)p_width) / 15;
                    gl = mid + sd; gr = mid - sd;
                    rl = rm + rs; rr = rm - rs;
                }
                side = (side * (int32_t)p_width) / 15;
                {
                    const uint32_t ag = (uint32_t)(gl < 0 ? -gl : gl), ab = (uint32_t)(bl < 0 ? -bl : bl);
                    const uint32_t av = (uint32_t)(x < 0 ? -x : x), ad = (uint32_t)(dd < 0 ? -dd : dd);
                    if (av > pk[0]) pk[0] = av;
                    if (ag > pk[1]) pk[1] = ag;
                    if (ab > pk[2]) pk[2] = ab;
                    if (ad > pk[3]) pk[3] = ad;
                }
                gl += bl; gr += bl;
            }
            band_s += ((int32_t)band_c - band_s) >> 6;
            if (band_s < 32700) {                               /* LFO BAND: the low-pass */
                blp += (((gl - blp) >> 1) * band_s) >> 14; gl = blp;
                blpr += (((gr - blpr) >> 1) * band_s) >> 14; gr = blpr;
            } else { blp = gl; blpr = gr; }
            {
                int32_t xl = x + side, xr = x - side, l, r;
                pan_s += (pan - pan_s) >> 6;
                if (pan_s > 0) xl = (xl * (32767 - pan_s)) >> 15;   /* LFO PAN: only the deck moves */
                if (pan_s < 0) xr = (xr * (32767 + pan_s)) >> 15;
                o = ((echo_y * 3) >> 2) + dd;
                l = ((o + xl + rl + gl) * 25000) >> 15;
                r = ((o + xr + rr + gr) * 25000) >> 15;
                {                                               /* THE BUS: glue, then a soft limit, no hard clip */
                    const int32_t al = l < 0 ? -l : l, ar = r < 0 ? -r : r, a = al > ar ? al : ar;
                    menv += a > menv ? (a - menv) >> 5 : -(menv >> 13);
                    l = (l * mg) >> 15; r = (r * mg) >> 15;
                }
                out[2u * (off + i)] += soft((l * 2) / 3);       /* unity when quiet, round up to +3.5 dB over full scale */
                out[2u * (off + i) + 1u] += soft((r * 2) / 3);
            }
        }
    }
    dk_pos_v = (uint32_t)dk.pos;
    dk_on_v = dk.mode != DK_OFF;
    dk_fader_v = (uint32_t)dk.fader;
    dk_rot_v = rot;
    {
        const int32_t r = dk.mode == DK_OFF || dk.mode == DK_REST ? 0 : dk.rate < 0 ? -dk.rate : dk.rate;
        dk_speed_v = (uint32_t)(r / ((p_rate >> 4) + 1));
    }
#ifndef DINGHY_HOST
    {                                           /* the audio load: this half's render time / its length, peak held */
        static uint32_t calls;
        const uint32_t pc = (fm1_ticks() - t_in) * 100u / (HALF_FRAMES * 1000000u / FS * FM1_TICKS_PER_US);
        if (pc > load_v) load_v = pc;
        else if (!(++calls & 63u) && load_v) load_v--;   /* the peak falls back ~3 % a second */
    }
#endif
    for (i = 0; i < 4u; i++) if (pk[i] > mt_v[i]) mt_v[i] = pk[i];   /* main takes them and lets them fall */
    dk_scr_v = dk.mode == DK_SCR; dk_tech_v = dk.tech; dk_ph_v = dk.ph;
    fx_state_v = (dk.mode == DK_SPIN ? ST_SPIN : 0u) | (braking ? ST_BRAKE : 0u) | (muted ? ST_MUTED : 0u) | (ext_wait ? ST_WAIT : 0u);
}

/* =========================================================================== the screen */
#define C_PINK RGB(150, 255, 0)                 /* toxic green */
#define C_CYAN RGB(255, 90, 20)                 /* hot orange */
#define C_GRY  RGB(100, 105, 95)
#define C_TXT  RGB(225, 230, 220)
#define C_DEEP RGB(24, 36, 20)
#define C_MID  RGB(66, 92, 50)
#define C_YEL  RGB(255, 220, 0)
#define C_RED  RGB(255, 50, 50)
static const uint16_t FX_COL[FX_COUNT] = {RGB(150, 255, 0), RGB(255, 140, 0), RGB(0, 230, 255), RGB(220, 30, 30),
                                          RGB(255, 60, 60), RGB(255, 220, 0), RGB(255, 40, 160), RGB(190, 120, 255),
                                          RGB(200, 120, 40)};

static char *put(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *put_u(char *d, uint32_t u)
{
    char t[10];
    uint32_t i = 0;
    do t[i++] = (char)('0' + u % 10u); while (u /= 10u);
    while (i) *d++ = t[--i];
    *d = 0;
    return d;
}
static char *put_s(char *d, int32_t v) { *d++ = v < 0 ? '-' : '+'; return put_u(d, (uint32_t)(v < 0 ? -v : v)); }

static void draw_progress(uint32_t done, uint32_t all)
{
    cv_begin(240, 20, C_BLACK);
    cv_rect(20, 6, 200, 8, C_DEEP);
    cv_rect(20, 6, (int32_t)(200u * done / all), 8, C_PINK);
    cv_blit(0, 150);
}

/* the top bar: the name, what is happening now (or the mode and tempo), REC */
static void draw_top(const char *now, uint16_t now_c, uint32_t rec)
{
    cv_begin(240, 20, C_BLACK);
    cv_text(4, 2, &FONT_S, "NU//METAL", C_PINK);
    cv_text(236 - text_w(&FONT_S, now), 2, &FONT_S, now, now_c);
    if (rec) cv_rect(124, 6, 8, 8, C_RED);
    cv_rect(0, 19, 240, 1, C_DEEP);
    cv_blit(0, 0);
}

/* the big line: the word, or a value just turned */
static void draw_big(const char *s, uint16_t c)
{
    if (text_w(&FONT_L, s) <= 236) draw_text_box(0, 22, 240, &FONT_L, s, c, 1);
    else { lcd_fill(0, 22, 240, 36, C_BLACK); draw_text_box(0, 30, 240, &FONT_S, s, c, 1); }
}

static uint8_t wave_env[160];
static int32_t wave_word = -2;
static void wave_cache(int32_t w)
{
    uint32_t x, len = w >= 0 ? bank[w].len : 0;
    wave_word = w;
    for (x = 0; x < 160u; x++) {
        uint32_t a = x * len / 160u, b = (x + 1u) * len / 160u, i, pk = 0;
        for (i = a; i < b; i++) {
            int32_t v = wpool[bank[w].off + i];
            uint32_t av = (uint32_t)(v < 0 ? -v : v);
            if (av > pk) pk = av;
        }
        wave_env[x] = (uint8_t)(pk * 28u / 128u);
    }
}

static int8_t ring_x[48], ring_y[48];
static void platter(int32_t cx, int32_t cy, uint32_t rot, uint32_t on, uint32_t fader, uint16_t acc)
{
    uint32_t k;
    int32_t x, y;
    for (k = 0; k < 48u; k++) {                 /* the grooves */
        cv_pset(cx + ring_x[k], cy + ring_y[k], C_GRY);
        if (k & 1u) cv_pset(cx + ring_x[k] * 3 / 4, cy + ring_y[k] * 3 / 4, C_DEEP);
        if (!(k % 3u)) cv_pset(cx + ring_x[k] / 2, cy + ring_y[k] / 2, C_MID);
    }
    cv_rect(cx - 6, cy - 6, 13, 13, on ? acc : C_DEEP);           /* the label */
    cv_rect(cx - 1, cy - 1, 3, 3, C_BLACK);
    x = cx + ((sine(rot) * 27) >> 15);
    y = cy + ((sine(rot + 0x40000000u) * 27) >> 15);
    cv_line(cx, cy, x, y, fader > 16000u ? C_TXT : C_GRY);       /* the sticker, turning with the record */
    cv_line(cx + 30, cy - 30, cx + 22, cy - 6, C_GRY);           /* the tonearm */
    cv_rect(cx + 20, cy - 8, 4, 5, on ? C_TXT : C_GRY);
}

static void score(int32_t x0, int32_t wd, uint32_t t, int32_t head, uint16_t acc)   /* a technique as TTM: time across, the record up, the fader as the line's breaks */
{
    const uint32_t cyc = (uint32_t)TECH[t].beats << 16;
    int32_t x, py = -1;
    for (x = 0; x < wd; x++) {
        const uint32_t ph = (uint32_t)x * (cyc / (uint32_t)wd);
        const int32_t y = 54 - tech_pos(t, ph) * 46 / 32767, open = (int32_t)tech_open(t, ph);
        if (py >= 0) cv_line(x0 + x - 1, py, x0 + x, y, open ? (x < head ? C_TXT : acc) : C_DEEP);
        cv_rect(x0 + x, 60, 1, 3, open ? acc : C_DEEP);
        py = y;
    }
    for (x = 1; x < (int32_t)TECH[t].beats * 4; x++) cv_rect(x0 + x * wd / ((int32_t)TECH[t].beats * 4), 58, 1, 1, C_MID);
    if (head >= 0 && head < wd) cv_rect(x0 + head, 0, 1, 58, C_YEL);
}

static void draw_deck(int32_t w, uint32_t pos, uint32_t rot, uint32_t on, uint32_t fader, uint16_t acc)
{
    uint32_t k, len = w >= 0 ? bank[w].len : 0, head;
    cv_begin(240, 64, C_BLACK);
    platter(34, 32, rot, on, fader, acc);
    if (dk_scr_v) {                             /* scratching: the technique's score, the playhead on it */
        const uint32_t t = dk_tech_v % NTECH;
        score(78, 158, t, (int32_t)((dk_ph_v >> 6) * 158u / ((uint32_t)TECH[t].beats << 10)), acc);
        cv_text(236 - text_w(&FONT_S, SC_NAME[t]), 0, &FONT_S, SC_NAME[t], C_YEL);
        cv_blit(0, 60);
        return;
    }
    {                                           /* the velocity meter: 1x at a quarter, 4x full */
        const int32_t h = dk_speed_v > 64u ? 60 : (int32_t)(dk_speed_v * 60u / 64u);
        cv_rect(69, 2, 3, 60, C_DEEP);
        cv_rect(69, 62 - h, 3, h, h > 30 ? C_PINK : h > 15 ? C_YEL : C_CYAN);
    }
    if (w != wave_word && w >= 0) wave_cache(w);
    head = len ? (uint32_t)((pos >> 16) * 160u / len) : 999u;
    for (k = 0; k < 160u; k++) {
        int32_t h = w >= 0 ? wave_env[k] : 0;
        cv_rect(76 + (int32_t)k, 32 - h, 1, 2 * h + 1, !on ? C_MID : k < head ? acc : C_DEEP);
    }
    cv_rect(76, 32, 160, 1, C_MID);
    if (on && head < 160u) cv_rect(76 + (int32_t)head, 0, 2, 64, fader > 16000u ? C_TXT : C_PINK);
    cv_blit(0, 60);
}

static void draw_pads(const uint32_t *hit_t, uint32_t now)   /* BEAT mode: the kit as 12 pads that flash */
{
    static const char *const PAD[NDRUM] = {"KICK", "SNARE", "CLAP", "HAT", "OPEN", "RIM", "LTOM", "HTOM", "808", "CHINA", "ZAP", "CRASH"};
    uint32_t d;
    cv_begin(240, 64, C_BLACK);
    for (d = 0; d < NDRUM; d++) {
        int32_t x = (int32_t)(d % 6u) * 40, y = (int32_t)(d / 6u) * 32;
        uint32_t age = now - hit_t[d];
        uint16_t c = age < 90u ? C_PINK : age < 220u ? C_MID : C_DEEP;
        cv_rect(x + 1, y + 1, 38, 30, c);
        cv_text(x + 20 - text_w(&FONT_S, PAD[d]) / 2, y + 8, &FONT_S, PAD[d], age < 90u ? C_BLACK : C_TXT);
    }
    cv_blit(0, 60);
}

static void draw_score_preview(uint32_t t, uint16_t acc)   /* STYLE just turned: what it will play */
{
    cv_begin(240, 64, C_BLACK);
    score(8, 224, t, -1, acc);
    cv_blit(0, 60);
}

static int32_t lfo_shape_at(uint32_t shape, uint32_t t)   /* the drawing of a shape, t 0..65535 */
{
    static const int16_t RS[4] = {18000, -9000, 29000, -25000};
    switch (shape) {
    case LS_TRI: return t < 16384u ? (int32_t)t * 2 : t < 49152u ? 32768 - ((int32_t)t - 16384) * 2 : ((int32_t)t - 65536) * 2;
    case LS_UP: return (int32_t)t - 32768;
    case LS_DOWN: return 32767 - (int32_t)t;
    case LS_SQUARE: return t < 32768u ? 32767 : -32767;
    case LS_SH: return RS[t >> 14];
    case LS_DRIFT: { const int32_t a = RS[t >> 14], b = RS[((t >> 14) + 1u) & 3u]; return a + (((b - a) * (int32_t)bump((t & 16383u) << 1)) >> 15); }
    default: return sine(t << 16);
    }
}
static void draw_lfo(uint32_t on, uint32_t shape, const char *rate, const char *dest, uint16_t acc)   /* LFO held: the PRO page */
{
    const uint32_t many = shape == LS_SH || shape == LS_DRIFT ? 1u : 2u;   /* two cycles; the random ones as four steps */
    int32_t x, py = 0;
    cv_begin(240, 64, C_BLACK);
    cv_rect(0, 32, 240, 1, C_DEEP);
    for (x = 0; x < 240; x++) {
        const int32_t y = 32 - lfo_shape_at(shape, (uint32_t)x * many * 65536u / 240u & 0xFFFFu) * 26 / 32767;
        if (x) cv_line(x - 1, py, x, y, on ? acc : C_MID);
        py = y;
    }
    if (on) {
        const int32_t hx = (int32_t)((lfo_ph_v >> 16) * 240u / 65536u / many), hy = 32 - lfo_v_v * 26 / 32767;
        cv_rect(hx - 2, hy - 2, 5, 5, C_TXT);
    }
    cv_text(2, 0, &FONT_S, rate, C_YEL);
    cv_text(238 - text_w(&FONT_S, dest), 0, &FONT_S, dest, C_YEL);
    if (!on) cv_text(120 - text_w(&FONT_S, "TAP LFO = ON") / 2, 46, &FONT_S, "TAP LFO = ON", C_GRY);
    cv_blit(0, 60);
}

static void draw_mixer(const uint8_t *lv, const uint8_t *of, const uint32_t *mt, uint16_t acc)   /* FX held: the MIXER */
{
    static const char *const CH[7] = {"VOX", "GTR", "BAS", "DRM", "VRB", "ECH", "WID"};
    uint32_t k;
    cv_begin(240, 64, C_BLACK);
    for (k = 0; k < 7u; k++) {
        const int32_t x = (int32_t)k * 34 + 2, top = 2, h = 42, cap = top + h - (int32_t)((uint32_t)h * lv[k] / of[k]);
        cv_rect(x + 9, top, 2, h, C_DEEP);                  /* the fader's slot */
        if (k < 4u) {                                       /* the meter: green, yellow, red at the top */
            const int32_t mh = mt[k] > 32767u ? h : (int32_t)(mt[k] * (uint32_t)h / 32767u);
            cv_rect(x + 20, top, 4, h, C_DEEP);
            cv_rect(x + 20, top + h - mh, 4, mh, mh > h * 7 / 8 ? C_RED : mh > h * 2 / 3 ? C_YEL : C_PINK);
        }
        cv_rect(x + 4, cap - 2, 12, 5, k == 0u ? acc : C_TXT);   /* the cap */
        cv_text(x + 16 - text_w(&FONT_S, CH[k]) / 2, 47, &FONT_S, CH[k], C_GRY);
    }
    cv_blit(0, 60);
}

static void draw_cheat(void)                    /* EDIT held: what the buttons do now */
{
    static const char *const L[8] = {"OCT-/+ SPIN/BRK", "PLAY TAP BPM", "ARP  MUTATE", "SEQ/REC CLEAR",
                                     "GLO  HARD", "ENV  FRZ LATCH", "LFO CUT SEL HAM", "SAVE SYNC"};
    uint32_t k;
    cv_begin(240, 64, C_DEEP);
    for (k = 0; k < 8u; k++) cv_text(4 + (int32_t)(k & 1u) * 120, (int32_t)(k >> 1) * 16, &FONT_S, L[k], C_YEL);
    cv_blit(0, 60);
}

static void draw_steps(uint32_t cur, uint32_t play, uint32_t hits_now)
{
    uint32_t s;
    cv_begin(240, 20, C_BLACK);
    for (s = 0; s < 16u; s++) {
        int32_t x = 4 + (int32_t)s * 14 + (int32_t)(s >> 2) * 2;
        uint16_t c = seq_word[s] >= 0 ? (seq_flag[s] & SEQ_SCR ? C_YEL : C_CYAN) : C_DEEP;
        cv_rect(x, 3, 12, 12, c);
        if (seq_drum[s]) cv_rect(x + 4, 7, 4, 4, C_PINK);
        if (rap_line[s] >= 0) cv_rect(x + 2, 13, 8, 2, C_TXT);   /* RAP MODE: where the MC says a word */
        if (play && s == cur) {
            cv_rect(x, 16, 12, 3, hits_now ? C_PINK : C_TXT);
            cv_rect(x, 0, 12, 2, C_TXT);
        }
    }
    cv_blit(0, 126);
}

typedef struct { const char *label; char val[12]; uint32_t bar, of; } cell_t;
static void cell_draw(int32_t x, int32_t y, int32_t w, const cell_t *c, uint16_t lc, uint16_t vc)
{
    cv_text(x + 3, y, &FONT_S, c->label, lc);
    cv_text(x + w - 3 - text_w(&FONT_S, c->val), y + 14, &FONT_S, c->val, vc);
    if (c->of) {
        cv_rect(x + 3, y + 30, w - 6, 1, C_DEEP);
        cv_rect(x + 3, y + 30, (int32_t)((uint32_t)(w - 6) * c->bar / c->of), 1, vc);
    }
}
static void draw_knobs(const cell_t *row_a, const cell_t *row_b, uint32_t shift, uint16_t acc)
{
    uint32_t k;
    const uint16_t lc = shift ? C_YEL : C_GRY;
    cv_begin(240, 64, C_BLACK);
    for (k = 0; k < 4u; k++) cell_draw((int32_t)k * 60, 0, 60, &row_a[k], lc, k == 0 && !shift ? acc : C_TXT);
    for (k = 0; k < 3u; k++) cell_draw((int32_t)k * 80, 32, 80, &row_b[k], lc, C_TXT);
    for (k = 1; k < 4u; k++) cv_rect((int32_t)k * 60, 2, 1, 26, C_DEEP);
    for (k = 1; k < 3u; k++) cv_rect((int32_t)k * 80, 34, 1, 26, C_DEEP);
    cv_blit(0, 150);
}

static int32_t chip(int32_t x, const char *s, uint32_t on, uint16_t c)
{
    int32_t w = text_w(&FONT_S, s) + 6;
    if (on) cv_rect(x, 2, w, 20, c);
    else { cv_rect(x, 2, w, 1, C_DEEP); cv_rect(x, 21, w, 1, C_DEEP); }
    cv_text(x + 3, 4, &FONT_S, s, on ? C_BLACK : C_GRY);
    return x + w + 3;
}
static const char *const MODE_NAME[3] = {"DJ", "BEAT", "TONE"};
enum { M_DJ, M_BEAT, M_TONE };
static void draw_chips(uint32_t rec, uint32_t aut, uint32_t rap, uint32_t mode, uint32_t lfo, uint32_t ham, int32_t semi)
{
    char p[6] = "P", *q = p + 1;
    int32_t x = 2;
    put_s(q, semi);
    cv_begin(240, 24, C_BLACK);
    x = chip(x, "REC", rec, C_RED);
    x = chip(x, "RAP", rap, C_YEL);
    x = chip(x, "AUTO", aut, C_CYAN);
    x = chip(x, MODE_NAME[mode], 1, mode == M_TONE ? C_YEL : C_MID);
    x = chip(x, ham ? "HAM" : "LFO", ham ? 1u : lfo, ham ? C_RED : C_PINK);
    if (ham) x = chip(x, "LFO", lfo, C_PINK);
    if (x + 40 <= 240) chip(x, p, semi != 0, C_TXT);
    cv_blit(0, 216);
}

/* =========================================================================== the main loop */
#ifdef DINGHY_HOST
#define NOW_US() (fm1_ms * 1000u)
#else
#define NOW_US() fm1_micros()
#endif
static const char *const DROP_NAME[13] = {"F", "F#", "G", "G#", "A", "A#", "B", "C", "C#", "D", "D#", "E", "F"};
static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static void deck_cmd(uint32_t kind, uint32_t arg) { cmd_arg = arg; cmd_kind = kind; cmd_n++; }
static void drums_now(uint32_t mask) { drum_mask = mask; drum_n++; }
static uint32_t lcg = 0x2468ACE1u;
static uint32_t rnd(uint32_t n) { lcg = lcg * 1103515245u + 12345u; return (lcg >> 8) % n; }

static void mutate(void)                        /* MUTATE: ghost notes, a new word or two, a scratch */
{
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        uint32_t s = rnd(16), r = rnd(10);
        static const uint8_t GHOST[6] = {D_SNARE, D_KICK, D_RIM, D_HAT, D_OPEN, D_ZAP};
        if (r < 6u) seq_drum[s] ^= (uint16_t)(1u << GHOST[rnd(6)]);
    }
    if (rnd(3)) { uint32_t s = rnd(4) * 4u + (rnd(2) ? 2u : 0u); seq_word[s] = (int8_t)rnd(NWORDS); seq_flag[s] = rnd(4) == 0 ? SEQ_SCR : 0; }
    if (!rnd(3)) { uint32_t s = rnd(16); seq_word[s] = -1; }
}

#define NTSRC 7
static const int8_t TSRC[NTSRC] = {TONE_WORD, BUZZ_WORD, AHH_WORD, FRESH_WORD, 23, 14, -1};   /* TONE mode's records; -1 = the last word */
static const char *const TSRC_NAME[NTSRC] = {"TONE", "BUZZ", "AHHH", "FRESH", "YEAH", "SCREAM", "LAST WORD"};

static void app_main(void)
{
    struct {                                    /* what survives a power-off */
        uint8_t fx, amt[FX_COUNT], echo, etime, style, reach, voice, groove, hard, beat, aut, swing, tune, decay, dlvl, vlvl, sync;
        uint8_t rap, glvl, hook, pad2;
        uint8_t lfo_on, lfo_rate, lfo_shape, lfo_depth, lfo_dest, curve, torque, hamster, tsrc, space, blvl, width, ver, pad3[3];
        int8_t semi, pad;
        uint16_t bpm;
        int8_t word[16];
        uint8_t flag[16];
        uint16_t drum[16];
    } set;
    uint32_t prev = 0, k, s, t_change = 0, dirty = 0, play = 0, rec = 0, shift = 0, shift_was = 0;
    uint32_t fx_held = 0, fx_turned = 0, t_fx = 0, mt[4] = {0, 0, 0, 0};
    uint32_t lfo_held = 0, lfo_turned = 0, t_lfo = 0, t_score = 0, last_word = AHH_WORD, tone_on = 0;
    uint32_t frz_latch = 0, knobs = 1, chips = 1, top_key = ~0u, t_draw = 0, steps_drawn = ~0u;
    uint32_t t_jog = 0, t_voice = 0, voice_pending = 0, t_toast = 0, toast_on = 0, deck_view = ~0u;
    uint32_t t_down[27] = {0}, scr[27] = {0}, qstep[27] = {0}, hit_t[NDRUM] = {0}, taps[4] = {0}, ntap = 0;
    int32_t word_drawn = -2, held_word[27];
    char toast[24];
    int32_t jog_v = 0, fx_boost = 0, last_tap_w = -1, boost_shown = 0;
    uint32_t t_boost = 0, t_tap = 0, tap_n = 0, t_loop = 0;
    uint32_t ext_us[49], ext_n = 0, ext_ticks = 0, t_tick = 0, ext_live = 0, ext_bpm = 0, ext_len = 0;

    for (k = 0; k < sizeof set; k++) ((uint8_t *)&set)[k] = 0;
    for (k = 0; k < FX_COUNT; k++) set.amt[k] = 7;
    set.echo = 3; set.etime = 2; set.reach = 3; set.groove = 1; set.hard = 1; set.bpm = 100;
    set.glvl = 11; set.hook = ADLIB;
    set.lfo_rate = 6; set.lfo_depth = 8; set.torque = 3; set.space = 7; set.blvl = 11; set.width = 15; set.ver = 2;
    set.tune = 6; set.decay = 2; set.dlvl = 12; set.vlvl = 12;
    for (s = 0; s < 16u; s++) set.word[s] = -1;
    settings_load(&set, sizeof set);
    set.fx %= FX_COUNT; set.style %= SC_COUNT; set.voice %= NVOICES; set.groove %= NGROOVES; set.etime &= 3u;
    for (k = 0; k < FX_COUNT; k++) if (set.amt[k] > 15u) set.amt[k] = 7;
    if (set.echo > 15u) set.echo = 4;
    if (set.reach > 7u) set.reach = 3;
    if (set.swing > 10u) set.swing = 0;
    if (set.tune > 12u) set.tune = 6;
    if (set.decay > 4u) set.decay = 2;
    if (set.dlvl > 15u) set.dlvl = 12;
    if (set.vlvl > 15u) set.vlvl = 12;
    if (set.glvl > 15u) set.glvl = 11;
    if (set.hook >= NKEYW) set.hook = ADLIB;
    set.beat %= 3u; set.lfo_on = set.lfo_on ? 1u : 0u; set.lfo_rate %= NLFO_RATE; set.lfo_shape %= LS_COUNT;
    set.lfo_depth &= 15u; set.lfo_dest %= LD_COUNT; set.curve %= 3u; set.torque &= 3u; set.hamster = set.hamster ? 1u : 0u;
    set.tsrc %= NTSRC;
    if (set.space > 15u) set.space = 7;
    if (set.ver != 2u) { set.blvl = 11; set.width = 15; set.ver = 2; }   /* saved before the MIXER */
    if (set.blvl > 15u) set.blvl = 11;
    if (set.width > 15u) set.width = 15;
    set.rap = set.rap ? 1u : 0u;
    set.sync = set.sync ? 1u : 0u;
    set.semi = (int8_t)clampi(set.semi, -12, 12);
    set.bpm = (uint16_t)clampi(set.bpm, 60, 180);
    for (s = 0; s < 16u; s++) {
        if (set.word[s] >= NKEYW) set.word[s] = -1;
        seq_word[s] = set.word[s]; seq_flag[s] = set.flag[s]; seq_drum[s] = set.drum[s] & 0x0FFFu;
        rap_line[s] = -1;
    }
    for (k = 0; k < 27u; k++) held_word[k] = -1;
    for (k = 0; k < NGROOVES; k++)              /* the grooves' lanes into drum masks */
        for (s = 0; s < 16u; s++) {
            uint32_t l, m = 0;
            for (l = 0; l < 4u; l++) if (GROOVES[k].lane[l][s] == 'x') m |= 1u << GROOVES[k].drum[l];
            groove_mask[k][s] = (uint16_t)m;
        }
    for (k = 0; k < NTECH; k++) {               /* the techniques' faders into bits */
        const uint8_t *f = TECH[k].fd;
        uint32_t a, j;
        tech_clicks[k] = f != OPEN_ALL;
        for (j = 0; j < 4u; j++) tech_fd[k][j] = f ? 0u : ~0u;
        for (; f && f[0] != KP_END; f += 2)
            for (a = f[0]; a < f[1] && a < 128u; a++) tech_fd[k][a >> 5] |= 1u << (a & 31u);
        if (TECH[k].beats == 1u) { tech_fd[k][2] = tech_fd[k][0]; tech_fd[k][3] = tech_fd[k][1]; }
    }
    for (k = 0; k < 48u; k++) {
        ring_x[k] = (int8_t)((sine(k * 89478485u) * 29) >> 15);
        ring_y[k] = (int8_t)((sine(k * 89478485u + 0x40000000u) * 29) >> 15);
    }
    lcg ^= fm1_ms * 2654435761u;

    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 44, 240, &FONT_L, "NU//METAL", C_PINK, 1);
    draw_text_box(0, 84, 240, &FONT_L, "SKRATCH", C_CYAN, 1);
    draw_text_box(0, 116, 240, &FONT_L, "MASTER", C_CYAN, 1);
    draw_text_box(0, 150, 240, &FONT_L, "PRO", C_YEL, 1);
    draw_text_box(0, 190, 240, &FONT_S, "tuning down to B", C_GRY, 1);
    make_tones();
    render_bank(set.voice);
    deck_cmd(CMD_SAY, NAME_WORD);               /* it says its name */
    lcd_fill(0, 0, 240, 240, C_BLACK);

    for (;;) {
        uint32_t keys, down, up, rel, b, btn, now;
        int32_t e;
        uint8_t st, d1, d2;
        platform_poll();
        now = fm1_ms;
        btn = fm1_in.buttons;
        shift = btn >> BTN_EDIT & 1u;
        if (shift != shift_was) { shift_was = shift; knobs = 1; deck_view = ~0u; }

#define TOAST(name, v) do { char *tp_ = put(toast, name); tp_ = put(tp_, " "); put(tp_, v); toast_on = 1; t_toast = now; } while (0)
        /* ---- encoders: two layers, EDIT held = the second */
        {
            char v[12];
            const uint32_t mix = fx_held && !shift && !lfo_held, pro = (lfo_held && !shift) || mix;
            if (mix) {                                      /* FX held: the MIXER */
                int32_t e2;
                uint32_t turned = 0;
                if ((e2 = fm1_enc_take(ENC_KNOB1)) != 0) { turned = 1; set.vlvl = (uint8_t)clampi(set.vlvl + e2, 0, 15); put_u(v, set.vlvl); TOAST("VOX", v); }
                if ((e2 = fm1_enc_take(ENC_KNOB2)) != 0) { turned = 1; set.glvl = (uint8_t)clampi(set.glvl + e2, 0, 15); put_u(v, set.glvl); TOAST("GUITAR", v); }
                if ((e2 = fm1_enc_take(ENC_KNOB3)) != 0) { turned = 1; set.blvl = (uint8_t)clampi(set.blvl + e2, 0, 15); put_u(v, set.blvl); TOAST("BASS", v); }
                if ((e2 = fm1_enc_take(ENC_KNOB4)) != 0) { turned = 1; set.dlvl = (uint8_t)clampi(set.dlvl + e2, 0, 15); put_u(v, set.dlvl); TOAST("DRUMS", v); }
                if ((e2 = fm1_enc_take(ENC_SELECT)) != 0) { turned = 1; set.space = (uint8_t)clampi(set.space + e2, 0, 15); put_u(v, set.space); TOAST("VERB", v); }
                if ((e2 = fm1_enc_take(ENC_ALGORITHM)) != 0) { turned = 1; set.echo = (uint8_t)clampi(set.echo + e2, 0, 15); put_u(v, set.echo); TOAST("ECHO", v); }
                if ((e2 = fm1_enc_take(ENC_PRESETS)) != 0) { turned = 1; set.width = (uint8_t)clampi(set.width + e2, 0, 15); put_u(v, set.width); TOAST("WIDTH", v); }
                if (turned) { fx_turned = 1; knobs = dirty = 1; }
            }
            else if (pro) {                                 /* LFO held: the PRO page */
                int32_t e2;
                uint32_t turned = 0;
                if ((e2 = fm1_enc_take(ENC_SELECT)) != 0) { turned = 1; set.space = (uint8_t)clampi(set.space + e2, 0, 15); put_u(v, set.space); TOAST("SPACE", v); }
                if ((e2 = fm1_enc_take(ENC_ALGORITHM)) != 0) { turned = 1; set.curve = (uint8_t)clampi(set.curve + e2, 0, 2); TOAST("CURVE", CURVE_NAME[set.curve]); }
                if ((e2 = fm1_enc_take(ENC_KNOB1)) != 0) { turned = 1; set.lfo_rate = (uint8_t)clampi(set.lfo_rate + e2, 0, NLFO_RATE - 1); TOAST("LFO", LFO_RATE_NAME[set.lfo_rate]); }
                if ((e2 = fm1_enc_take(ENC_KNOB2)) != 0) { turned = 1; set.lfo_shape = (uint8_t)clampi(set.lfo_shape + e2, 0, LS_COUNT - 1); TOAST("LFO", LFO_SHAPE_NAME[set.lfo_shape]); }
                if ((e2 = fm1_enc_take(ENC_KNOB3)) != 0) { turned = 1; set.lfo_depth = (uint8_t)clampi(set.lfo_depth + e2, 0, 15); put_u(v, set.lfo_depth); TOAST("DEPTH", v); }
                if ((e2 = fm1_enc_take(ENC_KNOB4)) != 0) { turned = 1; set.lfo_dest = (uint8_t)clampi(set.lfo_dest + e2, 0, LD_COUNT - 1); TOAST("LFO >", LFO_DEST_NAME[set.lfo_dest]); }
                if ((e2 = fm1_enc_take(ENC_PRESETS)) != 0) { turned = 1; set.torque = (uint8_t)clampi(set.torque + e2, 0, 3); TOAST("TORQUE", TORQUE_NAME[set.torque]); }
                if (turned) { lfo_turned = 1; knobs = dirty = 1; }
            }
            if (!pro && (e = fm1_enc_take(ENC_SELECT)) != 0) {     /* the jog, and how fast it turns (clicks a second) */
                const uint32_t dt = now - t_jog < 1u ? 1u : now - t_jog;
                const int32_t inst = dt > 300u ? e * 3 : e * 1000 / (int32_t)dt;
                jog_v = (jog_v * 3 + inst) / 4;
                t_jog = now;
                if (shift) {                                /* VELOCITY FX: the flick pumps the FX */
                    const int32_t av = jog_v < 0 ? -jog_v : jog_v;
                    fx_boost += e * (64 + av * 12);
                    t_boost = now;
                } else {
                    const int32_t av = jog_v < 0 ? -jog_v : jog_v;
                    p_jog_gain = av * 10 + 100 > 1600 ? 1600 : av * 10 + 100;
                    jog_total += e; jog_held = 1;
                }
            } else if (jog_held && now - t_jog > ((jog_v > 12 || jog_v < -12) ? 60u : 300u)) {   /* let go: a fast flick throws the record */
                const int32_t av = jog_v < 0 ? -jog_v : jog_v;
                jog_held = 0;
                if (av > 12) {
                    int32_t r = jog_v * p_jog_gain * 3 / 2;      /* clicks/s * samples/click / 44100 * 65536 */
                    r = r > 16 * p_rate ? 16 * p_rate : r < -16 * p_rate ? -16 * p_rate : r;
                    p_throw_rate = r;
                    p_throw++;
                }
                jog_v = 0;
            } else if (now - t_jog > 300u) jog_v = 0;
            if (!pro && (e = fm1_enc_take(ENC_ALGORITHM)) != 0) {
                if (shift) { set.swing = (uint8_t)clampi(set.swing + e, 0, 10); put_u(v, set.swing * 10u); TOAST("SWING", v); }
                else if (set.beat == M_TONE) { set.tsrc = (uint8_t)clampi(set.tsrc + e, 0, NTSRC - 1); TOAST("", TSRC_NAME[set.tsrc]); }
                else {
                    set.style = (uint8_t)clampi(set.style + e, 0, SC_COUNT - 1); TOAST("", SC_NAME[set.style]);
                    t_score = now; deck_view = ~0u;
                }
                knobs = dirty = 1;
            }
            if (pro) {}
            else if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {
                if (shift) { set.tune = (uint8_t)clampi(set.tune + e, 0, 12); TOAST("DROP", DROP_NAME[set.tune]); }
                else { set.amt[set.fx] = (uint8_t)clampi(set.amt[set.fx] + e, 0, 15); put_u(v, set.amt[set.fx]); TOAST(FX_NAME[set.fx], v); }
                knobs = dirty = 1;
            }
            if (!pro && (e = fm1_enc_take(ENC_KNOB2)) != 0) {
                if (shift) { set.echo = (uint8_t)clampi(set.echo + e, 0, 15); put_u(v, set.echo); TOAST("ECHO", v); }
                else { set.glvl = (uint8_t)clampi(set.glvl + e, 0, 15); put_u(v, set.glvl); TOAST("GUITAR", v); }
                knobs = dirty = 1;
            }
            if (!pro && (e = fm1_enc_take(ENC_KNOB3)) != 0) {
                if (shift) { set.decay = (uint8_t)clampi(set.decay + e, 0, 4); put_s(v, (int32_t)set.decay - 2); TOAST("DECAY", v); }
                else { set.bpm = (uint16_t)clampi(set.bpm + e, 60, 180); put_u(v, set.bpm); TOAST("BPM", v); }
                knobs = dirty = 1;
            }
            if (!pro && (e = fm1_enc_take(ENC_KNOB4)) != 0) {
                if (shift) { set.dlvl = (uint8_t)clampi(set.dlvl + e, 0, 15); put_u(v, set.dlvl); TOAST("DRUMS", v); }
                else { set.voice = (uint8_t)clampi(set.voice + e, 0, NVOICES - 1); voice_pending = 1; t_voice = now; TOAST("", VOICES[set.voice].name); }
                knobs = dirty = 1;
            }
            if (!pro && (e = fm1_enc_take(ENC_PRESETS)) != 0) {
                if (shift) { set.reach = (uint8_t)clampi(set.reach + e, 0, 7); put_u(v, set.reach + 1u); TOAST("REACH", v); }
                else { set.groove = (uint8_t)clampi(set.groove + e, 0, NGROOVES - 1); TOAST("", GROOVES[set.groove].name); }
                knobs = dirty = 1;
            }
        }

        /* ---- buttons: taps, and with EDIT held their second function */
        b = fm1_input_edges(&rel);
        if (shift) {
            if (b & (1u << BTN_OCT_DOWN)) { p_spin++; TOAST("", "BACKSPIN"); }
            if (b & (1u << BTN_OCT_UP)) { p_brake++; TOAST("", "BRAKE"); }
            if (b & (1u << BTN_PLAY)) {                 /* TAP tempo: the last four taps */
                if (ntap && now - taps[(ntap - 1u) & 3u] > 2000u) ntap = 0;
                taps[ntap & 3u] = now;
                ntap++;
                if (ntap >= 2u) {
                    uint32_t nn = ntap < 4u ? ntap : 4u, span = taps[(ntap - 1u) & 3u] - taps[(ntap - nn) & 3u];
                    char v[8];
                    if (span) set.bpm = (uint16_t)clampi((int32_t)(60000u * (nn - 1u) / span), 60, 180);
                    put_u(v, set.bpm); TOAST("TAP", v); knobs = dirty = 1;
                } else TOAST("", "TAP");
            }
            if (b & (1u << BTN_ARP)) { mutate(); TOAST("", "MUTATE"); dirty = 1; }
            if (b & (1u << BTN_SEQ)) { for (s = 0; s < 16u; s++) { seq_word[s] = -1; seq_flag[s] = 0; seq_drum[s] = 0; } TOAST("", "CLEARED"); dirty = 1; }
            if (b & (1u << BTN_REC)) { for (s = 0; s < 16u; s++) { seq_word[s] = -1; seq_flag[s] = 0; } TOAST("", "NO WORDS"); dirty = 1; }
            if (b & (1u << BTN_ENV)) { frz_latch = !frz_latch; TOAST("FREEZE", frz_latch ? "LATCH" : "OFF"); }
            if (b & (1u << BTN_HOME)) { set.semi = 0; chips = dirty = 1; TOAST("PITCH", "0"); }
            if (b & (1u << BTN_FX)) { set.fx = (uint8_t)((set.fx + FX_COUNT - 1u) % FX_COUNT); knobs = dirty = 1; TOAST("", FX_NAME[set.fx]); }
            if (b & (1u << BTN_SEL)) { set.hamster = !set.hamster; chips = dirty = 1; t_change = now; TOAST("HAMSTER", set.hamster ? "ON" : "OFF"); }
            if (b & (1u << BTN_GLO)) { set.hard = !set.hard; dirty = 1; t_change = now; TOAST("HARD", set.hard ? "ON" : "OFF"); }
        } else {
            if (b & (1u << BTN_PLAY)) { play = !play; if (play) p_restart++; chips = 1; }
            if (b & (1u << BTN_REC)) { rec = !rec; chips = 1; }
            if (b & (1u << BTN_ARP)) { set.rap = !set.rap; chips = dirty = 1; TOAST("RAP", set.rap ? "MODE" : "OFF"); }
            if (b & (1u << BTN_GLO)) { set.aut = !set.aut; chips = dirty = 1; TOAST("AUTO", set.aut ? "ON" : "OFF"); }
            if (b & (1u << BTN_FX)) { fx_held = 1; fx_turned = 0; t_fx = now; knobs = 1; deck_view = ~0u; }
            if (b & (1u << BTN_HOME)) {
                set.beat = (uint8_t)((set.beat + 1u) % 3u); chips = knobs = dirty = 1; deck_view = ~0u;
                if (tone_on) { deck_cmd(CMD_TONE_OFF, 0); tone_on = 0; }
                TOAST("", MODE_NAME[set.beat]);
            }
            if (b & (1u << BTN_LFO)) { lfo_held = 1; lfo_turned = 0; t_lfo = now; knobs = 1; deck_view = ~0u; }
            if (b & (1u << BTN_OCT_DOWN) && set.semi > -12) { set.semi--; chips = dirty = 1; }
            if (b & (1u << BTN_OCT_UP) && set.semi < 12) { set.semi++; chips = dirty = 1; }
        }
        if (fx_held && !(btn >> BTN_FX & 1u)) {       /* FX let go: a tap is the next FX, a turn was the MIXER */
            fx_held = 0; knobs = 1; deck_view = ~0u;
            if (!fx_turned && now - t_fx < 600u) { set.fx = (uint8_t)((set.fx + 1u) % FX_COUNT); knobs = dirty = 1; TOAST("", FX_NAME[set.fx]); }
        }
        if (lfo_held && !(btn >> BTN_LFO & 1u)) {     /* LFO let go: a tap toggles it, a turn was the page */
            lfo_held = 0; knobs = 1; deck_view = ~0u;
            if (!lfo_turned && now - t_lfo < 600u) { set.lfo_on = !set.lfo_on; chips = dirty = 1; t_change = now; TOAST("LFO", set.lfo_on ? "ON" : "OFF"); }
        }
        if (shift && b & (1u << BTN_SAVE)) {           /* SYNC: FOLLOW an incoming MIDI clock / OFF */
            static const char *const SY[2] = {"FOLLOW", "OFF"};
            set.sync = (uint8_t)(!set.sync);
            TOAST("SYNC", SY[set.sync]);
            dirty = 1;
            b &= ~(1u << BTN_SAVE);
        }
        if (b & (1u << BTN_SAVE)) {                     /* SAVE: now (the sound skips for a moment) */
            for (s = 0; s < 16u; s++) { set.word[s] = seq_word[s]; set.flag[s] = seq_flag[s]; set.drum[s] = seq_drum[s]; }
            settings_save(&set, sizeof set);
            dirty = 0;
            TOAST("", "SAVED");
        }
        if (dirty && (knobs || chips)) t_change = now;

        /* ---- to the ISR */
        {                                       /* the FX boost falls back once the hand stops */
            const uint32_t dt = now - t_loop;
            t_loop = now;
            if (fx_boost > 15 * 256) fx_boost = 15 * 256;
            if (fx_boost < -15 * 256) fx_boost = -15 * 256;
            if (now - t_boost > 120u && dt) fx_boost = fx_boost * (int32_t)(1024u - (dt * 4u > 1024u ? 1024u : dt * 4u)) / 1024;
            if ((fx_boost >> 8) != boost_shown) { boost_shown = fx_boost >> 8; knobs = 1; }
        }
        p_amtq = (uint32_t)clampi((int32_t)set.amt[set.fx] * 256 + fx_boost, 0, 15 * 256);
        p_lfo_on = set.lfo_on; p_lfo_rate = set.lfo_rate; p_lfo_shape = set.lfo_shape; p_lfo_depth = set.lfo_depth; p_lfo_dest = set.lfo_dest;
        p_curve = set.curve; p_torque = set.torque; p_hamster = set.hamster; p_space = set.space;
        p_blvl = set.blvl; p_width = set.width;
        p_fx = set.fx; p_amt = (uint32_t)clampi((int32_t)set.amt[set.fx] + (fx_boost >> 8), 0, 15); p_echo = set.echo; p_etime = set.etime; p_style = set.style; p_reach = set.reach;
        p_groove = set.groove; p_hard = set.hard; p_auto = set.aut; p_play = play; p_swing = set.swing;
        p_tune = set.tune; p_decay = set.decay; p_dlvl = set.dlvl; p_vlvl = set.vlvl;
        p_glvl = set.glvl; p_rap = set.rap; p_hook = set.hook; p_etime = 2;
        p_cut = btn >> BTN_LFO & 1u && shift;      /* EDIT + LFO held: the crossfader on 32nds */
        p_freeze = (btn >> BTN_ENV & 1u && !shift) || frz_latch;
        p_drop = btn >> BTN_SEL & 1u && !shift;
        p_fill = btn >> BTN_SEQ & 1u && !shift;
        if (ext_n && now - t_tick > 400u) ext_n = 0;    /* the clock stopped coming */
        ext_live = set.sync == 0u && ext_n >= 3u && ext_len;
        p_ext = ext_live;
        p_steplen = ext_live ? ext_len : 661500u / set.bpm;
        p_rate = (int32_t)((16384u * (uint32_t)PITCH[set.semi + 12]) >> 15);

        /* ---- the keys */
        keys = fm1_in.notes;
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        {
            uint32_t cur = seq_step_v & 15u, late = seq_frac_v >= 128u, q = late ? (cur + 1u) & 15u : cur, roll = 0;
            if (set.beat == M_TONE) {                  /* TONE: the keys are the pitch fader, the newest held key sounds */
                int32_t src = TSRC[set.tsrc] >= 0 ? TSRC[set.tsrc] : (int32_t)last_word;
                uint32_t best = 99u;
                for (k = 0; k < 27u; k++) if (down >> k & 1u) t_down[k] = now;
                if (down | up) {
                    for (k = 0; k < 27u; k++) if (keys >> k & 1u && (best == 99u || t_down[k] >= t_down[best])) best = k;
                    if (best != 99u) {
                        const int32_t semi = (int32_t)best - 12 + (int32_t)set.tune - 6;
                        deck_cmd(CMD_TONE, (uint32_t)src | (uint32_t)(semi + 64) << 8);
                        tone_on = 1;
                    } else if (tone_on) { deck_cmd(CMD_TONE_OFF, 0); tone_on = 0; }
                }
                down = up = 0;
            }
            for (k = 0; k < 27u && set.beat != M_TONE; k++) {
                int32_t w = -1, dr_i = -1;
                if (set.beat) { if (k < 12u) dr_i = (int32_t)k; else w = (int32_t)k - 12; }
                else w = (int32_t)k;
                if (down >> k & 1u) {
                    t_down[k] = now; scr[k] = 0; qstep[k] = q;
                    if (dr_i >= 0) {
                        drums_now(1u << dr_i);
                        out_msg(0x99, 36u + (uint32_t)dr_i, 110);
                        if (rec && play) { seq_drum[q] |= (uint16_t)(1u << dr_i); if (late) seq_skip[q] = 1; dirty = 1; }
                    } else {
                        const uint32_t gap = now - t_tap;
                        held_word[k] = w;
                        last_word = (uint32_t)w;
                        if (w == last_tap_w && gap < 450u && shift) {          /* EDIT + taps: pump the FX */
                            fx_boost += (int32_t)(256u * 600u / (gap < 60u ? 60u : gap));
                            t_boost = now;
                            deck_cmd(CMD_SAY, (uint32_t)w);
                        } else if (w == last_tap_w && gap < 450u) {             /* taps: finger strokes */
                            const uint32_t g = gap < 60u ? 60u : gap;
                            int32_t span = (int32_t)(1200u + 500000u / g);
                            if (span > 8000) span = 8000;
                            cmd_span = tap_n & 1u ? span : -span;               /* back first, then forward */
                            cmd_dur = g * 31u;                                   /* 70 % of the gap, in frames */
                            tap_n++;
                            scr[k] = 2;                                          /* no held scratch on a stroke */
                            deck_cmd(CMD_STROKE, (uint32_t)w);
                        } else {
                            tap_n = 0;
                            deck_cmd(CMD_SAY, (uint32_t)w);
                            if (set.rap && set.hook != (uint8_t)w) { set.hook = (uint8_t)w; dirty = 1; t_change = now; }   /* the hook */
                        }
                        last_tap_w = w;
                        t_tap = now;
                        out_msg(0x90, 48u + (uint32_t)w, 110);
                        if (rec && play) { seq_word[q] = (int8_t)w; seq_flag[q] = 0; if (late) seq_skip[q] = 1; dirty = 1; }
                    }
                }
                if (keys >> k & 1u) {
                    if (dr_i >= 0) { if (now - t_down[k] > 220u) roll |= 1u << dr_i; }
                    else if (!scr[k] && now - t_down[k] > 180u && held_word[k] >= 0) {   /* held: scratch it */
                        scr[k] = 1;
                        deck_cmd(CMD_SCR, (uint32_t)held_word[k]);
                        if (rec && play) { seq_flag[qstep[k]] |= SEQ_SCR; seq_word[qstep[k]] = (int8_t)held_word[k]; }
                    }
                }
                if (up >> k & 1u) {
                    if (scr[k] == 1u) deck_cmd(CMD_LET, 0);
                    if (held_word[k] >= 0) out_msg(0x80, 48u + (uint32_t)held_word[k], 0);
                    held_word[k] = -1;
                    scr[k] = 0;
                }
            }
            p_roll = roll;
        }
        while (midi_get(&st, &d1, &d2)) {       /* USB-MIDI in */
            if (st >= 0xF8u) {                  /* clock and transport */
                if (set.sync != 0u) continue;   /* OFF: ignored */
                if (st == 0xF8u) {
                    ext_us[ext_n % 49u] = NOW_US();
                    ext_n++;
                    ext_ticks++;
                    t_tick = now;
                    p_ext_ticks = ext_ticks;
                    if (ext_n >= 3u) {          /* the tempo over the last (up to) 48 ticks */
                        const uint32_t k2 = ext_n - 1u < 48u ? ext_n - 1u : 48u;
                        const uint32_t per = (ext_us[(ext_n - 1u) % 49u] - ext_us[(ext_n - 1u - k2) % 49u]) / k2;
                        if (per > 4000u && per < 60000u) { ext_len = per * 2646u / 10000u; ext_bpm = (2500000u + per / 2u) / per; }
                    }
                } else if (st == 0xFAu) { ext_ticks = 0; p_ext_ticks = 0; p_play = play = 1; p_ext_start++; chips = 1; }
                else if (st == 0xFBu) { p_play = play = 1; chips = 1; }
                else if (st == 0xFCu) { p_play = play = 0; chips = 1; }
                continue;
            }
            if ((st & 0xF0u) == 0xB0u && d1 == 119u) { out_msg(0xB0, 119, load_v > 127u ? 127u : load_v); continue; }   /* CC 119: the load, asked */
            if ((st & 0xF0u) != 0x90u || !d2) continue;
            if ((st & 0x0Fu) == 9u) { if (d1 >= 36u && d1 < 48u) drums_now(1u << (d1 - 36u)); }
            else if (d1 >= 48u && d1 < 48u + NKEYW) deck_cmd(CMD_SAY, d1 - 48u);
        }

        /* ---- a new voice: speak the bank again once the knob rests */
        if (voice_pending && now - t_voice > 700u) {
            voice_pending = 0;
            draw_big(VOICES[set.voice].name, C_PINK);
            render_bank(set.voice);
            wave_word = -2; word_drawn = -2; steps_drawn = ~0u; knobs = chips = 1; deck_view = ~0u; top_key = ~0u;
            deck_cmd(CMD_SAY, NAME_WORD);
        }

        /* ---- LEDs */
        {
            uint32_t h = drum_hit_v, lit;
            drum_hit_v = 0;
            for (k = 0; k < NDRUM; k++) if (h >> k & 1u) hit_t[k] = now;
            for (k = 0; k < 27u; k++) {
                lit = keys >> k & 1u;
                if (set.beat == M_TONE) {}
                else if (set.beat && k < 12u) lit |= now - hit_t[k] < 80u;
                else if (dk_on_v && held_word[k] < 0) lit |= (set.beat ? (int32_t)k - 12 : (int32_t)k) == dk_word_v;
                fm1_led_key(KEY_LED(k), lit);
            }
        }
        fm1_led_key(BTN_PLAY, play && (seq_step_v & 3u) < 2u);
        fm1_led_key(BTN_REC, rec);
        fm1_led_key(BTN_ARP, set.rap);
        fm1_led_key(BTN_GLO, set.aut);
        fm1_led_key(BTN_FX, fx_held || set.fx != FX_FILTER || set.amt[FX_FILTER] != 7u);
        fm1_led_key(BTN_HOME, set.beat);
        fm1_led_key(BTN_EDIT, shift);
        fm1_led_key(BTN_ENV, p_freeze);
        fm1_led_key(BTN_LFO, p_cut || lfo_held || (set.lfo_on && lfo_v_v > 0));   /* it pulses with the LFO */
        fm1_led_key(BTN_SEL, p_drop);
        fm1_led_key(BTN_SEQ, p_fill);
        fm1_led_key(BTN_SAVE, dirty);
        fm1_led_key(BTN_OCT_DOWN, set.semi < 0);
        fm1_led_key(BTN_OCT_UP, set.semi > 0);

        /* ---- the screen */
        {
            const uint16_t acc = FX_COL[set.fx];
            const uint32_t fs = fx_state_v;
            const char *now_s;
            uint16_t now_c = C_YEL;
            char bpm_s[16];
            uint32_t key;
            /* the top bar: the move happening now, else the mode and tempo */
            if (fx_held && !shift && !lfo_held) now_s = "MIXER";
            else if (lfo_held && !shift) now_s = "PRO PAGE";
            else if (shift) { char *lp = put(bpm_s, "LOAD "); lp = put_u(lp, load_v); put(lp, "%"); now_s = bpm_s; now_c = load_v > 85u ? C_RED : C_YEL; }
            else if (fx_boost > 255 || fx_boost < -255) now_s = "VELOCITY FX";
            else if (fs & ST_SPIN) now_s = "BACKSPIN";
            else if (fs & ST_BRAKE) now_s = "BRAKE";
            else if (p_fill) now_s = "FILL";
            else if (p_drop) now_s = "DROP";
            else if (p_freeze) now_s = "FREEZE";
            else if (p_cut) now_s = "CUT";
            else if (fs & ST_MUTED) now_s = "...";
            else if (fs & ST_WAIT) now_s = "SYNC...";
            else if (set.rap && play && rap_part_v) {
                char *p = put(bpm_s, rap_part_v <= 4u ? "VERSE " : "HOOK ");
                put_u(p, (rap_part_v - 1u) % 4u + 1u);
                now_s = bpm_s; now_c = rap_part_v <= 4u ? C_CYAN : C_PINK;
            } else {
                char *p = put(put(bpm_s, ext_live ? "EXT" : MODE_NAME[set.beat]), " ");
                put_u(p, ext_live ? ext_bpm : set.bpm);
                now_s = bpm_s; now_c = ext_live ? C_YEL : C_CYAN;
            }
            key = (uint32_t)now_s[0] * 7u + (uint32_t)now_s[1] + (uint32_t)now_s[2] * 31u + (shift ? load_v * 977u : 0u) + rap_part_v * 13331u + rec * 1000u + (ext_live ? ext_bpm : set.bpm) * 3u + set.beat * 99991u + ext_live * 7777u;
            if (key != top_key) { top_key = key; draw_top(now_s, now_c, rec); }

            /* the big line: a toast for a moment, else the word */
            if (toast_on && now - t_toast < 1100u) {
                if (word_drawn != -3 || toast_on == 1) { draw_big(toast[0] == ' ' ? toast + 1 : toast, C_YEL); word_drawn = -3; toast_on = 2; }
            } else {
                toast_on = 0;
                if (dk_word_v != word_drawn) { word_drawn = dk_word_v; draw_big(word_drawn >= 0 ? SHOW[word_drawn] : "", acc); }
            }

            /* the middle: the deck (DJ), the pads (BEAT), or the button map (EDIT held) */
            if (shift) { if (deck_view != 2u) { deck_view = 2u; draw_cheat(); } }
            else if (fx_held && !lfo_held) {
                if (now - t_draw >= 33u || deck_view != 5u + NTECH) {
                    const uint8_t lv[7] = {set.vlvl, set.glvl, set.blvl, set.dlvl, set.space, set.echo, set.width};
                    static const uint8_t OF[7] = {15, 15, 15, 15, 15, 15, 15};
                    for (k = 0; k < 4u; k++) {                  /* the meters: take the ISR's peaks, fall back */
                        const uint32_t v2 = mt_v[k];
                        mt_v[k] = 0;
                        mt[k] = v2 > mt[k] ? v2 : mt[k] - (mt[k] >> 3);
                    }
                    draw_mixer(lv, OF, mt, acc);
                    deck_view = 5u + NTECH; t_draw = now;
                }
            }
            else if (lfo_held) {
                if (now - t_draw >= 33u || deck_view != 3u) {
                    draw_lfo(set.lfo_on, set.lfo_shape, LFO_RATE_NAME[set.lfo_rate], LFO_DEST_NAME[set.lfo_dest], acc);
                    deck_view = 3u; t_draw = now;
                }
            }
            else if (set.beat != M_TONE && now - t_score < 1500u && set.style < NTECH && !dk_scr_v) {
                if (deck_view != 4u + set.style) { deck_view = 4u + set.style; draw_score_preview(set.style, acc); }
            }
            else if (now - t_draw >= 33u || deck_view == ~0u || deck_view >= 3u) {
                if (set.beat == M_BEAT) { draw_pads(hit_t, now); deck_view = 1u; t_draw = now; }
                else if (dk_on_v || play || jog_held || deck_view != 0u) {
                    draw_deck(dk_word_v, dk_pos_v, dk_rot_v, dk_on_v, dk_fader_v, acc);
                    deck_view = 0u; t_draw = now;
                }
            }

            {
                uint32_t sk = (seq_step_v & 15u) + play * 16u + (dirty << 5);
                if (sk != steps_drawn) { steps_drawn = sk; draw_steps(seq_step_v & 15u, play, now - hit_t[D_KICK] < 60u); }
            }

            if (knobs) {                        /* the encoders' strip: what each one does now */
                cell_t a[4], c[3];
                knobs = 0;
                if (fx_held && !shift && !lfo_held) {
                    a[0].label = "VOX"; put_u(a[0].val, set.vlvl); a[0].bar = set.vlvl; a[0].of = 15;
                    a[1].label = "GUITAR"; put_u(a[1].val, set.glvl); a[1].bar = set.glvl; a[1].of = 15;
                    a[2].label = "BASS"; put_u(a[2].val, set.blvl); a[2].bar = set.blvl; a[2].of = 15;
                    a[3].label = "DRUMS"; put_u(a[3].val, set.dlvl); a[3].bar = set.dlvl; a[3].of = 15;
                    c[0].label = "VERB"; put_u(c[0].val, set.space); c[0].bar = set.space; c[0].of = 15;
                    c[1].label = "ECHO"; put_u(c[1].val, set.echo); c[1].bar = set.echo; c[1].of = 15;
                    c[2].label = "WIDTH"; put_u(c[2].val, set.width); c[2].bar = set.width; c[2].of = 15;
                } else if (lfo_held && !shift) {
                    a[0].label = "RATE"; put(a[0].val, LFO_RATE_NAME[set.lfo_rate]); a[0].bar = set.lfo_rate; a[0].of = NLFO_RATE - 1;
                    a[1].label = "SHAPE"; put(a[1].val, LFO_SHAPE_NAME[set.lfo_shape]); a[1].bar = 0; a[1].of = 0;
                    a[2].label = "DEPTH"; put_u(a[2].val, set.lfo_depth); a[2].bar = set.lfo_depth; a[2].of = 15;
                    a[3].label = "DEST"; put(a[3].val, LFO_DEST_NAME[set.lfo_dest]); a[3].bar = 0; a[3].of = 0;
                    c[0].label = "SPACE"; put_u(c[0].val, set.space); c[0].bar = set.space; c[0].of = 15;
                    c[1].label = "CURVE"; put(c[1].val, CURVE_NAME[set.curve]); c[1].bar = set.curve; c[1].of = 2;
                    c[2].label = "TORQUE"; put(c[2].val, TORQUE_NAME[set.torque]); c[2].bar = set.torque; c[2].of = 3;
                } else if (!shift) {
                    a[0].label = FX_NAME[set.fx]; put_u(a[0].val, p_amt); a[0].bar = p_amt; a[0].of = 15;
                    a[1].label = "GUITAR"; put_u(a[1].val, set.glvl); a[1].bar = set.glvl; a[1].of = 15;
                    a[2].label = "BPM"; put_u(a[2].val, set.bpm); a[2].bar = set.bpm - 60u; a[2].of = 120;
                    a[3].label = "VOICE"; put(a[3].val, VOICES[set.voice].name); a[3].bar = 0; a[3].of = 0;
                    c[0].label = "JOG"; put(c[0].val, "VELOCITY"); c[0].of = 0;
                    if (set.beat == M_TONE) { c[1].label = "SOURCE"; put(c[1].val, TSRC_NAME[set.tsrc]); }
                    else { c[1].label = "STYLE"; put(c[1].val, set.style == SC_HYDRO ? "HYDRO" : SC_NAME[set.style]); }
                    c[1].of = 0;
                    c[2].label = "GROOVE"; put(c[2].val, GROOVES[set.groove].name); c[2].of = 0;
                } else {
                    a[0].label = "DROP"; put(a[0].val, DROP_NAME[set.tune]); a[0].bar = set.tune; a[0].of = 12;
                    a[1].label = "ECHO"; put_u(a[1].val, set.echo); a[1].bar = set.echo; a[1].of = 15;
                    a[2].label = "DECAY"; put_s(a[2].val, (int32_t)set.decay - 2); a[2].bar = set.decay; a[2].of = 4;
                    a[3].label = "DRUMS"; put_u(a[3].val, set.dlvl); a[3].bar = set.dlvl; a[3].of = 15;
                    c[0].label = "VEL FX"; put(c[0].val, "FLICK"); c[0].of = 0;
                    c[1].label = "SWING"; put_u(c[1].val, set.swing * 10u); c[1].bar = set.swing; c[1].of = 10;
                    c[2].label = "REACH"; put_u(c[2].val, set.reach + 1u); c[2].bar = set.reach; c[2].of = 7;
                }
                draw_knobs(a, c, shift, acc);
            }
            if (chips) { chips = 0; draw_chips(rec, set.aut, set.rap, set.beat, set.lfo_on, set.hamster, set.semi); }
        }

        if (dirty && !play && !dk_on_v && now - t_change > 2000u) {   /* saved when silent (an erase stalls the sound) */
            for (s = 0; s < 16u; s++) { set.word[s] = seq_word[s]; set.flag[s] = seq_flag[s]; set.drum[s] = seq_drum[s]; }
            settings_save(&set, sizeof set);
            dirty = 0;
        }
    }
}
