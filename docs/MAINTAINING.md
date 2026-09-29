# Maintainer guide

## Repository protection

Review the repository's GitHub settings after transferring ownership or recreating it. These settings are stored on GitHub, not in the source tree:

- Enable Secret Scanning, Push Protection, private vulnerability reporting and Dependabot alerts/security updates.
- Keep the default Actions token read-only and disable Actions approval of pull requests.
- Require approval before running workflows from external contributors.
- Protect `main` against deletion and force pushes.
- After the build and security workflows have run successfully, consider requiring their checks on pull requests. Choose the actual check names shown by GitHub before enforcing them.

The build workflow compiles Standalone, VST3 and CLAP and runs Catch2 on Linux, macOS and Windows. It has no release or signing steps, repository write permission or persisted checkout credentials. The security workflow grants `security-events: write` only to the CodeQL job so it can upload findings.

Actions are pinned to full commit hashes. Dependabot proposes Actions and submodule updates weekly; inspect upstream changes and test before merging. CPM's bootstrap is checksum-verified. Catch2 is selected by a version tag in the pinned CMake submodule. Gitleaks has a separately pinned version and SHA-256 in `security.yml`; update those together after checking the official release. Dependabot does not update that shell download or inventory every transitive C++ dependency.

References: [GitHub secure workflow guidance](https://docs.github.com/en/actions/reference/security/secure-use), [repository security quickstart](https://docs.github.com/en/code-security/getting-started/quickstart-for-securing-your-repository), and [CodeQL build-mode coverage](https://docs.github.com/en/code-security/how-tos/find-and-fix-code-vulnerabilities/manage-your-configuration/codeql-for-compiled-languages).

## Before committing

```sh
git status --short
git diff --check
git diff --cached
```

Review every staged file. `.gitignore` does not protect files already tracked and can be bypassed with `git add -f`. Never commit signing keys, tokens, private recordings or unredacted crash reports.

Use a repository-local noreply identity if desired, copying the address from GitHub's email settings. This affects future commits only. Published history may still contain earlier author names and email addresses; rewriting it does not retract forks or existing downloads.

For a local secret scan with Gitleaks installed:

```sh
gitleaks git --log-opts=--all --redact --no-banner .
```

This scans reachable commits, not unstaged files. Inspect staged changes and scan the working files separately as needed. A clean scan means no configured pattern matched; it is not proof that no sensitive information exists. If a credential is found, revoke it before considering history cleanup.

## Preparing a release

1. Update `VERSION` and document user-visible changes.
2. Run Release builds and tests on every claimed platform. Validate VST3 with a reviewed, version-pinned pluginval build and test supported hosts, sample rates and buffer sizes. Run the manual analyzer checks.
3. Resolve the licensing route described in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). Bundle the required notices, license texts and source or source offer where applicable.
4. Replace remaining template artwork and finish the installer scaffolding for the selected formats. Test installation and removal on a clean machine.
5. Configure macOS signing/notarization and Windows signing if distributing signed binaries. Store credentials only in a protected GitHub environment, with deployment approval. Restrict any release workflow to reviewed tags and give write permission only to the publishing job.
6. Validate the final signed packages, publish checksums, and clearly label any development or unsigned builds.

There is no automatic release on a tag push. Build verification can run without publishing artifacts or access to signing credentials.
