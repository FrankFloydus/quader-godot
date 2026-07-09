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
- Keep reusable Quader editor behavior engine-neutral when it can reasonably survive a later Unity,
  Unreal, or native host port. Godot code is the current host integration, not the permanent home
  for portable editor logic.

## Engine-Neutral Core Policy

This plugin should increasingly separate reusable Quader editor behavior from Godot host code, in
the same spirit as the copied modeling SDK. If a feature is intended to be portable later, its core
must live in a root-level domain package under `src/` and build from plain C++ without Godot
headers.

- Reusable package headers and sources must not include `godot_cpp`, use `godot::` types, inherit
  Godot classes, call `memnew`, register Godot classes, poll Godot input, or allocate Godot
  resources.
- Reusable packages may depend on the C++ standard library and the Quader modeling SDK when the
  domain genuinely needs modeling IDs, payloads, or operations.
- Godot-specific conversion, input polling, node ownership, material creation, mesh construction,
  camera application, editor-window behavior, settings persistence, and addon registration belong
  in Godot bridge, adapter, presenter, or host packages outside reusable packages.
- Do not bury reusable code inside Godot host packages such as viewport, render, editor, UI, or
  settings. Host packages wire reusable cores to Godot.
- Avoid generic engine interfaces, dependency-injection containers, or abstract factories until a
  second host exists. Prefer a plain reusable core plus one concrete Godot bridge.
- When adding a reusable feature, make the copy-paste target obvious: copying that root package
  should not require also copying Godot viewport, render, UI, editor, or settings code.

## Important Local Paths

- Plugin repository: `C:\Users\Drako\Desktop\quader-godot-editor\quader-godot`
- Native Quader Windows app reference: `C:\Users\Drako\Desktop\quader-windows\quader-app`
- Godot source reference: `C:\Users\Drako\Desktop\quader-godot-editor\godot-4.7`
- Godot executable: `C:\Users\Drako\Desktop\GODOT\Godot_v4.7-stable_win64_console.exe`

Use the native Quader app as the source of truth for viewport colors, overlay sizes, input behavior,
grid presets, material appearance, selection policy, transform behavior, and tool semantics. Use the
Godot source when renderer or GDExtension behavior is unclear.

## Repository Layout

- Reusable root-level packages under `src/`: Engine-neutral Quader editor behavior. These packages
  should be plain C++ and should expose semantic inputs and outputs rather than Godot types.
- `src/editor`: Godot editor plugin host/window code and addon integration. Editor window classes
  compose UI and viewport owners; they should not own widget construction or settings
  serialization.
- `src/ui`: Small Godot UI factories and controls such as the Quader top bar and viewport settings
  window.
- `src/settings`: Persistent plugin settings stores and migration logic.
- `src/viewport`: Godot viewport host orchestration. This package may own `Control`, `SubViewport`,
  viewport input precedence, Godot event handling, and wiring between reusable cores, modeling,
  render presenters, and UI.
- `src/render`: Godot render helpers and presenters for grid, material, overlays, and other
  Godot-owned visual resources.
- `src/modeling`: Adapter around the copied Quader modeling SDK for plugin/editor workflows. This
  package is plain C++ today, but it is host-facing adapter code, not a replacement for the SDK
  source of truth.
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
- Before adding a new package or moving code, decide whether the behavior is reusable core or Godot
  host glue. Put reusable core at a root-level package. Put Godot bridge/presenter/adapter code in
  viewport, render, editor, UI, settings, or an explicitly Godot-named package.
- Do not introduce Godot types into reusable package public APIs for convenience. Add narrow
  conversion helpers in the Godot bridge instead.
- Prefer semantic inputs at reusable boundaries, such as camera move input, viewport rays, selection
  edits, gizmo mutations, or overlay records. Raw Godot events stay at the Godot host boundary.
- When a current package mixes reusable and Godot code, split it by behavior instead of renaming the
  whole package. The reusable side should become plain C++; the Godot side should be a bridge or
  presenter.

## Architecture Guardrails

Add or update guardrail tests when creating reusable packages or moving engine-neutral code.
Guardrails should fail when reusable package files include Godot headers, use Godot symbols, inherit
Godot classes, allocate Godot resources, register Godot classes, or include Godot host packages.
Behavior tests for reusable packages should exercise deterministic core behavior; Godot editor-load
checks verify bridge wiring separately.

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
