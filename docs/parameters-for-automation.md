[Tobo Imager](../README.md) › Parameters for automation

<!-- Generated from manual/manual_en.md by tools/make_docs.py. Change the source, not this file. -->

## Parameters for automation

These are the names the DAW shows.

| Parameter | Range | Default |
|---|---|---|
| Bands | 1, 2, 3, 4 | 4 |
| Crossover 1 / 2 / 3 | 20–20000 Hz each | 120 / 1000 / 5000 Hz |
| Band 1 / 2 / 3 / 4 Width | 0–200 % | 100 % |
| Band 1 / 2 / 3 / 4 Stereoize | 0–100 % | 0 % |
| Stereoize Delay | 2–30 ms | 12 ms |
| Stereoize Type | Delay, Velvet A, Velvet B, FVN | Velvet A |
| Transient Guard | 0–100 % | 0 % |
| FVN Scatter | 0–100 % | 25 % |
| Solo Band | Off, Band 1, Band 2, Band 3, Band 4 | Off |
| Monitor | Stereo, Mid, Side, Delta | Stereo |
| Band 1 / 2 / 3 / 4 On | off, on | on |
| Band 1 / 2 / 3 / 4 Mute | off, on | off |
| Band 1 / 2 / 3 / 4 Safe | off, on | off |

Band 1 is the lowest band. A DAW can set each crossover to any value from 20 Hz to 20 kHz; the half-octave spacing applies only to the handles on the panel. With fewer than four bands, the parameters of the bands and crossovers that are not in use have no effect. If a DAW sets the crossovers in use out of order, the plugin uses them in ascending order. A crossover that is changed moves to its new frequency over a few hundredths of a second, rather than switching all at once.

The window size and the kind of stereo meter are saved in the project but are not parameters.

---

← [The stereo meters](the-stereo-meters.md)   |   [Contents](../README.md#documentation)   |   [Latency](latency.md) →
