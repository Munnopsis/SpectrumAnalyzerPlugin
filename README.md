# FullSpectrum

**A spectrum analyzer for seeing what is happening in your audio.**

[![Build and test](https://github.com/Munnopsis/SpectrumAnalyzerPlugin/actions/workflows/build_and_test.yml/badge.svg)](https://github.com/Munnopsis/SpectrumAnalyzerPlugin/actions/workflows/build_and_test.yml)
[![Security](https://github.com/Munnopsis/SpectrumAnalyzerPlugin/actions/workflows/security.yml/badge.svg)](https://github.com/Munnopsis/SpectrumAnalyzerPlugin/actions/workflows/security.yml)

FullSpectrum combines live frequency analysis, reference curves, stereo analysis and loudness metering in a JUCE plugin and standalone application. Use it to inspect a mix, compare channels, or compare incoming audio with a reference track.

> **Development status:** early development, version `0.0.1`. Frequency-dependent and VQT-like analysis are experimental. Metering has not been certified for broadcast compliance. Build from source; the CI checks builds and tests but does not publish signed installers.

[Features](#features) · [Quick start](#quick-start) · [Build from source](#build-from-source) · [Contributing](CONTRIBUTING.md) · [Security](SECURITY.md) · [Licensing](#licensing)

## Features

| Area | What you can do |
| --- | --- |
| Spectrum | View Live, RMS, Energy and Peak Hold curves on a logarithmic frequency axis, from 20 Hz to the lower of 20 kHz and Nyquist. |
| Analysis | Choose fixed FFT sizes from 1,024 to 32,768 samples, frequency-dependent modes, or the experimental VQT-like mode. |
| Display | Adjust averaging, peak decay, dB range, frequency weighting and smoothing, including octave, third-octave and sixth-octave views. |
| Channels | Analyze Stereo Sum, Left, Right, Mid or Side; compare both channels in L/R Dual and M/S Dual views. |
| References | Capture up to eight snapshots, name them, toggle their visibility and show a difference curve against the active reference. |
| Audio import | Drag WAV or AIFF files onto the analyzer to create spectral references from sampled sections of the files. |
| Stereo | Inspect correlation, balance, width, a phase scope and 31-band frequency correlation. |
| Loudness | View momentary, short-term and integrated loudness, with resettable metering. |
| Validation | Use built-in analyzer-only signals to check channel and metering behavior. |

### Formats

The project currently builds **VST3**, **CLAP** and **Standalone** targets. Audio Unit, AAX and AUv3 are not configured. The CI matrix covers macOS, Windows and Linux; host compatibility still needs testing in individual DAWs.

## Quick start

1. Build the desired target using the instructions below.
2. Open the standalone application, or install the plugin bundle in a folder scanned by your DAW.
3. In a DAW, insert FullSpectrum on an audio track or bus and start playback. The plugin analyzes the input while passing audio through.
4. Start with **Stereo Sum** and the **Live** curve. Adjust FFT size and RMS averaging to balance frequency detail and response time.
5. Use **Freeze** to capture a reference, or drag a WAV/AIFF file onto the display. Select a reference and enable **Difference** to compare it with the current curve.
6. Enable the stereo or loudness panels when you need more detail. Tooltips explain the controls.

The standalone application needs an audio input device. On macOS, grant microphone permission when prompted. Validation signals feed the analyzer only and should never appear at the audio output; they are disabled by default and reset when a saved state is loaded.

## Build from source

### Requirements

- Git, including recursive submodule checkout.
- CMake **3.25 or newer** and a C++23-capable compiler.
- Ninja for the commands below.
- macOS: Xcode Command Line Tools or Xcode.
- Windows: Visual Studio 2022 with **Desktop development with C++**; run commands in a developer terminal.
- Linux: a recent Clang/GCC toolchain and the JUCE system libraries below.

On Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends -y \
  build-essential clang cmake ninja-build git \
  libasound2-dev libx11-dev libxinerama-dev libxext-dev \
  libxrandr-dev libxcursor-dev libfreetype6-dev libfontconfig1-dev \
  libgl1-mesa-dev xvfb xauth
```

### Clone and build

```sh
git clone --recurse-submodules https://github.com/Munnopsis/SpectrumAnalyzerPlugin.git
cd SpectrumAnalyzerPlugin

cmake -S . -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug --config Debug --parallel 2 \
  --target FullSpectrum_Standalone FullSpectrum_VST3 FullSpectrum_CLAP Tests Benchmarks
ctest --test-dir cmake-build-debug -C Debug --output-on-failure --no-tests=error
```

For an existing checkout, initialize the **committed** dependency revisions with:

```sh
git submodule update --init --recursive
```

Configuration downloads CPM.cmake and Catch2, so the first build needs internet access. JUCE and the other submodules are pinned by Git. Do not use `git submodule update --remote` for a reproducible build.

On a Linux machine without a display, prefix the test command with `xvfb-run -a`.

### CLion and Xcode

CLion can open the root `CMakeLists.txt`. Use `cmake-build-debug` / `cmake-build-release` for its profiles. When sharing a build directory between CLion and a terminal, select the same Ninja executable; pass `-DCMAKE_MAKE_PROGRAM=/path/to/clion/ninja` on the first configure if needed.

For Xcode, use a separate directory:

```sh
cmake -S . -B Builds/Xcode -G Xcode
cmake --build Builds/Xcode --config Debug --target FullSpectrum_Standalone
```

### Release builds

Debug is the default for development. For regular audio work or CPU measurements, use Release:

```sh
cmake -S . -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --config Release --parallel 2 \
  --target FullSpectrum_Standalone FullSpectrum_VST3 FullSpectrum_CLAP
```

For a universal macOS build, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` when configuring a fresh build directory.

### Build outputs and installation

Outputs are under:

```text
cmake-build-debug/FullSpectrum_artefacts/Debug/
├── Standalone/   # FullSpectrum.app, FullSpectrum.exe, or FullSpectrum
├── VST3/         # FullSpectrum.vst3
└── CLAP/         # FullSpectrum.clap
```

Release builds use the corresponding `cmake-build-release/.../Release/` path. Copy the complete plugin bundle, then rescan plugins in your DAW. Common per-user locations on macOS are `~/Library/Audio/Plug-Ins/VST3/` and `~/Library/Audio/Plug-Ins/CLAP/`. On Linux, use `~/.vst3/` and `~/.clap/`. On Windows, use the plugin folder configured in your host, or the system VST3/CLAP folders under `C:\Program Files\Common Files\`.

Building does not install or overwrite plugins by default. To enable JUCE's automatic local copy, configure with `-DFULLSPECTRUM_COPY_PLUGIN_AFTER_BUILD=ON`. Intel IPP is optional and off by default; `-DFULLSPECTRUM_USE_IPP=ON` enables the template's platform-specific IPP integration.

## Privacy and saved data

FullSpectrum's application code processes audio locally and has no telemetry or upload feature. JUCE web browser and cURL support are disabled in this build. Dependency downloads happen at build time.

DAW state stores plugin parameters and reference curves, including reference names. Imported references initially use the audio filename without its extension. Rename references before sharing a session if those names reveal private project or client information.

## Development

```text
source/
├── PluginProcessor.*          Audio processing, parameters and saved state
├── PluginEditor.*             Controls and reference import
├── analyzer/                  Analysis engines, FIFO, meters and references
└── ui/SpectrumDisplay.*       Spectrum drawing and interaction
tests/                         Catch2 tests
benchmarks/                    Benchmark scaffolding
cmake/                         Pamplejuce CMake modules (submodule)
JUCE/                          JUCE framework (submodule)
modules/                       CLAP extensions and Melatonin Inspector
.github/workflows/             Builds and security checks
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for development conventions and [the manual validation guide](docs/AnalyzerValidationTests.md) for stereo and metering checks. The C++ test suite is a smoke-test suite, not a complete DSP accuracy or host compatibility test.

## Security

Please report vulnerabilities through [GitHub's private reporting form](https://github.com/Munnopsis/SpectrumAnalyzerPlugin/security/advisories/new), following [SECURITY.md](SECURITY.md). Use [public issues](https://github.com/Munnopsis/SpectrumAnalyzerPlugin/issues) for ordinary bugs and feature requests.

CI uses actions pinned to commit hashes, restricted token permissions and checkout without persisted credentials. Security checks include Gitleaks history scanning and CodeQL for C++ and GitHub Actions. See [the maintainer guide](docs/MAINTAINING.md) for repository controls and release preparation.

## Licensing

Project-owned source and the Pamplejuce template are available under the [MIT license](LICENSE). Dependencies retain their own licenses; **the MIT license does not make a compiled JUCE application MIT-only**.

JUCE 8 is dual-licensed under **AGPLv3** and the **commercial JUCE license**. Distribution must follow the applicable JUCE licensing path and all included dependency terms. A commercial JUCE license may be required for distribution that does not comply with AGPLv3. See [JUCE's license](JUCE/LICENSE.md), [the official JUCE terms](https://juce.com/legal/juce-8-licence/) and [third-party notices](THIRD_PARTY_NOTICES.md) before distributing binaries.

## Credits

Built with [JUCE](https://github.com/juce-framework/JUCE), using the [Pamplejuce](https://github.com/sudara/pamplejuce) template and CMake modules by Sudara Williams. The project also uses [CLAP JUCE extensions](https://github.com/free-audio/clap-juce-extensions), [Melatonin Inspector](https://github.com/sudara/melatonin_inspector) and [Catch2](https://github.com/catchorg/Catch2).
