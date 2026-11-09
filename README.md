# SF2

An ambisonic plugin for tetrahedral (A-format) and B-format microphones: a free replacement for SoundField by RØDE.
See [docs/DESIGN.md](docs/DESIGN.md) for the research and design.

## Status

v1.0 core in progress: the linear engine.

- A-format input for RØDE NT-SF1, SoundField SPS200, Sennheiser AMBEO VR, Core Sound TetraMic and Zoom H3-VR (idealised capsule model; measured calibration comes in v1.1)
- B-format input in FuMa or AmbiX
- Mic orientation (upright, inverted, end-fire) and scene rotate, tilt and tumble
- Virtual mics from omni to figure-8 for mono, stereo and any surround or immersive host layout up to 16 channels (5.1, 7.1, 5.1.2, 7.1.2, 5.1.4, 7.1.4, ...), with LFE low-pass
- First-order B-format output in AmbiX or FuMa
- VST3 and Standalone on all platforms, AU on macOS. AAX later.
- The editor is JUCE's generic parameter view until the sound-field UI is built.

## Layout

| Path | What |
| --- | --- |
| `dsp/` | Host-independent DSP library (`sf2dsp`), plain C++20, no JUCE |
| `tests/` | GoogleTest unit tests for the DSP |
| `plugin/` | JUCE 9 plugin wrapper: parameters, bus layouts, state |
| `docs/DESIGN.md` | Research and design |

## Building

Requires CMake 3.22+ and a C++20 compiler. JUCE 9.0.5 and GoogleTest are fetched automatically.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release
```

Options: `-DSF2_BUILD_PLUGIN=OFF` builds only the DSP library and tests; `-DSF2_JUCE_PATH=/path/to/JUCE` uses a local JUCE checkout.

On Linux, install JUCE's dependencies first:

```sh
sudo apt-get install libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev \
  libxcursor-dev libxext-dev libxi-dev libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev
```

Plugins are written to `build/plugin/SF2_artefacts/Release/`.

## Conventions

AmbiX (ACN channel order, SN3D) internally. +x front, +y left, +z up; azimuth is positive to the left.
Positive yaw moves a front source to the left, positive pitch moves it up, positive roll moves a left source up.
