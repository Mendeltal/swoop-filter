# Swoop Filter

Audio filter plugin (VST3 / AU / Standalone), built with JUCE.

- Filter: Low / High / Band / No Pass, slope 12–48 dB, resonance
- Autofilter: LFO (sine, tri, square, saw, random) modulating the cutoff
- FX rack: reverb, chorus, saturation, stereo spread, phaser
- Macro: the FX amount follows cutoff + resonance, with an FX Boost knob
- UI: neon dark theme, live filter-response curve with an animated cutoff dot

## Build
    git clone https://github.com/juce-framework/JUCE.git
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

Plugins land in `build/SwoopFilter_artefacts/`. Run the Standalone target to test quickly.

## Build in the cloud (no tools needed)
Push this folder to a GitHub repo -> Actions tab -> "Build VST3" -> download the artifact
for Windows or macOS when it finishes.
