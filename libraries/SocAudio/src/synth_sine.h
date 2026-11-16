// synth_sine.h - the synth's pre-scaled 256-entry sine table
//
// Plain C (no Arduino headers), so that tools/check_table.py can compile the
// same code for the host (--cc) and compare it with its Python model.
//
// amp = (2^23 - 1) / voices * volume / 255; entry k = amp * sin(2 pi k / 256)
// by the recurrence s[k+1] = 2 cos(t) s[k] - s[k-1] in Q7 (32x32->64
// multiplies, no table in flash: reading one would thrash the XIP cache),
// quarter wave mirrored to 256 entries. The recurrence is up to +1.6 LSB off:
// every entry is clamped to +-amp, so that `voices` voices at their peak sum
// to at most 2^23 - 1 (unclamped, 2 voices at volume 255 reached 2^23 and
// wrapped to -full scale after the 24-bit mask).
#ifndef SYNTH_SINE_H
#define SYNTH_SINE_H

#include <stdint.h>
#include "synth_tables.h"

#define SYNTH_SINE_FULL 8388607L   // 2^23 - 1

static inline int32_t synth_sine_amp(uint32_t voices, uint32_t volume)
{
    return (int32_t)((uint32_t)(SYNTH_SINE_FULL / (int32_t)voices) * volume / 255u);
}

static inline void synth_sine_build(int32_t *tab, int32_t amp)
{
    int32_t s0 = 0;
    int32_t s1 = (int32_t)(((int64_t)amp * SINE_S1) >> 24);
    tab[0] = 0;
    tab[128] = 0;
    for (int k = 1; k <= 64; k++) {
        int32_t v = (s1 + 64) >> 7;
        if (v > amp)
            v = amp;
        else if (v < -amp)
            v = -amp;
        tab[k] = v;
        tab[128 - k] = v;
        tab[128 + k] = -v;
        tab[(256 - k) & 255] = -v;
        int32_t s2 = (int32_t)(((int64_t)SINE_C2 * s1 + (1 << 29)) >> 30) - s0;
        s0 = s1;
        s1 = s2;
    }
}

#endif // SYNTH_SINE_H
