# Credits

Copyright (C) 2026 tobokegao

Tobo Imager is free software, released under the GNU Affero General Public License, version 3 or later (see `LICENSE`).

It was made with help from Claude (Anthropic).

## Software it is built with

- **JUCE 9** by Raw Material Software Limited (used under the AGPLv3) - https://github.com/juce-framework/JUCE
- **VST 3 SDK** by Steinberg Media Technologies GmbH (MIT License), as shipped with JUCE - https://github.com/steinbergmedia/vst3sdk
  VST is a registered trademark of Steinberg Media Technologies GmbH.

The licenses of these and of the other libraries that JUCE compiles into the binary (zlib, libpng, the IJG JPEG software, HarfBuzz, SheenBidi, LunaSVG, PlutoVG, FLAC, libogg, libvorbis) are in `THIRD_PARTY_LICENSES.txt`.

## Papers the signal processing follows

No code from these papers or their authors is included; the methods were written from the descriptions.

- **Velvet-noise decorrelator** (the VEL A and VEL B types): B. Alary, A. Politis and V. Välimäki, “Velvet Noise Decorrelator”, Proc. 20th Int. Conf. on Digital Audio Effects (DAFx-17), Edinburgh, UK, 2017, pp. 405–411; S. J. Schlecht, B. Alary, V. Välimäki and E. A. P. Habets, “Optimized Velvet-Noise Decorrelator”, Proc. 21st Int. Conf. on Digital Audio Effects (DAFx-18), Aveiro, Portugal, 2018, pp. 87–94.
- **Frequency domain variant of velvet noise, FVN** (the random phase of the FVN type): H. Kawahara, K.-I. Sakakibara, M. Morise, H. Banno, T. Toda and T. Irino, “Frequency Domain Variants of Velvet Noise and Their Application to Speech Processing and Synthesis”, Proc. Interspeech 2018, Hyderabad, India, pp. 2027–2031.
- **Transient handling for decorrelators** (GUARD): O. Das, “An Open Source Stereo Widening Plugin”, Proc. 27th Int. Conf. on Digital Audio Effects (DAFx24), Guildford, UK, 2024. The transient guard follows the idea of the paper's transient detection, which keeps percussive sounds out of the decorrelation filters.
- **Delay-based pseudo-stereo** (the DELAY type): the classic method of Lauridsen and Schodder.

The idea of SAFE comes from the antiphase prevention found in other stereo imagers.

## Origin

Tobo Imager started as a JSFX effect for REAPER and was ported to C++. The output of the VST3 was compared with that of the JSFX in a null test.
