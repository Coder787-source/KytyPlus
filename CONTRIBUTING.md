# Contributing to KytyPlus

Thanks for taking an interest. KytyPlus is **experimental** — useful contributions are crash fixes, HLE/GPU improvements, docs, and tooling. “Make GTA playable” PRs without a real path are not useful.

## Ground rules

- **Legal dumps only.** Do not request, attach, or link game dumps, Sony firmware, or `sce_module` SPRXs.
- **No piracy support** in issues, PRs, or Discussions.
- Prefer **logs + title ID + GPU/driver + KytyPlus version** over vague “game broken” reports.
- Use the [issue templates](https://github.com/Coder787-source/KytyPlus/issues/new/choose) when filing bugs, compat reports, or features.

## How to help without coding

1. Test an official [Release](https://github.com/Coder787-source/KytyPlus/releases) on hardware you own.
2. File a **Bug** or **Compatibility** issue with a log excerpt (last error / EXIT block).
3. Say whether stock KytyPS5 behaved differently, if you know.

## Development setup (Windows)

See the README for full details. Short version:

1. Install VS 2022 Build Tools (C++), CMake, Ninja, Qt 6 (MSVC 2022 64-bit), Vulkan SDK.
2. Configure and build:

```bat
cmake -S src -B _Build/windows -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"

cmake --build _Build/windows --target launcher
cmake --install _Build/windows --prefix _Build/windows/install
```

3. Run from the install tree (`launcher.exe` / `kyty_emulator.exe`).

Do **not** commit `_Build/` or Release zips into git.

## Pull requests

1. Fork and branch from `main`.
2. Keep PRs focused (one fix or feature).
3. Describe **why**, not only what.
4. Note how you tested (build only vs title + log before/after).
5. Fill out the PR template.
6. Soft-EXIT → warn patches are OK when labeled as such; prefer real fixes when possible.

### Good PR targets

- Reproducing EXIT signatures from public issues
- Shader / texture-cache / HLE correctness
- Logging improvements and crash signatures
- Docs, templates, CI

### Bad PR targets

- Bundling dumps or firmware
- Drive-by reformatting of unrelated files
- Fake “compatibility” claims without evidence

## Code style

Match the surrounding code. Don’t wholesale reformat files you didn’t need to touch.

## License

By contributing, you agree your changes are licensed under **GPL-2.0**, same as this repository. Credit Kyty / KytyPS5 lineage where appropriate.

## Questions

Use [GitHub Discussions](https://github.com/Coder787-source/KytyPlus/discussions) if enabled; otherwise open a Feature / Bug issue with the templates.
