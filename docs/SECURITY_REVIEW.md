# Repository review — 2026-09-29

## Scope

Review of the public repository's configuration, documentation, locally reachable Git history and current project files. This is a repository hardening review, not a comprehensive audit of audio parsers, DSP, operating-system security or all third-party source code.

## Applied on GitHub

- Secret Scanning and Push Protection enabled.
- Private vulnerability reporting enabled.
- Dependabot alerts and security updates enabled.
- `main` protected against deletion and force pushes.
- Approval required for workflows from all external contributors.
- Existing read-only default workflow permissions and disabled workflow approval of pull requests verified.

The repository's Secret Scanning and Dependabot alert lists were empty when checked. Those snapshots do not establish full dependency or historical coverage.

## Changes in this checkout

- Replaced the template README with FullSpectrum usage, build, privacy and licensing documentation.
- Added security policy, contribution guidance, issue forms, pull-request template and third-party notices.
- Replaced the template CI's signing/release pipeline with restricted build/test jobs and separate security checks. Actions use verified commit SHAs; the Gitleaks download has a fixed SHA-256.
- Added weekly submodule update proposals and ignored local environment files, signing material and build outputs.
- Corrected conflicting template EULA text and documented unfinished release packaging.
- Made plugin installation and optional IPP explicit build options.
- Fixed the stale plugin-name test, compilation issues and warnings; added audio passthrough and state-restoration regression tests.
- Configured the owner's requested GitHub noreply identity for future commits in this repository only.

## Verification

| Check | Result |
| --- | --- |
| Gitleaks 8.30.1, `git --log-opts=--all --redact` | 683 locally reachable commits scanned; no secrets detected |
| Gitleaks 8.30.1, current project-file snapshot | No secrets detected; build directories and submodule contents excluded |
| actionlint 1.7.12 | Workflow syntax passed |
| Local documentation links, installer JSON/XML, selected ignore rules, `git diff --check` | Passed |
| macOS Debug build | Standalone, VST3, CLAP, Tests and Benchmarks built successfully; encountered compiler warnings corrected |
| CTest | 4/4 passed: plugin instance, audio passthrough, state restoration, boot benchmark |

The passthrough test covers mono/stereo with validation off/on at 48 kHz and a 256-sample block. It checks that output samples are unchanged. The state test checks restored input selection and disabled validation signals.

## Remaining work and limits

- Commit and push the file changes before expecting the new README or workflows on the default branch. Online repository settings above are already active.
- Run the new workflow matrix and CodeQL on GitHub. Windows, Linux, macOS universal/Release builds and DAW compatibility were not validated in this local pass.
- Keep reviewing dependency updates; Dependabot does not provide a complete inventory or vulnerability scan of transitive C++ dependencies.
- Historical commit authorship was not rewritten. Earlier names and email metadata remain public.
- Before publishing binaries, resolve JUCE's AGPLv3/commercial licensing path, bundle the required notices and finish signing and installer validation. See [MAINTAINING.md](MAINTAINING.md).
