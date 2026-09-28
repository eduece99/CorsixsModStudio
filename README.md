# Corsix's Mod Studio: Definitive Edition

## What is Corsix's Mod Studio?

Corsix's Mod Studio: Definitive Edition (CDMS) is a modding IDE for Relic Entertainment's games — primarily **Dawn of War** (and its expansions Dark Crusade, Soulstorm, Winter Assault) and **Company of Heroes**. Originally written by [Corsix](mailto:corsix@gmail.com) around 2006–2007, this Definitive Edition is a ground-up modernization of the original codebase, bringing it to C++20, 64-bit, and modern tooling while preserving full compatibility with Relic's game file formats.

### New Features and Changes

- **SCAR editor with Lua Language Server** — real-time diagnostics, autocomplete, hover-to-inspect, go-to-definition, and find-in-file powered by [LuaLS](https://github.com/LuaLS/lua-language-server), with SCAR API stubs for full engine function coverage
  - **Hotkey Improvements** - Added some common hotkeys to make editor feel more modern. Save with CTRL+S. Use CTRL + TAB to switch tabs. Close tab with CTRL + W. Show autocomplete suggestions with CTRL + SPACE. Etc.
- **Preview Tab** - Files will open preview on one click for faster navigation
- **Performance Improvements** — uses multithreading to improve load times and UI responsiveness
- **DPI-aware Scaling** — updated icons and graphics for modern systems
- **Dawn of War: Definitive Edition mods** — browse and create mods in `%APPDATA%\Relic Entertainment\Dawn of War\mods`, resolve their game-install dependencies, and edit modern pipeline/burn/archive configuration
- **Dark Mode** - Configurable theming with dark mode support
- **Removed broken functionality** - Trimmed out or hid functionality that was never used such as the developers big red button, etc.

The software was released as open source under the **GNU GPL v2** (CDMS app) and **GNU LGPL v2.1** (Rainman library).

## Why Modernize?

The original codebase targets technology from 2006:

| Component         | Original                        | Definitive Edition                                        |
| ----------------- | ------------------------------- | --------------------------------------------------------- |
| C++ Standard      | Pre-C++11 (VS 2003/2005 era)    | C++20                                                     |
| Architecture      | 32-bit (x86)                    | 64-bit (x64)                                              |
| wxWidgets         | 2.8.0                           | 3.2+                                                      |
| Lua               | 5.0.2 + 5.1.2 (DLL)             | Vendored 5.0.2 + vendored 5.1.2 (static, renamed symbols) |
| zLib              | Bundled ancient copy            | vcpkg 1.3.1                                               |
| libsquish         | Runtime-loaded DLL              | Static link via vcpkg                                     |
| Build System      | Manual VS project (COMPILE.txt) | CMake 4.1 + vcpkg + presets                               |
| Compiler          | MSVC 2003/2005                  | MSVC 2026 (v19.50)                                        |
| Testing           | None                            | Google Test (635 tests)                                   |
| Static Analysis   | None                            | clang-tidy                                                |
| Memory Management | Raw `new`/`delete` throughout   | `std::unique_ptr`, `std::shared_ptr`, `std::string`       |
| Concurrency       | Single-threaded                 | Thread pool + async task runners                          |
| Logging           | None                            | spdlog (structured logging)                               |
| Script Editor     | Basic syntax highlighting       | Full LSP integration (LuaLS)                              |

The software no longer compiled on modern compilers and exhibited bugs on current Windows versions. This project brings it back to life — and significantly beyond the original.

## Architecture

The codebase has three layers

### Rainman Library (static lib)

The core I/O library for Relic game file formats. No UI dependencies — pure data parsing and manipulation. Organized into subdirectories: `core/`, `io/`, `archive/`, `formats/`, `lua/`, `localization/`, `module/`, `util/`.

**Key classes:**

- `CSgaFile` — SGA archive reader (v2, v4) with zLib decompression
- `CSgaCreator` — SGA archive writer with parallel compression via thread pool
- `CRgdFile` — Binary game data (RGD) with Lua integration
- `CUcsFile` — Unicode localization string tables with `CUcsMap` (dual ordered/unordered map)
- `CChunkyFile` — Relic's chunked container format
- `CModuleFile` — Game mod descriptor (.module files)
- `CResourceLoader` — Parallel mod resource loading with deferred error propagation
- `CRgtFile` — Texture files (DXT1/3/5, DDS, TGA)
- `IFileStore` / `CMemoryStore` / `CFileSystemStore` — Polymorphic I/O abstraction
- `CInheritTable` — Lua-based data inheritance resolution
- `CThreadPool` — Process-wide thread pool singleton (`std::thread::hardware_concurrency()` workers)

### CDMS Service Layer

An intermediate layer between Rainman and the GUI, providing error-safe wrappers and concurrency:

- `services/` — `ModuleService`, `FileService`, `FormatService`, `HashService` wrap Rainman calls with `Result<T>` error handling
- `async/` — `CTaskRunner` (pure C++) / `CWxTaskRunner` (posts results to main thread via `CallAfter`) for background work, `CCancellationToken` for cooperative cancellation, `CProgressChannel` with Rainman callback bridge

### CDMS GUI Application (wxWidgets 3.2)

The GUI application uses a **Model-View-Presenter (MVP)** architecture with 14 presenters and 16 view interfaces for separation of concerns and unit testability.

**Presenters** (`src/cdms/presenters/`):
`CModuleLoadPresenter`, `CFileTreePresenter`, `CScarEditorPresenter`, `CRgdEditorPresenter`, `CRgdMacroPresenter`, `CUcsEditorPresenter`, `CSgaMakePresenter`, `CLuaInheritTreePresenter`, `CModuleSettingsPresenter`, `CMassExtractPresenter`, `CImagePresenter`, `CRefreshFilesPresenter`, `CUcsToDatPresenter`, `CDpsCalculatorPresenter`

**View interfaces** (`src/cdms/views/interfaces/`):
`IMainFrameView`, `IFileTreeView`, `IScarEditorView`, `IRgdEditorView`, `IRgdMacroView`, `IUcsEditorView`, `ISgaMakeView`, `ILuaInheritTreeView`, `IModuleSettingsView`, `IMassExtractView`, `IImageView`, `IRefreshFilesView`, `IUcsToDatView`, `IDpsCalculatorView`, `ISaveable`, `ISearchable`

**Key views:**

- `ConstructFrame` — Main window with splitter, AUI notebook tabs, and file tree
- `frmFiles` — File browser with tree view, SVG icons, and expand/collapse navigation
- `frmScarEditor` — SCAR script editor with LSP integration, FindBar, hover popup, and go-to-definition
- `frmRgdEditor` — Property grid editor for RGD game data files
- `frmUCSEditor` — Unicode string table editor
- `frmSgaMake` — SGA archive creation wizard
- `frmModule` — Mod settings editor (dependencies, data folders, archives)
- `frmLoading` — Async loading dialog with progress bar and cancel button

### Lua Language Server Integration

The SCAR editor integrates [LuaLS](https://github.com/LuaLS/lua-language-server) (lua-language-server) for a modern editing experience:

- **Real-time diagnostics** — errors, warnings, and hints displayed inline with squiggly underlines
- **Autocomplete** — context-aware completions for SCAR engine functions and mod code
- **Hover-to-inspect** — rich popup with syntax-highlighted signatures, markdown descriptions, and merged diagnostics
- **Go to Definition** — Ctrl+Click or F12 to jump to function/variable definitions
- **Find in File** — VS Code-style Ctrl+F bar with match highlighting, count, and navigation
- **SCAR API stubs** — auto-generated type definitions for ~500+ Dawn of War and ~1000+ Company of Heroes engine functions (from ScarDoc HTML and scardoc.dat)
- **`--?` annotation translation** — Relic's `--?` doc comments are translated on-the-fly to LuaLS-compatible `---@param`/`---@return` annotations
- **Status bar indicator** — real-time LSP status (Idle → Starting → Loading → Ready) with colored dot
- **Toggle on/off** — View menu option to enable/disable the language server

See [`docs/lua-language-server-integration.md`](docs/lua-language-server-integration.md) for detailed documentation.

### Definitive Edition mod workflow

1. **Choose your game installation:** Select **File → Set DoW:DE Game Folder**
   and pick the folder containing `DoWDE.module` and `W40k.exe` (or
   `W40k_gog.exe` for GoG). Opening a mod won't change this setting.
2. **Open or create a mod:** Use **File → Browse DE Mods** for mods in AppData,
   **File → Open DoW:DE Mod** to open a `.module` file anywhere, or
   **File → New Mod → Dawn of War: Definitive Edition** to create a mod in
   AppData. New mods include the project folders and configuration files needed
   for the DE pipeline and Audio Editor.
3. **Edit your project:** Find `pipeline.ini` and `.sgaconfig` files under
   **Project Files** in the Files tree; `.burn` rules appear under **Generic**.
   Open a config to edit it, then use **Save** or Ctrl+S. Reload the mod after
   changing `pipeline.ini` so the changes take effect.
4. **Create an archive (optional):** Choose **Mod → Create DoW:DE Archive**
   to run the installed Relic `Archive.exe`. Select a `.sgaconfig`, a source
   folder matching its `relativeroot`, and an output `.sga` outside that source
   folder. The Corsix SGA packer is still available for older mods.

## Building

### Download a prebuilt release

On Windows 10 or 11 (x64), download the latest `CorsixModStudio-vX.Y.Z-win-x64.zip`
from [GitHub Releases](https://github.com/jbelford/CorsixsModStudio/releases).
Extract the entire ZIP to a **writable** folder, then run `ModStudioDE.exe`
from that folder. Keep `Mod_Studio_Files` next to the executable; launch from
the extracted folder (not directly from inside the ZIP). No Visual Studio,
vcpkg, or separate Lua Language Server download is needed. The `.sha256`
asset contains the archive's SHA-256 checksum for verification.

### Publishing a release

Update the version in `CMakeLists.txt`, `vcpkg.json`,
`src/cdms/common/strings.h`, and `src/cdms/resource.rc` together. Once the
release commit is on the intended branch, push a matching `vX.Y.Z` tag.
The Release workflow builds Windows x64, runs Release tests, checks the
bundled files and version, and publishes the ZIP and checksum only on success.
It will not overwrite an existing published release.
To make the same portable ZIP locally after a Release build, run
`cpack --preset release`; CMake install rules define its contents.

### Prerequisites

- **Visual Studio 2022 or later** (tested with VS 2026 Enterprise)
- **vcpkg** — `git clone https://github.com/microsoft/vcpkg.git C:\vcpkg && C:\vcpkg\bootstrap-vcpkg.bat`
- **CMake 3.20+** (bundled with Visual Studio)

### Build Commands

```powershell
# Set environment
$env:VCPKG_ROOT = "C:\vcpkg"

# Configure + Build + Test (uses CMake presets)
cmake --preset default
cmake --build --preset debug
ctest --preset debug

# Release build (LTO, static CRT)
cmake --build --preset release

# Static analysis with clang-tidy
cmake --preset tidy
cmake --build --preset tidy-debug

# Run clang-tidy manually
.\tools\run-clang-tidy.ps1                                    # all sources
.\tools\run-clang-tidy.ps1 src/rainman/archive/CSgaFile.cpp   # single file
.\tools\run-clang-tidy.ps1 -Fix                               # auto-fix
```

### Dependencies (auto-installed by vcpkg)

- **zlib 1.3.1** — compression for SGA archives
- **Google Test 1.17** — unit testing framework
- **libsquish** — DXT texture compression (for CRgtFile)
- **wxWidgets 3.x** — GUI framework
- **spdlog** — structured logging

CI and Release cache vcpkg binary packages with GitHub Actions. The first run
builds dependencies from source; later runs restore matching packages. The
cache key includes `vcpkg.json` and `triplets/`; bump the cache key version in
both workflows if the Windows compiler or runner image changes.

## Key Technical Decisions

### Lua Strategy

The original code uses two Lua versions for different purposes:

- **Lua 5.0.2** — compiled and linked statically for core game data parsing (RGD files use Lua internally)
- **Lua 5.1.2** — now statically linked (originally runtime-loaded via `LoadLibraryW`). All public symbols are renamed via `lua512_rename.h` (`lua_` → `lua51_`) to avoid linker conflicts with Lua 5.0.2.

We vendor both Lua versions' source directly (including Corsix's patches to `ltm.c/h` and `lvm.c/h` in 5.0.2).

### zLib Strategy

The original bundled a modified zlib with a custom `crc32_case_idt()` function for case-insensitive file path hashing (essential for Windows file system compatibility in Relic's formats). We now use system zlib from vcpkg and extracted `crc32_case_idt()` into a standalone file with its own CRC table.

### Static vs Dynamic Library

Rainman was originally a DLL (`__declspec(dllexport/import)`). We build it as a **static library** for simplicity — the only consumer is the CDMS executable. The `RAINMAN_NO_EXPORTS` define makes `RAINMAN_API` expand to nothing.

## Documentation

The `docs/` directory contains detailed specifications:

- [`architecture.md`](docs/architecture.md) — System architecture overview
- [`target-architecture-v2.md`](docs/target-architecture-v2.md) — Target architecture design
- [`lua-language-server-integration.md`](docs/lua-language-server-integration.md) — LuaLS integration design and implementation
- [`sga-archive-format.md`](docs/sga-archive-format.md) — SGA archive file format specification
- [`rgd-game-data-format.md`](docs/rgd-game-data-format.md) — RGD binary data format
- [`relic-chunky-format.md`](docs/relic-chunky-format.md) — Relic Chunky container format
- [`ucs-localisation-format.md`](docs/ucs-localisation-format.md) — UCS localization format
- [`module-file-format.md`](docs/module-file-format.md) — .module file format
- [`data-inheritance-system.md`](docs/data-inheritance-system.md) — Lua-based data inheritance
- [`hash-algorithms.md`](docs/hash-algorithms.md) — Hash algorithms used in Relic formats
- [`texture-material-script-formats.md`](docs/texture-material-script-formats.md) — Texture and material formats
- [`known-bugs.md`](docs/known-bugs.md) — Known issues and workarounds

## License

- **CDMS Application**: GNU General Public License v2.0
- **Rainman Library**: GNU Lesser General Public License v2.1
- **Vendored Lua 5.0.2 / 5.1.2**: MIT License
- **Lucide Icons**: ISC License
