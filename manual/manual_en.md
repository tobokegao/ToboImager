# Tobo Imager — stereo imager with one to four bands and a mono-compatible stereoizer

Tobo Imager is a free VST3 effect for Windows. It splits the sound into up to four frequency bands and sets the stereo width of each band. It can also make a mono sound wide (stereoize), and the mono sum of the result keeps the same spectrum as the mono sum of the input.

![Tobo Imager](docs/img/screenshot.png)

- **Format**: VST3, 64-bit Windows, stereo in and out
- **Price**: free
- **License**: AGPL-3.0-or-later (the source is in this repository)

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

## Install

1. Download `ToboImager_{VERSION}_win64.zip` from the [latest release](https://github.com/tobokegao/ToboImager/releases/latest) and unzip it.
2. Copy the folder `Tobo Imager.vst3` (the whole folder, not only the file inside it) to `C:\Program Files\Common Files\VST3\`.
3. Start your DAW and let it scan for new plugins. In REAPER: `Options` > `Preferences` > `Plug-ins` > `VST` > `Re-scan`.
4. The plugin is listed as **Tobo Imager** (by Tobokegao).

To update, close the DAW and replace the folder with the new one. To uninstall, delete the folder. The plugin writes nothing else to the PC: its settings are saved in the project of the DAW.

The plugin needs no installer and no extra runtime. It is not code-signed.

## Quick start

1. Put Tobo Imager on a stereo track or bus.
2. Choose the number of bands with **BANDS** and drag the crossover handles to where the bands should meet.
3. To widen a band that already has stereo content, turn its **WIDTH** above 100 %. To narrow it (for example the lows), turn it below 100 %; 0 % is mono.
4. To give width to a mono sound, turn up **STEREOIZE** on the bands that should be wide. Leave the LOW band at 0 % if the bass should stay in the centre.
5. Watch the stereo meter and **CORR**. If CORR goes below 0, switch on **SAFE** on the bands that are widened most, or turn the knobs down.
6. Press **MID** in MONITOR to hear what a mono listener gets.

## The panel

![The parts of the panel](docs/img/panel_map.png)

| No. | Part | What it does |
|---|---|---|
| 1 | I / O dot meter | The level of the side relative to the mid, going in (`I`, left column) and coming out (`O`, right column). One dot is 3 dB; the top dot is +6 dB (side louder than mid), the third dot from the top is 0 dB. |
| 2 | Spectrum display | The spectrum of the output: the mid as a line, the side as a grey fill. Behind it, each band is a coloured block as high as its WIDTH (full height = 200 %), with a line across it at its STEREOIZE amount (full height = 100 %). A band that is switched off, muted or not soloed is shown pale. |
| 3 | Crossover handles | The frequencies where the bands meet: one fewer than the number of bands. Drag a handle (showing its frequency) sideways. The handles stay at least half an octave apart from each other and from the two ends of the range (20 Hz and 20 kHz), so a crossover can be set from 29 Hz to 14.1 kHz. A handle that is dragged against its neighbor pushes the neighbor along. |
| 4 | MID / SIDE legend | Shows how the two curves of the spectrum are drawn. |
| 5 | Band header | The name of the band. With four bands: LOW, LO MID, HI MID, HIGH. With three: LOW, MID, HIGH. With two: LOW, HIGH. With one: FULL. Click the header to switch the band off or on. A band that is off passes audio through unchanged (WIDTH 100 %, no STEREOIZE). |
| 6 | WIDTH | The side level of the band, 0–200 %. The knob has the colour of the band. |
| 7 | SAFE | Holds the side of this band at or below its mid (see “SAFE” below). |
| 8 | STEREOIZE | How much side is made out of the mid of this band, 0–100 %. |
| 9 | M / S | Mute and solo of the band. Only one band can be soloed at a time. A soloed band is heard even if it is muted. |
| 10 | MONITOR | STEREO = the normal output. MID = only the mid (what a mono sum gives). SIDE = only the side. DELTA = only what the settings add or take away: the output minus what would come out with every band at WIDTH 100 %, no STEREOIZE and nothing muted. With nothing changed, DELTA is silent. Click MID, SIDE or DELTA again to go back to STEREO. |
| 11 | STEREOIZE TYPE | DELAY, VEL A, VEL B or FVN (see “The stereoize types” below). |
| 12 | DELAY / SCATTER / GUARD | DELAY: the delay time of the DELAY type, 2–30 ms. SCATTER: the size of the random part of the FVN type. GUARD: the transient guard, for every type. A knob the chosen type does not use is pale and cannot be turned. |
| 13 | Meter buttons | The kind of stereo meter: GONIO, POLAR or LEVEL. |
| 14 | Stereo meter | See “The stereo meters” below. |
| 15 | CORR and correlation bar | The correlation of left and right: +1 = mono, 0 = side and mid equally loud, below 0 = more side than mid. The bar shows the same value from the middle (0) to the right (+1) or left (−1). |
| 16 | LATENCY | The delay the plugin reports to the DAW. 0 ms except with the FVN type. |
| 17 | BANDS | The number of bands, 1 to 4. With fewer bands, the highest bands are removed and the top band that remains reaches up to 20 kHz. The remaining bands keep their settings. The settings of a removed band are kept and come back with the band. The filters of a crossover that is not in use are taken out of the signal path. When a band is added and there is no room above the highest crossover, that crossover and any below it move down to make room. |

### Mouse

- **Knob**: drag up or down. Hold `Shift` for fine control. Double-click (or `Ctrl`+click) resets it to the default value.
- **Crossover handle**: drag the line or its frequency label left or right. A handle that reaches its neighbor pushes the neighbor along. When the handle is dragged back, the neighbor stays where it was pushed to.
- **Right-click anywhere**: the size menu (1x, 1.25x, 1.5x, 2x) and the version number. The size is saved in the project.

When the plugin is bypassed, or the DAW has stopped processing it, the whole panel is shown pale and the meters are empty. The controls still work.

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

## The stereo meters

![The three stereo meters](docs/img/meters.png)

- **GONIO**: every sample as a dot. Up–down is the mid, left–right is the side. A mono sound is a vertical line; a wide sound fills the diamond; a sound lying flat has more side than mid.
- **POLAR**: the same dots folded into a half disc. Straight up is mono, 45 degrees is hard left or right, and dots near the floor have more side than mid.
- **LEVEL**: one ray for each short piece of the sound. The angle shows where the piece sits between left and right, the length how loud it is.

The picture is scaled to the loudest point, so it shows the shape of the sound, not its level.

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

## Latency

| Stereoize type | Latency |
|---|---|
| DELAY, VEL A, VEL B | 0 |
| FVN | 1579 samples at 44.1 kHz (35.8 ms), 1583 samples at 48 kHz (33.0 ms), 3167 samples at 96 kHz (33.0 ms) |

The plugin reports the latency to the DAW, and the DAW compensates for it. Changing to or from the FVN type changes the reported latency; some DAWs interrupt the sound for a moment when that happens.

## Tested environment and known limits

- **Tested on**: Windows 11, REAPER 7.85, 48 kHz, display scaling 100 %.
- **Not tested**: other DAWs, display scaling other than 100 %, sample rates other than 48 kHz.
- A crossover is kept below 0.45 times the sample rate. At 44.1 kHz and above, it can be set up to at least 19.8 kHz.
- The sound was checked by measurement: the output was compared with the JSFX version the plugin was ported from, and the difference was below −190 dB across 15 settings.
- There are no presets in the plugin. Use the preset function of the DAW.
- With the FVN type, the filter is rebuilt on the audio thread when a setting changes (see above).
- The labels use the font Meiryo UI, which is part of Windows.

If something does not work, please open an [issue](https://github.com/tobokegao/ToboImager/issues) with the name and version of your DAW.

## Troubleshooting

| Symptom | What to check |
|---|---|
| The DAW does not list the plugin | The whole folder `Tobo Imager.vst3` must be in `C:\Program Files\Common Files\VST3\`. Re-scan the plugins. The plugin is 64-bit only. |
| In mono the sound is much quieter than in stereo | A mono sum drops the side and keeps the mid, which the plugin does not change. If the side is louder than the mid (CORR below 0), most of what is heard in stereo is missing in mono. Switch on SAFE on the widened bands, or turn WIDTH and STEREOIZE down. |
| STEREOIZE does nothing | The band is switched off (band header), or MONITOR is on MID. STEREOIZE needs a mid signal: a sound that is only in the side has nothing to work with. |
| The sound comes late | The FVN type is chosen and the track is played live. Choose DELAY, VEL A or VEL B. |
| The panel is pale and the meters are empty | The plugin is bypassed, or the DAW is not processing it (some DAWs stop processing silent tracks). |
| The window is too small or too large | Right-click the panel and choose a size. |

## Building from source

You need Visual Studio 2022 or later (or the Build Tools) with the C++ workload and CMake.

```
git clone https://github.com/tobokegao/ToboImager.git
cd ToboImager
git clone --depth 1 --branch 9.0.5 https://github.com/juce-framework/JUCE.git
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

The plugin is then in `build/ToboImager_artefacts/Release/VST3/Tobo Imager.vst3`.

- `Source/Engine.h`: the signal processing.
- `Source/PluginProcessor.*`: the parameters, the saved state and the latency report.
- `Source/PluginEditor.*`: the panel. It is drawn pixel by pixel into an image.
- `test/panel_shot.cpp`: builds `PanelShot.exe`, which draws the panel at the four sizes to PNG files and works the controls with a simulated mouse, without a DAW.
- `test/stress.cpp`: builds `StressTest.exe`, which feeds the plugin unusual sample rates, invalid input values and corrupted saved states, operates the controls with random mouse movements, and checks that nothing goes wrong.
- `manual/` and `tools/make_docs.py`: the source of this manual and the script that makes `README.md`, `README_ja.md`, `docs/` and the PDF files from it.

## License and credits

Tobo Imager is free software under the GNU Affero General Public License, version 3 or later (`LICENSE`). Copyright (C) 2026 tobokegao.

It is built with the JUCE framework (AGPLv3) and the VST 3 SDK (MIT License). VST is a registered trademark of Steinberg Media Technologies GmbH. The papers the stereoizers are based on, and the licenses of the libraries in the binary, are listed in `CREDITS.md` and `THIRD_PARTY_LICENSES.txt`.

## Version history

### 0.1.1 ({DATE})

- Fixed the power symbol on the band headers. At the 2x size, its vertical bar sat half a pixel to the left of the centre of the ring. At the 1.25x, 1.5x and 2x sizes, the bottom of the ring was fainter than the rest.

### 0.1.0 (2026-11-17)

- First release.
