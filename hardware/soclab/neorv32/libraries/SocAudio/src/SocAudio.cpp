// SocAudio.cpp - audio peripheral driver and sine synth (own implementation)
//
// Render path (DMEM, RAMFUNC): per block of SYNTH_BLOCK frames, voice by
// voice into a mix buffer, then the block goes to the FIFO. Per voice and
// frame: phase += inc; table index = phase[31:24] read as a byte from a
// scratch word (sw + lbu instead of a 24-bit shift, the shifter is serial);
// table lookup. At unity gain (at the start and the end of the block) no
// multiply. Otherwise the gain moves linearly from its value at the start of
// the block to the one at its end (one add per frame: no gain steps at the
// block edges) and one mulhsu: table x gain in Q32, whose upper word is the
// product (no shift). The envelope (attack/release, auto release) advances
// once per block.
// The sine table (synth_sine.h) is pre-scaled to (2^23 - 1) / SYNTH_VOICES *
// volume, every entry clamped to that amplitude, and |table x gain| <=
// |table|: the sum of all voices never clips (tools/check_table.py).
#include "SocAudio.h"
#include "arduino_internal.h"
#include "synth_tables.h"
#include "synth_sine.h"

SocAudioClass Audio;
SynthClass Synth;

// ================================================================ Synth state (DMEM)
enum { V_IDLE = 0, V_ATTACK, V_SUSTAIN, V_RELEASE };

struct voice_t {
    uint32_t phase;
    uint32_t inc;
    int32_t  gain;         // Q16, 65536 = 1, at the end of the last rendered block
    int32_t  target;       // Q16 level of the note (velocity)
    int32_t  frames_left;  // > 0: release after that many frames
    uint32_t state;
};

#define GAIN_ONE       65536
#define PIN_LOW_MASK   (1u << PIN_AUDIO_LOW)
#define ENV_BLOCKS(ms) ((uint32_t)(ms) * (SOC_AUDIO_RATE / 1000) / SYNTH_BLOCK)   // 3 per ms
#define LOW_WATER_MIN  SYNTH_BLOCK
#define LOW_WATER_MAX  (SOC_AUDIO_FIFO_DEPTH - 2 * SYNTH_BLOCK)   // a refill ends above 240
// One refill renders while a block of space is free, but at most this many
// blocks: a render slower than the DAC (other interrupt load) must not keep
// the handler busy forever with interrupts off.
#define FILL_MAX_BLOCKS (2 * SOC_AUDIO_FIFO_DEPTH / SYNTH_BLOCK)
#define REFILL_TRIES    3

static_assert(SYNTH_BLOCK == 16, "the per-frame gain step of render_block divides by 16 (>> 4)");

static voice_t voices[SYNTH_VOICES];
static int32_t sine_tab[256];               // pre-scaled
static int32_t mixbuf[SYNTH_BLOCK];
static volatile uint32_t ph_scratch;        // phase MSB as a byte load
static int32_t atk_step = GAIN_ONE / ENV_BLOCKS(5);    // Q16 per block (default 5 ms)
static int32_t rel_step = GAIN_ONE / ENV_BLOCKS(20);   // (default 20 ms)
static uint8_t s_mode = SYNTH_IRQ;
static volatile uint8_t s_running = 0;
static volatile uint8_t s_in_update = 0;    // update() re-entrancy guard
static uint8_t s_volume = 255;
static SynthStats st;
static uint64_t st_t0;
// synth_stop, set by Synth.begin: Audio.end() stops a running synth without
// linking the synth into sketches that use Audio only
static void (*s_stop_hook)(void) = 0;
// Static average-output mode. The DAC holds these samples after the FIFO
// empties, so a DC level needs one frame, not an interrupt refill loop.
static uint8_t s_analog_active = 0;
static int32_t s_analog_left = SOC_AUDIO_MIN;
static int32_t s_analog_right = SOC_AUDIO_MIN;

static inline uint32_t __attribute__((always_inline)) rdcycle(void)
{
    uint32_t c;
    __asm__ volatile ("csrr %0, mcycle" : "=r"(c));
    return c;
}

// upper word of the signed x unsigned 64-bit product (one serial multiply,
// as long as mul)
static inline int32_t __attribute__((always_inline)) mulhsu(int32_t a, uint32_t b)
{
    int32_t r;
    __asm__ ("mulhsu %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

static uint16_t clamp_low_water(uint16_t lw)
{
    if (lw < LOW_WATER_MIN)
        return LOW_WATER_MIN;
    if (lw > LOW_WATER_MAX)
        return LOW_WATER_MAX;
    return lw;
}

static void set_low_water(uint16_t lw)
{
    if (s_running && s_mode == SYNTH_IRQ)
        lw = clamp_low_water(lw);
    SOC_AUDIO_FIFO_LOW = lw & 0x1FFu;
}

// ================================================================ Audio
void SocAudioClass::begin(uint16_t lowWater)
{
    uint32_t s = __arduino_irq_save();
    if (s_stop_hook)
        s_stop_hook();
    s_analog_active = 0;
    SOC_AUDIO_CTRL0 = SOC_AUDIO_CTRL_RESET | SOC_AUDIO_CTRL_ENABLE;
    SOC_AUDIO_CTRL0 = SOC_AUDIO_CTRL_ENABLE;
    set_low_water(lowWater);
    __arduino_irq_restore(s);
}

void SocAudioClass::end()
{
    if (s_stop_hook)
        s_stop_hook();
    s_analog_active = 0;
    SOC_AUDIO_CTRL0 = SOC_AUDIO_CTRL_RESET;
    SOC_AUDIO_CTRL0 = 0;
}

void SocAudioClass::setLowWater(uint16_t lowWater)
{
    set_low_water(lowWater);
}

int SocAudioClass::level()
{
    return (int)(SOC_AUDIO_FIFO_LEVEL & 0x1FFu);
}

int SocAudioClass::availableForWrite()
{
    return SOC_AUDIO_FIFO_DEPTH - level();
}

bool SocAudioClass::full()
{
    return (SOC_AUDIO_STAT0 & SOC_AUDIO_STAT_FULL) != 0;
}

bool SocAudioClass::empty()
{
    return (SOC_AUDIO_STAT0 & SOC_AUDIO_STAT_EMPTY) != 0;
}

bool SocAudioClass::low()
{
    return (SOC_AUDIO_STAT0 & SOC_AUDIO_STAT_LOW) != 0;
}

static inline uint32_t to24(int32_t s)
{
    if (s > SOC_AUDIO_MAX)
        s = SOC_AUDIO_MAX;
    else if (s < SOC_AUDIO_MIN)
        s = SOC_AUDIO_MIN;
    return (uint32_t)s & 0x00FFFFFFu;
}

size_t SocAudioClass::write(int32_t left, int32_t right)
{
    uint32_t l = to24(left);
    uint32_t r = to24(right) | SOC_AUDIO_COMMIT;
    uint32_t s = __arduino_irq_save();   // full check, L, R + commit: one frame
    if (s_analog_active) {
        // Explicit PCM output takes ownership from analogWrite and discards
        // queued static-level changes before accepting this frame.
        s_analog_active = 0;
        SOC_AUDIO_CTRL0 = SOC_AUDIO_CTRL_RESET | SOC_AUDIO_CTRL_ENABLE;
        SOC_AUDIO_CTRL0 = SOC_AUDIO_CTRL_ENABLE;
    }
    if (SOC_AUDIO_STAT0 & SOC_AUDIO_STAT_FULL) {
        __arduino_irq_restore(s);
        return 0;                        // a commit into a full FIFO would be dropped
    }
    SOC_AUDIO_L = l;
    SOC_AUDIO_R = r;
    __arduino_irq_restore(s);
    return 1;
}

// Arduino analogWrite backend. Kept independent of SynthClass: the optional
// stop hook stops a synth only when that synth was actually linked/started.
extern "C" void __arduino_analog_write(uint8_t pin, uint32_t value, uint8_t bits)
{
    uint32_t maximum = (1u << bits) - 1u;
    uint32_t offset = (uint32_t)(((uint64_t)value * 0x00FFFFFFu + maximum / 2u) / maximum);
    int32_t sample = (int32_t)offset - 8388608L;
    uint32_t s = __arduino_irq_save();
    if (!s_analog_active || !(SOC_AUDIO_CTRL0 & SOC_AUDIO_CTRL_ENABLE)) {
        if (s_stop_hook)
            s_stop_hook();
        SOC_AUDIO_CTRL0 = SOC_AUDIO_CTRL_RESET | SOC_AUDIO_CTRL_ENABLE;
        SOC_AUDIO_CTRL0 = SOC_AUDIO_CTRL_ENABLE;
        SOC_AUDIO_FIFO_LOW = 0;
        s_analog_left = SOC_AUDIO_MIN;
        s_analog_right = SOC_AUDIO_MIN;
        s_analog_active = 1;
    }
    if (pin == DAC_LEFT)
        s_analog_left = sample;
    else
        s_analog_right = sample;
    // A rapid caller can fill 256 frames. At most one 48-kHz frame elapses
    // before space appears; keep L/R and the retained values atomic.
    while (SOC_AUDIO_STAT0 & SOC_AUDIO_STAT_FULL) {}
    SOC_AUDIO_L = (uint32_t)s_analog_left & 0x00FFFFFFu;
    SOC_AUDIO_R = ((uint32_t)s_analog_right & 0x00FFFFFFu) | SOC_AUDIO_COMMIT;
    __arduino_irq_restore(s);
}

// ================================================================ render (DMEM)
// Q16 gain (0..65536) -> Q32 multiplier; 1.0 -> 0xFFFFFFFF (2^-32 short)
static inline uint32_t __attribute__((always_inline)) gain_q32(int32_t g)
{
    return (g >= GAIN_ONE) ? 0xFFFFFFFFu : ((uint32_t)g << 16);
}

// One block: envelopes, voices into mixbuf, mixbuf to the FIFO. The caller
// guarantees SYNTH_BLOCK free FIFO entries.
static void RAMFUNC render_block(void)
{
    uint32_t t0 = rdcycle();
    voice_t *v = voices;
    for (int k = 0; k < SYNTH_VOICES; k++, v++) {
        uint32_t state = v->state;
        if (state == V_IDLE)
            continue;
        int32_t g0 = v->gain;          // gain at the start of the block
        int32_t gain = g0;             // ... and at its end
        if (state == V_ATTACK) {
            int32_t tgt = v->target;
            if (gain < tgt) {
                gain += atk_step;
                if (gain >= tgt) { gain = tgt; state = V_SUSTAIN; }
            } else {
                gain -= rel_step;
                if (gain <= tgt) { gain = tgt; state = V_SUSTAIN; }
            }
        } else if (state == V_RELEASE) {
            gain -= rel_step;
            if (gain <= 0) { gain = 0; state = V_IDLE; }   // this block still ramps down
        }
        if ((state == V_ATTACK || state == V_SUSTAIN) && v->frames_left > 0) {
            int32_t fl = v->frames_left - SYNTH_BLOCK;
            if (fl <= 0) { fl = 0; state = V_RELEASE; }
            v->frames_left = fl;
        }
        v->gain = gain;
        v->state = state;
        if ((g0 | gain) == 0)
            continue;

        uint32_t ph = v->phase;
        uint32_t inc = v->inc;
        const int32_t *tab = sine_tab;
        int32_t *m = mixbuf;
        int32_t *end = mixbuf + SYNTH_BLOCK;
        volatile uint8_t *msb = (volatile uint8_t *)&ph_scratch + 3;
        st.voice_blocks++;
        if (g0 == GAIN_ONE && gain == GAIN_ONE) {
            do {
                ph += inc;
                ph_scratch = ph;
                *m++ += tab[*msb];
            } while (m != end);
        } else {
            st.mul_blocks++;
            // frame i (1..16) at g0 + i (gain - g0) / 16; both Q32 ends >> 4
            // keep the difference in int32 and the last frame lands on the end
            // value (rounded down to 16)
            uint32_t ga = gain_q32(g0);
            uint32_t gb = gain_q32(gain);
            int32_t dg = (int32_t)((gb >> 4) - (ga >> 4));
            uint32_t gq = ga & ~15u;
            do {
                ph += inc;
                ph_scratch = ph;
                gq += (uint32_t)dg;
                *m++ += mulhsu(tab[*msb], gq);
            } while (m != end);
        }
        v->phase = ph;
    }

    int32_t *m = mixbuf;
    int32_t *end = mixbuf + SYNTH_BLOCK;
    do {
        uint32_t s = (uint32_t)*m & 0x00FFFFFFu;
        *m++ = 0;
        SOC_AUDIO_L = s;
        SOC_AUDIO_R = s | SOC_AUDIO_COMMIT;
    } while (m != end);
    st.frames += SYNTH_BLOCK;
    st.render_cycles += (uint32_t)(rdcycle() - t0);
}

// Render while at least one block of space is free (the level is read again
// per block), at most FILL_MAX_BLOCKS blocks. After every block the UART RX
// poll hook (installed by Serial.begin) moves a received character from the
// one-character UART RX FIFO into the ring buffer: the refill runs with
// interrupts off for up to ~0.7 ms (4 voices with the gain multiply).
static void RAMFUNC synth_fill(void)
{
    __arduino_handler_t rx = __arduino_rx_poll_hook;
    for (uint32_t n = FILL_MAX_BLOCKS; n != 0; n--) {
        uint32_t space = SOC_AUDIO_FIFO_DEPTH - (SOC_AUDIO_FIFO_LEVEL & 0x1FFu);
        if (space < SYNTH_BLOCK)
            break;
        render_block();
        if (rx)
            rx();
    }
}

// FIFO-low interrupt (gpio_i[22] rising): mask it, refill, unmask. Masking
// keeps the DAC reads around the threshold during the refill from
// re-triggering (fifo_low has no hysteresis). A rising edge while masked is
// lost: if the FIFO is still below FIFO_LOW after unmasking, no new edge would
// come, so refill again (bounded; counted as retries / stalls).
static void RAMFUNC synth_isr(void)
{
    uint32_t t0 = rdcycle();
    uint32_t f0 = st.frames;
    for (uint32_t tries = REFILL_TRIES;;) {
        NEORV32_GPIO->IRQ_ENABLE &= ~PIN_LOW_MASK;
        synth_fill();
        NEORV32_GPIO->IRQ_ENABLE |= PIN_LOW_MASK;
        if (!(SOC_AUDIO_STAT0 & SOC_AUDIO_STAT_LOW))
            break;
        if (--tries == 0) {
            st.stalls++;
            break;
        }
        st.refill_retries++;
    }
    uint32_t nf = st.frames - f0;
    uint32_t dt = rdcycle() - t0;
    st.irqs++;
    st.irq_cycles += dt;
    if (dt > st.irq_cycles_max)
        st.irq_cycles_max = dt;
    if (nf < st.irq_frames_min)
        st.irq_frames_min = nf;
    if (nf > st.irq_frames_max)
        st.irq_frames_max = nf;
}

// SYNTH_POLL refill: Synth.update() and the idle hook (called for every
// character Serial prints and in every delay() loop iteration, mostly
// returning at once). In DMEM: no XIP refills on that path. Not re-entrant:
// an interrupt handler that prints or delays during an update sees the guard;
// the level is read again per block, so a nested refill cannot make this one
// overfill the FIFO.
static void RAMFUNC synth_poll(void)
{
    if (!s_running || s_mode != SYNTH_POLL || s_in_update)
        return;
    s_in_update = 1;
    for (uint32_t n = FILL_MAX_BLOCKS; n != 0; n--) {
        uint32_t space = SOC_AUDIO_FIFO_DEPTH - (SOC_AUDIO_FIFO_LEVEL & 0x1FFu);
        if (space < SYNTH_BLOCK)
            break;
        // blocks with interrupts off: a noteOn from an ISR cannot interleave
        uint32_t s = __arduino_irq_save();
        render_block();
        __arduino_irq_restore(s);
    }
    s_in_update = 0;
}

// ================================================================ Synth API (flash)
static void build_table(void)
{
    synth_sine_build(sine_tab, synth_sine_amp(SYNTH_VOICES, s_volume));
}

// Stop the interrupt / polling feed (voices and DAC untouched)
static void synth_stop(void)
{
    if (!s_running)
        return;
    uint32_t s = __arduino_irq_save();
    neorv32_gpio_irq_disable(PIN_LOW_MASK);
    __arduino_gpio_handler(PIN_AUDIO_LOW, 0);
    if (__arduino_idle_hook == synth_poll)
        __arduino_set_idle_hook(0);
    s_running = 0;
    __arduino_irq_restore(s);
}

// Every sounding voice down to 0 within one block, written to the FIFO, then
// all voices idle: the last frame in the FIFO is 0, which the DAC holds after
// playing the FIFO out (no step). Waits for one block of space (with the DAC
// on at most 16 frames = 0.33 ms; bounded at 1 ms).
static void fade_out(void)
{
    uint32_t s = __arduino_irq_save();
    int32_t rs = rel_step;
    bool sounding = false;
    rel_step = GAIN_ONE;
    for (int k = 0; k < SYNTH_VOICES; k++) {
        if (voices[k].state != V_IDLE) {
            voices[k].state = V_RELEASE;
            voices[k].frames_left = 0;
            sounding = true;
        }
    }
    __arduino_irq_restore(s);
    if (sounding && (SOC_AUDIO_CTRL0 & SOC_AUDIO_CTRL_ENABLE)) {
        uint32_t c0 = cycles();
        uint32_t space;
        do {
            space = SOC_AUDIO_FIFO_DEPTH - (SOC_AUDIO_FIFO_LEVEL & 0x1FFu);
        } while (space < SYNTH_BLOCK && (uint32_t)(cycles() - c0) < 3u * SYNTH_BLOCK * 2048u);
        if (space >= SYNTH_BLOCK) {
            s = __arduino_irq_save();
            render_block();
            __arduino_irq_restore(s);
        }
    }
    s = __arduino_irq_save();
    for (int k = 0; k < SYNTH_VOICES; k++) {
        voices[k].state = V_IDLE;
        voices[k].gain = 0;
    }
    rel_step = rs;
    __arduino_irq_restore(s);
}

void SynthClass::begin(uint8_t mode, uint16_t lowWater)
{
    if (s_running)
        end();
    mode = (mode == SYNTH_POLL) ? SYNTH_POLL : SYNTH_IRQ;
    lowWater = clamp_low_water(lowWater);
    build_table();
    for (int k = 0; k < SYNTH_VOICES; k++)
        voices[k] = voice_t{0, NOTE_INC[69], 0, 0, 0, V_IDLE};
    for (int n = 0; n < SYNTH_BLOCK; n++)
        mixbuf[n] = 0;
    s_mode = mode;
    uint16_t lw = (mode == SYNTH_IRQ) ? lowWater : 0;   // no low flag in polling mode
    if (s_analog_active)
        Audio.begin(lw);   // leave static DAC mode and discard its queued frames
    else if (SOC_AUDIO_CTRL0 & SOC_AUDIO_CTRL_ENABLE)
        SOC_AUDIO_FIFO_LOW = lw;   // DAC already on (Audio.begin, Synth.end): no reset, no pop
    else
        Audio.begin(lw);
    resetStats();
    uint32_t s = __arduino_irq_save();   // the RX poll hook must not race the RX interrupt
    synth_fill();   // silence, so that fifo_low falls and the next rise is an edge
    __arduino_irq_restore(s);
    s_stop_hook = synth_stop;
    if (mode == SYNTH_IRQ) {
        neorv32_gpio_irq_setup(PIN_AUDIO_LOW, GPIO_TRIG_EDGE_RISING);
        __arduino_gpio_handler(PIN_AUDIO_LOW, synth_isr);
        neorv32_gpio_irq_enable(PIN_LOW_MASK);
    } else {
        __arduino_set_idle_hook(synth_poll);
    }
    s_running = 1;
}

void SynthClass::end()
{
    if (!s_running)
        return;
    synth_stop();
    fade_out();
}

bool SynthClass::running() const
{
    return s_running != 0;
}

uint32_t SynthClass::noteIncrement(uint8_t note)
{
    return NOTE_INC[note & 127u];
}

uint32_t SynthClass::hzToIncrement(float hz)
{
    if (!(hz > 0.0f))
        return 0;
    if (hz > 24000.0f)
        hz = 24000.0f;
    return (uint32_t)(hz * (4294967296.0f / (float)SOC_AUDIO_RATE) + 0.5f);
}

static void voice_start(uint8_t voice, uint32_t inc, uint8_t velocity, uint32_t frames)
{
    if (voice >= SYNTH_VOICES)
        return;
    if (velocity == 0) {
        Synth.noteOff(voice);
        return;
    }
    if (velocity > 127)
        velocity = 127;
    int32_t target = (int32_t)(((uint32_t)velocity * GAIN_ONE + 63u) / 127u);   // 127 -> 65536
    uint32_t s = __arduino_irq_save();
    voice_t *v = &voices[voice];
    v->inc = inc;
    v->target = target;
    v->frames_left = (int32_t)frames;
    v->state = V_ATTACK;
    __arduino_irq_restore(s);
}

void SynthClass::noteOn(uint8_t voice, uint8_t note, uint8_t velocity)
{
    voice_start(voice, NOTE_INC[note & 127u], velocity, 0);
}

void SynthClass::noteOnHz(uint8_t voice, float hz, uint8_t velocity, uint32_t duration_ms)
{
    // duration in sketch time (scaled in SIM builds like delay())
    uint32_t frames = (uint32_t)((uint64_t)duration_ms * (SOC_AUDIO_RATE / 1000) / ARDUINO_TIME_SCALE);
    if (duration_ms && frames == 0)
        frames = 1;
    voice_start(voice, hzToIncrement(hz), velocity, frames);
}

void SynthClass::noteOff(uint8_t voice)
{
    if (voice >= SYNTH_VOICES)
        return;
    uint32_t s = __arduino_irq_save();
    voice_t *v = &voices[voice];
    if (v->state != V_IDLE)
        v->state = V_RELEASE;
    v->frames_left = 0;
    __arduino_irq_restore(s);
}

void SynthClass::allOff()
{
    for (uint8_t k = 0; k < SYNTH_VOICES; k++)
        noteOff(k);
}

void SynthClass::setFrequency(uint8_t voice, float hz)
{
    if (voice < SYNTH_VOICES)
        voices[voice].inc = hzToIncrement(hz);   // one aligned word store: atomic
}

void SynthClass::setEnvelope(uint16_t attack_ms, uint16_t release_ms)
{
    // 3 blocks per ms; 0 ms = one block (the per-frame ramp of render_block)
    uint32_t a = ENV_BLOCKS(attack_ms);
    uint32_t r = ENV_BLOCKS(release_ms);
    int32_t as = a ? (int32_t)(GAIN_ONE / a) : GAIN_ONE;
    int32_t rs = r ? (int32_t)(GAIN_ONE / r) : GAIN_ONE;
    if (as < 1) as = 1;
    if (rs < 1) rs = 1;
    uint32_t s = __arduino_irq_save();
    atk_step = as;
    rel_step = rs;
    __arduino_irq_restore(s);
}

void SynthClass::setVolume(uint8_t volume)
{
    s_volume = volume;
    // A block rendered meanwhile mixes old and new entries: harmless, each is
    // at most the larger of the two amplitudes, the sum still cannot clip.
    build_table();
}

bool SynthClass::isPlaying(uint8_t voice) const
{
    return voice < SYNTH_VOICES && voices[voice].state != V_IDLE;
}

uint32_t SynthClass::frequencyMilliHz(uint8_t voice) const
{
    if (voice >= SYNTH_VOICES)
        return 0;
    return (uint32_t)(((uint64_t)voices[voice].inc * (SOC_AUDIO_RATE * 1000ull)) >> 32);
}

void SynthClass::update()
{
    synth_poll();
}

SynthStats SynthClass::stats() const
{
    uint32_t s = __arduino_irq_save();
    SynthStats r = st;
    r.elapsed_cycles = neorv32_cpu_get_cycle() - st_t0;
    __arduino_irq_restore(s);
    return r;
}

void SynthClass::resetStats()
{
    uint32_t s = __arduino_irq_save();
    st = SynthStats{};
    st.irq_frames_min = 0xFFFFFFFFu;
    st_t0 = neorv32_cpu_get_cycle();
    __arduino_irq_restore(s);
}

// num / den with 0..2 decimals, rounded. 32-bit integer math as long as it
// fits: soft-float and the 64-bit division routines are slow from XIP.
static void print_fixed(Print &out, uint64_t num, uint64_t den, int dec)
{
    uint32_t scale = (dec >= 2) ? 100u : (dec == 1 ? 10u : 1u);
    uint32_t lim = 0xFFFFFFFFu / scale / 2u;   // num * scale + den / 2 < 2^32
    uint32_t q;
    if (den == 0)
        q = 0;
    else if (num <= lim && den <= 0x7FFFFFFFull)
        q = ((uint32_t)num * scale + (uint32_t)den / 2u) / (uint32_t)den;
    else
        q = (uint32_t)((num * scale + den / 2) / den);
    out.print((unsigned long)(q / scale));
    if (scale > 1) {
        uint32_t f = q % scale;
        out.print('.');
        if (scale == 100u && f < 10u)
            out.print('0');
        out.print((unsigned long)f);
    }
}

void SynthClass::printStats(Print &out, const char *label) const
{
    SynthStats s = stats();
    uint64_t busy = (s_mode == SYNTH_IRQ) ? s.irq_cycles : s.render_cycles;
    out.print("stats ");
    out.print(label);
    out.print(" frames ");
    out.print(s.frames);
    out.print(" cycles/frame ");
    print_fixed(out, s.render_cycles, s.frames, 1);
    out.print(" mul ");
    out.print(s.mul_blocks);
    out.print("/");
    out.print(s.voice_blocks);
    out.print(" irqs ");
    out.print(s.irqs);
    if (s.irqs) {
        out.print(" frames/irq ");
        out.print(s.irq_frames_min);
        out.print("..");
        out.print(s.irq_frames_max);
        out.print(" irq_max_us ");
        print_fixed(out, (uint64_t)s.irq_cycles_max * 1000u, F_CPU / 1000u, 1);
    }
    out.print(" load ");
    print_fixed(out, busy * 100u, s.elapsed_cycles, 2);
    out.print(" %");
    if (s.refill_retries || s.stalls) {
        out.print(" retries ");
        out.print(s.refill_retries);
        out.print(" stalls ");
        out.print(s.stalls);
    }
    out.println();
}

// ================================================================ tone() backend
extern "C" void __arduino_tone(uint8_t pin, unsigned int frequency, unsigned long duration_ms)
{
    (void)pin;
    if (!s_running)
        Synth.begin(SYNTH_IRQ);
    if (frequency == 0) {
        Synth.noteOff(0);
        return;
    }
    Synth.noteOnHz(0, (float)frequency, 127, duration_ms);
}

extern "C" void __arduino_no_tone(uint8_t pin)
{
    (void)pin;
    Synth.noteOff(0);
}
