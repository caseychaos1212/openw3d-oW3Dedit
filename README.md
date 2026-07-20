# oW3DEdit

oW3DEdit is an open-source viewer and editor for Westwood 3D (`.w3d`) assets. It provides a chunk-level editor, JSON conversion tools, archive-aware asset loading, and an interactive 3D preview for models and animations.

> [!IMPORTANT]
> oW3DEdit is alpha software. Keep backups of original assets and validate edited files in their target game or toolchain.

## Features

- Browse the complete W3D chunk hierarchy and inspect raw chunk data.
- Edit supported mesh, material, texture, hierarchy, animation, transform, and HLOD fields.
- Rename hierarchies, pivots, meshes, and related references while keeping dependent data synchronized.
- Save edited W3D files and preserve unsupported chunks when possible.
- Export individual files to JSON and rebuild W3D files from edited JSON.
- Batch-export chunk reports or JSON, and safely batch-import JSON matched by relative path.
- Open W3D assets directly or browse assets stored in MIX, DAT, and DBS archives.
- Preview meshes, materials, textures, skeletons, HLODs, and animations in the render pane.
- Load supplemental skeletons and animation libraries into a render session.
- Play, scrub, blend, and draft animation edits, with undo and redo support.
- Manipulate scene objects and pivots interactively and export animation previews as GIFs.
- Reorder hierarchy bones with the SkeletonHack workflow.

## Download

Prebuilt Windows packages are published on the [GitHub Releases](https://github.com/caseychaos1212/openw3d-oW3Dedit/releases) page.

Extract the release archive and run `oW3DEdit.exe`. The Qt runtime files shipped beside the executable must remain in the extracted directory.

### Runtime prerequisite

oW3DEdit does not bundle Microsoft's C++ runtime DLLs. Install the [latest supported Microsoft Visual C++ v14 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170) for x64 before running the editor. This supplies dependencies such as `MSVCP140.dll`, `VCRUNTIME140.dll`, and `VCRUNTIME140_1.dll`; it is already present on many Windows systems.

## Quick start

1. Start `oW3DEdit.exe`.
2. Open a `.w3d` file, or open a supported archive and choose a W3D entry.
3. Select chunks in the hierarchy to inspect or edit their supported fields.
4. Use the render pane to preview the asset. Add a model, skeleton, animations, or a texture folder from the **Render** menu when dependencies are stored separately.
5. Save to a new file and test the result before replacing the original asset.

JSON import and export are available from the **File** menu. Folder-wide operations are under **Batch Tools**.

Batch JSON import recursively mirrors the directory layout created by batch export. It skips ambiguous `.w3d`/`.wlt` matches and imports that produce warnings. Each successful replacement is serialized and validated before the target is changed, and the original target is retained beside it as a `.bak` file.

## Building from source

The checked-in solution targets Windows x64 and currently expects Qt in `C:\Qt\6.9.0\msvc2022_64`.

### Requirements

- Windows 10 or later
- Visual Studio 2022 with the **Desktop development with C++** workload
- Qt 6.9.x for MSVC 2022 64-bit
- Qt Visual Studio Tools
- Windows SDK with Direct3D development libraries

### Build

1. Clone the repository and open `oW3DEdit.sln` in Visual Studio.
2. Select the `Release` and `x64` configuration.
3. Build the solution.
4. Run `Release\oW3DEdit.exe`.

The Release build runs `windeployqt` automatically to copy the required Qt runtime into the output directory. If Qt is installed elsewhere, update the Qt paths in `oW3DDumpQt.vcxproj` or configure the project through Qt Visual Studio Tools.

### Versioning

The release version is defined in `AppVersion.h` and supplies both the application's Semantic Versioning string and the numeric Windows file version. Use `MAJOR.MINOR.PATCH` for stable releases and suffix prereleases with `-alpha`, `-beta`, or `-rc` (optionally followed by a number such as `-rc.1`). Increment the Windows version's fourth component only when repackaging the same public version. Keep the changelog release heading synchronized with this file.

### Verification

From PowerShell, build the Debug x64 editor and run byte-for-byte JSON round-trip
validation against the configured local W3D corpus:

```powershell
.\tools\verify.ps1
```

The script writes timestamped CSV reports and failure artifacts beneath the
ignored `test-output` directory. Set the private corpus path in the ignored
`tools\verify.local.psd1`, pass `-CorpusPath`, or set `OW3D_TEST_CORPUS`.
Use `-Mode Structured` or `-Mode Hex` to run one serialization mode, `-NoBuild`
to reuse an existing executable, or `-BuildOnly` to stop after MSBuild.

## Current limitations

- Only Windows x64 builds are currently configured.
- W3D is a broad format and not every chunk type has a dedicated editor.
- Rendering and serialization may differ from the original Westwood tools for uncommon or malformed assets.
- Archive contents are treated as sources; save edited assets as standalone files unless a workflow explicitly says otherwise.

When reporting a problem, include the affected chunk type, reproduction steps, and a sample asset when redistribution is permitted. Please use [GitHub Issues](https://github.com/caseychaos1212/openw3d-oW3Dedit/issues).

## Roadmap

- Expand dedicated editors and validation for less common chunk types.
- Improve material and animation rendering fidelity.
- Reduce machine-specific build configuration.
- Continue hardening lossless JSON and W3D round trips.

See [CHANGELOG.md](CHANGELOG.md) for release highlights.

## License

oW3DEdit is licensed under the [GNU General Public License v3.0](LICENSE).

This is an independent community project and is not affiliated with or endorsed by Electronic Arts or the original Westwood Studios.
