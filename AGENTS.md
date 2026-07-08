# Quader Godot Plugin Agent Guide

This repository is the Godot 4 C++ GDExtension port of the Quader modeling editor. Future work here
is not a generic Godot sample and not a new modeling app from scratch: it is a port of the Quader
app modeling suite into a Godot editor plugin, keeping behavior visually and semantically aligned
with the native Windows Quader app.

## Project Purpose

- Build a Godot editor plugin that exposes Quader modeling inside Godot.
- Keep the plugin implemented in C++ GDExtension code.
- Use GDScript only for the minimal Godot addon entry point where Godot requires it, such as
  `project/addons/quader/plugin.gd`.
- Port Quader viewport behavior, modeling SDK usage, selection, overlays, transforms, materials,
  grid rendering, settings, and navigation with parity against the native app.
- Do not invent behavior when a Quader reference exists. Check the native app first.

## Important Local Paths

- Plugin repository: `C:\Users\Drako\Desktop\quader-godot-editor\quader-godot`
- Native Quader Windows app reference: `C:\Users\Drako\Desktop\quader-windows\quader-app`
- Godot source reference: `C:\Users\Drako\Desktop\quader-godot-editor\godot-4.7`
- Godot executable: `C:\Users\Drako\Desktop\GODOT\Godot_v4.7-stable_win64_console.exe`

Use the native Quader app as the source of truth for viewport colors, overlay sizes, input behavior,
grid presets, material appearance, selection policy, transform behavior, and tool semantics. Use the
Godot source when renderer or GDExtension behavior is unclear.

## Repository Layout

- `src/editor`: Godot editor plugin host/window code, top bar, settings UI, and addon integration.
- `src/viewport`: Quader viewport control, camera navigation, input handling, picking, selection,
  transform actions, and runtime viewport orchestration.
- `src/render`: Godot render helpers for grid, material, overlays, transform gizmo, and viewport
  visual settings.
- `src/modeling`: Adapter between the plugin and the copied Quader modeling SDK.
- `src/quader_modeling_sdk`: Copied portable Quader modeling SDK. Read
  `src/quader_modeling_sdk/AGENTS.md` before editing this subtree.
- `project/addons/quader`: Godot addon files, plugin descriptor, icon, material textures, and the
  minimal `plugin.gd`.
- `project/bin/quader_godot.gdextension`: GDExtension descriptor.
- `godot-cpp`: Godot C++ bindings submodule.

## Build And Verification

Build the editor GDExtension from the repository root:

```powershell
scons platform=windows target=editor arch=x86_64
```

The build installs the DLL into:

```text
project/bin/windows/quader_godot.windows.editor.x86_64.dll
```

Run a fast Godot editor-load check after C++ or addon changes:

```powershell
& 'C:\Users\Drako\Desktop\GODOT\Godot_v4.7-stable_win64_console.exe' --headless --editor --path 'C:\Users\Drako\Desktop\quader-godot-editor\quader-godot\project' --quit
```

If Godot is already open, it may hold the GDExtension DLL or a temporary copied DLL. Close or reload
the editor/plugin before expecting a rebuilt DLL to be used. A headless check may fail with a DLL
copy/open error while a live Godot editor process still owns the extension.

Build artifacts, `.godot`, object files, and generated binaries are ignored and should not be
committed.

## Development Rules

- Prefer existing plugin patterns before adding new abstractions.
- Keep edits scoped to the owner package.
- Do not hand-roll modeling operations that already exist in the Quader modeling SDK.
- Use the SDK adapter for creating and mutating Quader meshes; rendering, picking, and Godot UI stay
  in the plugin layers.
- Do not add broad GDScript systems. C++ is the implementation language for the plugin.
- Do not use Godot scene lights for the Quader shaded viewport unless the native app behavior
  requires it. The plugin viewport is meant to mimic Quader's shaded viewport material path.
- Persist user-visible settings when settings UI changes behavior.
- Keep defaults aligned with the native Quader app unless the user explicitly changes them.

## Viewport Parity Priorities

The following areas are high-sensitivity parity work:

- Orbit, pan, fly navigation signs and mouse capture.
- Grid colors, background clear color, grid line thickness, grid presets, and F1-F12 grid sizing.
- Mesh material and mesh surface grid shader.
- Selection modes: `1` vertex, `2` edge, `3` face, `4` mesh.
- Sticky component source behavior, picking, deselection, Shift/Ctrl selection modifiers.
- Overlay colors, alpha, sizes, depth policy, selected/hover/remove colors, vertex quads, edge and
  face fills, selected object outline, and component source wire.
- Inside-out/flipped mesh behavior, including visibility and picking for internal components.
- Transform gizmo visuals, picking, move/rotate/scale keybinds, and snapping behavior.

When parity is in doubt, inspect the native Quader implementation in
`C:\Users\Drako\Desktop\quader-windows\quader-app` before changing plugin behavior.

## Current Plugin Behavior Notes

- The plugin adds a Quader button to the Godot editor and opens a custom Quader editor window.
- The Quader viewport uses a Godot `SubViewport` with custom environment/background, grid material,
  Quader mesh material, C++ overlays, selection, and transform gizmo rendering.
- The scene starts empty. Press `B`, drag a footprint on the ground grid, and release to create a
  Quader SDK box whose height equals the current grid size.
- Mesh normals can be flipped in mesh selection mode with `F`.
- Move/rotate/scale use grid/angle snapping by default and Ctrl disables snapping during gizmo drag.

## Git Workflow

- Main branch: `main`
- Remote: `https://github.com/FrankFloydus/quader-godot.git`
- Before committing, run at least:

```powershell
scons platform=windows target=editor arch=x86_64
```

- For C++/addon changes, also run the Godot headless editor-load check above.
- Do not commit ignored build outputs.
