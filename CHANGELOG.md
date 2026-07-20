# Changelog

Notable changes to oW3DEdit are documented here.

## [0.6.0-alpha] - 2026-07-12

This release expands oW3DEdit from a chunk-focused utility into a more complete asset inspection, editing, and preview workflow.

### Highlights

- Added interactive model and material rendering with texture lookup.
- Added skeleton and animation loading, playback, scrubbing, multi-animation sessions, and GIF export.
- Added draft animation editing, clip overrides, pivot blending, scene manipulation, and undo/redo support.
- Added archive-aware loading for W3D assets and their render dependencies.
- Added JSON import/export hardening for byte-faithful round trips and unsupported chunks.
- Added batch JSON export and filename-matched batch JSON import.
- Added bulk chunk-list export and pure-animation batch copy/rename workflows.
- Improved hierarchy, pivot, mesh, animation, and HLOD rename synchronization.
- Restored and expanded texture, material, mesh, shader, surface, and transform editing.
- Added support for extended `max2w3d` vertex influences.
- Migrated static editor layouts to Qt Designer UI files for easier maintenance.
- Added application branding and improved import/save validation and error handling.

### Notes

- This remains an alpha release. Back up source assets before saving changes.
- Prebuilt packages target Windows x64.
- Some uncommon W3D chunks can be preserved and represented in JSON without having a dedicated graphical editor.

[0.6.0-alpha]: https://github.com/caseychaos1212/openw3d-oW3Dedit/compare/v0.5.0-alpha...v0.6.0-alpha
