/* SPDX-License-Identifier: GPL-3.0-only */
/* NU//METAL SKRATCHMASTER PRO (2.0) FM-1: a fork of CYBER//SKRATCH for the M-VAVE FM-1 on Dinghy. The robot DJ joins a
 * nu metal band: drop-tuned chug guitars and a bass (the PUNK amp: detuned saws, high-pass, fuzz, a two-pole cab) play a
 * riff locked to each groove, SAM shouts a nu metal lexicon onto the turntable deck, and RAP MODE turns the robot into
 * the MC: four bars of VERSE, four bars of HOOK. Integer-only: the chip has no FPU.
 * 4.0, SKRATCH: streamlined for playing next to a drum machine. One deck: the jog is the hand on the record, the keys
 * drop the needle and scratch, ALGORITHM picks the technique, the four knobs are four FX at once. RAP MODE, TONE and
 * BEAT modes, the step sequencer, the LFO page, the mixer and the band are gone; a drum beat stays for practice.
 *
 *   KEYS       27 spots: WORDS (the SAM bank) or the INFINITE record (a WORLD). Tap = drop the needle there,
 *              tap again quickly = finger strokes, HOLD = the technique while held, locked to the beat
 *
 *   encoder    turn                               HOME held + turn: SETTINGS
 *   SELECT     JOG: scratch by hand               BPM (when no clock comes in)
 *   ALGORITHM  TECHNIQUE (17 + COMBO)             REACH
 *   KNOB1      FILTER (low-pass .. open .. high)  SAM VOICE
 *   KNOB2      ECHO (synced throw)                FLOW
 *   KNOB3      CRUSH                              VERB
 *   KNOB4      STUTTER (1/4 1/8 1/16, synced)     BEAT level
 *   PRESETS    RECORD: WORDS, WORLD 0..99         motor TORQUE
 *
 *   button     tap / hold
 *   PLAY       the practice beat (drums only: a swung boom bap that never plays a bar quite the same) on / off
 *   FX         hold = DRY (every FX off)
 *   ENV        hold = FREEZE          LFO   hold = CUT (the fader on 32nds)
 *   REC        BACKSPIN               SEQ   BRAKE
 *   GLO        AUTO (the robot)       SEL   HAMSTER
 *   ARP        WORDS <-> the WORLD    HOME  hold = SETTINGS, tap = pitch back to 0
 *   OCT- OCT+  the deck's pitch       SAVE  save now (EDIT + SAVE = SYNC: FOLLOW / OFF)
 *   EDIT       hold = the button map and the audio LOAD
 *
 *   MIDI SYNC  FOLLOW (the default): an incoming clock (USB or TRS) sets the tempo and the grid; Start puts the grid
 *              on the one but never starts the practice beat. USB-MIDI ch 10 notes 36..47 = the kit; other channels
 *              notes 48..74 = the 27 spots; CC 20 21 22 out = scratching, fader, platter.
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
#define SURF_WORD 29                            /* not in the pool: the INFINITE record (SURF mode), made as it plays */
#define NBANK 30
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
    "NU METAL SKRATCHMASTER", "AHHH", "FRESH", "TONE", "BUZZ", "INFINITE"};

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

static const uint8_t CURVE_SH[3] = {2, 4, 7};    /* the fader's slew, per sample: 0.1 ms .. 3 ms */
static const char *const TORQUE_NAME[4] = {"LOW", "MID", "HIGH", "MAX"};
static const uint8_t TORQUE_SH[4] = {9, 7, 5, 0};   /* the motor pulling the record to speed, per block (0 = at once) */
static const uint8_t BRAKE_STEPS[4] = {16, 12, 9, 6};

static volatile uint32_t p_filt = 7, p_crush, p_stut;     /* the knob FX: FILTER (7..8 open), CRUSH, STUTTER (0 = off) */
static volatile uint32_t p_curve, p_torque = 3, p_hamster, p_space = 7;   /* SPACE: the plate's return */
static volatile uint32_t load_v;
static volatile uint32_t p_blvl = 12;                     /* the practice beat's level (12 = unity) */
static volatile uint32_t dk_tech_v, dk_ph_v, dk_scr_v;   /* ISR -> main: the technique being played, where in it */
static volatile uint32_t p_echo = 3, p_style, p_reach = 3, p_play, p_auto;
static volatile uint32_t p_steplen = 7190, p_tune = 4, p_vlvl = 12;
static volatile int32_t p_rate = 16384;                 /* the deck's speed, Q16 word samples per frame (1/4 = 11025 into 44100) */
static volatile uint32_t p_restart;
static volatile uint32_t p_cut, p_freeze;                 /* held performance moves */
static volatile uint32_t p_spin, p_brake;                  /* one-shots: counters, the ISR acts when they move */
static volatile uint32_t p_ext, p_ext_ticks, p_ext_start;  /* MIDI clock in: following, ticks since Start, Starts */
static void out_msg(uint32_t st, uint32_t d1, uint32_t d2) { midi_send(st, d1, d2); }   /* main loop only */
enum { CMD_SAY, CMD_SCR, CMD_LET, CMD_STROKE };
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

/* =========================================================================== drums */
/* the kit is played back, not synthesised: 16-bit PCM rendered offline (tools/render_kit.py -> gen/kit_pcm.h) */
#include "kit_pcm.h"
typedef struct { const char *name; const int16_t *pcm; uint32_t len, fs; int16_t gain; } perc_t;
#define PCM(n) PCM_##n, sizeof PCM_##n / 2u, PCM_##n##_FS
#define NDRUM 12
static const perc_t PERC[NDRUM] = {
    {"KICK", PCM(KICK), 28000}, {"SNARE", PCM(SNARE), 21000}, {"CLAP", PCM(CLAP), 24000}, {"HAT", PCM(HAT), 5600},
    {"OPEN", PCM(OPEN), 8400}, {"RIM", PCM(RIM), 13500}, {"TOM-L", PCM(TOML), 18000}, {"TOM-H", PCM(TOMH), 17200},
    {"808", PCM(B808), 30000}, {"CHINA", PCM(CHINA), 6900}, {"ZAP", PCM(ZAP), 7600}, {"CRASH", PCM(CRASH), 8600},
};
enum { D_KICK, D_SNARE, D_CLAP, D_HAT, D_OPEN, D_RIM, D_TOML, D_TOMH, D_808, D_CHINA, D_ZAP, D_CRASH };
#define HATS ((1u << D_HAT) | (1u << D_OPEN))

/* 4.0: THE PRACTICE BEAT, drums only. A swung boom bap: kick, snare on 2 and 4, eighth hats. Every bar a thing or two
 * changes (a kick moves, a ghost, an open hat, a hat roll) and drifts back home; every 8th bar ends in a fill and the
 * next one lands on a crash. Now and then the home moves on. */
#define SWING 44u                               /* the off 16ths land 44/256 of a step late: ~58 % swing */
static uint16_t bt_drum[16], bt_hdrum[16], bt_roll, bt_hroll;   /* the bar now and its home; roll = a hat 32nd on that step */
static uint32_t bt_init;

/* =========================================================================== the audio ISR */
enum { DK_OFF, DK_PLAY, DK_SCR, DK_HAND, DK_LET, DK_SPIN, DK_FRZ, DK_THROW, DK_STROKE, DK_REST };
static struct {
    int32_t word, pos, rate, hand, frz;         /* pos, hand, frz: Q16 word samples */
    int32_t st_from, st_to;                     /* a finger stroke: from, to (Q16) */
    uint32_t st_t, st_dur;                      /* frames into it, its length */
    uint32_t mode, ph, once, stut, stut_n;
    int32_t fader;
    uint32_t tech, phrase, pstep;               /* the technique a scratch plays; COMBO: the routine, where in it */
    int32_t anchor;                             /* a scratch's spot on the record (Q16): 0 on a word, SURF: anywhere */
    uint32_t szone;                             /* SURF: the zone a tap plays (it stops when the needle leaves it) */
} dk = {.word = -1, .mode = DK_OFF};
static struct { int32_t env; uint32_t pos, inc, end; } dr[NDRUM];   /* pos, inc: Q16 samples of the PCM */
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

static uint32_t rn(uint32_t n) { return (noise() >> 8) % n; }
#define B(d) (1u << (d))
static void beat_home(void)                     /* a fresh boom bap bar: the home the changes wander from */
{
    static const char *const K[6] = {"x.........x.....", "x......x..x.....", "x.x.......x..x..", "x......x.x......",
                                     "x..x......x..x..", "x.....x...x....x"};
    const char *kk = K[rn(6)];
    const uint32_t clap = !rn(3);               /* a third of the time the snare gets a clap on top */
    uint32_t st;
    for (st = 0; st < 16u; st++) {
        uint32_t m = 0;
        if (kk[st] == 'x') m |= B(D_KICK);
        if (st == 4u || st == 12u) m |= B(D_SNARE) | (clap ? B(D_CLAP) : 0u);
        if (!(st & 1u)) m |= B(D_HAT);
        bt_hdrum[st] = (uint16_t)m;
    }
    if (rn(2)) bt_hdrum[rn(2) ? 7u : 15u] |= B(D_RIM);   /* a ghost before the snare */
    if (rn(2)) bt_hdrum[14] = (uint16_t)((bt_hdrum[14] & ~B(D_HAT)) | B(D_OPEN));
    bt_hroll = 0;
    for (st = 0; st < 16u; st++) bt_drum[st] = bt_hdrum[st];
    bt_roll = bt_hroll;
}
static void beat_mutate(uint32_t bar)           /* a bar's changes; every 8th bar a fill, then a crash */
{
    uint32_t k;
    for (k = 0; k < 16u; k++) if (!rn(4)) bt_drum[k] = bt_hdrum[k];   /* what wandered comes home */
    bt_roll = bt_hroll;
    if ((bar & 7u) == 0u && bar) {
        bt_drum[0] |= B(D_KICK);
        if (rn(2)) bt_drum[0] |= B(D_CRASH);
        if (!rn(3)) beat_home(), bt_drum[0] |= B(D_CRASH);   /* somewhere new */
    }
    if ((bar & 7u) == 7u) {                     /* THE FILL: the last beat of the 8th bar */
        switch (rn(4)) {
        case 0: for (k = 12u; k < 16u; k++) bt_drum[k] = (uint16_t)(B(D_SNARE) | (k & 1u ? 0u : B(D_KICK))); break;
        case 1: bt_drum[12] = B(D_TOMH) | B(D_KICK); bt_drum[13] = B(D_TOMH); bt_drum[14] = B(D_TOML); bt_drum[15] = B(D_TOML); break;
        case 2: bt_drum[13] |= B(D_SNARE); bt_drum[14] |= B(D_SNARE); bt_drum[15] = B(D_SNARE) | B(D_CLAP); break;
        default: bt_roll |= (uint16_t)(B(12) | B(13) | B(14) | B(15)); for (k = 12u; k < 16u; k++) bt_drum[k] |= B(D_HAT); break;
        }
        return;
    }
    if (rn(3)) {                                /* most bars: one small thing */
        const uint32_t st = rn(16);
        switch (rn(6)) {
        case 0:                                 /* a kick moves */
            if (st && st != 4u && st != 12u) bt_drum[st] ^= (uint16_t)B(D_KICK);
            break;
        case 1:                                 /* a ghost */
            if (st != 4u && st != 12u) bt_drum[st] |= (uint16_t)B(D_RIM);
            break;
        case 2:                                 /* an open hat, the next 16th left empty */
            if (!(st & 1u)) { bt_drum[st] = (uint16_t)((bt_drum[st] & ~B(D_HAT)) | B(D_OPEN)); bt_drum[(st + 1u) & 15u] &= (uint16_t)~B(D_HAT); }
            break;
        case 3:                                 /* a hat 16th */
            bt_drum[st | 1u] |= (uint16_t)B(D_HAT);
            break;
        case 4:                                 /* a hat roll: 32nds over two steps */
            bt_roll |= (uint16_t)(B(st) | B((st + 1u) & 15u));
            bt_drum[st] |= (uint16_t)B(D_HAT); bt_drum[(st + 1u) & 15u] |= (uint16_t)B(D_HAT);
            break;
        default:                                /* a double kick into the snare */
            bt_drum[11] |= (uint16_t)B(D_KICK);
            break;
        }
    }
}

static uint32_t bump(uint32_t ph16)             /* 0 .. 32767 .. 0 over a cycle (1 - cos) / 2 */
{
    return (uint32_t)((32767 - sine((ph16 << 16) + 0x40000000u)) >> 1);
}

/* =========================================================================== INFINITE SKRATCH: the endless record
 * SURF mode's record never ends and takes no flash: every stretch of it is made from where it is (its sample number)
 * and the WORLD, so scratching back over a spot plays the same sound again. It is cut into zones of 0.74 s, each a
 * "sample on a battle record" with its own attack, body and gap (vinyl crackle), made one of seven ways:
 *   AHHH   a vowel of FOF grains (a pulse a pitch period, three formants ringing off it), pitch gliding, the vowel morphing
 *   CHIRP  a tone sweeping octaves, or a LASER (the sweep repeating), sine, fat square, saw or sine + octave
 *   FM     two operators, the index decaying, rising or wobbling: bells, screeches, the FM-1 being itself
 *   BYTE   a bytebeat formula (t * (t >> a | t >> b) ...) read at the record's speed: the 8-bit shrapnel
 *   DUST   the SAM bank shattered: grains of the words, re-pitched, some backwards, two streams overlapped
 *   CHAOS  the logistic map, its r swept through the bifurcations: a tone splits into two, four, then noise
 *   HISS   noise through a resonant filter that sweeps: the record's whistle and its kssh
 *   STAB   a horn / power-chord stab in drop A: stacked saws, a fifth, a sub, driven: the fat deep cut
 *   CUT    a whole word off the bank, AHHH and FRESH most of all, pitched down: the battle record classic
 * Half the zones are a classic (STAB, CUT, AHHH): those are the ones that cut deep.
 * A WORLD leans on three of them. 1.5 s of the record lives in RAM, made 256 samples at a time as the needle gets
 * near, and the window slides along with the needle, so the record goes on in both directions for days. */
#define ZSH 13u
#define ZLEN (1u << ZSH)                        /* a zone: 8192 samples, 0.74 s */
#define SURF_TILE 256u
#define SURF_NT 64u
#define SURF_WIN (SURF_TILE * SURF_NT)          /* the window: 16384 samples, 1.49 s */
#define SURF_STEP (SURF_WIN / 4u)               /* it slides a quarter at a time */
#define SURF_KEY 32                             /* commands and steps: SURF_KEY + k = the k-th spot on the record */
#define SURF_HERE 127                           /* ...or wherever the needle is */
#define NSURFK 27
enum { Z_AHHH, Z_CHIRP, Z_FM, Z_BYTE, Z_DUST, Z_CHAOS, Z_HISS, Z_STAB, Z_CUT, Z_COUNT };
static const char *const ZNAME[Z_COUNT] = {"AHHH", "CHIRP", "FM", "BYTE", "DUST", "CHAOS", "HISS", "STAB", "CUT"};
enum { ZE_STAB, ZE_HOLD, ZE_SWELL, ZE_CHOP };
typedef struct { uint32_t h, h2, h3, mat, len, shape; } zone_t;
static int8_t surf_buf[SURF_WIN + 64u] __attribute__((aligned(4)));
static int32_t surf_tmp[SURF_TILE];
static uint8_t surf_ok[SURF_NT];
static uint32_t surf_base, surf_world = ~0u;
static volatile uint32_t p_world, p_surf = 4, p_surfmode, surf_abs_v;

static uint32_t hsh(uint32_t x) { x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16; return x; }
static void zone_of(uint32_t world, uint32_t z, zone_t *zp)   /* what zone z of a world is (main and ISR: no state) */
{
    const uint32_t wh = hsh(world * 0x9E3779B9u + 0x51u), h = hsh(z * 0x85EBCA6Bu ^ wh), r = h & 7u;
    zp->h = h; zp->h2 = hsh(h + 1u); zp->h3 = hsh(h + 2u);
    static const uint8_t CLASSIC[3] = {Z_STAB, Z_CUT, Z_AHHH};
    zp->mat = r < 4u ? CLASSIC[(h >> 8) % 3u] : r < 7u ? (wh >> (r % 3u * 5u)) % Z_COUNT : (h >> 8) % Z_COUNT;   /* classics, the world's three */
    zp->len = 2048u + ((h >> 3) & 3u) * 2048u;                                /* the sound; the rest is the gap */
    zp->shape = (h >> 12) & 3u;
    if (zp->mat == Z_STAB) { zp->shape &= 1u; if (zp->len < 4096u) zp->len = 4096u; }   /* a stab: hit or held, never a swell */
    if (zp->mat == Z_CUT) { zp->shape = ZE_HOLD; zp->len = 6144u; }               /* the word's own shape */
}
static int32_t zone_env(const zone_t *zp, uint32_t t)   /* the zone's envelope at t, Q15 */
{
    const uint32_t len = zp->len;
    int32_t e, u;
    if (t >= len) return 0;
    u = (int32_t)(t * 32767u / len);
    switch (zp->shape) {
    case ZE_STAB: e = 32767 - u; e = (e * ((e >> 1) + 16384)) >> 15; if (t < 24u) e = e * (int32_t)t / 24; break;   /* between linear and squared */
    case ZE_SWELL: e = (u * u) >> 15; if (len - t < 64u) e = e * (int32_t)(len - t) / 64; break;   /* a reverse cymbal */
    case ZE_CHOP: {                             /* chopped in 16ths, a pattern of its own */
        const uint32_t sl = len >> 4, seg = t / sl, in = t - seg * sl;
        e = !seg || (zp->h3 >> seg & 1u) ? 30000 - u / 3 : 0;
        if (in < 24u) e = e * (int32_t)in / 24;
        if (sl - in < 24u) e = e * (int32_t)(sl - in) / 24;
        break;
    }
    default: e = 32767; if (t < 96u) e = (int32_t)t * 341; if (len - t < 640u) e = (int32_t)(len - t) * 51; break;
    }
    return e;
}

#define HZ(f) ((uint32_t)(f) * 389566u)          /* Hz -> phase a sample at 11025 */
static const uint16_t VOW[5][3] = {{730, 1090, 2440}, {570, 840, 2410}, {530, 1840, 2480}, {300, 870, 2240}, {270, 2290, 3010}};
static const uint16_t VOW_AMP[3] = {32767, 18000, 10000}, VOW_DEC[3] = {14, 22, 30};
static const uint16_t GLIDE8[8] = {128, 192, 256, 341, 384, 512, 171, 256};
static const uint16_t SWEEP8[8] = {64, 128, 512, 1024, 256, 32, 768, 192};
static const uint16_t FMR8[8] = {256, 362, 512, 896, 128, 1792, 640, 1024};
static const uint16_t DUST8[8] = {128, 171, 192, 256, 256, 304, 384, 512};

static uint32_t zone_len(const zone_t *zp, uint32_t flow)   /* FLOW stretches the sound over the gap */
{
    return zp->len + (ZLEN - zp->len) * (flow < 10u ? flow : 10u) / 10u;   /* 10 and up: no gaps at all */
}
static int32_t zone_env_flow(const zone_t *zp, uint32_t t, uint32_t flow)   /* the envelope with FLOW: longer, flatter */
{
    zone_t q = *zp;
    int32_t e;
    q.len = zone_len(zp, flow);
    if (t >= ZLEN) t = ZLEN - 1u;                   /* a zone's tail into the next: held at its end */
    if (t >= q.len) return 0;
    e = zone_env(&q, t);
    if (!flow) return e;
    e += (32767 - e) * (int32_t)flow / 15;          /* 15: flat, the crossfades shape it */
    if (q.len < ZLEN && q.len - t < 256u) e = e * (int32_t)(q.len - t) / 256;   /* into the gap, softly */
    return e;
}
static volatile uint32_t p_flow = 12;
static int32_t surf_tmp2[SURF_TILE];

static void surf_gen(const zone_t *zpp, uint32_t n0, uint32_t t0, int32_t *o)   /* a zone's material, t0 .. + 256 (t0 may run past its end) */
{
    uint32_t i, k;
    const zone_t zp = *zpp;
    for (i = 0; i < SURF_TILE; i++) o[i] = 0;
    switch (zp.mat) {
    case Z_AHHH: {                              /* FOF: phase of a gliding pulse, the time since it, three formants ringing */
        const uint32_t f0 = 60u + (zp.h2 & 127u), f1 = f0 * GLIDE8[zp.h2 >> 7 & 7u] >> 8;
        const uint32_t va = (zp.h2 >> 10) % 5u, vb = (zp.h2 >> 13) % 5u, L = zp.len;
        const int32_t inc0 = (int32_t)HZ(f0), dinc = ((int32_t)HZ(f1) - inc0) / (int32_t)L;
        const uint32_t phL = (uint32_t)inc0 * L + (uint32_t)dinc * (L * (L - 1u) / 2u), incL = (uint32_t)(inc0 + dinc * (int32_t)L);
        for (i = 0; i < SURF_TILE; i++) {
            const uint32_t t = t0 + i, tg = t < L ? t : L;   /* past its end: the pitch holds where the glide ended */
            const uint32_t ph = t < L ? (uint32_t)inc0 * t + (uint32_t)dinc * (t * (t - 1u) / 2u) : phL + incL * (t - L);
            const uint32_t d = ((uint32_t)(inc0 + dinc * (int32_t)tg) >> 8) + 1u, tau = (ph >> 4) / d, per = 0x0FFFFFFFu / d;
            const int32_t u = t >= zp.len ? 255 : (int32_t)(t * 256u / zp.len);
            int32_t acc = 0;
            for (k = 0; k < 3u; k++) {
                const int32_t F = VOW[va][k] + ((((int32_t)VOW[vb][k] - VOW[va][k]) * u) >> 8);
                const uint32_t fi = HZ(F) >> 4;
                int32_t g1 = 32767 - (int32_t)tau * VOW_DEC[k], g2 = 32767 - (int32_t)(tau + per) * VOW_DEC[k];
                g1 = g1 < 0 ? 0 : (g1 * g1) >> 15;
                g2 = g2 < 0 ? 0 : (g2 * g2) >> 15;                      /* the last pulse's tail, still ringing */
                acc += (((sine(fi * tau) * g1) >> 15) + ((sine(fi * (tau + per)) * g2) >> 15)) * VOW_AMP[k] >> 15;
            }
            o[i] = acc * 2 / 5;
        }
        break;
    }
    case Z_CHIRP: {
        const uint32_t f0 = 150u + (zp.h2 & 1023u) * 2u, wf = zp.h3 & 3u;
        const uint32_t P = zp.h2 >> 20 & 1u ? 512u << (zp.h2 >> 21 & 3u) : zp.len;   /* a LASER: the sweep repeats */
        uint32_t f1 = f0 * SWEEP8[zp.h2 >> 12 & 7u] >> 8;
        int32_t inc0, dinc;
        if (f1 > 4000u) f1 = 4000u;
        if (f1 < 30u) f1 = 30u;
        inc0 = (int32_t)HZ(f0); dinc = ((int32_t)HZ(f1) - inc0) / (int32_t)P;
        for (i = 0; i < SURF_TILE; i++) {
            const uint32_t tp = (t0 + i) % P, ph = (uint32_t)inc0 * tp + (uint32_t)dinc * (tp * (tp - 1u) / 2u);
            o[i] = wf == 0u ? sine(ph) : wf == 1u ? soft(sine(ph) * 3) : wf == 2u ? (int32_t)ph >> 17 : (sine(ph) * 2 + sine(ph * 2u)) / 3;
        }
        break;
    }
    case Z_FM: {
        const uint32_t fc = 55u + (zp.h2 & 511u), fm = fc * FMR8[zp.h2 >> 9 & 7u] >> 8, ic = HZ(fc), im = HZ(fm);
        const uint32_t ish = zp.h3 >> 14 & 3u, il = HZ(3u + (zp.h3 >> 16 & 7u));
        int32_t imax = 6000 + (int32_t)(zp.h3 & 16383u) * 5 / 3;
        if (imax > 32767) imax = 32767;
        for (i = 0; i < SURF_TILE; i++) {
            const uint32_t t = t0 + i;
            const int32_t u = t >= zp.len ? 32767 : (int32_t)(t * 32767u / zp.len), du = 32767 - u;
            const int32_t I = ish == 0u ? (int32_t)(((imax * ((du * du) >> 15)) >> 15)) : ish == 1u ? (imax * u) >> 15
                            : ish == 2u ? (imax * (32767 + sine(il * t))) >> 16 : imax;
            const uint32_t mod = (uint32_t)(sine(im * t) * (I >> 1)) << 3;
            o[i] = sine(ic * t + mod);
        }
        break;
    }
    case Z_BYTE: {
        const uint32_t br = 140u + (zp.h2 & 255u) * 2u, a = 2u + (zp.h2 >> 8 & 7u), b = 5u + (zp.h2 >> 11 & 7u);
        const uint32_t c = (zp.h2 >> 14 & 255u) | 1u, f = zp.h3 & 7u, off = zp.h3 >> 8 & 0x3FFFFu, mel = 0xCA98u ^ (c << 4);
        for (i = 0; i < SURF_TILE; i++) {
            const uint32_t T = (((t0 + i) * br) >> 8) + off;
            uint32_t v;
            switch (f) {
            case 0: v = T * (T >> a | T >> b); break;
            case 1: v = T * ((T >> a) & (T >> b) & c); break;
            case 2: v = (T * a & T >> b) | (T * 3u & T >> (a + 4u)); break;
            case 3: v = T * (((T >> a) | (T >> b)) & (c & 63u)); break;
            case 4: v = ((T >> a) ^ (T >> b)) * T; break;
            case 5: v = (T * (T >> 8 | T >> 9) & 46u & T >> 8) ^ ((T & T >> 13) | T >> 6); break;
            case 6: v = T * (mel >> (T >> b & 14u) & 15u); break;
            default: v = (T * 5u & T >> 7) | (T * 3u & T >> 10); break;
            }
            o[i] = ((int32_t)(v & 255u) - 128) * 190;
        }
        break;
    }
    case Z_DUST: {                              /* grains of the words: two overlapped streams, a fresh grain each */
        const uint32_t gsh = 8u + (zp.h2 & 1u) + (zp.h2 >> 1 & 1u), G = 1u << gsh;
        if (bank_busy) break;
        for (i = 0; i < SURF_TILE; i++) {
            const uint32_t t = t0 + i;
            int32_t acc = 0;
            for (k = 0; k < 2u; k++) {
                const uint32_t tt = t + k * (G >> 1), gi = tt >> gsh, pa = tt & (G - 1u);
                const uint32_t hg = hsh(zp.h ^ ((gi * 2u + k) * 0x9E3779B9u)), w = hg % NKEYW, L = bank[w].len;
                uint32_t idx, p, st;
                if (L < 64u) continue;
                idx = (pa * DUST8[hg >> 20 & 7u] >> 8) % L;
                st = (hg >> 8) % L;
                p = hg >> 31 ? (st + L - idx) % L : (st + idx) % L;   /* some of them backwards */
                acc += (wpool[bank[w].off + p] * (int32_t)bump(pa << (16u - gsh))) >> 7;
            }
            o[i] = acc;
        }
        break;
    }
    case Z_CHAOS: {                             /* x = r x (1 - x), r swept from r0 to r1, a value held H samples */
        const int32_t r0 = 58000 + (int32_t)(zp.h2 % 7536u), r1 = 58000 + (int32_t)((zp.h2 >> 13) % 7536u);
        const int32_t H0 = 2 + (int32_t)(zp.h3 & 15u), H1 = 2 + (int32_t)(zp.h3 >> 4 & 15u);
        uint32_t x = 16384u + (hsh(n0 ^ zp.h) >> 17);
        int32_t cnt = 0;
        const int32_t u0 = t0 >= zp.len ? 32767 : (int32_t)(t0 * 32767u / zp.len), rr0 = r0 + (((r1 - r0) * u0) >> 15);
        for (k = 0; k < 24u; k++) x = ((x * (65536u - x)) >> 16) * (uint32_t)rr0 >> 14;   /* onto the attractor */
        for (i = 0; i < SURF_TILE; i++) {
            const uint32_t t = t0 + i;
            const int32_t u = t >= zp.len ? 32767 : (int32_t)(t * 32767u / zp.len);
            if (++cnt >= H0 + (((H1 - H0) * u) >> 15)) {
                cnt = 0;
                x = ((x * (65536u - x)) >> 16) * (uint32_t)(r0 + (((r1 - r0) * u) >> 15)) >> 14;
                if (x < 16u || x > 65520u) x = 20000u + (t & 4095u);
            }
            o[i] = ((int32_t)x - 32768) * 3 / 4;
        }
        break;
    }
    case Z_HISS: {                              /* noise through a resonant state-variable band-pass, swept */
        const int32_t fa = 150 + (int32_t)(zp.h2 & 1023u) * 3 / 2, fb = 150 + (int32_t)(zp.h2 >> 10 & 1023u) * 3 / 2;
        const int32_t q = 3000 + (int32_t)(zp.h3 & 16383u);
        int32_t lo = 0, bd = 0;
        for (i = 0; i < SURF_TILE + 48u; i++) {  /* 48 samples before the tile first: the filter warms up the same way */
            const uint32_t n = n0 - 48u + i, t = t0 + i - 48u;   /* (wraps huge before the zone: u holds at the end) */
            const int32_t u = t >= zp.len ? 32767 : (int32_t)(t * 32767u / zp.len);
            const int32_t ns = (int32_t)(hsh(n * 0x9E3779B1u ^ zp.h) >> 17) - 16384, f = (fa + (((fb - fa) * u) >> 15)) * 19;
            const int32_t hp = ns - lo - ((q * bd) >> 15);
            bd += (f * (hp >> 1)) >> 14;
            lo += (f * (bd >> 1)) >> 14;
            bd = bd > 60000 ? 60000 : bd < -60000 ? -60000 : bd;
            lo = lo > 60000 ? 60000 : lo < -60000 ? -60000 : lo;
            if (i >= 48u) o[i - 48u] = (bd >> 1) + (ns >> 3);
        }
        break;
    }
    case Z_STAB: {                              /* saw + saw a hair sharp + the fifth + a sub, driven: a horn hit in drop A */
        static const uint16_t ROOT8[8] = {41, 44, 49, 55, 55, 62, 65, 73};
        const uint32_t f0 = ROOT8[zp.h2 & 7u], i0 = HZ(f0), i1 = i0 + (i0 >> 8) * (2u + (zp.h2 >> 3 & 3u)), i5 = i0 * 3u / 2u;
        const int32_t drv = 3 + (int32_t)(zp.h2 >> 5 & 3u), oct = zp.h2 >> 7 & 1u;
        for (i = 0; i < SURF_TILE; i++) {
            const uint32_t t = t0 + i;
            int32_t v = ((int32_t)(i0 * t) >> 18) + ((int32_t)(i1 * t) >> 18) + ((int32_t)(i5 * t) >> 19) + (sine(i0 * t) >> 2);
            if (oct) v += (int32_t)(i0 * 2u * t) >> 19;
            o[i] = soft(v * drv / 4) * 3 / 4;
        }
        break;
    }
    case Z_CUT: {                               /* a word read straight, a little slow: the record everyone cuts */
        static const uint16_t CRATE[4] = {160, 176, 192, 216};
        const uint32_t w = (zp.h2 & 3u) < 3u ? ((zp.h2 & 3u) == 2u ? FRESH_WORD : AHH_WORD) : (zp.h2 >> 4) % NKEYW;
        const uint32_t L = bank[w].len, rt = CRATE[zp.h2 >> 8 & 3u];
        if (bank_busy || L < 64u) break;
        for (i = 0; i < SURF_TILE; i++) {
            const uint32_t q = (t0 + i) * rt, idx = q >> 8;
            if (idx + 1u >= L) break;
            {
                const int32_t a = wpool[bank[w].off + idx], b = wpool[bank[w].off + idx + 1u];
                o[i] = soft(((a << 8) + (((b - a) * (int32_t)(q & 255u)))) * 3 / 2);
            }
        }
        break;
    }
    default: break;
    }
}

static void surf_tile(uint32_t ti)              /* make 256 samples of the record into the window */
{
    const uint32_t n0 = surf_base + ti * SURF_TILE, t0 = n0 & (ZLEN - 1u), flow = p_flow, F = flow * 128u;
    int32_t *o = surf_tmp, *op = surf_tmp2;
    int8_t *out = surf_buf + ti * SURF_TILE;
    zone_t zp, pz;
    uint32_t i, len;
    zone_of(surf_world, n0 >> ZSH, &zp);
    surf_ok[ti] = 1;
    len = zone_len(&zp, flow);
    if (t0 < len) surf_gen(&zp, n0, t0, o); else for (i = 0; i < SURF_TILE; i++) o[i] = 0;
    if (t0 < F) {                               /* FLOW: the zone before runs on under this one's start, fading out */
        zone_of(surf_world, (n0 >> ZSH) - 1u, &pz);
        surf_gen(&pz, n0, t0 + ZLEN, op);
    }
    for (i = 0; i < SURF_TILE; i++) {           /* the envelope, 8 bit; the gaps crackle */
        const uint32_t t = t0 + i;
        int32_t v;
        if (t < F) {                            /* the crossfade: equal-ish power (both halves bowed) */
            const int32_t x = (int32_t)(t * 32767u / F), fi = 32767 - (((32767 - x) * (32767 - x)) >> 15), fo = 32767 - ((x * x) >> 15);
            const int32_t ei = (zone_env_flow(&zp, t, flow) * fi) >> 15, eo = (zone_env_flow(&pz, ZLEN - 1u, flow) * fo) >> 15;
            v = ((o[i] * (ei >> 7)) >> 16) + ((op[i] * (eo >> 7)) >> 16);
        } else if (t < len) v = (o[i] * (zone_env_flow(&zp, t, flow) >> 7)) >> 16;
        else {
            const uint32_t n = n0 + i, c = hsh(n ^ 0xC0FFEEu ^ zp.h), c1 = hsh((n - 1u) ^ 0xC0FFEEu ^ zp.h);
            v = ((c & 1023u) < 3u ? (int32_t)(c >> 26) + 30 : 0) - ((c1 & 1023u) < 3u ? ((int32_t)(c1 >> 26) + 30) / 2 : 0);
            v += ((int32_t)(c >> 29) - 4) >> 1;   /* the surface */
        }
        out[i] = (int8_t)(v > 127 ? 127 : v < -127 ? -127 : v);
    }
}
static void surf_flush(void) { uint32_t k; for (k = 0; k < SURF_NT; k++) surf_ok[k] = 0; }
static void surf_slide(int32_t ds)              /* the window moves along the record a quarter, either way */
{
    const uint32_t nt = SURF_STEP / SURF_TILE, nw = (SURF_WIN - SURF_STEP) / 4u;
    const int32_t q = ds * 65536;
    uint32_t *b = (uint32_t *)surf_buf, k;
    if (ds > 0) {
        for (k = 0; k < nw; k++) b[k] = b[k + SURF_STEP / 4u];
        for (k = 0; k < SURF_NT; k++) surf_ok[k] = k + nt < SURF_NT ? surf_ok[k + nt] : 0u;
    } else {
        for (k = nw; k-- > 0u;) b[k + SURF_STEP / 4u] = b[k];
        for (k = SURF_NT; k-- > 0u;) surf_ok[k] = k >= nt ? surf_ok[k - nt] : 0u;
    }
    surf_base += (uint32_t)ds;
    dk.pos -= q; dk.hand -= q; dk.frz -= q; dk.st_from -= q; dk.st_to -= q; dk.anchor -= q;
    dk.stut = (uint32_t)((int32_t)dk.stut - q);
}
static uint32_t surf_zone_now(void) { return (surf_base + (uint32_t)(dk.pos >> 16)) >> ZSH; }
static void surf_go(uint32_t z)                 /* the needle onto zone z's start; the window jumps if it is far */
{
    const uint32_t zs = z << ZSH;
    if (zs - surf_base < SURF_STEP || zs - surf_base > SURF_WIN - SURF_STEP) { surf_base = zs - SURF_STEP; surf_flush(); }
    dk.pos = (int32_t)((zs - surf_base) << 16);
}

static void deck_start(int32_t w, uint32_t kind, uint32_t once)
{
    uint32_t surf = 0, here = w == SURF_HERE;
    if (bank_busy) return;
    if (here) {                                 /* SURF: on from wherever the needle is */
        surf = 1;
        if (dk.word != SURF_WORD) surf_go((noise() >> 8) % NSURFK);
    } else if (w >= SURF_KEY && w < SURF_KEY + NSURFK) { surf = 1; surf_go((uint32_t)(w - SURF_KEY)); }
    else if (w < 0 || w >= SURF_WORD || !bank[w].len) return;
    if (surf) w = SURF_WORD;
    dk.word = w;
    dk.stut = 0; dk.stut_n = 0;
    if (kind == CMD_SCR) {
        dk.mode = DK_SCR; dk.ph = 0; dk.once = once;
        if (!surf) dk.pos = 0;
        dk.anchor = dk.pos;                     /* a word: from its start; the record: from the spot */
        if (p_style == SC_COMBO) { dk.phrase = (noise() >> 8) % NPHRASE; dk.pstep = 0; dk.tech = PHRASE[dk.phrase][0]; }
        else dk.tech = p_style % NTECH;
    } else {
        if (dk.mode == DK_OFF || dk.mode == DK_REST || dk.mode == DK_LET) dk.rate = TORQUE_SH[p_torque] ? 0 : dk.rate;   /* the motor winds up */
        dk.mode = DK_PLAY;
        if (surf) dk.szone = surf_zone_now();
        else dk.pos = 0;
    }
    dk_word_v = w;
}

static void drum_hit(uint32_t d)
{
    const perc_t *p = &PERC[d];
    const uint32_t tune = (uint32_t)PITCH[p_tune + 6u];        /* -6..+6 semitones */
    dr[d].env = 32767 << 8;
    dr[d].pos = 0;
    dr[d].inc = mulq15((p->fs << 16) / 44100u, tune);
    dr[d].end = (p->len - 1u) << 16;
    if (d == D_HAT) dr[D_OPEN].env = 0;         /* the closed hat chokes the open one */
    drum_hit_v |= 1u << d;
}

static int32_t stroke_len(uint32_t surf)        /* a technique's throw in samples: REACH; SURF half as far again (deep cuts), up to 10240 */
{
    const int32_t r = (int32_t)(2600u + p_reach * 800u);
    return !surf ? r : r * 3 / 2 > 10240 ? 10240 : r * 3 / 2;
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
    static uint32_t G, gacc, stepn, restarted, cmd_seen, drum_seen, crush_h, ew, rot, half_hat, sw_g;
    static uint32_t spin_seen, brake_seen, braking, brake_g0, muted, slam, frz_was;
    static uint32_t ext_seen, ext_start_seen, ext_wait;
    static int32_t jog_seen, f1, crush_v, duck, echo_y, dcz;
    static int32_t tame1, tame2, tame_k = 16384;     /* TAME: the deck's top end, shaved as the record speeds up */
    static int32_t deep1;                          /* DEEP: SURF's low end, pushed up (a shelf under ~160 Hz) */
    static uint32_t last_mode;
    static int32_t mot;
    static int32_t dbuf[64], vbuf[64], wbuf[64];   /* drums, deck, the wet drums */
    static int32_t va1, va2, vlo, vex, venv, vg = 32767, mg = 32767, menv, rvl, rvr;
    static uint32_t dbw, dph;
#ifndef DINGHY_HOST
    const uint32_t t_in = fm1_ticks();
#endif
    uint32_t off, i;
    const uint32_t fsh = CURVE_SH[p_curve % 3u], filt = p_filt, crush = p_crush, stut = p_stut;
    const uint32_t play = p_play, steplen = p_steplen, freeze = p_freeze;
    const int32_t rate_base = p_rate;
    int32_t rate0 = rate_base;
    const int32_t dl_gain = (int32_t)p_blvl * 2731, vl_gain = (int32_t)p_vlvl * 2731;   /* 12 = unity */

    /* the clock: G = where the song is, in 16ths (Q16). Steps fire as G passes them. */
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
            int32_t w = (int32_t)cmd_arg;
            if (w >= SURF_KEY && w < SURF_KEY + NSURFK && !bank_busy) {   /* SURF: the spot, unless the needle is in it */
                if (dk.word != SURF_WORD || surf_zone_now() != (uint32_t)(w - SURF_KEY)) surf_go((uint32_t)(w - SURF_KEY));
                dk.word = w = SURF_WORD; dk_word_v = w;
            }
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
        else deck_start((int32_t)cmd_arg, cmd_kind, 0);
    }
    if (drum_n != drum_seen) {
        uint32_t m = drum_mask, d;
        drum_seen = drum_n;
        for (d = 0; d < NDRUM; d++) if (m >> d & 1u) drum_hit(d);
    }
    if (dk.word < 0 && dk_word_v >= 0) dk.word = dk_word_v;

    /* BACKSPIN: the record thrown backwards, the beat cut until the next bar */
    if (p_spin != spin_seen) {
        spin_seen = p_spin;
        if (dk.word >= 0 && !bank_busy) {
            int32_t len = (int32_t)bank[dk.word].len << 16;
            if (dk.word != SURF_WORD && (dk.mode == DK_OFF || dk.pos >= len || dk.pos <= 0)) dk.pos = len - (1 << 16);
            dk.mode = DK_SPIN;
            dk.rate = -14 * rate0;
        }
        if (play) muted = 1;
    }
    /* BRAKE: the turntable's power cut, slowing down to nothing (the beat with it, unless a clock leads), back on the one */
    if (p_brake != brake_seen) { brake_seen = p_brake; braking = 256; brake_g0 = G; }

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
        uint32_t m = n - off < 64u ? n - off : 64u, d, gblk = 0;
        int32_t gate = 1, ft;

        /* ---- SURF: a new WORLD (or a new voice for the dust) remakes the record; the window follows the needle */
        {
            static uint32_t flow_seen = ~0u;
            if (bank_busy) surf_world = ~0u;
            else if (p_world != surf_world || p_flow != flow_seen) { surf_world = p_world; flow_seen = p_flow; surf_flush(); }
        }
        if (dk.word == SURF_WORD) {
            const int32_t ip = dk.pos >> 16;
            if (dk.mode == DK_SCR) {            /* a technique: keep the whole stroke in the window, by its middle */
                const int32_t mid = (dk.anchor >> 16) + stroke_len(1) / 2;
                if (mid > (int32_t)(SURF_WIN / 2u + SURF_STEP / 2u)) surf_slide((int32_t)SURF_STEP);
                else if (mid < (int32_t)(SURF_WIN / 2u - SURF_STEP / 2u)) surf_slide(-(int32_t)SURF_STEP);
            } else if (ip > (int32_t)(SURF_WIN - SURF_STEP)) surf_slide((int32_t)SURF_STEP);
            else if (ip < (int32_t)SURF_STEP) surf_slide(-(int32_t)SURF_STEP);
            surf_abs_v = surf_base + (uint32_t)(dk.pos >> 16);
        }

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
                if (!braking && play && !p_ext) muted = 1;
                if (!p_ext) ginc = ginc * braking >> 8;         /* a clock leads: the beat goes on, only the record stops */
                rate0 = rate0 * (int32_t)braking >> 8;
            }
            if (ext_wait) ginc = 0;
            G += ginc;
            gblk = ginc;
            dk.ph += ginc >> 2;                                 /* scratches move in beats */
            while (due < 2u) {
                const uint32_t s = stepn & 15u, at = stepn << 16;
                if ((int32_t)(G - at) < 0) break;
                due++;
                if (!bt_init) { bt_init = 1; beat_home(); }
                half_hat = 0;
                if (muted && s == 0u) { muted = 0; slam = 1; }
                if (!muted) {
                    if (play) {                                     /* the practice beat */
                        uint32_t g;
                        if (s == 0u) beat_mutate(stepn >> 4);
                        g = bt_drum[s];
                        if (slam) { g |= 1u << D_CRASH | 1u << D_KICK; slam = 0; }
                        for (d = 0; d < NDRUM; d++) if (sw_g >> d & 1u) drum_hit(d);   /* a late 16th still owed */
                        sw_g = 0;
                        if (s & 1u) sw_g = g;                       /* the off 16ths swing: they land a little late */
                        else for (d = 0; d < NDRUM; d++) if (g >> d & 1u) drum_hit(d);
                    } else slam = 0, sw_g = 0;
                    if (p_auto && p_surfmode) {                     /* ETERNAL SKRATCH: the robot surfs on, forever */
                        const uint32_t r = noise() >> 24;
                        if (dk.mode == DK_FRZ || dk.mode == DK_HAND || dk.mode == DK_THROW || dk.mode == DK_SPIN || dk.mode == DK_STROKE) {}
                        else if (dk.word != SURF_WORD || dk.mode == DK_OFF || dk.mode == DK_REST) deck_start(SURF_HERE, CMD_SAY, 0);
                        else if (dk.mode == DK_PLAY && ((s == 0u && r < 120u) || (s == 8u && r < 90u) || (s == 12u && r < 50u)))
                            deck_start(SURF_HERE, CMD_SCR, 1);          /* a technique right where it is */
                        else if (dk.mode == DK_PLAY && s == 4u && r < 40u) {   /* a wave: skip on a zone or two */
                            surf_go(surf_zone_now() + 1u + (r & 1u));
                            deck_start(SURF_HERE, CMD_SAY, 0);
                        }
                    } else if (p_auto && dk.mode != DK_FRZ && dk.mode != DK_HAND) {   /* the robot DJ */
                        uint32_t r = noise() >> 24;
                        if ((s == 0 && r < 180) || (s == 8 && r < 100) || (s == 6 && r < 30))
                            deck_start((int32_t)(noise() >> 8) % NWORDS, CMD_SAY, 0);
                        else if ((s == 12 && r < 90) || (s == 4 && r < 40))
                            deck_start(dk.word >= 0 ? dk.word : 0, CMD_SCR, 1);
                    }
                }
                if (stut && dk.mode == DK_PLAY) {               /* STUTTER: back to the slice's start, a new slice a beat */
                    const uint32_t every = stut < 6u ? 4u : stut < 11u ? 2u : 1u;
                    if (!(s % every)) {
                        if (!dk.stut_n || !(s % (every * 4u))) { dk.stut = (uint32_t)dk.pos; dk.stut_n = 1; }
                        else dk.pos = (int32_t)dk.stut;
                    }
                }
                seq_step_v = stepn++;
            }
            {
                const uint32_t into = G - ((stepn - 1u) << 16);
                seq_frac_v = into >= 65536u ? 255u : into >> 8;
            }
            if (sw_g && seq_frac_v >= SWING) {                  /* the swung 16th lands */
                for (d = 0; d < NDRUM; d++) if (sw_g >> d & 1u) drum_hit(d);
                sw_g = 0;
            }
            if (play && !muted && !half_hat && seq_frac_v >= 128u + SWING / 2u && (bt_roll >> ((stepn - 1u) & 15u) & 1u)) {
                half_hat = 1;                                   /* a hat roll: the 32nd between */
                drum_hit(D_HAT);
            }
        }

        /* ---- the deck: where the record goes this block */
        if (dk.mode == DK_PLAY && last_mode != DK_PLAY) {   /* the motor takes the record as it finds it */
            mot = dk.rate;
            if (dk.word == SURF_WORD) dk.szone = surf_zone_now();   /* let go on the record: it plays that zone out */
        }
        last_mode = dk.mode;
        if (dk.word >= 0 && !bank_busy) {
            int32_t len = (int32_t)bank[dk.word].len << 16;
            switch (dk.mode) {
            case DK_PLAY: {                                     /* the motor pulls it to speed at its TORQUE */
                const int32_t tr = rate0;
                const uint32_t ts = TORQUE_SH[p_torque & 3u];
                mot = ts ? mot + ((tr - mot) >> ts) + (tr > mot) - (tr < mot) : tr;
                dk.rate = mot;
                if (dk.word == SURF_WORD) {         /* a tap: its zone, and with FLOW the crossfade into the next; ETERNAL: never stops */
                    const uint32_t into = surf_base + (uint32_t)(dk.pos >> 16) - (dk.szone << ZSH);
                    if (!(p_auto && p_surfmode) && into >= ZLEN + p_flow * 128u) dk.mode = DK_OFF;
                }
                else if (dk.pos >= len || dk.pos < 0) dk.mode = DK_OFF;
                break;
            }
            case DK_FRZ: dk.rate = rate0; break;
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
                if (dk.word == SURF_WORD) dk.anchor += (int32_t)(p_surf * 256u * (gblk >> 2));   /* SURF: the spot drifts on, p_surf x 256 samples a beat */
                target = dk.anchor + ((tech_pos(dk.tech, dk.ph) * stroke_len(dk.word == SURF_WORD)) << 1);   /* Q16 */
                if (target > len) target = len;
                if (target < 0) target = 0;
                dk.rate = (target - dk.pos) / (int32_t)m;
                break;
            }
            case DK_HAND:
            {                                   /* the hand: 1/4 of the way each block, up to 16x (SURF 32x) */
                const int32_t cap = (dk.word == SURF_WORD ? 32 : 16) * rate0;
                dk.rate = ((dk.hand - dk.pos) >> 2) / (int32_t)m;
                if (dk.rate > cap) dk.rate = cap;
                if (dk.rate < -cap) dk.rate = -cap;
                break;
            }
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
            const int8_t *w = dk.word == SURF_WORD ? surf_buf : wpool + bank[dk.word].off;
            const int32_t len = (int32_t)bank[dk.word].len - 1, frz = dk.mode == DK_FRZ;
            int32_t pos = dk.pos, rate = dk.rate, fd = dk.fader;
            const int32_t fast = rate > 65536 || rate < -65536;   /* over a word sample a frame: average what it passed */
            {   /* TAME: two one-poles; flat up to 1.75x the motor, then the faster it goes the darker (to ~2 kHz at 8x) */
                const int32_t ar0 = rate < 0 ? -rate : rate, ar = ar0 > (1 << 22) ? 1 << 22 : ar0, r0 = rate0 > 0 ? rate0 : 1;
                const int32_t x = (ar << 8) / r0, x0 = dk.word == SURF_WORD ? 320 : 448;   /* speed, Q8: 256 = the motor; SURF darkens sooner */
                int32_t k = x <= x0 ? 16384 : 16384 * x0 / x;
                if (k < 4000) k = 4000;
                tame_k += (k - tame_k) >> 1;
            }
            if (dk.word == SURF_WORD) {                 /* the record under the needle this block: made if not yet */
                int32_t a = pos >> 16, b = (pos + rate * (int32_t)m) >> 16, k;
                if (a > b) { k = a; a = b; b = k; }
                a -= 26; b += 2;
                if (frz) { if (a > (dk.frz >> 16)) a = dk.frz >> 16; if (b < ((dk.frz + FRZ_WIN) >> 16) + 1) b = ((dk.frz + FRZ_WIN) >> 16) + 1; }
                if (a < 0) a = 0;
                if (b > (int32_t)SURF_WIN - 1) b = (int32_t)SURF_WIN - 1;
                {   /* at most 3 a block (a jump lands on nothing made yet): the rest silent for now, made in the blocks after */
                    uint32_t made = 0;
                    for (k = a >> 8; k <= b >> 8 && a <= b; k++) {
                        if (surf_ok[k]) continue;
                        if (made++ < 3u) surf_tile((uint32_t)k);
                        else { uint32_t j, *z = (uint32_t *)(surf_buf + (uint32_t)k * SURF_TILE); for (j = 0; j < SURF_TILE / 4u; j++) z[j] = 0; }
                    }
                }
            }
            for (i = 0; i < m; i++) {
                int32_t ip, s = 0;
                if (frz && pos >= dk.frz + FRZ_WIN) pos -= FRZ_WIN;
                ip = pos >> 16;
                fd += (ft - fd) >> fsh;
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
                }
                tame1 += ((s - tame1) * tame_k) >> 14;          /* TAME (k 16384 = straight through) */
                tame2 += ((tame1 - tame2) * tame_k) >> 14;
                s = tame2;
                if (dk.word == SURF_WORD) { deep1 += ((s - deep1) * 1150) >> 15; s = s - (s >> 2) + deep1; }   /* DEEP: ~+7 dB under 250 Hz, the top -2.5 */
                s = (s * fd) >> 15;
                dcz += (s - dcz) >> 10;                         /* a still record is silence, not a step */
                vbuf[i] = s - dcz;
                pos += rate;
            }
            dk.pos = pos;
            dk.fader = fd;
        } else {                                                /* shut, the record still moves */
            dk.fader = 0; dk.pos += dk.rate * (int32_t)m;
        }
        rot += (uint32_t)(dk.mode != DK_OFF ? dk.rate : play ? rate0 : 0) * 3u * m;

        /* ---- the drums */
        for (i = 0; i < m; i++) dbuf[i] = wbuf[i] = 0;
        for (d = 0; d < NDRUM; d++) {
            uint32_t wet;
            const perc_t *p = &PERC[d];
            const int16_t *pc = p->pcm;
            const uint32_t inc = dr[d].inc, end = dr[d].end;
            int32_t env = dr[d].env;
            uint32_t pos = dr[d].pos;
            if (env < 512 || pos >= end) continue;
            wet = (1u << d) & WET_DRUMS;
            for (i = 0; i < m && pos < end; i++) {
                const int32_t a = pc[pos >> 16], b = pc[(pos >> 16) + 1u];
                int32_t s = a + (((b - a) * (int32_t)((pos >> 1) & 0x7FFFu)) >> 15);
                pos += inc;
                s = (s * (env >> 8)) >> 15;
                s = (s * p->gain) >> 15;
                dbuf[i] += s;
                if (wet) wbuf[i] += s;
            }
            dr[d].env = pos < end ? env : 0; dr[d].pos = pos;
        }
        if ((dr[D_KICK].env && dr[D_KICK].pos < (240u << 16)) || (dr[D_808].env && dr[D_808].pos < (240u << 16))) duck = 32767;

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
            int32_t x = vbuf[i], o, dd, side, rl = rvl, rr = rvr;
            va1 += ((x - va1) * 21000) >> 15;                   /* de-image: the 11 kHz record's mirror fizz off */
            va2 += ((va1 - va2) * 21000) >> 15;
            vlo += ((va2 - vlo) * 1150) >> 15;                  /* body: + ~3 dB under 250 Hz */
            x = va2 + ((vlo * 3) >> 3);
            if (filt < 7u) { f1 += ((x - f1) * (int32_t)(1500u + filt * 4200u)) >> 15; x = f1; }   /* FILTER: 0..6 low-pass, 7..8 open, 9..15 high-pass */
            else if (filt > 8u) { f1 += ((x - f1) * (int32_t)((filt - 8u) * 1800u)) >> 15; x -= f1; }
            else f1 = x;
            if (crush) {                                        /* CRUSH: fewer bits, a lower rate */
                const uint32_t keep = 12u - (crush * 9u >> 4), hold = 1u + (crush >> 2);
                if (++crush_h >= hold) { crush_h = 0; crush_v = x & ~(int32_t)((1u << (16u - keep)) - 1u); }
                x = crush_v;
            }
            {                                                   /* the vocal comp (fast, upfront), then the exciter (air) */
                const int32_t a = x < 0 ? -x : x;
                venv += a > venv ? (a - venv) >> 4 : -(venv >> 11);
                x = ((x * vg) >> 15) * 11 >> 3;
                vex += ((x - vex) * 11500) >> 15;
                x += (soft((x - vex) * 3) * 5) >> 4;
            }
            x = (x * vl_gain) >> 15;
            if (duck) { x = (x * (32767 - (duck * 5 >> 3))) >> 15; duck -= (duck >> 11) + 1; if (duck < 0) duck = 0; }
            else duck = 0;

            /* echo at 22050 Hz, the feedback darkened */
            if (!(i & 1u)) {
                uint32_t dl = steplen * 3u / 2u, rd;            /* a dotted 8th */
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
                    const int32_t vs = 9000;
                    int32_t in = ((x * vs) >> 15) + ((wbuf[i] * 9000) >> 15) + ((echo_y * 8000) >> 15), y, v, d0, d1, d2, d3, s1, s2, s3, s4;
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
            dd = soft(dd + (dd >> 1));                            /* the kit into the red */
            {                                                   /* the doubler: the vocal ~11 ms later, drifting, as the side */
                const uint32_t dq = 128000u + (uint32_t)((sine(dph) + 32768) >> 4) * 46u / 8u;   /* Q8 samples: 500 .. 590 */
                const uint32_t r0 = (dbw - (dq >> 8)) & 1023u, r1 = (r0 - 1u) & 1023u;
                const int32_t a = dbl[r0], b = dbl[r1];
                dbl[dbw] = (int16_t)CLIP(x);
                dbw = (dbw + 1u) & 1023u;
                dph += 58434u;                                  /* 0.6 Hz */
                side = ((a + (((b - a) * (int32_t)(dq & 255u)) >> 8)) * 8200) >> 15;
            }
            {
                int32_t xl = x + side, xr = x - side, l, r;
                o = ((echo_y * 3) >> 2) + dd;
                l = ((o + xl + rl) * 25000) >> 15;
                r = ((o + xr + rr) * 25000) >> 15;
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

/* the top bar: the name, what is happening now (or the record and tempo) */
static void draw_top(const char *now, uint16_t now_c)
{
    cv_begin(240, 20, C_BLACK);
    cv_text(4, 2, &FONT_S, "NU//METAL", C_PINK);
    cv_text(236 - text_w(&FONT_S, now), 2, &FONT_S, now, now_c);
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

/* SURF: the endless record as a landscape scrolling under the needle, five zones of it; each zone's envelope is a
 * mountain in its material's green (it is nu metal: everything glows toxic), lit along its ridge with a haze above,
 * brighter where the surfer rides; the 27 spots the keys play are marked */
static const uint16_t ZCOL[Z_COUNT] = {RGB(150, 255, 0), RGB(0, 255, 140), RGB(70, 210, 40), RGB(210, 255, 60),
                                       RGB(120, 190, 70), RGB(0, 200, 90), RGB(150, 180, 130), RGB(100, 255, 20),
                                       RGB(220, 255, 150)};
static uint16_t dim565(uint16_t c, uint32_t q)   /* c at q/8 */
{
    return (uint16_t)(((((uint32_t)c >> 11) * q >> 3) << 11) | (((((uint32_t)c >> 5) & 63u) * q >> 3) << 5) | ((c & 31u) * q >> 3));
}
static void draw_surf(uint32_t world, uint32_t abs, uint32_t rot, uint32_t on, uint32_t fader, uint16_t acc)
{
    const uint32_t SPX = 256u, n0 = abs - 80u * SPX;
    uint32_t x, zc = ~0u;
    int32_t hn = 0;
    zone_t zp;
    cv_begin(240, 64, C_BLACK);
    platter(34, 32, rot, on, fader, acc);
    {
        const int32_t h = dk_speed_v > 64u ? 60 : (int32_t)(dk_speed_v * 60u / 64u);
        cv_rect(69, 2, 3, 60, C_DEEP);
        cv_rect(69, 62 - h, 3, h, h > 30 ? RGB(220, 255, 150) : h > 15 ? C_PINK : C_MID);
    }
    for (x = 0; x < 160u; x++) {
        const uint32_t n = n0 + x * SPX, z = n >> ZSH, t = n & (ZLEN - 1u);
        int32_t h;
        if (z != zc) { zc = z; zone_of(world, z, &zp); }
        h = zone_env_flow(&zp, t, p_flow) * 40 >> 15;
        if (h && (zp.mat == Z_HISS || zp.mat == Z_CHAOS || zp.mat == Z_BYTE)) h -= (int32_t)(hsh(n >> 8) & 3u);   /* rough ground */
        if (h < 1) h = t < zone_len(&zp, p_flow) ? 1 : 0;
        if (h) {
            const int32_t dx = (int32_t)x - 80, ax = dx < 0 ? -dx : dx;
            const uint32_t lit = ax < 24 && on ? 6u - (uint32_t)ax / 6u : 2u;   /* the glow around the surfer */
            const uint16_t c = ZCOL[zp.mat];
            cv_rect(76 + (int32_t)x, 60 - h, 1, h, dim565(c, lit));
            cv_rect(76 + (int32_t)x, 60 - h, 1, h < 2 ? h : 2, c);               /* the ridge */
            if (h < 57) cv_rect(76 + (int32_t)x, 57 - h, 1, 3, dim565(c, fader > 16000u && on ? 3u : 1u));   /* the haze */
        }
        if (t < SPX) cv_rect(76 + (int32_t)x, 61, 1, 3, z < NSURFK ? RGB(220, 255, 150) : C_DEEP);   /* a zone starts: bright = a key */
        if (x == 80u) hn = h;
    }
    cv_rect(76, 60, 160, 1, C_DEEP);
    cv_rect(156, 0, 1, 60 - hn - 4, on ? (fader > 16000u ? C_TXT : C_PINK) : C_GRY);   /* the needle */
    cv_rect(149, 60 - hn - 5, 15, 4, on ? dim565(C_PINK, 3u) : C_DEEP);                 /* the board's glow */
    cv_rect(151, 60 - hn - 4, 11, 2, on ? C_PINK : C_GRY);                           /* the surfer's board */
    cv_rect(155, 60 - hn - 9, 3, 5, on ? C_TXT : C_GRY);
    cv_blit(0, 60);
}

static void draw_score_preview(uint32_t t, uint16_t acc)   /* STYLE just turned: what it will play */
{
    cv_begin(240, 64, C_BLACK);
    score(8, 224, t, -1, acc);
    cv_blit(0, 60);
}

static void draw_cheat(void)                    /* EDIT held: what the buttons do */
{
    static const char *const L[9] = {"PLAY BEAT", "FX DRY", "ENV FRZ", "REC SPIN", "SEQ BRAKE", "LFO CUT",
                                     "GLO AUTO", "SEL HAM", "ARP WRLD"};
    uint32_t k;
    cv_begin(240, 64, C_DEEP);
    for (k = 0; k < 9u; k++) cv_text(2 + (int32_t)(k % 3u) * 80, (int32_t)(k / 3u) * 16, &FONT_S, L[k], C_YEL);
    cv_text(2, 48, &FONT_S, "HOME held = SETTINGS", C_TXT);
    cv_blit(0, 60);
}

static void draw_grid(uint32_t cur, uint32_t run, uint32_t ext, uint32_t beat)   /* the bar: where the clock is */
{
    uint32_t s;
    cv_begin(240, 20, C_BLACK);
    for (s = 0; s < 16u; s++) {
        const int32_t x = 4 + (int32_t)s * 14 + (int32_t)(s >> 2) * 2;
        const uint32_t here = run && s == cur;
        cv_rect(x, 6, 12, 8, here ? (ext ? C_YEL : C_PINK) : !(s & 3u) ? C_MID : C_DEEP);
        if (beat && !(s & 3u)) cv_rect(x + 4, 16, 4, 2, C_PINK);
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
static void draw_chips(uint32_t aut, uint32_t ham, uint32_t beat, uint32_t ext, uint32_t sync_off, int32_t semi)
{
    char p[6] = "P", *q = p + 1;
    int32_t x = 2;
    put_s(q, semi);
    cv_begin(240, 24, C_BLACK);
    x = chip(x, "BEAT", beat, C_PINK);
    x = chip(x, "AUTO", aut, C_CYAN);
    x = chip(x, "HAM", ham, C_RED);
    x = chip(x, sync_off ? "NO SYNC" : "SYNC", ext, C_YEL);
    if (x + 40 <= 240) chip(x, p, semi != 0, C_TXT);
    cv_blit(0, 216);
}

/* =========================================================================== the main loop */
#ifdef DINGHY_HOST
#define NOW_US() (fm1_ms * 1000u)
#else
#define NOW_US() fm1_micros()
#endif
static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static void deck_cmd(uint32_t kind, uint32_t arg) { cmd_arg = arg; cmd_kind = kind; cmd_n++; }
#define WNOTE(w) (48u + (uint32_t)((w) >= SURF_KEY ? (w) - SURF_KEY : (w)))   /* a key's MIDI note out: a word or a SURF spot */
static void drums_now(uint32_t mask) { drum_mask = mask; drum_n++; }
/* General MIDI drum notes 35..59 (channel 10) -> the pads, so an e-kit plays them (Roland TD-8 defaults:
 * kick 36, snare 38 / rim 40, toms 48 45 43 41 (rims 50 47 58 39), hats 42 46 44, crash 49 / 57, ride 51 / bell 53) */
static const int8_t GM_DRUM[25] = {
    D_808, D_KICK, D_RIM, D_SNARE, D_CLAP, D_SNARE, D_TOML, D_HAT, D_TOML, D_HAT, D_TOML, D_OPEN, D_TOMH,  /* 35..47 */
    D_TOMH, D_CRASH, D_TOMH, D_HAT, D_CHINA, D_ZAP, -1, D_CRASH, -1, D_CRASH, D_TOML, D_HAT              /* 48..59 */
};
/* ...and back: every drum the FM-1 plays (band, pads, e-kit) goes out on ch 10 as this GM note, for visuals and gear */
static const uint8_t DRUM_GM[NDRUM] = {36, 38, 39, 42, 46, 37, 45, 48, 35, 52, 53, 49};
static void filt_name(char *v, uint32_t f)       /* LP 7 .. OPEN .. HP 7 */
{
    if (f < 7u) put_u(put(v, "LP "), 7u - f);
    else if (f > 8u) put_u(put(v, "HP "), f - 8u);
    else put(v, "OPEN");
}
static const char *stut_name(uint32_t st) { return !st ? "OFF" : st < 6u ? "1/4" : st < 11u ? "1/8" : "1/16"; }
static void rec_name(char *v, uint32_t r) { if (!r) put(v, "WORDS"); else put_u(put(v, "WLD "), r - 1u); }

static void app_main(void)
{
    struct {                                    /* what survives a power-off (4.0: a new layout, the old one is ignored) */
        uint8_t ver, filt, echo, crush, stut, style, reach, voice, rec, last_world, aut, hamster, torque, space, blvl, flow, sync, surf;
        int8_t semi, pad;
        uint16_t bpm;
    } set;
    uint32_t prev = 0, k, t_change = 0, dirty = 0, play = 0, shift = 0, shift_was = 0, home = 0, home_was = 0;
    uint32_t home_turned = 0, t_home = 0, fx_held = 0, t_score = 0;
    uint32_t knobs = 1, chips = 1, top_key = ~0u, t_draw = 0, grid_drawn = ~0u;
    uint32_t t_jog = 0, t_voice = 0, voice_pending = 0, t_toast = 0, toast_on = 0, deck_view = ~0u;
    uint32_t t_down[27] = {0}, scr[27] = {0}, dmask;
    uint32_t t_cc = 0, cc_on = ~0u, cc_fd = ~0u, cc_an = ~0u;
    int32_t word_drawn = -2, held_word[27];
    char toast[24];
    int32_t jog_v = 0, last_tap_w = -1;
    uint32_t t_tap = 0, tap_n = 0;
    uint32_t ext_us[49], ext_n = 0, ext_ticks = 0, t_tick = 0, ext_live = 0, ext_bpm = 0, ext_len = 0;

    for (k = 0; k < sizeof set; k++) ((uint8_t *)&set)[k] = 0;
    set.ver = 40; set.filt = 7; set.echo = 3; set.reach = 3; set.rec = 1; set.last_world = 1; set.torque = 3; set.space = 7;
    set.blvl = 12; set.flow = 12; set.surf = 4; set.bpm = 120;
    settings_load(&set, sizeof set);
    if (set.ver != 40u) { set.ver = 40; set.filt = 7; set.echo = 3; set.rec = 1; }
    if (set.filt > 15u) set.filt = 7;
    if (set.echo > 15u) set.echo = 3;
    if (set.crush > 15u) set.crush = 0;
    if (set.stut > 15u) set.stut = 0;
    set.style %= SC_COUNT; set.voice %= NVOICES; set.torque &= 3u;
    if (set.reach > 7u) set.reach = 3;
    if (set.rec > 100u) set.rec = 1;
    if (!set.last_world || set.last_world > 100u) set.last_world = 1;
    set.aut = set.aut ? 1u : 0u; set.hamster = set.hamster ? 1u : 0u; set.sync = set.sync ? 1u : 0u;
    if (set.space > 15u) set.space = 7;
    if (set.blvl > 15u) set.blvl = 12;
    if (set.flow > 15u) set.flow = 12;
    if (set.surf > 15u) set.surf = 4;
    set.semi = (int8_t)clampi(set.semi, -12, 12);
    set.bpm = (uint16_t)clampi(set.bpm, 60, 180);
    for (k = 0; k < 27u; k++) held_word[k] = -1;
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

    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 44, 240, &FONT_L, "NU//METAL", C_PINK, 1);
    draw_text_box(0, 84, 240, &FONT_L, "SKRATCH", C_CYAN, 1);
    draw_text_box(0, 116, 240, &FONT_L, "MASTER", C_CYAN, 1);
    draw_text_box(0, 150, 240, &FONT_L, "PRO", C_YEL, 1);
    draw_text_box(0, 190, 240, &FONT_S, "jog. tap. cut.", C_GRY, 1);
    draw_text_box(0, 210, 240, &FONT_S, "SKRATCH 4.0", C_PINK, 1);
    make_tones();
    bank[SURF_WORD].off = 0; bank[SURF_WORD].len = SURF_WIN - 64u; bank[SURF_WORD].loop = 0;   /* the window, read like a word */
    render_bank(set.voice);
    deck_cmd(CMD_SAY, NAME_WORD);               /* it says its name */
    lcd_fill(0, 0, 240, 240, C_BLACK);

    for (;;) {
        uint32_t keys, down, up, rel, b, btn, now;
        const uint32_t surf = set.rec != 0u;
        int32_t e;
        uint8_t st, d1, d2;
        platform_poll();
        now = fm1_ms;
        btn = fm1_in.buttons;
        shift = btn >> BTN_EDIT & 1u;
        home = btn >> BTN_HOME & 1u;
        if (shift != shift_was || home != home_was) { shift_was = shift; knobs = 1; deck_view = ~0u; top_key = ~0u; }

#define TOAST(name, v) do { char *tp_ = put(toast, name); tp_ = put(tp_, " "); put(tp_, v); toast_on = 1; t_toast = now; } while (0)
        /* ---- encoders: one layer; HOME held = SETTINGS */
        {
            char v[12];
            if (home) {
                int32_t e2;
                uint32_t turned = 0;
                if ((e2 = fm1_enc_take(ENC_SELECT)) != 0) { turned = 1; set.bpm = (uint16_t)clampi(set.bpm + e2, 60, 180); put_u(v, set.bpm); TOAST("BPM", v); }
                if ((e2 = fm1_enc_take(ENC_ALGORITHM)) != 0) { turned = 1; set.reach = (uint8_t)clampi(set.reach + e2, 0, 7); put_u(v, set.reach + 1u); TOAST("REACH", v); }
                if ((e2 = fm1_enc_take(ENC_KNOB1)) != 0) { turned = 1; set.voice = (uint8_t)clampi(set.voice + e2, 0, NVOICES - 1); voice_pending = 1; t_voice = now; TOAST("", VOICES[set.voice].name); }
                if ((e2 = fm1_enc_take(ENC_KNOB2)) != 0) { turned = 1; set.flow = (uint8_t)clampi(set.flow + e2, 0, 15); put_u(v, set.flow); TOAST("FLOW", v); }
                if ((e2 = fm1_enc_take(ENC_KNOB3)) != 0) { turned = 1; set.space = (uint8_t)clampi(set.space + e2, 0, 15); put_u(v, set.space); TOAST("VERB", v); }
                if ((e2 = fm1_enc_take(ENC_KNOB4)) != 0) { turned = 1; set.blvl = (uint8_t)clampi(set.blvl + e2, 0, 15); put_u(v, set.blvl); TOAST("BEAT", v); }
                if ((e2 = fm1_enc_take(ENC_PRESETS)) != 0) { turned = 1; set.torque = (uint8_t)clampi(set.torque + e2, 0, 3); TOAST("TORQUE", TORQUE_NAME[set.torque]); }
                if (turned) { home_turned = 1; knobs = dirty = 1; }
            } else {
                if ((e = fm1_enc_take(ENC_SELECT)) != 0) {     /* the jog, and how fast it turns (clicks a second) */
                    const uint32_t dt = now - t_jog < 1u ? 1u : now - t_jog;
                    const int32_t inst = dt > 300u ? e * 3 : e * 1000 / (int32_t)dt;
                    int32_t av;
                    jog_v = (jog_v * 3 + inst) / 4;
                    av = jog_v < 0 ? -jog_v : jog_v;
                    t_jog = now;
                    {   /* samples a click: slow = fine control, fast = big throws (the 2.x feel); SURF 4x (its zones are long, deep cuts) */
                        const int32_t sf = surf ? 4 : 1, g = (av * 10 + 100) * sf;
                        p_jog_gain = g > 1600 * sf ? 1600 * sf : g;
                    }
                    jog_total += e; jog_held = 1;
                } else if (jog_held && now - t_jog > ((jog_v > 12 || jog_v < -12) ? 60u : 300u)) {   /* let go: a fast flick throws the record */
                    const int32_t av = jog_v < 0 ? -jog_v : jog_v;
                    jog_held = 0;
                    if (av > 12) {
                        int32_t r = jog_v * p_jog_gain * 3 / 2;      /* clicks/s * samples/click / 44100 * 65536 */
                        const int32_t tc = (surf ? 32 : 16) * p_rate;
                        p_throw_rate = r > tc ? tc : r < -tc ? -tc : r;
                        p_throw++;
                    }
                    jog_v = 0;
                } else if (now - t_jog > 300u) jog_v = 0;
                if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {
                    set.style = (uint8_t)clampi(set.style + e, 0, SC_COUNT - 1); TOAST("", SC_NAME[set.style]);
                    t_score = now; deck_view = ~0u; knobs = dirty = 1;
                }
                if ((e = fm1_enc_take(ENC_KNOB1)) != 0) { set.filt = (uint8_t)clampi(set.filt + e, 0, 15); filt_name(v, set.filt); TOAST("FILTER", v); knobs = dirty = 1; }
                if ((e = fm1_enc_take(ENC_KNOB2)) != 0) { set.echo = (uint8_t)clampi(set.echo + e, 0, 15); put_u(v, set.echo); TOAST("ECHO", v); knobs = dirty = 1; }
                if ((e = fm1_enc_take(ENC_KNOB3)) != 0) { set.crush = (uint8_t)clampi(set.crush + e, 0, 15); put_u(v, set.crush); TOAST("CRUSH", v); knobs = dirty = 1; }
                if ((e = fm1_enc_take(ENC_KNOB4)) != 0) { set.stut = (uint8_t)clampi(set.stut + e, 0, 15); TOAST("STUTTER", stut_name(set.stut)); knobs = dirty = 1; }
                if ((e = fm1_enc_take(ENC_PRESETS)) != 0) {
                    set.rec = (uint8_t)clampi(set.rec + e, 0, 100); rec_name(v, set.rec); TOAST("", v);
                    if (set.rec) set.last_world = set.rec;
                    knobs = chips = dirty = 1; deck_view = ~0u; word_drawn = -2;
                }
            }
        }

        /* ---- buttons */
        b = fm1_input_edges(&rel);
        if (shift && b & (1u << BTN_SAVE)) {           /* SYNC: FOLLOW an incoming MIDI clock / OFF */
            set.sync = (uint8_t)(!set.sync);
            TOAST("SYNC", set.sync ? "OFF" : "FOLLOW");
            chips = dirty = 1;
            b &= ~(1u << BTN_SAVE);
        }
        if (b & (1u << BTN_PLAY)) { play = !play; if (play && !ext_live) p_restart++; chips = 1; TOAST("BEAT", play ? "ON" : "OFF"); }
        if (b & (1u << BTN_REC)) { p_spin++; TOAST("", "BACKSPIN"); }
        if (b & (1u << BTN_SEQ)) { p_brake++; TOAST("", "BRAKE"); }
        if (b & (1u << BTN_GLO)) { set.aut = !set.aut; chips = dirty = 1; TOAST("AUTO", set.aut ? "ON" : "OFF"); }
        if (b & (1u << BTN_SEL)) { set.hamster = !set.hamster; chips = dirty = 1; TOAST("HAMSTER", set.hamster ? "ON" : "OFF"); }
        if (b & (1u << BTN_ARP)) {                     /* WORDS <-> the last WORLD */
            char v[12];
            set.rec = set.rec ? 0u : set.last_world; rec_name(v, set.rec); TOAST("", v);
            knobs = chips = dirty = 1; deck_view = ~0u; word_drawn = -2;
        }
        if (b & (1u << BTN_HOME)) { home_turned = 0; t_home = now; }
        if (home_was && !home) {                       /* HOME let go: a tap = pitch back to 0 */
            if (!home_turned && now - t_home < 600u && set.semi) { set.semi = 0; chips = dirty = 1; TOAST("PITCH", "0"); }
            knobs = 1; deck_view = ~0u;
        }
        home_was = home;
        if (b & (1u << BTN_OCT_DOWN) && set.semi > -12) { set.semi--; chips = dirty = 1; }
        if (b & (1u << BTN_OCT_UP) && set.semi < 12) { set.semi++; chips = dirty = 1; }
        if (b & (1u << BTN_SAVE)) { settings_save(&set, sizeof set); dirty = 0; TOAST("", "SAVED"); }
        if ((btn >> BTN_FX & 1u) != fx_held) { fx_held = btn >> BTN_FX & 1u; knobs = 1; top_key = ~0u; }
        if (dirty && (knobs || chips)) t_change = now;

        /* ---- to the ISR */
        p_filt = fx_held ? 7u : set.filt; p_crush = fx_held ? 0u : set.crush; p_stut = fx_held ? 0u : set.stut;
        p_echo = fx_held ? 0u : set.echo;
        p_torque = set.torque; p_hamster = set.hamster; p_space = set.space; p_blvl = set.blvl;
        p_style = set.style; p_reach = set.reach; p_auto = set.aut; p_play = play;
        p_world = set.rec ? set.rec - 1u : set.last_world - 1u; p_surf = set.surf; p_flow = set.flow; p_surfmode = surf;
        p_cut = btn >> BTN_LFO & 1u;
        p_freeze = btn >> BTN_ENV & 1u;
        if (ext_n && now - t_tick > 400u) ext_n = 0;    /* the clock stopped coming */
        if (ext_live != (set.sync == 0u && ext_n >= 3u && ext_len)) { ext_live = !ext_live; chips = 1; }
        p_ext = ext_live;
        p_steplen = ext_live ? ext_len : 661500u / set.bpm;
        p_rate = (int32_t)((16384u * (uint32_t)PITCH[set.semi + 12]) >> 15);

        /* ---- the keys: 27 spots, tap = the needle, taps again = strokes, held = the technique */
        keys = fm1_in.notes;
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        for (k = 0; k < 27u; k++) {
            const int32_t w = surf ? SURF_KEY + (int32_t)k : (int32_t)k;
            if (down >> k & 1u) {
                const uint32_t gap = now - t_tap;
                t_down[k] = now; scr[k] = 0;
                held_word[k] = w;
                if (w == last_tap_w && gap < 450u) {             /* taps: finger strokes */
                    const uint32_t g = gap < 60u ? 60u : gap;
                    int32_t span = (int32_t)(1200u + 500000u / g);
                    if (span > 8000) span = 8000;
                    cmd_span = tap_n & 1u ? span : -span;       /* back first, then forward */
                    cmd_dur = g * 31u;                           /* 70 % of the gap, in frames */
                    tap_n++;
                    scr[k] = 2;                                  /* no held scratch on a stroke */
                    deck_cmd(CMD_STROKE, (uint32_t)w);
                } else {
                    tap_n = 0;
                    deck_cmd(CMD_SAY, (uint32_t)w);
                }
                last_tap_w = w;
                t_tap = now;
                out_msg(0x90, WNOTE(w), 110);
            }
            if (keys >> k & 1u && !scr[k] && now - t_down[k] > 180u && held_word[k] >= 0) {   /* held: scratch it */
                scr[k] = 1;
                deck_cmd(CMD_SCR, (uint32_t)held_word[k]);
            }
            if (up >> k & 1u) {
                if (scr[k] == 1u) deck_cmd(CMD_LET, 0);
                if (held_word[k] >= 0) out_msg(0x80, WNOTE(held_word[k]), 0);
                held_word[k] = -1;
                scr[k] = 0;
            }
        }
        dmask = 0;
        while (midi_get(&st, &d1, &d2)) {       /* USB-MIDI and TRS MIDI IN */
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
                } else if (st == 0xFAu) { ext_ticks = 0; p_ext_ticks = 0; p_ext_start++; }   /* Start: the grid on the one (not the beat) */
                continue;
            }
            if ((st & 0xF0u) == 0xB0u && d1 == 119u) { out_msg(0xB0, 119, load_v > 127u ? 127u : load_v); continue; }   /* CC 119: the load, asked */
            if ((st & 0xF0u) != 0x90u || !d2) continue;
            if ((st & 0x0Fu) == 9u) { if (d1 >= 35u && d1 < 60u && GM_DRUM[d1 - 35u] >= 0) dmask |= 1u << GM_DRUM[d1 - 35u]; }
            else if (d1 >= 48u && d1 < 48u + NKEYW) deck_cmd(CMD_SAY, surf ? (uint32_t)SURF_KEY + d1 - 48u : d1 - 48u);
        }
        if (dmask) drums_now(dmask);            /* hits in one pass land together (kick + crash) */

        /* ---- a new voice: speak the bank again once the knob rests */
        if (voice_pending && now - t_voice > 700u) {
            voice_pending = 0;
            draw_big(VOICES[set.voice].name, C_PINK);
            render_bank(set.voice);
            wave_word = -2; word_drawn = -2; grid_drawn = ~0u; knobs = chips = 1; deck_view = ~0u; top_key = ~0u;
            deck_cmd(CMD_SAY, NAME_WORD);
        }

        /* ---- the deck out to USB-MIDI for visuals, on change, ~66 Hz: CC 20 scratching, 21 fader, 22 platter angle */
        if (now - t_cc >= 15u) {
            const uint32_t on = dk_on_v ? 127u : 0u, fd = dk_fader_v >> 8, an = dk_rot_v >> 25;
            t_cc = now;
            if (on != cc_on) { cc_on = on; out_msg(0xB0, 20, on); }
            if (fd != cc_fd) { cc_fd = fd; out_msg(0xB0, 21, fd); }
            if (an != cc_an) { cc_an = an; out_msg(0xB0, 22, an); }
        }

        /* ---- LEDs */
        {
            uint32_t h = drum_hit_v, lit;
            const uint32_t fs = fx_state_v;
            drum_hit_v = 0;
            for (k = 0; k < NDRUM; k++) if (h >> k & 1u) { out_msg(0x99, DRUM_GM[k], 110); out_msg(0x89, DRUM_GM[k], 0); }
            for (k = 0; k < 27u; k++) {
                lit = keys >> k & 1u;
                if (surf) lit |= dk_on_v && dk_word_v == SURF_WORD && surf_abs_v >> ZSH == k;   /* the spot the needle is on */
                else if (dk_on_v && held_word[k] < 0) lit |= (int32_t)k == dk_word_v;
                fm1_led_key(KEY_LED(k), lit);
            }
            fm1_led_key(BTN_PLAY, play && (seq_step_v & 3u) < 2u);
            fm1_led_key(BTN_REC, (fs & ST_SPIN) != 0u);
            fm1_led_key(BTN_SEQ, (fs & ST_BRAKE) != 0u);
            fm1_led_key(BTN_ARP, surf);
            fm1_led_key(BTN_GLO, set.aut);
            fm1_led_key(BTN_FX, fx_held || set.filt != 7u || set.echo || set.crush || set.stut);
            fm1_led_key(BTN_HOME, home);
            fm1_led_key(BTN_EDIT, shift);
            fm1_led_key(BTN_ENV, p_freeze);
            fm1_led_key(BTN_LFO, p_cut);
            fm1_led_key(BTN_SEL, set.hamster);
            fm1_led_key(BTN_SAVE, dirty);
            fm1_led_key(BTN_OCT_DOWN, set.semi < 0);
            fm1_led_key(BTN_OCT_UP, set.semi > 0);
        }

        /* ---- the screen */
        {
            const uint16_t acc = C_PINK;
            const uint32_t fs = fx_state_v;
            const char *now_s;
            uint16_t now_c = C_YEL;
            char bpm_s[16];
            uint32_t key;
            /* the top bar: the move happening now, else the record and the tempo */
            if (home) now_s = "SETTINGS";
            else if (shift) { char *lp = put(bpm_s, "LOAD "); lp = put_u(lp, load_v); put(lp, "%"); now_s = bpm_s; now_c = load_v > 85u ? C_RED : C_YEL; }
            else if (fx_held) now_s = "DRY";
            else if (fs & ST_SPIN) now_s = "BACKSPIN";
            else if (fs & ST_BRAKE) now_s = "BRAKE";
            else if (p_freeze) now_s = "FREEZE";
            else if (p_cut) now_s = "CUT";
            else if (fs & ST_MUTED) now_s = "...";
            else if (fs & ST_WAIT) now_s = "SYNC...";
            else {
                char *p = put(put(bpm_s, ext_live ? "EXT" : "BPM"), " ");
                put_u(p, ext_live ? ext_bpm : set.bpm);
                now_s = bpm_s; now_c = ext_live ? C_YEL : C_PINK;
            }
            key = (uint32_t)now_s[0] * 7u + (uint32_t)now_s[1] + (uint32_t)now_s[2] * 31u + (shift ? load_v * 977u : 0u) + (ext_live ? ext_bpm : set.bpm) * 3u + ext_live * 7777u;
            if (key != top_key) { top_key = key; draw_top(now_s, now_c); }

            /* the big line: a toast for a moment, else the word or the zone */
            if (toast_on && now - t_toast < 1100u) {
                if (word_drawn != -3 || toast_on == 1) { draw_big(toast[0] == ' ' ? toast + 1 : toast, C_YEL); word_drawn = -3; toast_on = 2; }
            } else {
                toast_on = 0;
                if (dk_word_v == SURF_WORD) {           /* the record: what this zone is made of, and which */
                    const uint32_t z = surf_abs_v >> ZSH;
                    zone_t zp;
                    zone_of(p_world, z, &zp);
                    if (word_drawn != (int32_t)(1000u + (z & 0xFFFFFu) * 16u + zp.mat)) {
                        char b2[20], *p2 = put(b2, ZNAME[zp.mat]);
                        word_drawn = (int32_t)(1000u + (z & 0xFFFFFu) * 16u + zp.mat);
                        p2 = put(p2, " ");
                        put_u(p2, z);
                        draw_big(b2, ZCOL[zp.mat]);
                    }
                } else if (dk_word_v != word_drawn) { word_drawn = dk_word_v; draw_big(word_drawn >= 0 ? SHOW[word_drawn] : "", acc); }
            }

            /* the middle: the deck, the landscape, a technique just picked, or the button map (EDIT held) */
            if (shift) { if (deck_view != 2u) { deck_view = 2u; draw_cheat(); } }
            else if (now - t_score < 1500u && set.style < NTECH && !dk_scr_v) {
                if (deck_view != 4u + set.style) { deck_view = 4u + set.style; draw_score_preview(set.style, acc); }
            }
            else if (now - t_draw >= 33u || deck_view == ~0u || deck_view >= 3u) {
                if (!dk_scr_v && (dk_word_v == SURF_WORD || (surf && dk_word_v < 0))) {   /* the landscape */
                    draw_surf(p_world, dk_word_v == SURF_WORD ? surf_abs_v : 2u * ZLEN, dk_rot_v, dk_on_v, dk_fader_v, acc);
                    deck_view = 0u; t_draw = now;
                }
                else if (dk_on_v || play || jog_held || deck_view != 0u) {
                    draw_deck(dk_word_v, dk_pos_v, dk_rot_v, dk_on_v, dk_fader_v, acc);
                    deck_view = 0u; t_draw = now;
                }
            }

            {
                const uint32_t run = play || ext_live, gk = (seq_step_v & 15u) + run * 16u + ext_live * 32u + play * 64u;
                if (gk != grid_drawn) { grid_drawn = gk; draw_grid(seq_step_v & 15u, run, ext_live, play); }
            }

            if (knobs) {                        /* the encoders' strip: what each one does now */
                cell_t a[4], c[3];
                knobs = 0;
                if (home) {
                    a[0].label = "VOICE"; put(a[0].val, VOICES[set.voice].name); a[0].bar = 0; a[0].of = 0;
                    a[1].label = "FLOW"; put_u(a[1].val, set.flow); a[1].bar = set.flow; a[1].of = 15;
                    a[2].label = "VERB"; put_u(a[2].val, set.space); a[2].bar = set.space; a[2].of = 15;
                    a[3].label = "BEAT"; put_u(a[3].val, set.blvl); a[3].bar = set.blvl; a[3].of = 15;
                    c[0].label = "BPM"; put_u(c[0].val, set.bpm); c[0].bar = set.bpm - 60u; c[0].of = 120;
                    c[1].label = "REACH"; put_u(c[1].val, set.reach + 1u); c[1].bar = set.reach; c[1].of = 7;
                    c[2].label = "TORQUE"; put(c[2].val, TORQUE_NAME[set.torque]); c[2].bar = set.torque; c[2].of = 3;
                } else {
                    const uint32_t dry = fx_held;
                    a[0].label = "FILTER"; filt_name(a[0].val, dry ? 7u : set.filt); a[0].bar = dry ? 7u : set.filt; a[0].of = 15;
                    a[1].label = "ECHO"; put_u(a[1].val, dry ? 0u : set.echo); a[1].bar = dry ? 0u : set.echo; a[1].of = 15;
                    a[2].label = "CRUSH"; put_u(a[2].val, dry ? 0u : set.crush); a[2].bar = dry ? 0u : set.crush; a[2].of = 15;
                    a[3].label = "STUTTER"; put(a[3].val, stut_name(dry ? 0u : set.stut)); a[3].bar = dry ? 0u : set.stut; a[3].of = 15;
                    c[0].label = "JOG"; put(c[0].val, "SCRATCH"); c[0].of = 0;
                    c[1].label = "STYLE"; put(c[1].val, set.style == SC_HYDRO ? "HYDRO" : SC_NAME[set.style]); c[1].of = 0;
                    c[2].label = "RECORD"; rec_name(c[2].val, set.rec); c[2].of = 0;
                }
                draw_knobs(a, c, home, acc);
            }
            if (chips) { chips = 0; draw_chips(set.aut, set.hamster, play, ext_live, set.sync, set.semi); }
        }

        if (dirty && !play && !dk_on_v && now - t_change > 2000u) {   /* saved when silent (an erase stalls the sound) */
            settings_save(&set, sizeof set);
            dirty = 0;
        }
    }
}
