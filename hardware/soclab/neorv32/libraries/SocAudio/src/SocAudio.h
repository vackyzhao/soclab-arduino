// SocAudio.h - SoC Lab audio peripheral (0xFFD0_0000) and a small synth
//
// Audio: low level access to the 256-frame FIFO in front of the sigma-delta
//   DAC (48 kHz, one frame every 2048 clocks). Samples are signed 24 bit.
// Synth: SYNTH_VOICES sine voices (default 4, 1..16, -DSYNTH_VOICES=n),
//   phase accumulators, attack/release envelope per block of SYNTH_BLOCK
//   frames (the gain is interpolated per frame), fed either by the FIFO-low
//   interrupt (gpio_i[22]) or by update() from loop(). Render path and
//   interrupt handler run from DMEM.
#ifndef SOC_AUDIO_H
#define SOC_AUDIO_H

#include <Arduino.h>

// ---------------------------------------------------------------- registers
#define SOC_AUDIO_BASE        0xFFD00000u
#define SOC_AUDIO_CTRL0       (*(volatile uint32_t *)(SOC_AUDIO_BASE + 0x00u))
#define SOC_AUDIO_STAT0       (*(volatile uint32_t *)(SOC_AUDIO_BASE + 0x04u))
#define SOC_AUDIO_FIFO_LOW    (*(volatile uint32_t *)(SOC_AUDIO_BASE + 0x08u))
#define SOC_AUDIO_FIFO_LEVEL  (*(volatile uint32_t *)(SOC_AUDIO_BASE + 0x0Cu))
#define SOC_AUDIO_L           (*(volatile uint32_t *)(SOC_AUDIO_BASE + 0x10u))
#define SOC_AUDIO_R           (*(volatile uint32_t *)(SOC_AUDIO_BASE + 0x14u))

#define SOC_AUDIO_CTRL_RESET  (1u << 0)   // software reset of FIFO + DAC
#define SOC_AUDIO_CTRL_ENABLE (1u << 1)   // DAC enable
#define SOC_AUDIO_STAT_LOW    (1u << 0)
#define SOC_AUDIO_STAT_EMPTY  (1u << 1)
#define SOC_AUDIO_STAT_FULL   (1u << 2)
#define SOC_AUDIO_COMMIT      (1u << 31)

#define SOC_AUDIO_FIFO_DEPTH  256
#define SOC_AUDIO_RATE        48000
#define SOC_AUDIO_MAX         8388607L    // 2^23 - 1
#define SOC_AUDIO_MIN         (-8388608L)

// ---------------------------------------------------------------- Audio
class SocAudioClass {
public:
    constexpr SocAudioClass() {}

    // Reset FIFO + DAC and enable the DAC (CTRL0 = 3, then 2), set FIFO_LOW
    // (fifo_low = level < lowWater, 0 = never low).
    void begin(uint16_t lowWater = 0);
    // Stops the synth if it runs, FIFO reset, DAC off. The DAC output then
    // stays low (= -full scale): a pop. Synth.end() leaves the DAC on.
    void end();
    // While the synth runs in SYNTH_IRQ mode clamped to 16..224 (a threshold
    // above the refill level would leave fifo_low high: no new interrupt).
    void setLowWater(uint16_t lowWater);
    int  level();                        // frames in the FIFO, 0..256
    int  availableForWrite();            // 256 - level
    bool full();
    bool empty();
    bool low();                          // FIFO level < FIFO_LOW

    // One frame, non-blocking: 0 if the FIFO is full (nothing written), else 1.
    // Signed 24-bit samples, clamped to +-2^23 and masked to 24 bits. The
    // full check and the two register writes run with interrupts off (an
    // interrupt writing frames in between would commit a mixed frame).
    size_t write(int32_t left, int32_t right);
    size_t write(int32_t mono) { return write(mono, mono); }
    size_t write16(int16_t left, int16_t right) { return write((int32_t)left * 256, (int32_t)right * 256); }
};

extern SocAudioClass Audio;

// ---------------------------------------------------------------- Synth
#ifndef SYNTH_VOICES
#define SYNTH_VOICES 4
#endif
// 16 voices with the gain multiply cost ~1.5k of the 2048 clocks per frame
static_assert(SYNTH_VOICES >= 1 && SYNTH_VOICES <= 16, "SYNTH_VOICES must be 1..16");
#define SYNTH_BLOCK 16              // frames per render block (envelope step)

#define SYNTH_IRQ  0                // FIFO-low interrupt feeds the FIFO
#define SYNTH_POLL 1                // update() (also called inside delay())

struct SynthStats {
    uint32_t frames;                // frames written to the FIFO
    uint64_t render_cycles;         // mcycle spent rendering + writing them
    uint32_t irqs;                  // FIFO-low interrupts handled
    uint64_t irq_cycles;            // mcycle inside the interrupt handler
    uint32_t irq_cycles_max;
    uint32_t irq_frames_min;        // frames written per interrupt
    uint32_t irq_frames_max;
    uint64_t elapsed_cycles;        // mcycle since resetStats()
    uint32_t voice_blocks;          // blocks rendered per voice, summed over voices
    uint32_t mul_blocks;            // of these with a gain multiply (not unity gain)
    uint32_t refill_retries;        // FIFO still low after a refill: refilled again
    uint32_t stalls;                // ... and still low after the last try (render too slow)
};

class SynthClass {
public:
    constexpr SynthClass() {}

    // mode SYNTH_IRQ or SYNTH_POLL (any other value: SYNTH_IRQ). lowWater:
    // FIFO_LOW threshold for the interrupt (clamped to 16..224). Starts the
    // DAC (Audio.begin) unless it is already on, and fills the FIFO with
    // silence. A running synth is ended first.
    void begin(uint8_t mode = SYNTH_IRQ, uint16_t lowWater = 128);
    // Stops rendering: every voice fades to 0 within one block (0.33 ms),
    // the DAC stays on and holds the last sample (0) once the FIFO has
    // played out. Audio.end() switches the DAC off.
    void end();
    bool running() const;

    void noteOn(uint8_t voice, uint8_t note, uint8_t velocity = 127);
    void noteOnHz(uint8_t voice, float hz, uint8_t velocity = 127, uint32_t duration_ms = 0);
    void noteOff(uint8_t voice);                 // release
    void allOff();
    void setFrequency(uint8_t voice, float hz);  // pitch only, no (re)trigger
    // Linear attack / release, real ms (not time-scaled), rounded down to
    // blocks of 0.33 ms; 0 = one block. Default 5 / 20 ms.
    void setEnvelope(uint16_t attack_ms, uint16_t release_ms);
    void setVolume(uint8_t volume);              // master, 0..255 (rebuilds the sine table)
    bool isPlaying(uint8_t voice) const;
    uint32_t frequencyMilliHz(uint8_t voice) const;   // exact pitch of the phase increment

    void update();                               // SYNTH_POLL: refill the FIFO

    SynthStats stats() const;
    void resetStats();
    // One line: "stats <label> frames .. cycles/frame .. mul .. irqs .. frames/irq ..
    // irq_max .. load .. %" (load = time in the FIFO-low handler, or the
    // render time in SYNTH_POLL mode, over the time since resetStats()),
    // " retries n stalls n" at the end only if not 0.
    void printStats(Print &out, const char *label) const;

    static uint32_t noteIncrement(uint8_t note);
    static uint32_t hzToIncrement(float hz);
};

extern SynthClass Synth;

#endif // SOC_AUDIO_H
