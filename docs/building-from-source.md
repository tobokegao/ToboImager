[Tobo Imager](../README.md) › Building from source

<!-- Generated from manual/manual_en.md by tools/make_docs.py. Change the source, not this file. -->

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

---

← [Troubleshooting](troubleshooting.md)   |   [Contents](../README.md#documentation)   |   [License and credits](license-and-credits.md) →
