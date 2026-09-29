# Contributing to FullSpectrum

Bug reports, focused fixes and documentation improvements are welcome. For substantial changes, open an issue explaining the problem and proposed behavior before starting.

## Report a bug

Include the FullSpectrum commit/version, operating system and architecture, DAW/version or standalone mode, plugin format, sample rate, buffer size and reproduction steps. Describe the expected and actual behavior. Use generated audio or a small sample you are allowed to share.

Review logs and screenshots for private file paths, names and project information. Report security vulnerabilities privately using [SECURITY.md](SECURITY.md).

## Build and test

Follow the [README build instructions](README.md#build-from-source). Keep submodules at the committed revisions. Use Debug for development and Release when measuring performance.

```sh
cmake --build cmake-build-debug --config Debug --parallel 2 \
  --target Tests Benchmarks FullSpectrum_Standalone FullSpectrum_VST3 FullSpectrum_CLAP
ctest --test-dir cmake-build-debug -C Debug --output-on-failure --no-tests=error
```

On headless Linux, run CTest through `xvfb-run -a`. For changes to stereo analysis or metering, also use the [manual validation guide](docs/AnalyzerValidationTests.md). Fix compiler warnings encountered in changed code; investigate dependency warnings instead of hiding them globally.

## Code conventions

- C++23, four-space indentation and Allman braces; follow `.clang-format`.
- Keep the audio callback free of allocation, locks, blocking I/O and UI calls. Allocate in construction or `prepareToPlay`.
- Pass simple state through atomics and larger audio data through the existing FIFO. UI updates belong on the message thread.
- Keep plugin IDs and parameter IDs stable; changing them can break existing DAW sessions.
- Add regression tests for behavior changes where they can catch a real failure.
- Avoid mixing dependency upgrades or unrelated formatting into a focused fix.

## Pull requests

Explain the problem, resulting behavior and how you validated it. Include a screenshot for visible UI changes when useful. Mention any platform or host you could not test. CI must pass before merging; a passing smoke test does not establish DSP correctness.

Do not commit access tokens, signing certificates, `.env` files, private recordings, generated builds or installers. The ignore rules are a convenience, not a security boundary. Use GitHub's noreply address if you want to keep your personal email out of future commits.

Keep discussions respectful and technical. Harassment, threats and disclosure of another person's private information are not acceptable.

Contributions are provided under the project's existing [MIT license](LICENSE), subject to the separate licenses of any third-party code. Do not add code or assets unless you have the right to contribute them.
