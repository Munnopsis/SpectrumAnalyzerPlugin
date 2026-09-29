# Packaging scaffolding

These files are not used by the build/test workflow. They require release
validation before use; they do not currently produce a supported installer.

- `installer.iss`: Windows paths for Standalone, VST3 and CLAP Release builds.
- `distribution.xml.template`: macOS package layout for Standalone, VST3 and CLAP.
- `dmg.json`: optional macOS disk-image layout for manually staged bundles.
- `resources/`: installer introduction and license summary.

The existing icon artwork is inherited from Pamplejuce. Replace it for a branded
release. Include complete dependency license texts and finish signing,
notarization and clean-machine installation tests as described in
[the maintainer guide](../docs/MAINTAINING.md).
