# Security policy

## Supported versions

Only the latest GitHub **Release** and current `main` are considered for security fixes. Older builds may not be patched.

KytyPlus is **experimental hobby software**. It is not a hardened product. Treat it like other early emulators: run it on a machine you’re comfortable using for that purpose.

## What counts as a security issue

Please report:

- Remote code execution or unexpected native code execution from untrusted inputs (e.g. malformed files the emulator parses)
- Path traversal / arbitrary file write outside intended paths
- Credential or token leakage introduced by KytyPlus itself
- Dependency issues that clearly affect the emulator binaries we ship

## What usually is not a security issue

- Game crashes, hangs, black screens, wrong graphics/audio (use a **Bug** or **Compatibility** issue)
- Missing HLE, incomplete GPU features, or “game doesn’t boot”
- Someone using KytyPlus with illegal dumps (legal problem, not a vuln report)
- Theoretical risks with no practical impact

## How to report

**Do not** open a public issue with exploit details if the impact is serious.

1. Email or contact the repository owner via GitHub (see the [profile](https://github.com/Coder787-source) / repo owner) with:
   - KytyPlus version or commit
   - Description of the issue
   - Steps to reproduce
   - Impact assessment
2. Give a reasonable time to respond before public disclosure (e.g. 90 days unless we agree otherwise).

If GitHub **Private vulnerability reporting** is enabled for this repo, you may use that instead (Settings → Code security).

## Scope notes

- KytyPlus does not ship games or Sony firmware.
- Third-party libraries (Qt, Vulkan SDK components, etc.) have their own upstream security processes — report those upstream when appropriate, and tell us if our packaging is affected.

## Thanks

Responsible reports that help keep testers safer are appreciated.
