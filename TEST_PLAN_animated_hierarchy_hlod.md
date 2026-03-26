# Test Plan: Animated Rename, Hierarchy Browser, HLOD Delete Sync

## Scope
This pass covers:
- animated object rename propagation
- hierarchy browser rename and mesh selection behavior
- automatic HLOD LOD cleanup when deleting meshes
- manual editing of hierarchy names
- JSON export of pivot fixup indices

## Test Environment
- Run on a Windows build of `main`
- Prefer testing in the editor plus external validation in W3D Viewer
- Use a known animated object sample such as `mpgdr_1`
- Use at least one file with HLOD data and one pure-animation `.w3d`

## 1. Animated Object Rename
### Steps
1. Open the known animated object sample `mpgdr_1`.
2. Rename the mesh/container so the saved result should become `mpgdr_2`.
3. Save the file.
4. Open the saved file in W3D Viewer.
5. Inspect the following chunks in the editor or exported JSON:
   - `CHUNK_MESH_HEADER3`
   - `CHUNK_HIERARCHY_HEADER`
   - `CHUNK_ANIMATION_HEADER` or compressed/morph variants
   - `CHUNK_HLOD_HEADER`

### Expected Results
- The saved file opens in W3D Viewer without crashing.
- `CHUNK_MESH_HEADER3.ContainerName` matches the new container name.
- `CHUNK_HIERARCHY_HEADER.Name` matches the new container name.
- Animation header `HierarchyName` matches the new container name.
- `CHUNK_HLOD_HEADER.Name` and `CHUNK_HLOD_HEADER.HierarchyName` match the new container name when they previously matched the old one.

## 2. Hierarchy Browser
### ROOTTRANSFORM Rename Guard
1. Open a file with hierarchy data.
2. Open the Hierarchy Browser.
3. Double-click `ROOTTRANSFORM`.

Expected:
- No rename prompt opens.

### Normal Pivot Rename
1. Double-click a non-root pivot.
2. Rename it to a new valid name.

Expected:
- Rename succeeds.
- The pivot label updates in the Hierarchy Browser.

### Proxy Pivot Rename Propagation
1. Use a file with HLOD proxy data.
2. Rename a pivot that has a matching HLOD proxy entry.
3. Save and re-open if needed.

Expected:
- The corresponding `HLOD_PROXY_ARRAY` subobject updates to the new pivot name.
- Matching should follow the pivot index and stay scoped to the correct hierarchy/HLOD object.

### Select Mesh
1. In the Hierarchy Browser, choose a mesh row under a pivot.
2. Click `Select Mesh`.
3. Repeat for both:
   - a normal HModel-backed mesh
   - an HLOD-only mesh binding if available

Expected:
- The chunk tree jumps to the actual chunk.
- Ancestor nodes expand so the selected chunk is visible.

## 3. Mesh Delete -> HLOD Cleanup
### Steps
1. Open a file where a mesh is referenced from `HLOD_LOD_ARRAY`.
2. Delete a `CHUNK_MESH` subtree that has a corresponding HLOD subobject.
3. Save the file.
4. Re-open the saved file and inspect HLOD data.
5. Open the file in W3D Viewer.

### Expected Results
- Matching `HLOD_SUB_OBJECT` entries under `HLOD_LOD_ARRAY` are removed automatically.
- Proxy-array entries are not deleted as part of this feature.
- HLOD header/array counts remain valid.
- The saved file stays viewable in W3D Viewer.

## 4. Manual Hierarchy Name Editing
### Hierarchy Header
1. Select a `CHUNK_HIERARCHY_HEADER`.
2. Edit `Hierarchy Name` in the editor panel.
3. Save and re-open the file.

Expected:
- The new hierarchy name persists.

### Animation Headers
1. Select each available animation header type:
   - `CHUNK_ANIMATION_HEADER`
   - `CHUNK_COMPRESSED_ANIMATION_HEADER`
   - `CHUNK_MORPHANIM_HEADER`
2. Edit `Hierarchy Name`.
3. Save and re-open the file.

Expected:
- The edited `HierarchyName` persists.

### Pure Animation File
1. Open a pure-animation `.w3d`.
2. Edit `Hierarchy Name` on the animation header.
3. Save As using a different base filename.
4. Re-open the saved file.

Expected:
- Animation header `Name` still follows the output filename.
- Edited `HierarchyName` is preserved.

## 5. JSON Export / Import
### Pivot Fixups Export
1. Export a file containing `PIVOT_FIXUPS` to JSON.
2. Inspect the `PIVOT_FIXUPS` array.

Expected:
- Each entry includes:
  - `PIVOT_INDEX`
  - `TM`
- `PIVOT_INDEX` is zero-based.

### Backward Compatibility
1. Import an older JSON file that has `PIVOT_FIXUPS[*].TM` but no `PIVOT_INDEX`.
2. Save and re-open.

Expected:
- Import still succeeds.
- No new requirement is introduced for `PIVOT_INDEX`.

## Out of Scope / Known Non-Goals
- JSON import still does not auto-repair `NUMPIVOTS` after pivot deletion.
- Mesh deletion cleanup is limited to `HLOD_LOD_ARRAY` and does not remove proxy-array entries.

## Suggested Report Format
- File used:
- Exact steps:
- Expected result:
- Actual result:
- Whether the saved file opens in W3D Viewer:
- If broken, which chunk names/fields differ from expected:
