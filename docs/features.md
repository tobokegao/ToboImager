[Tobo Imager](../README.md) › Features

<!-- Generated from manual/manual_en.md by tools/make_docs.py. Change the source, not this file. -->

## Features

- **One to four bands**: choose the number of bands with BANDS. Drag the crossover points on the spectrum display. They keep at least half an octave between them, and a crossover that is dragged against the next one pushes it along. Each band has its own WIDTH and STEREOIZE knob.
- **WIDTH (0–200 %)**: the level of the side signal (the difference between left and right) of that band. 0 % makes the band mono, 100 % leaves it as it is, 200 % doubles the side (+6 dB).
- **STEREOIZE (0–100 %)**: makes a side signal out of the mid signal (the sum of left and right) of that band, so a mono sound gets width. The added signal has opposite polarity in the left and right channels, so it cancels when the two channels are summed to mono.
- **Four stereoize types**: DELAY (one delayed copy), VEL A and VEL B (velvet noise: many sparse echoes instead of one), and FVN (an all-pass filter, the smoothest of the four; it adds latency).
- **The mid is never changed in level**: no setting changes the level of the mid signal. Only the phase shift of the crossover filters is added, the same on mid and side.
- **SAFE (per band)**: holds the side of the band at or below the level of its mid, so if the input correlation of the band is 0 or above, it does not go below 0 however far the band is widened.
- **GUARD**: keeps the first tens of milliseconds of a new attack out of the stereoizer, so attacks stay in the centre.
- **Listen to the parts**: solo or mute each band, and listen to the mid alone, the side alone, or only what the settings change (MONITOR).
- **Meters**: the spectrum of mid and side, three kinds of stereo meter (GONIO, POLAR, LEVEL), the correlation, and the side-to-mid ratio going in and coming out.
- **Four window sizes**: 1x, 1.25x, 1.5x and 2x, from the right-click menu.

---

[Contents](../README.md#documentation)   |   [Install](install.md) →
