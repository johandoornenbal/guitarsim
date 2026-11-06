# GuitarSim

A JUCE VST3 interface for **ChimeyGuitar2**, **ChimeyGuitar**, and **ChimeyDeluxe** by [Airwindows](https://github.com/airwindows/airwindows). Built in the visual family of [ToTape Studio](https://github.com/johandoornenbal/tape) and [Wide](https://github.com/johandoornenbal/wide): cream faceplate, dark green chassis, gold rotary controls, and a speaker-cloth model badge.

**GuitarSim** is the name shown in your DAW. ChimeyGuitar2 is selected by default. Every algorithm keeps its own ten controls and original Airwindows defaults when you switch models and save your DAW session. There is no LUFS compensation, automatic gain adjustment, or level-learning system.

## Controls

- **Algorithm:** choose ChimeyGuitar, ChimeyGuitar2, or ChimeyDeluxe. Switching resets the entering engine's audio history to avoid carrying old filter/compressor state into a new audition.
- **Rotary controls:** drag or type a value from 0 to 1; double-click to restore that control's original default. Each model exposes its actual Airwindows parameters.
- **Reset this model:** restores only the selected algorithm's controls. Other algorithms retain their settings.
- **A: Original input / B: GuitarSim:** compare dry and processed audio with a 10 ms crossfade. This does not match their loudness. The engines continue running while listening to Original.
- **Output meters:** stereo sample peaks, with red segments at and above 0 dBFS. The display decays when playback stops. No limiter is applied.
- **Interface size:** 75%, 100%, or 125%, saved with the session.

ChimeyGuitar and ChimeyGuitar2 expose Compression, Presence, High, Mid, Low, Sub, High Speaker, Low Speaker, Speed, and Output. ChimeyDeluxe exposes Hell, Fizz, Pick, Satan, Danger, Critical, High Meat, Low Meat, Swing, and Rarely. Values use the upstream normalized scale; the UI does not invent dB or frequency units for these nonlinear controls.

Mono and stereo channel layouts and float/double processing are supported. Mono input is fed to both engine channels; the left output is returned. No fixed latency is added or reported. Internal filter/compressor phase and dynamics are part of each model's sound.

## Download and install

Open [Actions → Build GuitarSim](https://github.com/johandoornenbal/guitarsim/actions/workflows/build.yml), select a successful run, and download the artifact for your OS. Extract it and copy the entire **GuitarSim.vst3** bundle to your VST3 directory:

| OS | User VST3 location |
| --- | --- |
| Windows | `%LOCALAPPDATA%\Programs\Common\VST3` (or the system `C:\Program Files\Common Files\VST3`) |
| macOS | `~/Library/Audio/Plug-Ins/VST3` |
| Linux | `~/.vst3` |

Rescan plugins in your DAW. Builds are unsigned; macOS distribution is not notarized. GitHub requires you to be signed in to download workflow artifacts.

## Build

Requires CMake 3.22+, a C++17 compiler, and Git. CMake fetches JUCE 8.0.12 at a pinned commit, or accepts `-DJUCE_PATH=/path/to/JUCE`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Linux dependencies:

```sh
sudo apt-get install libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev xvfb
xvfb-run -a ctest --test-dir build --output-on-failure
```

The plugin is written to `build/GuitarSim_artefacts/Release/VST3/GuitarSim.vst3`. CI builds and tests on Windows, macOS, and Linux, and renders the actual JUCE editor for all three algorithms into the `GuitarSim-UI-previews` artifact.

For the DSP tests without JUCE or GUI dependencies:

```sh
cmake -S . -B build-dsp -DGUITARSIM_DSP_ONLY=ON
cmake --build build-dsp
ctest --test-dir build-dsp --output-on-failure
```

The DSP tests compare the ports against the unmodified upstream implementations at multiple rates and parameter endpoints, including smaller blocks, and check float silence. Plugin checks cover model routing, saved banks, dry comparison, mono processing, editor attachment rebinding, per-model reset, size selection, and rendering.

## Source and licensing

Airwindows DSP is MIT licensed. Unmodified reference sources and their license are under `third_party/airwindows`; the SDK-free DSP ports are under `Source`. Run `python3 scripts/port-engines.py` to reproduce the ports. DSP processing is unchanged; ports use deterministic dither seeds, omit the VST2 interface, and add bounds/finite checks to the parameter setter. Scratch audio buffers are allocated in `prepareToPlay`, not on the audio thread. Host blocks up to the greater of the announced maximum and 8192 samples preserve upstream block boundaries; oversized blocks are processed in bounded chunks.

GuitarSim is an independent interface, not an official Airwindows release. See [THIRD_PARTY.md](THIRD_PARTY.md) for provenance and JUCE licensing.
