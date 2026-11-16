[Tobo Imager](../README.md) › Tested environment and known limits

<!-- Generated from manual/manual_en.md by tools/make_docs.py. Change the source, not this file. -->

## Tested environment and known limits

- **Tested on**: Windows 11, REAPER 7.85, 48 kHz, display scaling 100 %.
- **Not tested**: other DAWs, display scaling other than 100 %, sample rates other than 48 kHz.
- A crossover is kept below 0.45 times the sample rate. At 44.1 kHz and above, it can be set up to at least 19.8 kHz.
- The sound was checked by measurement: the output was compared with the JSFX version the plugin was ported from, and the difference was below −190 dB across 15 settings.
- There are no presets in the plugin. Use the preset function of the DAW.
- With the FVN type, the filter is rebuilt on the audio thread when a setting changes (see above).
- The labels use the font Meiryo UI, which is part of Windows.

If something does not work, please open an [issue](https://github.com/tobokegao/ToboImager/issues) with the name and version of your DAW.

---

← [Latency](latency.md)   |   [Contents](../README.md#documentation)   |   [Troubleshooting](troubleshooting.md) →
