# Agent Guidance

## Goal: Native Linux Build

The repository currently targets Windows/MSVC. The checked-in `default` preset
uses a Visual Studio generator, and the `tidy-ninja` preset still selects the
Windows `x64-windows-static` vcpkg triplet. Do not describe either preset as a
native Linux build. Treat Linux support as a port that must preserve the
existing Windows build and behavior.

Before making Linux-port changes:

1. Inspect the relevant CMake targets, source files, tests, and current worktree.
   Preserve unrelated and pre-existing worktree changes.
2. Keep Windows and Linux behavior behind CMake platform conditions or small,
   explicit platform-specific implementations. Avoid broad rewrites.
3. Identify the user-visible contract for any Windows API being replaced
   (paths, file-name case handling, process lifetime, encoding, or data
   directories) before choosing a replacement.
4. Do not modify `CDMSSrc_055/`; it is read-only reference code.
5. Do not modify or upgrade vendored Lua sources in
   `src/rainman/vendor/lua502/` or `src/rainman/vendor/lua512/`.

## Known Linux Porting Work

These are investigation points, not claims that every item is necessarily a
blocker. Verify each against the current source before changing it.

- **CMake and presets:** Add a Linux/Ninja configure preset with a separate
  binary directory and Linux dependency triplet (for example,
  `x64-linux`). Keep existing Windows presets intact. Review
  `CMakeLists.txt`, `src/rainman/CMakeLists.txt`, `src/lsp/CMakeLists.txt`,
  `src/cdms/CMakeLists.txt`, and all test targets for MSVC flags, Windows
  libraries, resource files, static-link assumptions, and platform-specific
  install/package naming.
- **Dependencies:** Ensure the chosen Linux package source provides compatible
  wxWidgets components, zlib, libsquish, spdlog, nlohmann-json, and Google Test.
  Document required system development packages if using system packages.
  Validate actual package discovery and linkage rather than assuming Windows
  vcpkg settings carry over.
- **Win32 APIs:** Find and isolate OS-specific code. Current areas include
  `src/lsp/Transport.*` (Win32 process and pipe management),
  `src/rainman/core/WriteTime.cpp`, `src/rainman/core/RainmanLog.cpp`,
  `src/rainman/util/Util.cpp`, `src/rainman/io/CFileSystemStore.*`,
  `src/cdms/frame/Construct.cpp`, and CDMS call sites using `_wfopen`.
  Prefer a tested platform abstraction or paired platform implementation over
  leaking OS APIs into shared interfaces.
- **Filesystem compatibility:** Preserve the game formats' expected path
  separators, lookup/case behavior, timestamps, and Unicode handling. Linux
  filesystems are commonly case-sensitive; do not silently assume that a
  direct replacement of Win32 path APIs has equivalent semantics.
- **Application data and resources:** Define Linux locations for application
  data/configuration (typically respecting XDG conventions), and review
  Windows-only resources, executable startup behavior, and install layout.
- **Lua Language Server:** `src/lsp/CMakeLists.txt` currently downloads a
  Windows `.exe` release archive at configure time, and CDMS packaging assumes
  that executable name and layout. Select the correct supported Linux release
  asset and executable, or make the server an explicitly optional runtime
  dependency. Do not make configure fail silently when the download is
  unavailable.
- **CI and packaging:** Add Linux CI that configures, builds, and runs relevant
  tests. Keep Windows CI and release packaging working, and give Linux
  packages an accurate platform-specific name and install layout.

## Development and Verification

- Use CMake configure/build/test presets; do not replace the existing Windows
  presets with ad-hoc command lines.
- Build and run focused tests for each platform-specific change. Before calling
  Linux support complete, verify a clean Linux configure, full build, and
  `ctest --preset <linux-test-preset>` run, then confirm the Windows presets
  still configure and build in their supported Windows environment.
- Report platform limitations and unverified behavior explicitly. A successful
  Rainman-only build is not evidence that the wxWidgets GUI or Lua Language
  Server integration works on Linux.
- Follow the repository's C++ conventions in `.github/copilot-instructions.md`
  and relevant `.github/skills/cpp-developer/references/` guidance.
