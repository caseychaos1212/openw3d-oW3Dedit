# AGENTS.md

## Project overview

oW3DEdit is a Windows x64 C++/Qt editor and viewer for Westwood 3D (`.w3d`)
assets. It parses, edits, renders, exports, and rebuilds a broad chunk-based
binary format. Treat preservation of user data as the primary constraint:
successful compilation alone is not sufficient evidence that a format change
is correct.

Read `README.md` before making broad changes. Read `QT_DESIGNER_GUIDE.md` before
editing UI layouts. For animated hierarchy, rename, HLOD, or pivot work, also
read `TEST_PLAN_animated_hierarchy_hlod.md`.

## Repository map and ownership

- `backend/`: W3D structures, chunk parsing, JSON conversion, serialization,
  mutation, and format utilities.
- `backend/render/`: renderer-independent scene and animation data.
- `frontend/render/`: Qt viewport integration and D3D11/Vulkan render backends.
- `mainWindow.cpp`, `MainWindow.h`, and `EditorWidgets.h`: application behavior,
  editor dispatch, and Qt bindings. Keep format logic in `backend/` when it can
  be independent of the UI.
- `MainWindow.ui`, `RenderPanel.ui`, and `ui/*.ui`: Designer-owned static layout.
- `thirdparty/`: vendored dependencies. Do not modify these for project-specific
  behavior.

Qt Designer owns static layout, standard controls, menus, actions, and default
properties. C++ owns behavior, signal connections, runtime data, dynamic pages,
and the render viewport. Stable Qt `objectName` values are interfaces with C++;
rename one only when all matching bindings are updated in the same change.
Never edit or commit generated `ui_*.h` files.

## Build environment

The checked-in solution targets Windows x64 with Visual Studio 2022, MSVC v143,
and Qt 6.9.x. The project currently expects Qt at
`C:\Qt\6.9.0\msvc2022_64`. Prefer the checked-in solution and project files over
introducing a second build system.

From Developer PowerShell, a representative Debug build is:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  oW3DEdit.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m
```

Release builds run `windeployqt`. If the current environment cannot invoke the
Windows toolchain, say that explicitly and do not claim the build passed.

Use `tools\verify.ps1` as the canonical build and round-trip verification entry
point. `tools\verify.ps1 -BuildOnly` performs only the x64 build;
`tools\verify.ps1` builds and then validates the configured corpus in both JSON
serialization modes. Use `-NoBuild` only when the executable is known to be
current.

## W3D correctness rules

- Preserve unknown or unsupported chunks and their payloads whenever possible.
- Do not silently normalize, reorder, discard, or regenerate unrelated data.
- Validate all byte counts, element counts, offsets, indices, and chunk sizes
  before reading or writing. Treat malformed input as expected input, not as a
  reason for an unchecked access or crash.
- Keep binary parsing and serialization symmetric. When changing one, trace the
  corresponding JSON conversion, editor mutation, save path, and reload path.
- Keep backward compatibility with older exported JSON when a missing field can
  be safely inferred or defaulted.
- Rename and delete operations must account for dependent references across
  meshes, hierarchies, pivots, animations, HModels, HLODs, and proxies. Scope
  propagation to the correct containing object; do not update unrelated assets
  merely because names match.
- Avoid broad rewrites of serializers. Make the smallest change that preserves
  existing behavior and add focused regression evidence.
- Never overwrite a source asset during development or testing. Save to a new
  file or a dedicated output directory.

## External test corpus

A local corpus of `.w3d` files may be exposed to the repository through a WSL
symlink or a Windows directory mounted beneath `/mnt/<drive>/...`. Treat that
corpus as read-only, potentially private, and not redistributable unless the
user explicitly says otherwise.

- Do not add corpus assets, extracted archive contents, absolute corpus paths,
  or generated reports to Git.
- Do not rename, modify, or delete files in the corpus.
- Put rebuilt files, exported JSON, screenshots, logs, and comparison reports in
  an ignored repository-local output directory such as `test-output/`.
- Before a corpus run, resolve the configured path and confirm it is outside the
  Git index. If a symlink is used, do not recursively stage its target.
- Corpus results are regression evidence, not permission to expose filenames or
  proprietary content in commits, issues, or responses. Summarize failures with
  the minimum identifying information needed.

No corpus path is committed here. Obtain the current WSL-visible path from the
user or local environment when a corpus test is requested.

## Verification

Choose verification proportional to the change and report exactly what ran.

1. Build the affected x64 configuration with `tools\verify.ps1 -BuildOnly` when
   the Windows toolchain is available.
2. For parser, serializer, JSON, or mutation changes, run `tools\verify.ps1`
   against the read-only external corpus. The equivalent interactive workflow
   is **Batch Tools > Validate Round Trip Batch**.
3. Reopen rebuilt assets in oW3DEdit and inspect the affected chunks and fields.
4. For rendering or animation changes, exercise the relevant viewport workflow,
   including playback or scrubbing where applicable.
5. When format compatibility is material, validate the saved copy in the target
   game or an independent W3D viewer if one is available.

For every failure, distinguish parse failures, semantic differences, binary
differences, crashes, and expected preservation differences. Do not weaken a
comparison or update an expected result merely to make a failing test pass
without explaining the underlying format reason.

If GUI-only validation cannot be automated in the current environment, complete
all safe static/build checks and provide concise manual steps rather than
claiming full verification.

## Change discipline

- Inspect `git status` before editing and preserve unrelated user changes.
- Keep changes focused; avoid opportunistic formatting or generated-file churn.
- Do not modify vendored libraries unless the task specifically requires it.
- Add new source, header, or `.ui` files to `oW3DDumpQt.vcxproj` and its filters
  when Visual Studio requires them.
- Prefer tests or minimal reproducible fixtures that can legally be committed.
  Never promote a private corpus file into a fixture without explicit approval.
- Update documentation when changing user-visible workflows, supported chunks,
  build prerequisites, or known limitations.

## Completion report

Summarize:

- the behavior changed and the invariant it preserves;
- files or subsystems affected;
- build configuration and result;
- round-trip corpus scope and result, if run;
- manual render/editor validation performed; and
- anything not verified, with the reason.
