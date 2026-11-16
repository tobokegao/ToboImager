[Tobo Imager](../README.md) › The stereoize types

<!-- Generated from manual/manual_en.md by tools/make_docs.py. Change the source, not this file. -->

## The stereoize types

All four make the side signal out of the mid, band by band, in the amount set by the STEREOIZE knobs. They differ in what is done to the mid on the way.

- **DELAY**: one copy of the mid, delayed by the DELAY time (2–30 ms). The classic method. Each ear hears a regular comb filter, and long delays are heard as an echo on sharp attacks. No latency.
- **VEL A** (the default): the mid goes through a 30 ms sequence of 30 sparse, decaying impulses (velvet noise) whose positions and levels were chosen for an even frequency response in each ear. No single echo and no regular comb. No latency.
- **VEL B**: the same with a faster decay: a shorter tail and sharper attacks, with a little less width in the lows. No latency.
- **FVN**: an all-pass filter. It turns the phase of the side by 90 degrees, so both ears keep the level of the mid at every frequency, and adds a random part whose size is set by SCATTER. The response of this filter spreads evenly before and after the sound, so the plugin has to delay everything: about 33–36 ms at 44.1–96 kHz. The DAW compensates for this delay during playback and rendering. For playing live through the plugin, use one of the other types.

With the FVN type the stereoize amounts are part of the filter, so a change of a STEREOIZE knob, a crossover or SCATTER rebuilds it; this may be heard as a small click while the sound runs.

### GUARD (transient guard)

At a new attack, GUARD turns down what goes into the stereoizer for the first tens of milliseconds, so the attack stays in the centre and only the body of the sound is spread. 0 % is off. It looks 1 ms ahead; this adds no latency with the DELAY and velvet types.

### SAFE

A band that is widened far enough has more side than mid, and its correlation goes below 0. SAFE prevents this for its band. It measures the levels of mid and side of the band (over about 30 ms). Then it turns down what the plugin adds, which is the WIDTH above 100 % and the STEREOIZE, by as much as needed to keep the side at or below the mid.

- It never makes a band narrower than it came in.
- With the FVN type it can only turn down the WIDTH part.
- It works per band: the correlation of the whole signal can still be lower than that of each band alone.

---

← [The panel](the-panel.md)   |   [Contents](../README.md#documentation)   |   [The stereo meters](the-stereo-meters.md) →
