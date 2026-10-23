/* SPDX-License-Identifier: GPL-3.0-only */
/* CYBER//SKRATCH FM-1: the robot DJ from the keypad synth (cyberskratch_3d), rebuilt for the M-VAVE FM-1 on Dinghy.
 * SAM (Software Automatic Mouth) speaks a cybernetic lexicon onto a turntable deck you scratch. A hard hip-hop drum
 * machine, a 16-step sequencer that records what you play, a robot that DJs by itself, an FX rack, and DJ moves on
 * every button. Integer-only rewrite: the chip has no FPU. (The 3D intro, VJ view, Talkie mouth, vocoder, grains and
 * reverb of the original stay on the keypad synth.)
 *
 *   KEYS       DJ mode: 27 words, tap = say it, HOLD = tempo-synced scratch in the STYLE while held
 *              BEAT mode (HOME): keys 0..11 = the kit (hold = roll), keys 12..26 = the first 15 words
 *
 *   button     tap / hold                              EDIT held + button
 *   PLAY       start / stop                            TAP tempo
 *   REC        record into the 16 steps (quantized)    clear the words, keep the drums
 *   SEQ        hold = FILL (snare roll, toms, slam)    clear the steps
 *   ARP        AUTO: the robot DJs                     MUTATE the steps
 *   GLO        HARD: drums driven, kick ducks the deck BRAKE
 *   FX         next FX                                 previous FX
 *   SEL        hold = DROP (only the hats; slam back)
 *   ENV        hold = FREEZE (loop a sliver)           latch FREEZE
 *   LFO        hold = CUT (crossfader on 32nds)        latch CUT
 *   HOME       DJ / BEAT                               pitch back to 0
 *   OCT- OCT+  the deck's pitch, a semitone            BACKSPIN / BRAKE (the turntable's power cut)
 *   SAVE       save everything now                     SYNC: AUTO / SEND / OFF
 *   EDIT       hold = SHIFT: the screen shows the second layer
 *
 *   encoder    turn                                    EDIT held + turn
 *   SELECT     JOG: scratch by hand                    SWING
 *   ALGORITHM  STYLE: BABY CHIRP TRANSFORM TEAR FLARE CRAB   REACH (how far the scratch goes)
 *   KNOB1      the FX's amount                         KIT TUNE (-6..+6)
 *   KNOB2      ECHO                                    ECHO TIME (1/16 1/8 3/16 1/4)
 *   KNOB3      BPM 60..180                             KIT DECAY
 *   KNOB4      SAM VOICE (renders the bank again)      DRUMS level
 *   PRESETS    GROOVE                                  DECK level
 *   FX: FILTER CRUSH RADIO RING CRUNCH STUTTER REVERSE.  GROOVES: OFF BOOM BAP BREAK ELECTRO HALFTIME RAP METAL TRAP.
 *   USB-MIDI   channel 10 notes 36..47 = the kit; other channels notes 48..74 = the 27 words
 *   MIDI SYNC  AUTO: an incoming clock (24 ppqn) leads, Start / Stop / Continue drive PLAY; without one the FM-1
 *              sends its clock and Start / Stop. SEND: always leads. OFF: no clock either way.
 *
 * SAM: C core from earlephilhower/ESP8266SAM (GPL v3). Built on Dinghy / Felucca / SLOOP (GPL v3). */
#define APP_NAME "CYBERSKRATCH"
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
#define NWORDS 24                               /* pad words */
#define NBANK 27                                /* + the name and its halves */
#define NAME_WORD 24
#define WFS 11025u                              /* the bank is kept at 11025 Hz: 8 bit, ~26 s */
#define WPOOL 286720u
static int8_t wpool[WPOOL] POOL;
static struct { uint32_t off, len; } bank[NBANK];
static volatile uint32_t bank_busy = 1;

static const char *const SAY[NBANK] = {         /* spelled for SAM */
    "AUTOMATON", "FEEDBACK", "ENTROPY", "SIGNAL", "NOISE", "HIGHER STATE",
    "HOMEOSTASIS", "INFORMATION", "MESSAGE", "LOOP", "OSCILLATE", "ZERO ONE",
    "MEMORY", "PATTERN", "SYNAPSE", "PARADOX", "ECHO", "NEXUS",
    "KALEIDOSCOPE", "WONDER", "ALWAYS MERRY AND BRIGHT", "STAND STILL LIKE THE HUMMINGBIRD", "INNER SPACE", "CRYSTALLINE",
    "SIGHBER SKRATCH", "SIGHBER", "SKRATCH"};
static const char *const SHOW[NBANK] = {        /* on screen */
    "AUTOMATON", "FEEDBACK", "ENTROPY", "SIGNAL", "NOISE", "HIGHER STATE",
    "HOMEOSTASIS", "INFORMATION", "MESSAGE", "LOOP", "OSCILLATE", "ZERO ONE",
    "MEMORY", "PATTERN", "SYNAPSE", "PARADOX", "ECHO", "NEXUS",
    "KALEIDOSCOPE", "WONDER", "MERRY + BRIGHT", "HUMMINGBIRD", "INNER SPACE", "CRYSTALLINE",
    "CYBER SKRATCH", "CYBER", "SKRATCH"};

#define NVOICES 6
static const struct { const char *name; uint8_t speed, pitch, throat, mouth; } VOICES[NVOICES] = {
    {"ROBOT", 92, 60, 190, 190}, {"SAM", 72, 64, 128, 128}, {"ELF", 72, 64, 110, 160},
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

static void render_bank(uint32_t vi)
{
    uint32_t i, k;
    bank_busy = 1;
    samdata = &sam_mem;
    sam_w = 0;
    for (k = 0; k < NBANK; k++) {
        char in[256];
        const char *s = SAY[k];
        uint32_t a, b, start = sam_w, n = 0;
        platform_poll();
        draw_progress(k, NBANK);
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
        if (!TextToPhonemes(in)) { sam_w = start; continue; }
        SetInput(in);
        SAMMain(sam_out, 0);
        a = start; b = sam_w;                              /* trim the silence at both ends */
        while (a < b && (wpool[a] > -3 && wpool[a] < 3)) a++;
        while (b > a && (wpool[b - 1] > -3 && wpool[b - 1] < 3)) b--;
        bank[k].off = a;
        bank[k].len = b > a + 8u ? b - a : 0;
    }
    draw_progress(NBANK, NBANK);
    bank_busy = 0;
}

/* =========================================================================== shared state (main writes, ISR reads) */
enum { FX_FILTER, FX_CRUSH, FX_RADIO, FX_RING, FX_CRUNCH, FX_STUTTER, FX_REVERSE, FX_COUNT };
static const char *const FX_NAME[FX_COUNT] = {"FILTER", "CRUSH", "RADIO", "RING", "CRUNCH", "STUTTER", "REVERSE"};
enum { SC_BABY, SC_CHIRP, SC_TRANS, SC_TEAR, SC_FLARE, SC_CRAB, SC_COUNT };
static const char *const SC_NAME[SC_COUNT] = {"BABY", "CHIRP", "TRANSFORM", "TEAR", "FLARE", "CRAB"};
static const char *const ETIME_NAME[4] = {"1/16", "1/8", "3/16", "1/4"};

static volatile uint32_t p_fx, p_amt = 7, p_echo = 4, p_etime = 2, p_style, p_reach = 3, p_play, p_auto, p_hard = 1;
static volatile uint32_t p_steplen = 7190, p_swing, p_tune = 6, p_decay = 2, p_dlvl = 12, p_vlvl = 12;
static volatile int32_t p_rate = 16384;                 /* the deck's speed, Q16 word samples per frame (1/4 = 11025 into 44100) */
static volatile uint32_t p_groove = 1, p_roll, p_restart;
static volatile uint32_t p_cut, p_freeze, p_drop, p_fill;   /* held performance moves */
static volatile uint32_t p_spin, p_brake;                  /* one-shots: counters, the ISR acts when they move */
static volatile uint32_t p_ext, p_ext_ticks, p_ext_start;  /* MIDI clock in: following, ticks since Start, Starts */
static volatile uint32_t p_send_clock;                     /* MIDI clock out: leading */
static volatile uint32_t mo_buf[32], mo_w, mo_r;           /* the main loop's MIDI out; the ISR sends it (one producer) */
static void out_msg(uint32_t st, uint32_t d1, uint32_t d2) { if (mo_w - mo_r < 32u) { mo_buf[mo_w % 32u] = st | d1 << 8 | d2 << 16; mo_w++; } }
enum { CMD_SAY, CMD_SCR, CMD_LET };
static volatile uint32_t cmd_n, cmd_kind, cmd_arg;      /* main: arg, kind, then n++ */
static volatile uint32_t drum_n, drum_mask;             /* main: live drum hits (bits), then n++ */
static volatile int32_t jog_total;                      /* main: detents turned, ever */
static volatile uint32_t jog_held;                      /* main: 1 while the jog is being turned */

/* ISR -> main */
static volatile int32_t dk_word_v = -1;
static volatile uint32_t dk_pos_v, dk_on_v, dk_rot_v, dk_fader_v, seq_step_v, seq_frac_v, drum_hit_v, fx_state_v;
enum { ST_SPIN = 1, ST_BRAKE = 2, ST_MUTED = 4, ST_WAIT = 8 };

/* the sequencer: 16 steps */
#define SEQ_SCR 1u
static volatile int8_t seq_word[16];
static volatile uint8_t seq_flag[16], seq_skip[16];
static volatile uint16_t seq_drum[16];

/* =========================================================================== drums */
enum { W_KICK, W_SNR, W_CLAP, W_HAT, W_CRASH, W_COW, W_ZAP };
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
    {"COWBL", W_COW,   0, 10, 0, 0,                13000, 0,     21},
    {"ZAP",   W_ZAP,   9, 10, 253218027u, 6817408u, 18000, 0,     0},
    {"CRASH", W_CRASH, 0, 13, 0, 0,                13000, 0,     0},
};
enum { D_KICK, D_SNARE, D_CLAP, D_HAT, D_OPEN, D_RIM, D_TOML, D_TOMH, D_808, D_COWBL, D_ZAP, D_CRASH };
#define HATS ((1u << D_HAT) | (1u << D_OPEN))
static const uint32_t HAT_INC[6] = {31991176u, 47433580u, 57593466u, 81450500u, 84146298u, 124661182u};
static const uint32_t CRASH_INC[6] = {19994485u, 29645987u, 35995916u, 50906562u, 52591436u, 77913239u};

typedef struct { const char *name; const char *lane[4]; uint8_t drum[4]; } groove_t;
#define NGROOVES 7
static const groove_t GROOVES[NGROOVES] = {
    {"OFF",       {"................", "................", "................", "................"}, {D_KICK, D_SNARE, D_HAT, D_OPEN}},
    {"BOOM BAP",  {"x.......x.x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "..............x."}, {D_KICK, D_SNARE, D_HAT, D_OPEN}},
    {"BREAK",     {"x.x.......xx....", "....x..x.x..x..x", "x.x.x.x.x.x.x.x.", "..............x."}, {D_KICK, D_SNARE, D_HAT, D_OPEN}},
    {"ELECTRO",   {"x..x..x...x.....", "....x.......x...", "x.xxx.xxx.xxx.xx", "x.........x....."}, {D_KICK, D_CLAP, D_HAT, D_808}},
    {"HALFTIME",  {"x.........x.....", "........x.......", "x.x.x.x.x.x.x.x.", "..............x."}, {D_KICK, D_SNARE, D_HAT, D_OPEN}},
    {"RAP METAL", {"x.x...x.x.x...x.", "....x.......x...", "x...x...x...x...", "x..............."}, {D_KICK, D_SNARE, D_OPEN, D_CRASH}},
    {"TRAP",      {"x......x..x.....", "........x.......", "x.x.xxx.x.x.xxxx", "x......x..x....."}, {D_KICK, D_CLAP, D_HAT, D_808}},
};
static uint16_t groove_mask[NGROOVES][16];

/* =========================================================================== the audio ISR */
enum { DK_OFF, DK_PLAY, DK_SCR, DK_HAND, DK_LET, DK_SPIN, DK_FRZ };
static struct {
    int32_t word, pos, rate, hand, frz;         /* pos, hand, frz: Q16 word samples */
    uint32_t mode, ph, once, stut, stut_n;
    int32_t fader;
} dk = {-1, 0, 0, 0, 0, DK_OFF, 0, 0, 0, 0, 0};
static struct { int32_t env, nenv; uint32_t ph, inc, inc1, step, t; int32_t z1, z2; } dr[NDRUM];
static int16_t echo_buf[12288] POOL;            /* at 22050 Hz: 0.55 s */
static uint32_t nrng = 0x13579BDFu;
static uint32_t noise(void) { nrng = nrng * 1664525u + 1013904223u; return nrng; }
static const int32_t PITCH[25] = {16384, 17358, 18390, 19484, 20643, 21870, 23170, 24548, 26008, 27554, 29193, 30929,
    32768, 34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858, 65536};
static uint32_t mulq15(uint32_t x, uint32_t q) { return (x >> 15) * q + (((x & 0x7FFFu) * q) >> 15); }   /* x * q / 32768, no 64 bit */

#define FRZ_WIN (1400 << 16)                    /* the freeze loop: 127 ms of the word */

static void deck_start(int32_t w, uint32_t kind, uint32_t once)
{
    if (w < 0 || w >= NBANK || !bank[w].len || bank_busy) return;
    dk.word = w;
    dk.stut = 0; dk.stut_n = 0;
    if (kind == CMD_SCR) {
        dk.mode = DK_SCR; dk.ph = 0; dk.once = once; dk.pos = 0;
    } else {
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
    dr[d].ph = 0;
    dr[d].t = 0;
    if (d == D_HAT) dr[D_OPEN].env = 0;         /* the closed hat chokes the open one */
    drum_hit_v |= 1u << d;
}

static uint32_t bump(uint32_t ph16)             /* 0 .. 32767 .. 0 over a cycle (1 - cos) / 2 */
{
    return (uint32_t)((32767 - sine((ph16 << 16) + 0x40000000u)) >> 1);
}

static void scratch_shape(uint32_t f, int32_t *shape, int32_t *gate)   /* f = 0..65535 over one beat */
{
    int32_t s = (int32_t)bump(f), g = 1;
    switch (p_style) {
    case SC_CHIRP: g = f < 26000u; break;                               /* the fader shuts at the top */
    case SC_TRANS: g = !((f >> 12) & 1u); break;                        /* 8 cuts a beat */
    case SC_TEAR:                                                      /* out in one, back in two */
        s = f < 32768u ? (int32_t)f : f < 45875u ? 32767 - (int32_t)((f - 32768u) * 16384u / 13107u)
          : f < 52428u ? 16384 : (int32_t)((65535u - f) * 16384u / 13107u);
        break;
    case SC_FLARE: g = !((f >= 13107u && f < 19660u) || (f >= 45875u && f < 52428u)); break;
    case SC_CRAB:  g = !((f >> 11) & 1u); break;                       /* four fingers on the fader */
    default: break;
    }
    *shape = s;
    *gate = g;
}

static void app_audio(int32_t *out, uint32_t n)
{
    static uint32_t G, gacc, stepn, restarted, cmd_seen, drum_seen, crush_h, ring_ph, ew, rot, half_done;
    static uint32_t spin_seen, brake_seen, braking, brake_g0, muted, slam, was_held, fill_n, frz_was;
    static uint32_t ext_seen, ext_start_seen, ext_wait, tick_acc, play_was;
    static int32_t jog_seen, f1, r1, r2, crush_v, cz, duck, echo_y;
    int32_t dbuf[64], vbuf[64];
    uint32_t off;
    const uint32_t fx = p_fx, amt = p_amt, play = p_play, hard = p_hard, steplen = p_steplen;
    const uint32_t freeze = p_freeze, drop = p_drop, fill = p_fill, held = drop | fill;
    const int32_t rate_base = p_rate;
    int32_t rate0 = rate_base;
    const int32_t dl_gain = (int32_t)p_dlvl * 2731, vl_gain = (int32_t)p_vlvl * 2731;   /* 12 = unity */

    /* the clock: G = where the song is, in 16ths (Q16). Steps fire as G passes them (odd ones later with swing). */
    if (p_restart != restarted) { restarted = p_restart; G = 0; gacc = 0; stepn = 0; braking = 0; muted = 0; tick_acc = 65536u; }
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
    while (mo_r != mo_w) {                      /* MIDI out: the notes from the main loop */
        const uint32_t q = mo_buf[mo_r % 32u];
        midi_send(q & 0xFFu, q >> 8 & 0x7Fu, q >> 16 & 0x7Fu);
        mo_r++;
    }
    if (p_send_clock && play != play_was) midi_send(play ? 0xFAu : 0xFCu, 0, 0);   /* leading: Start / Stop */
    play_was = play;
    if (cmd_n != cmd_seen) {
        cmd_seen = cmd_n;
        if (cmd_kind == CMD_LET) { if (dk.mode == DK_SCR) dk.mode = DK_LET; }
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
            dk.hand += d * (300 << 16);
            if (dk.hand < 0) dk.hand = 0;
            if (dk.hand > len) dk.hand = len;
        }
    } else if (dk.mode == DK_HAND && !jog_held)
        dk.mode = DK_PLAY;                      /* let go: the record plays on */

    for (off = 0; off < n; off += 64u) {
        uint32_t m = n - off < 64u ? n - off : 64u, i, d;
        int32_t gate = 1, ft;

        /* ---- the clock, once per block */
        rate0 = rate_base;
        {
            uint32_t ginc, due = 0;
            gacc += m << 16;
            ginc = gacc / steplen;
            gacc -= ginc * steplen;
            if (braking) {                                      /* BRAKE: slower and slower over 9 steps */
                const uint32_t gone = (G - brake_g0) >> 8;
                braking = gone >= 9u * 240u ? 0u : 256u - gone / 9u;
                if (!braking) muted = 1;
                ginc = ginc * braking >> 8;
                rate0 = rate0 * (int32_t)braking >> 8;
            }
            if (ext_wait) ginc = 0;
            G += ginc;
            dk.ph += ginc >> 2;                                 /* scratches move in beats */
            if (p_send_clock) {                                 /* leading: 24 ticks a beat = 6 a step */
                tick_acc += ginc * 6u;
                while (tick_acc >= 65536u) { tick_acc -= 65536u; midi_send(0xF8u, 0, 0); }
            }
            while (due < 2u) {
                const uint32_t s = stepn & 15u, at = (stepn << 16) + (s & 1u ? p_swing * 65536u / 40u : 0u);
                uint32_t g;
                if ((int32_t)(G - at) < 0) break;
                due++;
                g = groove_mask[p_groove % NGROOVES][s] | seq_drum[s];
                half_done = 0;
                if (muted && s == 0u) { muted = 0; slam = 1; }
                if (play && !muted) {
                    if (drop) g &= HATS;                            /* DROP: only the hats stay */
                    if (fill) {                                     /* FILL: a snare roll, toms at the end */
                        g = (g & HATS) | 1u << D_SNARE;
                        if (fill_n >= 6u && fill_n & 1u) g |= 1u << (fill_n & 2u ? D_TOMH : D_TOML);
                        fill_n++;
                    } else fill_n = 0;
                    if (slam && !held) { g |= 1u << D_CRASH | 1u << D_KICK; slam = 0; }
                    for (d = 0; d < NDRUM; d++) if (g >> d & 1u) drum_hit(d);
                    if (seq_word[s] >= 0 && !seq_skip[s] && dk.mode != DK_FRZ && dk.mode != DK_SPIN)
                        deck_start(seq_word[s], seq_flag[s] & SEQ_SCR ? CMD_SCR : CMD_SAY, 1);
                    if (p_auto && dk.mode != DK_FRZ) {              /* the robot DJ */
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

        /* ---- the deck: where the record goes this block */
        if (dk.word >= 0 && !bank_busy) {
            int32_t len = (int32_t)bank[dk.word].len << 16;
            switch (dk.mode) {
            case DK_PLAY:
                dk.rate = fx == FX_REVERSE ? -rate0 : rate0;
                if (dk.pos >= len || dk.pos < 0) dk.mode = DK_OFF;
                break;
            case DK_FRZ: dk.rate = rate0; break;
            case DK_SCR: {
                int32_t shape, target;
                if (dk.ph >= 65536u) { dk.ph &= 65535u; if (dk.once) { dk.mode = DK_LET; break; } }
                scratch_shape(dk.ph, &shape, &gate);
                target = (shape * (int32_t)(2600u + p_reach * 800u)) << 1;   /* Q16 */
                if (target > len) target = len;
                dk.rate = (target - dk.pos) / (int32_t)m;
                break;
            }
            case DK_HAND:
                dk.rate = ((dk.hand - dk.pos) >> 2) / (int32_t)m;
                if (dk.rate > 8 * rate0) dk.rate = 8 * rate0;
                if (dk.rate < -8 * rate0) dk.rate = -8 * rate0;
                break;
            case DK_SPIN:                                       /* the throw slows down */
                dk.rate -= dk.rate >> 7;
                if (dk.pos <= 0 || dk.rate > -(rate0 >> 3)) { dk.mode = DK_LET; gate = 0; }
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
            const int32_t len = (int32_t)bank[dk.word].len - 1, frz = dk.mode == DK_FRZ;
            int32_t pos = dk.pos, rate = dk.rate, fd = dk.fader;
            for (i = 0; i < m; i++) {
                int32_t ip, s = 0;
                if (frz && pos >= dk.frz + FRZ_WIN) pos -= FRZ_WIN;
                ip = pos >> 16;
                fd += (ft - fd) >> 5;
                if (ip >= 0 && ip < len) {
                    int32_t a = w[ip], b = w[ip + 1];
                    s = (a << 8) + (((b - a) * (int32_t)((uint32_t)(pos & 0xFFFF) >> 1)) >> 7);
                }
                if (frz) {                                      /* soft edges on the loop */
                    int32_t e1 = (pos - dk.frz) >> 16, e2 = (dk.frz + FRZ_WIN - pos) >> 16, e = e1 < e2 ? e1 : e2;
                    if (e < 160) s = (s * (e < 0 ? 0 : e)) / 160;
                }
                vbuf[i] = (s * fd) >> 15;
                pos += rate;
            }
            dk.pos = pos;
            dk.fader = fd;
        } else { dk.fader = 0; dk.pos += dk.rate * (int32_t)m; }   /* shut, the record still moves */
        rot += (uint32_t)(dk.mode != DK_OFF ? dk.rate : play ? rate0 : 0) * 3u * m;

        /* ---- the drums */
        for (i = 0; i < m; i++) dbuf[i] = 0;
        for (d = 0; d < NDRUM; d++) {
            const perc_t *p = &PERC[d];
            int32_t env = dr[d].env, nenv = dr[d].nenv, z1 = dr[d].z1, z2 = dr[d].z2, dsh;
            uint32_t ph = dr[d].ph, inc = dr[d].inc, t = dr[d].t;
            const uint32_t inc1 = dr[d].inc1, step = dr[d].step;
            if (env < 512) continue;
            dsh = (int32_t)p->dsh + (int32_t)p_decay - 2;
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
                case W_COW: {
                    uint32_t tt;
                    ph += step;
                    tt = (ph >> 8) & 0xFFFFFu;
                    s = ((int32_t)((52591436u * tt) >> 31) ? 9000 : -9000) + ((int32_t)((77913239u * tt) >> 31) ? 9000 : -9000);
                    z1 += ((s - z1) * 9000) >> 15;
                    s = z1 * 2;
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
            }
            dr[d].env = env; dr[d].nenv = nenv; dr[d].ph = ph; dr[d].inc = inc; dr[d].t = t; dr[d].z1 = z1; dr[d].z2 = z2;
        }
        if (dr[D_KICK].env > (30000 << 8) || dr[D_808].env > (30000 << 8)) duck = 32767;

        /* ---- FX on the deck, the echo, the mix */
        for (i = 0; i < m; i++) {
            int32_t x = vbuf[i], o, dd;
            switch (fx) {
            case FX_FILTER:                                      /* 0..6 low-pass, 7..8 open, 9..15 high-pass */
                if (amt < 7u) { f1 += ((x - f1) * (int32_t)(1500u + amt * 4200u)) >> 15; x = f1; }
                else if (amt > 8u) { f1 += ((x - f1) * (int32_t)((amt - 8u) * 1800u)) >> 15; x -= f1; }
                break;
            case FX_CRUSH: {
                const uint32_t keep = 12u - (amt * 9u >> 4), hold = 1u + (amt >> 2);
                if (++crush_h >= hold) { crush_h = 0; crush_v = x & ~(int32_t)((1u << (16u - keep)) - 1u); }
                x = crush_v;
                break;
            }
            case FX_RADIO:
                r1 += ((x - r1) * 2270) >> 15; x -= r1;
                r2 += ((x - r2) * 10900) >> 15;
                x = soft((r2 * (int32_t)(4u + amt)) >> 1) + ((int32_t)noise() >> 22);
                break;
            case FX_RING: {
                static const uint32_t RING[16] = {3895662u, 5338424u, 7315515u, 10024823u, 13737526u, 18825232u, 25797176u,
                    35351185u, 48443531u, 66384641u, 90970258u, 124661182u, 170829574u, 234096475u, 320794335u, 439600833u};
                ring_ph += RING[amt & 15u];
                x = (x * sine(ring_ph)) >> 15;
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
            }

            dd = (dbuf[i] * dl_gain) >> 15;
            if (hard) dd = soft(dd + (dd >> 1));                  /* HARD: the kit into the red */
            o = x + ((echo_y * 3) >> 2) + dd;
            o = (o * 20000) >> 15;
            out[2u * (off + i)] += o;
            out[2u * (off + i) + 1u] += o;
        }
    }
    dk_pos_v = (uint32_t)dk.pos;
    dk_on_v = dk.mode != DK_OFF;
    dk_fader_v = (uint32_t)dk.fader;
    dk_rot_v = rot;
    fx_state_v = (dk.mode == DK_SPIN ? ST_SPIN : 0u) | (braking ? ST_BRAKE : 0u) | (muted ? ST_MUTED : 0u) | (ext_wait ? ST_WAIT : 0u);
}

/* =========================================================================== the screen */
#define C_PINK RGB(255, 40, 160)
#define C_CYAN RGB(0, 230, 255)
#define C_GRY  RGB(90, 90, 110)
#define C_TXT  RGB(220, 220, 230)
#define C_DEEP RGB(36, 18, 56)
#define C_MID  RGB(80, 56, 120)
#define C_YEL  RGB(255, 220, 0)
#define C_RED  RGB(255, 50, 50)
static const uint16_t FX_COL[FX_COUNT] = {RGB(0, 230, 255), RGB(255, 140, 0), RGB(140, 255, 90), RGB(190, 120, 255),
                                          RGB(255, 60, 60), RGB(255, 220, 0), RGB(255, 40, 160)};

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
    cv_text(4, 2, &FONT_S, "CYBER//SKRATCH", C_PINK);
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

static void draw_deck(int32_t w, uint32_t pos, uint32_t rot, uint32_t on, uint32_t fader, uint16_t acc)
{
    uint32_t k, len = w >= 0 ? bank[w].len : 0, head;
    cv_begin(240, 64, C_BLACK);
    platter(34, 32, rot, on, fader, acc);
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
    static const char *const PAD[NDRUM] = {"KICK", "SNARE", "CLAP", "HAT", "OPEN", "RIM", "LTOM", "HTOM", "808", "COW", "ZAP", "CRASH"};
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

static void draw_cheat(void)                    /* EDIT held: what the buttons do now */
{
    static const char *const L[8] = {"OCT- BACKSPIN", "OCT+ BRAKE", "PLAY TAP BPM", "ARP  MUTATE",
                                     "SEQ  CLEAR", "REC  CLR WORDS", "ENV/LFO LATCH", "SAVE SYNC"};
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
static void draw_chips(uint32_t play, uint32_t rec, uint32_t aut, uint32_t hard, uint32_t beat, int32_t semi)
{
    char p[6] = "P", *q = p + 1;
    int32_t x = 2;
    put_s(q, semi);
    cv_begin(240, 24, C_BLACK);
    x = chip(x, "PLAY", play, C_PINK);
    x = chip(x, "REC", rec, C_RED);
    x = chip(x, "AUTO", aut, C_CYAN);
    x = chip(x, "HARD", hard, C_YEL);
    x = chip(x, beat ? "BEAT" : "DJ", 1, C_MID);
    chip(x, p, semi != 0, C_TXT);
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

static void app_main(void)
{
    struct {                                    /* what survives a power-off */
        uint8_t fx, amt[FX_COUNT], echo, etime, style, reach, voice, groove, hard, beat, aut, swing, tune, decay, dlvl, vlvl, sync;
        int8_t semi, pad;
        uint16_t bpm;
        int8_t word[16];
        uint8_t flag[16];
        uint16_t drum[16];
    } set;
    uint32_t prev = 0, k, s, t_change = 0, dirty = 0, play = 0, rec = 0, shift = 0, shift_was = 0;
    uint32_t frz_latch = 0, cut_latch = 0, knobs = 1, chips = 1, top_key = ~0u, t_draw = 0, steps_drawn = ~0u;
    uint32_t t_jog = 0, t_voice = 0, voice_pending = 0, t_toast = 0, toast_on = 0, deck_view = ~0u;
    uint32_t t_down[27] = {0}, scr[27] = {0}, qstep[27] = {0}, hit_t[NDRUM] = {0}, taps[4] = {0}, ntap = 0;
    int32_t word_drawn = -2, held_word[27];
    char toast[24];
    uint32_t ext_us[49], ext_n = 0, ext_ticks = 0, t_tick = 0, ext_live = 0, ext_bpm = 0, ext_len = 0;

    for (k = 0; k < sizeof set; k++) ((uint8_t *)&set)[k] = 0;
    for (k = 0; k < FX_COUNT; k++) set.amt[k] = 7;
    set.echo = 4; set.etime = 2; set.reach = 3; set.groove = 1; set.hard = 1; set.bpm = 92;
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
    set.sync %= 3u;
    set.semi = (int8_t)clampi(set.semi, -12, 12);
    set.bpm = (uint16_t)clampi(set.bpm, 60, 180);
    for (s = 0; s < 16u; s++) {
        if (set.word[s] >= NBANK) set.word[s] = -1;
        seq_word[s] = set.word[s]; seq_flag[s] = set.flag[s]; seq_drum[s] = set.drum[s] & 0x0FFFu;
    }
    for (k = 0; k < 27u; k++) held_word[k] = -1;
    for (k = 0; k < NGROOVES; k++)              /* the grooves' lanes into drum masks */
        for (s = 0; s < 16u; s++) {
            uint32_t l, m = 0;
            for (l = 0; l < 4u; l++) if (GROOVES[k].lane[l][s] == 'x') m |= 1u << GROOVES[k].drum[l];
            groove_mask[k][s] = (uint16_t)m;
        }
    for (k = 0; k < 48u; k++) {
        ring_x[k] = (int8_t)((sine(k * 89478485u) * 29) >> 15);
        ring_y[k] = (int8_t)((sine(k * 89478485u + 0x40000000u) * 29) >> 15);
    }
    lcg ^= fm1_ms * 2654435761u;

    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 60, 240, &FONT_L, "CYBER//", C_PINK, 1);
    draw_text_box(0, 96, 240, &FONT_L, "SKRATCH", C_CYAN, 1);
    draw_text_box(0, 180, 240, &FONT_S, "teaching the mouth to speak", C_GRY, 1);
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
            if ((e = fm1_enc_take(ENC_SELECT)) != 0) {
                if (shift) { set.swing = (uint8_t)clampi(set.swing + e, 0, 10); put_u(v, set.swing * 10u); TOAST("SWING", v); knobs = dirty = 1; }
                else { jog_total += e; jog_held = 1; t_jog = now; }
            } else if (jog_held && now - t_jog > 250u) jog_held = 0;
            if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {
                if (shift) { set.reach = (uint8_t)clampi(set.reach + e, 0, 7); put_u(v, set.reach + 1u); TOAST("REACH", v); }
                else { set.style = (uint8_t)clampi(set.style + e, 0, SC_COUNT - 1); TOAST("", SC_NAME[set.style]); }
                knobs = dirty = 1;
            }
            if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {
                if (shift) { set.tune = (uint8_t)clampi(set.tune + e, 0, 12); put_s(v, (int32_t)set.tune - 6); TOAST("TUNE", v); }
                else { set.amt[set.fx] = (uint8_t)clampi(set.amt[set.fx] + e, 0, 15); put_u(v, set.amt[set.fx]); TOAST(FX_NAME[set.fx], v); }
                knobs = dirty = 1;
            }
            if ((e = fm1_enc_take(ENC_KNOB2)) != 0) {
                if (shift) { set.etime = (uint8_t)clampi(set.etime + e, 0, 3); TOAST("ECHO", ETIME_NAME[set.etime]); }
                else { set.echo = (uint8_t)clampi(set.echo + e, 0, 15); put_u(v, set.echo); TOAST("ECHO", v); }
                knobs = dirty = 1;
            }
            if ((e = fm1_enc_take(ENC_KNOB3)) != 0) {
                if (shift) { set.decay = (uint8_t)clampi(set.decay + e, 0, 4); put_s(v, (int32_t)set.decay - 2); TOAST("DECAY", v); }
                else { set.bpm = (uint16_t)clampi(set.bpm + e, 60, 180); put_u(v, set.bpm); TOAST("BPM", v); }
                knobs = dirty = 1;
            }
            if ((e = fm1_enc_take(ENC_KNOB4)) != 0) {
                if (shift) { set.dlvl = (uint8_t)clampi(set.dlvl + e, 0, 15); put_u(v, set.dlvl); TOAST("DRUMS", v); }
                else { set.voice = (uint8_t)clampi(set.voice + e, 0, NVOICES - 1); voice_pending = 1; t_voice = now; TOAST("", VOICES[set.voice].name); }
                knobs = dirty = 1;
            }
            if ((e = fm1_enc_take(ENC_PRESETS)) != 0) {
                if (shift) { set.vlvl = (uint8_t)clampi(set.vlvl + e, 0, 15); put_u(v, set.vlvl); TOAST("DECK", v); }
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
            if (b & (1u << BTN_LFO)) { cut_latch = !cut_latch; TOAST("CUT", cut_latch ? "LATCH" : "OFF"); }
            if (b & (1u << BTN_HOME)) { set.semi = 0; chips = dirty = 1; TOAST("PITCH", "0"); }
            if (b & (1u << BTN_FX)) { set.fx = (uint8_t)((set.fx + FX_COUNT - 1u) % FX_COUNT); knobs = dirty = 1; TOAST("", FX_NAME[set.fx]); }
            if (b & (1u << BTN_GLO)) { p_brake++; TOAST("", "BRAKE"); }
        } else {
            if (b & (1u << BTN_PLAY)) { play = !play; if (play) p_restart++; chips = 1; }
            if (b & (1u << BTN_REC)) { rec = !rec; chips = 1; }
            if (b & (1u << BTN_ARP)) { set.aut = !set.aut; chips = dirty = 1; TOAST("AUTO", set.aut ? "ON" : "OFF"); }
            if (b & (1u << BTN_GLO)) { set.hard = !set.hard; chips = dirty = 1; TOAST("HARD", set.hard ? "ON" : "OFF"); }
            if (b & (1u << BTN_FX)) { set.fx = (uint8_t)((set.fx + 1u) % FX_COUNT); knobs = dirty = 1; TOAST("", FX_NAME[set.fx]); }
            if (b & (1u << BTN_HOME)) { set.beat = !set.beat; chips = dirty = 1; deck_view = ~0u; }
            if (b & (1u << BTN_OCT_DOWN) && set.semi > -12) { set.semi--; chips = dirty = 1; }
            if (b & (1u << BTN_OCT_UP) && set.semi < 12) { set.semi++; chips = dirty = 1; }
        }
        if (shift && b & (1u << BTN_SAVE)) {           /* SYNC: AUTO (follow MIDI clock, else send it) / SEND / OFF */
            static const char *const SY[3] = {"AUTO", "SEND", "OFF"};
            set.sync = (uint8_t)((set.sync + 1u) % 3u);
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
        p_fx = set.fx; p_amt = set.amt[set.fx]; p_echo = set.echo; p_etime = set.etime; p_style = set.style; p_reach = set.reach;
        p_groove = set.groove; p_hard = set.hard; p_auto = set.aut; p_play = play; p_swing = set.swing;
        p_tune = set.tune; p_decay = set.decay; p_dlvl = set.dlvl; p_vlvl = set.vlvl;
        p_cut = (btn >> BTN_LFO & 1u && !shift) || cut_latch;
        p_freeze = (btn >> BTN_ENV & 1u && !shift) || frz_latch;
        p_drop = btn >> BTN_SEL & 1u;
        p_fill = btn >> BTN_SEQ & 1u && !shift;
        if (ext_n && now - t_tick > 400u) ext_n = 0;    /* the clock stopped coming */
        ext_live = set.sync == 0u && ext_n >= 3u && ext_len;
        p_ext = ext_live;
        p_send_clock = set.sync != 2u && !ext_live;
        p_steplen = ext_live ? ext_len : 661500u / set.bpm;
        p_rate = (int32_t)((16384u * (uint32_t)PITCH[set.semi + 12]) >> 15);

        /* ---- the keys */
        keys = fm1_in.notes;
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        {
            uint32_t cur = seq_step_v & 15u, late = seq_frac_v >= 128u, q = late ? (cur + 1u) & 15u : cur, roll = 0;
            for (k = 0; k < 27u; k++) {
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
                        held_word[k] = w;
                        deck_cmd(CMD_SAY, (uint32_t)w);
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
                    if (scr[k]) deck_cmd(CMD_LET, 0);
                    if (held_word[k] >= 0) out_msg(0x80, 48u + (uint32_t)held_word[k], 0);
                    held_word[k] = -1;
                    scr[k] = 0;
                }
            }
            p_roll = roll;
        }
        while (midi_get(&st, &d1, &d2)) {       /* USB-MIDI in */
            if (st >= 0xF8u) {                  /* clock and transport */
                if (set.sync != 0u) continue;   /* only AUTO follows */
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
            if ((st & 0xF0u) != 0x90u || !d2) continue;
            if ((st & 0x0Fu) == 9u) { if (d1 >= 36u && d1 < 48u) drums_now(1u << (d1 - 36u)); }
            else if (d1 >= 48u && d1 < 48u + NBANK) deck_cmd(CMD_SAY, d1 - 48u);
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
                if (set.beat && k < 12u) lit |= now - hit_t[k] < 80u;
                else if (dk_on_v && held_word[k] < 0) lit |= (set.beat ? (int32_t)k - 12 : (int32_t)k) == dk_word_v;
                fm1_led_key(KEY_LED(k), lit);
            }
        }
        fm1_led_key(BTN_PLAY, play && (seq_step_v & 3u) < 2u);
        fm1_led_key(BTN_REC, rec);
        fm1_led_key(BTN_ARP, set.aut);
        fm1_led_key(BTN_GLO, set.hard);
        fm1_led_key(BTN_FX, set.fx != FX_FILTER || set.amt[FX_FILTER] != 7u);
        fm1_led_key(BTN_HOME, set.beat);
        fm1_led_key(BTN_EDIT, shift);
        fm1_led_key(BTN_ENV, p_freeze);
        fm1_led_key(BTN_LFO, p_cut);
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
            if (shift) now_s = "SHIFT";
            else if (fs & ST_SPIN) now_s = "BACKSPIN";
            else if (fs & ST_BRAKE) now_s = "BRAKE";
            else if (p_fill) now_s = "FILL";
            else if (p_drop) now_s = "DROP";
            else if (p_freeze) now_s = "FREEZE";
            else if (p_cut) now_s = "CUT";
            else if (fs & ST_MUTED) now_s = "...";
            else if (fs & ST_WAIT) now_s = "SYNC...";
            else {
                char *p = put(bpm_s, ext_live ? "EXT " : set.beat ? "BEAT " : "DJ ");
                put_u(p, ext_live ? ext_bpm : set.bpm);
                now_s = bpm_s; now_c = ext_live ? C_YEL : C_CYAN;
            }
            key = (uint32_t)now_s[0] * 7u + (uint32_t)now_s[1] + rec * 1000u + (ext_live ? ext_bpm : set.bpm) * 3u + set.beat * 99991u + ext_live * 7777u;
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
            else if (now - t_draw >= 33u || deck_view == ~0u) {
                if (set.beat) { draw_pads(hit_t, now); deck_view = 1u; t_draw = now; }
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
                if (!shift) {
                    a[0].label = FX_NAME[set.fx]; put_u(a[0].val, set.amt[set.fx]); a[0].bar = set.amt[set.fx]; a[0].of = 15;
                    a[1].label = "ECHO"; put_u(a[1].val, set.echo); a[1].bar = set.echo; a[1].of = 15;
                    a[2].label = "BPM"; put_u(a[2].val, set.bpm); a[2].bar = set.bpm - 60u; a[2].of = 120;
                    a[3].label = "VOICE"; put(a[3].val, VOICES[set.voice].name); a[3].bar = 0; a[3].of = 0;
                    c[0].label = "JOG"; put(c[0].val, "SCRATCH"); c[0].of = 0;
                    c[1].label = "STYLE"; put(c[1].val, SC_NAME[set.style]); c[1].of = 0;
                    c[2].label = "GROOVE"; put(c[2].val, GROOVES[set.groove].name); c[2].of = 0;
                } else {
                    a[0].label = "TUNE"; put_s(a[0].val, (int32_t)set.tune - 6); a[0].bar = set.tune; a[0].of = 12;
                    a[1].label = "E.TIME"; put(a[1].val, ETIME_NAME[set.etime]); a[1].bar = set.etime; a[1].of = 3;
                    a[2].label = "DECAY"; put_s(a[2].val, (int32_t)set.decay - 2); a[2].bar = set.decay; a[2].of = 4;
                    a[3].label = "DRUMS"; put_u(a[3].val, set.dlvl); a[3].bar = set.dlvl; a[3].of = 15;
                    c[0].label = "SWING"; put_u(c[0].val, set.swing * 10u); c[0].bar = set.swing; c[0].of = 10;
                    c[1].label = "REACH"; put_u(c[1].val, set.reach + 1u); c[1].bar = set.reach; c[1].of = 7;
                    c[2].label = "DECK"; put_u(c[2].val, set.vlvl); c[2].bar = set.vlvl; c[2].of = 15;
                }
                draw_knobs(a, c, shift, acc);
            }
            if (chips) { chips = 0; draw_chips(play, rec, set.aut, set.hard, set.beat, set.semi); }
        }

        if (dirty && !play && !dk_on_v && now - t_change > 2000u) {   /* saved when silent (an erase stalls the sound) */
            for (s = 0; s < 16u; s++) { set.word[s] = seq_word[s]; set.flag[s] = seq_flag[s]; set.drum[s] = seq_drum[s]; }
            settings_save(&set, sizeof set);
            dirty = 0;
        }
    }
}
