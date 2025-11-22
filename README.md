# Open W3D Viewer & Editor — Development Roadmap

## Phase 1: Core File Viewer
**Goal:** Load .w3d files and display their structure

## Phase 2: UX & Readability Improvements
**Goal:** Improve chunk display clarity and basic interactivity

| Feature              | Description                                                  |
|----------------------|--------------------------------------------------------------|
| Chunk ID to name map | Display CHUNK_MESH instead of raw ID via lookup              |
| Table view on select | Populate table with chunk field values                       |
| Hex view panel       | Show raw bytes in a hex viewer                               |
| Basic validation     | Warn about malformed chunks or unsupported versions          |

## Phase 3: Editing Capabilities
**Goal:** Enable non-destructive editing of names and properties

| Feature              | Description                                               |
|----------------------|-----------------------------------------------------------|
| Rename fields        | Edit names like MeshName or HierarchyName                 |
| Save modified file   | Rebuild .w3d with updated chunk tree                      |
| Add/remove chunks    | UI for inserting or deleting or moving chunk blocks       |


## Phase 4: Data Export Tools
**Goal:** Make .w3d content accessible for analysis or conversion

| Feature              | Description                                                   |
|----------------------|----------------------------------------------------------------|
| Export to JSON       | Serialize chunk tree and fields to structured JSON (metadata) |
| Dependency reports   | Cross-reference to skeletons, hierarchies, etc.               |


## Stretch Goals (Future Ideas)
| Feature              | Description                                               |
|----------------------|-----------------------------------------------------------|
| Format converter     | .fbx to .w3d using import logic                           |
| W3dviewer Features   | add support for emitter creation and any other features   |

## Utilities

### Definition DB hash dump

`tools/ddb_hash_dump.cpp` is a small console helper that walks a `.ddb` file, extracts every definition name stored in `CHUNKID_VARIABLES`, computes the CRC32 hash the game uses, and writes a JSON mapping (`hash -> name`).

Usage:

```
g++ -std=c++17 tools/ddb_hash_dump.cpp backend/ChunkData.cpp -o ddb_hash_dump
./ddb_hash_dump /path/to/objects.ddb objects_hashes.json
```

You can feed the resulting JSON back into the viewer/exporter so hashed chunk IDs (e.g. `0x363D5EEA`) show the friendly name instead of the raw value.
