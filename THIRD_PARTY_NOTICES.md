# Third-party notices

The top-level [MIT license](LICENSE) covers project-owned code and the Pamplejuce template. It does not replace licenses for dependencies. Preserve existing copyright and license notices when copying or distributing code.

| Component | Role | License / authoritative notice |
| --- | --- | --- |
| Pamplejuce | Project template | MIT; original copyright retained in [LICENSE](LICENSE) |
| JUCE 8 | Audio, DSP, UI and plugin framework | [AGPLv3 or commercial JUCE license](JUCE/LICENSE.md) |
| CLAP JUCE extensions | CLAP plugin wrapper | [MIT](modules/clap-juce-extensions/LICENSE.md) |
| CLAP SDK | CLAP interfaces | [MIT](modules/clap-juce-extensions/clap-libs/clap/LICENSE) |
| Melatonin Inspector | UI inspection | [MIT](modules/melatonin_inspector/LICENSE) |
| CMake includes | Build helpers | [Upstream source and file notices](https://github.com/sudara/cmake-includes) |
| CPM.cmake | Build-time dependency manager | [MIT](https://github.com/cpm-cmake/CPM.cmake/blob/v0.42.0/LICENSE) |
| Catch2 3.8.1 | Test framework | [BSL-1.0](https://github.com/catchorg/Catch2/blob/v3.8.1/LICENSE.txt) |

Submodule revisions are recorded in Git. JUCE includes additional third-party components and codecs: consult [JUCE/LICENSE.md](JUCE/LICENSE.md) and the notices it links for the selected platform and formats. Optional Intel IPP builds also require review of Intel's licensing and redistribution terms.

## Binary distribution

Select and document the applicable JUCE licensing path before publishing installers. Distribution under JUCE's AGPLv3 option requires compliance with AGPLv3, including applicable source and license obligations for the combined work. Distribution under the commercial option requires the corresponding JUCE license. Project source being MIT-licensed does not waive either requirement.

The repository does not currently automate binary releases. The files under `packaging/` are development scaffolding, not a complete release or license-compliance process. Before using them, include the complete license texts and notices required for the actual binary, resolve signing and notarization, and validate installation on a clean machine.
