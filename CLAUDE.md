# Repository guidance

## Project

FullSpectrum is an audio analysis plugin and standalone application, maintained in `Munnopsis/SpectrumAnalyzerPlugin`. It uses the Pamplejuce CMake template, C++23 and JUCE 8. The configured formats are Standalone, VST3 and CLAP.

The established bundle ID (`com.jakob.fullspectrum`), manufacturer code (`Jkob`), plugin code (`Fsp1`) and parameter IDs must remain stable for compatibility with existing sessions. The project is already configured; do not restart the template setup wizard.

## Build and test

The local IDE is CLion. Share its `cmake-build-debug` / `cmake-build-release` directories and use its configured Ninja executable when building from the terminal. Avoid changing the generator or Ninja version inside an existing build directory.

```sh
cmake -S . -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug --config Debug --parallel 2 \
  --target Tests Benchmarks FullSpectrum_Standalone FullSpectrum_VST3 FullSpectrum_CLAP
ctest --test-dir cmake-build-debug -C Debug --output-on-failure --no-tests=error
```

Debug is the development default. Use Release for performance measurements or regular audio use. Builds do not install plugins unless `FULLSPECTRUM_COPY_PLUGIN_AFTER_BUILD=ON`. Optional Intel IPP is off by default. Initialize dependencies with `git submodule update --init --recursive`; do not move submodules to upstream branch tips as part of an unrelated change.

Treat build warnings as errors to resolve before completion. Do not confuse incomplete clangd diagnostics with actual compiler failures. Run checks appropriate to the change and report untested platforms honestly.

## Architecture

- `source/PluginProcessor.*`: audio callback, parameters, saved state and references.
- `source/PluginEditor.*` and `source/ui/`: message-thread UI and visualization.
- `source/analyzer/`: FIFO, analysis engines, stereo/loudness meters and reference storage.
- `tests/`: Catch2 smoke and regression tests. Manual meter checks are in `docs/AnalyzerValidationTests.md`.
- `cmake/`, `JUCE/` and `modules/`: pinned submodules; changes need explicit dependency review.
- `SharedCode`: interface target linking the same project code into plugin and test targets.

## Audio and message threads

The audio callback must not allocate, block, lock, grow containers, perform I/O or call UI code. Allocate in constructors or `prepareToPlay`. Use atomics for simple shared values and the existing FIFO or a preallocated queue for larger data. Poll UI state from the message thread with a timer or async updater.

Validation signals are analyzer-only, disabled by default and reset on state restoration. Preserve transparent audio passthrough.

## Code and repository conventions

Use `.clang-format`: Allman braces, four spaces, no column limit. Preserve unrelated user edits. Add tests when they can catch a meaningful behavioral regression.

Use pinned Git submodules for JUCE modules and CPM for other C++ dependencies. Document third-party licenses. Keep plugin identities and saved-state compatibility stable.

Keep CI tokens minimally scoped, actions pinned to full SHAs, and checkout credentials disabled. Never add signing credentials to pull-request workflows. Do not enable automatic binary publishing without finishing the licensing and signing setup in `docs/MAINTAINING.md`.

Do not commit secrets, signing certificates, local `.env` files, builds, private audio, or personal paths. Use the configured repository-local Git identity. The root MIT license does not replace JUCE's AGPLv3/commercial licensing terms; see `THIRD_PARTY_NOTICES.md`.
