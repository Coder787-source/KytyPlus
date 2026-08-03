# KytyPlus v2.0 — Performance & Compatibility Release

Download:

Windows: KytyPS5-Windows-x64.zip
macOS: KytyPS5-macOS-x86_64.zip
Linux: KytyPS5-Linux-x86_64.zip

What's new vs v1.9:

### Performance

- **JIT O(1) LRU cache (jit.h):** Linear JIT cache lookup replaced with an O(1) LRU eviction policy. Dramatically reduces cache thrashing in games with large or frequently-changing shader workloads. Lower CPU overhead per draw call.
- **Shader disk cache (shader.cpp):** Compiled shaders are now cached to disk between sessions. Eliminates recompilation stutter on subsequent launches. Significantly faster load times after the first run.
- **Adaptive frame pacing (window.cpp):** Dynamic frame pacing that adapts to workload — no more fixed intervals. Fixed a division-by-zero edge case in the frame timing logic. Smoother gameplay across varying scene complexity.

### Ray Tracing

- **Full RT pipeline wiring** with proper KHR dispatcher integration. `GetBufferDeviceAddress` moved out of the header into `vma.cpp` for cleaner architecture and cross-platform compatibility. Fixed struct ordering in `graphicContext.h` for macOS builds.

### Portable Mode & Fullscreen Flag

- **Portable mode:** Run KytyPlus entirely from a USB stick — all config and caches stored alongside the executable.
- **Fullscreen flag:** New launcher toggle for starting directly in fullscreen.

### Bug Fixes

- **Arcade Spirits save fix (Windows):** Filenames containing Windows-invalid characters (`: * ? " < > |`) are now automatically sanitized. Fixes save/load failures in Arcade Spirits and other titles that use special characters in save filenames.
- **GE_INDX_OFFSET fix:** Added missing `m_index_offset` member to CommandProcessor.

### Infrastructure

- **Cross-platform CI fixes:** Updated all GitHub Actions to latest versions. Replaced broken Node.js 20 actions with Node 24 compatible versions. Fixed macOS `HOMEBREW_NO_REQUIRE_TAP_TRUST=1` build step. Fixed Linux CI build failure (missing `SanitizeFilenameForWindows` definition).
- **Repository cleanup:** Removed all build artifacts (`.exe`, `.dll`, zips) from the repo — size reduced by ~200 MB. Added proper `.gitignore`. Removed unused scaffolding files.

### Expectations

- This release is a performance and compatibility milestone
- Smoother frame pacing, faster load times, and better cache efficiency across all platforms
- Tag v2.0, GPL-2.0.
