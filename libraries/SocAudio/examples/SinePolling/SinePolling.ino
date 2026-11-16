// SinePolling - the course dac_sin with the SocAudio API: the 109-entry sine
// table (48 kHz / 109 = 440.37 Hz) on both channels, the FIFO filled by
// polling. Prints "Configuring DAC" like dac_sin (tb_soc checks=dac_sin).
#include <SocAudio.h>

// round(sin(2 pi k / 109) * (2^23 - 1)). Not const on purpose: in .data, so
// crt0 copies it to DMEM. As const it stays in flash, and the loop's reads of
// it thrash the 1 KB XIP cache with the loop code (the FIFO then barely
// keeps up with the DAC). Not static either: GCC moves a static array that
// is never written to .rodata.
int32_t sine[109] = {
           0,   483284,   964963,  1443437,  1917115,  2384425,  2843814,  3293756,
     3732757,  4159358,  4572142,  4969738,  5350824,  5714136,  6058466,  6382671,
     6685673,  6966465,  7224116,  7457769,  7666648,  7850060,  8007394,  8138128,
     8241828,  8318150,  8366840,  8387736,  8380769,  8345961,  8283430,  8193381,
     8076115,  7932021,  7761577,  7565351,  7343993,  7098239,  6828905,  6536887,
     6223153,  5888747,  5534779,  5162425,  4772922,  4367564,  3947697,  3514716,
     3070060,  2615206,  2151664,  1680974,  1204700,   724425,   241743,  -241743,
     -724425, -1204700, -1680974, -2151664, -2615206, -3070060, -3514716, -3947697,
    -4367564, -4772922, -5162425, -5534779, -5888747, -6223153, -6536887, -6828905,
    -7098239, -7343993, -7565351, -7761577, -7932021, -8076115, -8193381, -8283430,
    -8345961, -8380769, -8387736, -8366840, -8318150, -8241828, -8138128, -8007394,
    -7850060, -7666648, -7457769, -7224116, -6966465, -6685673, -6382671, -6058466,
    -5714136, -5350824, -4969738, -4572142, -4159358, -3732757, -3293756, -2843814,
    -2384425, -1917115, -1443437,  -964963,  -483284,
};

static unsigned idx = 0;

void setup()
{
    Serial.begin(19200);
    Serial.println("Configuring DAC");
    Audio.begin();
    Serial.println("SinePolling: 109-sample sine, 440.37 Hz");
}

void loop()
{
    // Audio.write returns 0 once the FIFO is full; the rest of loop() could
    // do other work for up to 256 frames (5.3 ms).
    while (Audio.write(sine[idx])) {
        if (++idx == 109)
            idx = 0;
    }
}
