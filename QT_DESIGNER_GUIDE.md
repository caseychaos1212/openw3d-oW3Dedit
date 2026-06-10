# Qt Designer Workflow

The project now uses Qt Designer `.ui` files for static widget layout while C++
continues to own behavior, data population, dynamic pages, and the render
viewport.

## Open A Form

From a Developer PowerShell prompt:

```powershell
C:\Qt\6.9.0\msvc2022_64\bin\designer.exe MainWindow.ui
```

Useful starting forms:

- `MainWindow.ui`: menus, actions, main splitter, chunk tabs, and details shell.
- `RenderPanel.ui`: render splitter, viewport host, controls, and render tabs.
- `ui/HierarchyHeaderEditorWidget.ui`: small editor form for initial practice.

## Guided Checkpoint 1: Main Window

1. Open `MainWindow.ui`.
2. Use **Object Inspector** to find `mainSplitter`, `detailSplitter`, and
   `renderPane`.
3. Select a widget and inspect its `objectName` in **Property Editor**.
4. Preview the form with **Form > Preview**.
5. Change a harmless layout spacing value, save, build, and inspect the result.

Stable `objectName` values are C++ interfaces. Rename one only when the matching
C++ binding is updated in the same change.

## Guided Checkpoint 2: Render Panel

1. Open `RenderPanel.ui`.
2. Find `renderViewportHost`. The custom `RenderViewportWidget` is inserted into
   this host at runtime and should not be replaced in Designer.
3. Change a label or spacing value and rebuild.
4. Confirm render controls, tabs, animation selection, and playback still work.

## Guided Checkpoint 3: Editor Form

1. Open `ui/HierarchyHeaderEditorWidget.ui`.
2. Inspect the form layout, `nameEdit`, and `applyButton`.
3. Compare those names with the constructor bindings in `mainWindow.cpp`.
4. Adjust the layout without changing the widget names, then rebuild.

## Build And Generated Files

The Visual Studio project registers forms as `QtUic` items. Qt/MSBuild runs
`uic` automatically and places generated `ui_*.h` files in the build
intermediate directory.

Build from PowerShell:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  oW3DEdit.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m
```

Commit `.ui` files. Never edit or commit generated `ui_*.h` files.

## Ownership Rules

- Designer owns static layout, standard controls, menus, actions, and defaults.
- C++ owns signal connections, application behavior, runtime data, and dynamic
  widget creation.
- Reusable parameterized editors such as string, raw-text, and mapper-args
  editors remain code-built.
- Material basic flags remain data-driven and are inserted into the
  Designer-owned `basicFlagsLayout`.
