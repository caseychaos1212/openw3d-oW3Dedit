# 🗺️ Open W3D Viewer & Editor — Development Roadmap

## ✅ Phase 1: Core File Viewer (MVP)
**Goal:** Load .w3d files and display their structure

| Feature           | Description                                     | Status         |
|------------------|-------------------------------------------------|----------------|
| Project skeleton | Qt6 app using CMake + C++20                     | ✅ Complete     |
| Chunk backend    | ChunkData, ChunkItem to parse .w3d files        | ✅ Implemented  |
| File open UI     | Menu bar + QFileDialog                          | ✅ Implemented  |
| Chunk tree viewer| Show chunk hierarchy recursively                | ✅ Implemented  |
| Chunk ID display | Display raw chunk IDs and sizes                 | ✅ Implemented  |

## 🚧 Phase 2: UX & Readability Improvements
**Goal:** Improve chunk display clarity and basic interactivity

| Feature               | Description                                                  |
|----------------------|--------------------------------------------------------------|
| Chunk ID to name map | Display CHUNK_MESH instead of raw ID via lookup              |
| Table view on select | Populate table with chunk field values                       |
| Hex view panel       | Show raw bytes in a hex viewer                               |
| Basic validation     | Warn about malformed chunks or unsupported versions          |

## 🔨 Phase 3: Editing Capabilities
**Goal:** Enable non-destructive editing of names and properties

| Feature               | Description                                               |
|----------------------|-----------------------------------------------------------|
| Rename fields        | Edit names like MeshName or HierarchyName                 |
| Save modified file   | Rebuild .w3d with updated chunk tree                      |
| Add/remove chunks    | UI for inserting or deleting or moving chunk blocks       |
| Unsaved change warn  | Prompt when unsaved edits exist                           |

## 🧪 Phase 4: Data Export Tools
**Goal:** Make .w3d content accessible for analysis or conversion

| Feature               | Description                                                   |
|----------------------|----------------------------------------------------------------|
| Export to JSON       | Serialize chunk tree and fields to structured JSON (metadata) |
| Export animation CSV | Frame-by-frame translation/rotation for each pivot            |
| Dependency report    | Cross-reference to skeletons, hierarchies, etc.               |

## 🎞️ Phase 5: Visualization
**Goal:** Visualize geometry or animation content

| Feature               | Description                                               |
|----------------------|-----------------------------------------------------------|
| Skeleton viewer      | Node graph of pivots and hierarchy                        |
| Animation player     | Frame playback of bone transforms                         |
| Simple mesh viewer   | Render geometry                                           |

## 🎯 Stretch Goals (Future Ideas)
| Feature               | Description                                               |
|----------------------|-----------------------------------------------------------|
| Batch tools          | CLI tools to batch process .w3d files                     |
| Format converter     | .fbx to .w3d using import logic                           |
| File diff tool       | Compare two .w3d files visually                           |
| W3dviwer Features    | add support for emitter creation and any other features   |
