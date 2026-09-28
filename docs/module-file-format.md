# Module File Format

Module files (`.module`) define a mod's structure for Dawn of War and Company of Heroes.
They declare which archives, folders, localisations, and dependencies make up a mod.
`CModuleFile` parses these files and orchestrates resource loading.

## Definitive Edition mod creation

New Definitive Edition mods are created under `%APPDATA%\Relic Entertainment\Dawn of War\mods\<name>\`,
not in the game installation. The mod's `<name>.module` and `pipeline.ini` sit alongside
`DataGeneric`, `DataSrc`, `DataIntermediate`, and `Mod`. `Mod` contains `Data`,
`Locale\English`, and `Data.sgaconfig`. The module's `RequiredMod` chain and the
pipeline's project parent both follow the selected base game.
An empty `DataGeneric\Sound\_default.rat` is included: without it, Relic's
Audio Editor crashed when opening a newly generated DE mod.

The generator uses a bundled `_default.burn` from `src/cdms/res/`.
CMake embeds its bytes in the application, so creating a mod does not depend
on another mod or the game installation providing burn rules. The template
contains 14 targets covering entity blueprints, models, textures, audio, UI
assets and other file types, with a final ignore fallback.
`Mod\Data.sgaconfig` is also generated in code, using the documented default
compression rules and four file/size overrides. No files are copied from
other mods.

## Module Types

| Type | Games |
|------|-------|
| `MT_DawnOfWar` | Dawn of War, Winter Assault, Dark Crusade, Soulstorm |
| `MT_CompanyOfHeroesEarly` | CoH pre-release / early MP beta |
| `MT_CompanyOfHeroes` | CoH release, Opposing Fronts |

## File Format

Module files are INI-style text with the general structure:

```ini
; Comment
[SectionName]
Key = Value
```

Semicolons (`;`) start comments. Blank lines are ignored.

## Dawn of War Directives

### `[global]` Section

| Key | Description |
|-----|-------------|
| `UIName` | Display name shown in game UI |
| `Description` | Mod description text |
| `DllName` | Name of the mod's DLL |
| `ModFolder` | Relative path to mod folder |
| `TextureFE` | Front-end texture path |
| `TextureIcon` | Icon texture path |
| `ModVersion` | Version string (`major.minor.revision`) |
| `DataFolder.N` | Numbered data folder path |
| `ArchiveFile.N` | Numbered SGA archive (`.sga` auto-appended) |
| `RequiredMod.N` | Numbered required mod reference |
| `CompatableMod.N` | Numbered compatible mod reference |

The `.N` suffix is a priority number. Resources with lower numbers take precedence.

### Dynamic Placeholders

The `_DawnOfWarRemoveDynamics()` function replaces placeholders in paths:

| Placeholder | Replacement |
|-------------|-------------|
| `%LOCALE%` | `Locale\{locale}` (e.g. `Locale\English`) |
| `%TEXTURE-LEVEL%` | `Full` |
| `%SOUND-LEVEL%` | `Full` |
| `%MODEL-LEVEL%` | `High` |

## Company of Heroes Directives

### `[global]` Section

Same as DoW plus:

| Key | Description |
|-----|-------------|
| `Name` | Internal module name |
| `LocFolder` | Localisation folder path |
| `ScenarioPackFolder` | Scenario pack folder |

### `[datasource_*]` Sections

CoH replaces flat archive/folder lists with data sources:

```ini
[datasource_LocaleName]
TOC = FolderType
Option = common|sound_high|art_high|locale_code
Folder = RelativePath
Archive.1 = path/to/archive
Archive.2 = path/to/other
```

| Key | Description |
|-----|-------------|
| `TOC` | Table of Contents type identifier |
| `Option` | Filtering option (common, sound quality, art quality, locale) |
| `Folder` | Folder path for this data source |
| `Archive.N` | Numbered SGA archives within this source |

## Resource Loading Algorithm

`ReloadResources()` loads mod resources in a controlled order using a bitmask:

| Flag | Value | Description |
|------|-------|-------------|
| `RR_LocalisedText` | 1 | Load UCS localisation files |
| `RR_DataFolders` | 2 | Load data folders |
| `RR_DataArchives` | 4 | Load SGA archives |
| `RR_RequiredMods` | 8 | Recursively parse required mods |
| `RR_Engines` | 16 | Parse engine settings |
| `RR_DataGeneric` | 32 | Load the datageneric folder |
| `RR_MapArchives` | 64 | Load CoH map SGA packs |
| `RR_All` | 127 | Load everything |

Required mods and engines are themselves `CModuleFile` instances, parsed recursively with
their own `iReloadWhat` masks.

For Definitive Edition mods stored outside the game installation, call
`SetGameInstallPath(installRoot)` after `LoadModuleFile()` and before `ReloadResources()`.
Required modules resolve beside the current module first, then at the install root;
each child loads its own data relative to its `.module` file. Engine data resolves at
the install root. `pipeline.ini` is read beside the module, or from the install root
if absent locally; its `[project:<module filename without .module>]` section selects
`DataGeneric`, and `%app%` paths resolve at the install root. A new
`LoadModuleFile()` call clears the install root. Without an install root, legacy
same-directory resolution and `ModFolder` pipeline sections are retained.

## File Resolution

`CModuleFile` implements both `IFileStore` and `IDirectoryTraverser`, presenting the
combined contents of all loaded archives, folders, and required mods as a unified virtual
filesystem. File lookups search sources in priority order (by their `.N` number).

## Source Files

- `CModuleFile.h` / `CModuleFile.cpp`
