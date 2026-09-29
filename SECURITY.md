# Security policy

## Supported code

Security fixes target the latest `main` revision. FullSpectrum is in early development; older commits and experimental builds do not have a separate maintenance branch.

## Report a vulnerability privately

Use [Report a vulnerability](https://github.com/Munnopsis/SpectrumAnalyzerPlugin/security/advisories/new) on GitHub. Private vulnerability reporting is enabled for this repository.

Include the affected commit or version, platform, reproduction steps, expected impact and a minimal example when safe to share. Remove credentials, personal information and copyrighted client audio. Do not put exploitable security details or secrets in a public issue.

Reports are reviewed on a best-effort basis. There is no guaranteed response time or bug bounty program.

## Scope

Relevant reports include unsafe handling of audio files or saved plugin state, unintended disclosure of audio or project information, and vulnerabilities in repository automation or dependencies. For ordinary crashes, UI issues and inaccurate readings without a security impact, use a public bug report.

## If a credential is exposed

Revoke or rotate the credential immediately. Removing a file or editing the latest commit does not remove it from Git history, forks, caches or earlier downloads. Coordinate any history cleanup with the repository owner after the credential has been revoked.

## Security controls and limits

GitHub Secret Scanning, Push Protection and private reporting are repository settings. The workflows add a redacted Gitleaks history scan and CodeQL analysis of C++ and GitHub Actions. Checks are defenses against known patterns, not a guarantee that the code is free of vulnerabilities. CodeQL's C++ scan uses `build-mode: none`, so generated code and some build-specific behavior are outside its coverage.

Dependency updates require review and testing. Git submodule update proposals do not imply complete vulnerability coverage for native dependencies. See [MAINTAINING.md](docs/MAINTAINING.md) for operational details.
