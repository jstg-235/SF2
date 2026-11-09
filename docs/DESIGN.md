# SF2: SoundField plugin replacement — research & design

Nov 9, 2026 · J Rock

Living version with diagrams: https://claude.ai/artifact/TSnRMPw3a1F9cQcgdj8mp9

## Summary

SF2 should be a single plugin that turns any tetrahedral (A-format) or first-order B-format recording into mono, stereo, surround, immersive, higher-order Ambisonic or binaural output, with quality at least equal to SoundField by RØDE and the directional sharpness of Harpex-X.

The three headline recommendations:

1. **Two decode engines in one plugin.** A clean linear engine (virtual mics and Ambisonic decoding, phase-coherent, low latency) for faithful work, and a parametric engine (direction plus diffuseness per time-frequency tile, in the Harpex/DirAC family) for sharper imaging, shotgun-style beams and upmixing to 3rd order.
2. **Measured, per-capsule calibration as a first-class feature.** Load per-serial-number capsule data, then convert A to B with frequency-dependent matrices derived from that data, not a single average filter. This is where most of the real audio quality is won or lost.
3. **A visual-first interface.** A live sound-field heat map with virtual mics dragged directly on it, a fixed output section that follows the chosen format, and automation-friendly parameters throughout.

The main open questions are licensing (free or paid), which mic models to support at launch, and whether a JUCE-based C++ stack is acceptable. They are listed at the end.

## Reference products

SoundField by RØDE wins on its A-to-B conversion and price (free); Harpex-X wins on output flexibility, binaural, higher-order output and its sound-field display.

| Area | SoundField by RØDE | Harpex-X |
| --- | --- | --- |
| Price | Free | Paid, 30-day trial, 2-computer licence |
| Inputs | A-format (NT-SF1 and SoundField mics), B-format FuMa, B-format AmbiX | AmbiX, FuMa, Double M/S, raw tetrahedral from Ambeo, SPS-200, NT-SF1, TetraMic, Zoom H3-VR |
| A-to-B method | Time-frequency adaptive: capsules phase-aligned per band before the matrix, no static correction filters | Standard matrix plus per-mic handling; 1.4 build 152 improved A-format channel separation |
| Decoding | Linear virtual mics; patterns from omni through cardioid to figure-8, plus beamformed "shotgun-type" patterns | Parametric "active matrix" (Harpex method), separates two plane waves per frequency |
| Outputs | Mono, Stereo, 5.1, 7.1, 5.1.2, 7.1.2, 5.1.4, 7.1.4, B-format FuMa and AmbiX | Binaural, Stereo (coincident and spaced), Shotgun (up to 8 virtual mics), Surround (up to 8 speakers), AmbiX up to 3rd order, Speakerdome (Atmos 9, Auro 9 to 13, THX 10/12, 22.0), up to 16 outputs |
| Stereo modes | Coincident virtual pair | Angle, Polar pattern, Distance; presets XY, Blumlein, AB, ORTF, DIN, NOS |
| Binaural | None built in | KU100 default HRTF, SOFA loading, EQ method (Diffuse, Horizontal, Front, None) |
| Field controls | Mic orientation, rotation, tilt and tumble (approximate, from memory of the UI) | 3D rotation, Horizontal dominance, Highpass, LFE cutoff, Shelf crossover, Output delay 0 to 100 ms |
| Display | Static mic diagram | Live sound-field map (equal-area projection, red/green/blue for low/mid/high frequency) |
| Formats | VST, AAX (Win/Mac), AU (Mac) | AAX, VST, VST3, AU, Apple silicon |

Gaps worth filling in both: no head tracking, no metering per output, no capsule calibration from measured data, no spatial noise or reverb control, and no visual undo or A/B snapshots. Sources: [RØDE on its A-to-B conversion](https://rode.com/en-de/about/news-info/demystifying-soundfield-by-rodes-a-format-to-b-format-conversion), [Newsshooter launch article](https://www.newsshooter.com/2018/09/18/rode-releases-new-soundfield-rode-plugin-ambisonic-editing/), [Harpex home page](https://harpex.net/), [Harpex-X manual](https://www.harpex.net/manual.pdf).

## Feature parity checklist

Version 1.0 ships only when every item below works; together they cover everything SoundField by RØDE does today.

- [ ] A-format input for NT-SF1 and SoundField SPS200/ST450-class mics, with frequency-dependent A-to-B conversion
- [ ] B-format input in FuMa (WXYZ, -3 dB W) and AmbiX (ACN/SN3D), with automatic channel-order and W-gain handling
- [ ] Mic orientation: upright, inverted, end-fire (and arbitrary angle)
- [ ] Whole-field rotation, tilt and tumble (yaw, pitch, roll)
- [ ] Virtual mics with a continuous pattern control from omni through cardioid to figure-8
- [ ] Higher-directivity "shotgun" beam beyond hypercardioid
- [ ] Per-virtual-mic azimuth, elevation, pattern and gain
- [ ] Mono and stereo outputs (coincident pairs with width/angle)
- [ ] Surround and immersive layouts: 5.1, 7.1, 5.1.2, 7.1.2, 5.1.4, 7.1.4
- [ ] B-format output in FuMa and AmbiX
- [ ] Full automation of every steering parameter, sample-accurate and zipper-free
- [ ] VST3, AU and AAX on macOS (Intel and Apple silicon) and Windows
- [ ] Hosts: Pro Tools, Logic Pro, Cubase/Nuendo, Reaper, Final Cut Pro (AU), DaVinci Resolve Fairlight (VST3)

## Improvements and new features

The highest-value additions are measured capsule calibration, a parametric engine with an adjustable blend, binaural with head tracking, and a live sound-field display; everything else builds on those.

| Priority | Feature | Why it matters | Inspired by |
| --- | --- | --- | --- |
| P1 | Per-capsule calibration profiles (load by mic serial, or measure your own) | Capsule mismatch is the biggest source of high-frequency spatial error | New |
| P1 | Linear / Parametric engine with a 0 to 100% "Sharpness" blend | Users pick faithful or focused imaging per track instead of per plugin | Harpex, DirAC |
| P1 | Binaural output with SOFA HRTF loading, headphone EQ and head tracking (OSC, MIDI, IEM/Supperware-style trackers) | Most immersive delivery is now on headphones; neither reference tracks heads | Harpex (SOFA) |
| P1 | Live sound-field heat map with virtual mics dragged on it | Aim mics at what you can see, not by numbers | Harpex display |
| P1 | Spaced and near-coincident stereo (AB, ORTF, NOS, DIN) with real time-of-arrival delays | Gives spaciousness a coincident pair cannot | Harpex Stereo mode |
| P2 | Upmix to 2nd and 3rd order AmbiX | Sharper decodes on large rigs and better binaural | Harpex |
| P2 | Direct/diffuse controls: "Room" and "Direct" faders, diffuse-to-surround routing | Dry up a boomy room or open up a tight one after the fact | DirAC |
| P2 | Source-follow: a virtual mic tracks the loudest source within a cone | Interviews, wildlife, moving performers | New |
| P2 | Spatial noise reduction: steer a null or a band-limited reject at a noise direction | Remove an air-con unit or road noise from one side | New |
| P2 | Horizontal dominance / zoom (Lorentz-style), front-back balance | Push the image forward without re-aiming every mic | SoundField hardware, Harpex |
| P2 | Snapshots A/B/C/D with morphing, plus undo history | Fast comparisons, automatable scene changes | New |
| P2 | Per-output metering, phase-correlation and LFE management (crossover, delay, trims) | Deliverables pass QC first time | Harpex LFE cutoff |
| P3 | Video overlay: show the heat map over a 360° or flat video frame | Aim by picture for VR and broadcast | New |
| P3 | Wind and handling detector with auto high-pass per capsule | Field recordings with one capsule hit by wind | New |
| P3 | Custom speaker layouts from a text/JSON file, up to 16 or 64 channels | Installations and unusual rooms | Harpex Speakerdome |
| P3 | Offline render mode with larger FFTs and look-ahead | Best possible quality on a bounce at zero cost in real time | New |

## DSP architecture

Audio quality comes from three choices: calibrated, frequency-dependent A-to-B conversion; a 64-bit, phase-linear processing path; and a parametric engine that is blended in, never forced.

```
Input stage (4 capsules: trim, polarity, wind HPF)
        |
A-to-B conversion (per-bin 4x4 matrix)  <--  Calibration profile (serial data or own sweep)
        |
Rotation and field shaping (yaw, pitch, roll, dominance, zoom)
        |
   +----+-----------------+
   |                      |
Linear engine        Parametric engine
(virtual mics,       (STFT, direction
 AllRAD)              and diffuseness)
   |                      |
   +----+-----------------+
        |
Engine blend (Sharpness 0-100%)
        |
Output stage (LFE, trims, delay, meters)
```

Every output, linear or parametric, is fed by the same calibrated A-to-B stage, so improving calibration improves every format at once.

### 1. Input stage

- Accept 4 channels at any host rate up to 192 kHz; process internally in 64-bit float.
- Per-capsule trim (±6 dB, 0.01 dB steps) and polarity, then a per-capsule 4th-order high-pass (off, 20 to 200 Hz) for wind.
- Format detection hint: warn when channel energy suggests FuMa was loaded as AmbiX or the channel order is wrong.

### 2. A-to-B conversion (the core of quality)

- Model the tetrahedral array physically: 4 capsules at radius r (about 1.47 cm for NT-SF1-class arrays; read from the mic profile), each with measured magnitude and phase per frequency.
- Compute a frequency-dependent 4×4 encoding matrix per FFT bin by regularised least squares against the ideal W, X, Y, Z responses, over a dense measured or modelled direction set (for example 2,000+ directions). This corrects both on-axis response and directivity, which a single average filter cannot.
- Regularise with Tikhonov weighting that rises with frequency, so noise gain stays under about +6 dB at the top where the array is spatially aliased (above roughly 7 to 9 kHz for a 1.5 cm radius).
- Above the aliasing frequency, fall back to an energy-preserving (diffuse-field equalised) solution, so timbre stays natural even when direction is ambiguous.
- Apply as minimum-phase-plus-delay or linear-phase FIR (user choice): linear phase for music and sound design, minimum phase for low-latency live use.
- Calibration sources, in order of preference: per-serial factory data, a user measurement made with SF2's built-in sweep and a turntable, then the model's generic profile.

### 3. Rotation and field shaping

- Yaw, pitch and roll as a 3×3 rotation on X, Y, Z (and its higher-order equivalents after upmixing), smoothed per sample to avoid zipper noise.
- Dominance/zoom as a Lorentz-style boost of one direction, gain-compensated so overall loudness does not jump.

### 4. Linear engine

- Each virtual mic is a weighted sum: p·W + (1−p)·(direction · [X, Y, Z]), with p from 1 (omni) to 0 (figure-8). Higher-order patterns (supercardioid up to the order available) come from max-rE weighting after upmix.
- Loudspeaker decoding uses AllRAD for irregular layouts and dual-band (max-rE above about 700 Hz, basic below) for first order.
- Fully phase-coherent: summing virtual mics back gives the original W exactly. This is the "reference" path.

### 5. Parametric engine

- Short-time Fourier transform, 2,048-point window, 50 to 75% overlap, Hann or Kaiser window at 48 kHz (scaled by sample rate). Offline render mode doubles the size.
- Per tile: estimate direction of arrival from the active intensity vector (Re{W·[X,Y,Z]\*}) and diffuseness from its ratio to energy, averaged over about 20 to 50 ms with frequency-dependent smoothing.
- Optional two-source model per tile (Harpex-style) for dense scenes.
- Render direct sound to the target (speakers via VBAP, binaural via HRTF, higher-order AmbiX via spherical harmonics) and diffuse sound through decorrelators to all outputs.
- Musical-noise control: a temporal and spectral smoothing control, a floor on the diffuse stream, and a "Sharpness" blend with the linear path. This is what keeps parametric processing from sounding watery.

### 6. Output stage

- LFE with a 4th-order Linkwitz-Riley crossover (40 to 200 Hz), +10 dB or 0 dB conventions, and bass management.
- Per-output trim, delay (0 to 100 ms) and solo/mute.
- True-peak metering per output, plus optional 2× oversampled soft limiting for binaural.
- Report latency to the host exactly; linear mode at 0 to 64 samples (minimum phase) or FIR length/2 (linear phase), parametric mode at the STFT length.

### Quality targets

| Measure | Target |
| --- | --- |
| Pattern accuracy (cardioid, 125 Hz to 8 kHz) | Within ±1.5 dB of ideal, 0 to 120° off-axis |
| On-axis frequency response, 20 Hz to 16 kHz | Within ±1 dB after calibration |
| Self-noise added by processing | Under −120 dBFS |
| CPU, linear mode, 7.1.4 at 48 kHz | Under 2% of one Apple M1 core |
| CPU, parametric mode, binaural at 48 kHz | Under 8% of one Apple M1 core |

## Interface design

The window is built around one large sound-field view where virtual mics are aimed by dragging; inputs sit on the left, outputs on the right, and the window resizes from 900×560 to 4K with vector graphics.

```
+--------------------------------------------------------------------------+
| Top bar: Presets · Snapshots A-D · Undo · Engine + Sharpness · Latency   |
+----------------+-----------------------------------+---------------------+
| Input          | Sound field                       | Output              |
| Mic + serial   | Live heat map, coloured by band   | Format picker       |
| Format         | Drag mic handles to aim           | Strip per channel:  |
| Orientation    | Yaw / pitch / roll rings          |  pattern, az, el,   |
| Capsule meters | Optional 360° video underlay      |  gain, solo, meter  |
| Trim, HPF      |                                   | Binaural: HRTF,     |
| Wind warning   |                                   |  tracker            |
+----------------+-----------------------------------+---------------------+
| Drawer: Field shaping · Direct/Room · LFE · Noise-reject · Source-follow |
+--------------------------------------------------------------------------+
```

### Layout

- **Top bar:** preset browser, snapshots A to D with a morph slider, undo/redo, engine switch (Linear / Parametric) with the Sharpness blend, latency readout, settings.
- **Left column, Input:** mic model and serial (loads calibration), input format (A-format, FuMa, AmbiX, Double M/S), orientation (Upright, Inverted, End-fire, Custom angle), four capsule meters with trim, polarity and wind high-pass, and a wind/handling warning light.
- **Centre, Sound field:** an equal-area or equirectangular map (toggle), coloured by energy and by frequency band, with optional 360° video underlay. Virtual mics show as handles whose shape shows their polar pattern; drag to aim, scroll to tighten, Alt-drag to change elevation. Speakers of the chosen layout show as fixed dots. Yaw, pitch and roll appear as three rings around the map.
- **Right column, Output:** output format picker (Mono, Stereo, Spaced stereo, 5.1 to 7.1.4, custom, AmbiX 1st to 3rd order, Binaural), then a strip per output channel with name, pattern, azimuth, elevation, gain, solo, mute and meter. Stereo adds Width and Spacing; binaural adds HRTF, headphone EQ and head-tracker source.
- **Bottom drawer (collapsible):** Field shaping (dominance, zoom, front/back), Direct/Room faders, LFE and bass management, noise-reject beams, source-follow.

### Interaction rules

- Every control has a numeric entry, double-click to reset, fine adjust with Shift, and a host-automatable parameter with a stable ID.
- Linked mode: moving one mic of a stereo or surround array moves the whole array; unlinked mode edits one mic.
- Solo-listen: hold a mic handle to hear only that virtual mic, for quick aiming.
- Colour-blind-safe palette for the heat map (viridis-like), and a high-contrast theme.
- Keyboard: arrow keys nudge the selected mic by 1°, Shift for 0.1°, numbers 1 to 4 recall snapshots.

### Key workflows

1. **Drop and go:** insert on a 4-channel track, choose mic and output format; SF2 picks a sensible default array and the job is done.
2. **Re-aim in post:** play the clip, watch the heat map, drag the mics onto the talent, automate in Touch or Latch.
3. **Deliver many formats:** one instance per deliverable, all sharing a linked calibration and rotation (an instance group), so stereo, 5.1 and binaural stay consistent.

## Formats, compatibility and stack

Build in C++20 on JUCE 8 so one codebase ships VST3, AU, AAX and a standalone app, with the DSP in a separate host-independent library that can be unit-tested and reused.

| Topic | Decision |
| --- | --- |
| Plugin formats | VST3, AU (incl. Logic surround), AAX (Pro Tools Ultimate multichannel), CLAP, standalone app for batch conversion |
| Platforms | macOS 12+ universal (arm64 + x86_64), Windows 10+ x64 |
| Channel layouts | In: 4 ch (A, FuMa, AmbiX, Double M/S). Out: 1, 2, 5.1, 7.1, 5.1.2, 7.1.2, 5.1.4, 7.1.4, 9.1.6, AmbiX 4/9/16 ch, binaural 2 ch, custom up to 16 (64 later) |
| Ambisonic conventions | AmbiX (ACN/SN3D) internally; FuMa, N3D and channel orders converted at the edges |
| DSP libraries | JUCE dsp module for FFT and FIR; libmysofa for SOFA HRTFs; Eigen for per-bin matrix solves (offline, at profile load, never on the audio thread) |
| Real-time safety | No allocation, locks or file I/O on the audio thread; matrices swapped with lock-free double-buffering |
| State | Parameters through the host's automation tree; calibration and custom layouts saved inside the session so projects open on any machine |
| Head tracking | OSC (port configurable), MIDI CC, and the standard Ambisonic yaw/pitch/roll parameters for host automation |
| Testing | Unit tests for matrices and rotations, null tests against reference decodes, pluginval in CI on every commit |

## Roadmap and open questions

Ship parity first as v1.0, then quality, then the parametric engine; each phase starts only when the gate before it passes.

| Phase | Scope | Gate to next phase |
| --- | --- | --- |
| 1. Core (v1.0) | Linear engine, A-to-B with stock profiles, all parity outputs, VST3/AU/AAX, automation and presets | Null tests pass |
| 2. Quality (v1.1) | Capsule calibration, sound-field heat map, binaural and SOFA, spaced stereo, metering and LFE | Blind A/B test |
| 3. Parametric (v2.0) | Parametric engine, Sharpness blend, upmix to 3rd order, Direct and Room, head tracking | Beta feedback |
| 4. Pro (v2.x) | Source-follow, noise-reject beams, video overlay, custom layouts, offline render | — |

The blind A/B gate compares SF2 against SoundField by RØDE on the same NT-SF1 recordings; v1.1 should match or beat it before the parametric work starts. No dates are set yet.

### Open questions

- [ ] Free, paid, or free core with a paid Pro tier?
- [ ] Which mics at launch: NT-SF1 and SPS200 only, or also Ambeo, TetraMic, Zoom H3-VR?
- [ ] Can we get per-serial capsule data from manufacturers, or do we rely on user measurement?
- [ ] Is JUCE (commercial licence for closed source) acceptable, or should the project be open source under GPL?
- [ ] Is AAX needed at v1.0? It requires an Avid developer account and PACE signing.
