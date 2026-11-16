[Tobo Imager](../README.md) › Troubleshooting

<!-- Generated from manual/manual_en.md by tools/make_docs.py. Change the source, not this file. -->

## Troubleshooting

| Symptom | What to check |
|---|---|
| The DAW does not list the plugin | The whole folder `Tobo Imager.vst3` must be in `C:\Program Files\Common Files\VST3\`. Re-scan the plugins. The plugin is 64-bit only. |
| In mono the sound is much quieter than in stereo | A mono sum drops the side and keeps the mid, which the plugin does not change. If the side is louder than the mid (CORR below 0), most of what is heard in stereo is missing in mono. Switch on SAFE on the widened bands, or turn WIDTH and STEREOIZE down. |
| STEREOIZE does nothing | The band is switched off (band header), or MONITOR is on MID. STEREOIZE needs a mid signal: a sound that is only in the side has nothing to work with. |
| The sound comes late | The FVN type is chosen and the track is played live. Choose DELAY, VEL A or VEL B. |
| The panel is pale and the meters are empty | The plugin is bypassed, or the DAW is not processing it (some DAWs stop processing silent tracks). |
| The window is too small or too large | Right-click the panel and choose a size. |

---

← [Tested environment and known limits](tested-environment-and-known-limits.md)   |   [Contents](../README.md#documentation)   |   [Building from source](building-from-source.md) →
