# Quader Godot Plugin Refactor Plan

## 1. Current Architecture Summary

The current Godot plugin is a C++ GDExtension port of Quader with a mostly native-Godot host shell around a copied portable modeling SDK. The addon entry point stays minimal in `project/addons/quader/plugin.gd`, while C++ registration happens in `src/register_types.cpp`. `src/editor` owns the Godot editor toolbar/window host, `src/ui` builds Godot controls, `src/settings` persists viewport settings through Godot `ConfigFile`, `src/viewport` owns the Godot `Control` and `SubViewport` orchestration, `src/render` creates Godot resources and overlay meshes, and `src/modeling` wraps the copied Quader modeling SDK for plugin workflows.

There are already two good examples of reusable engine-neutral extraction:

- `src/camera` is plain C++ camera behavior with no Godot dependency, bridged by `src/viewport/godot_editor_camera_bridge.*`.
- `src/selection` is plain C++ component-source policy with direct unit coverage in `tests/viewport_component_source_policy_tests.cpp`.

The main architectural gap is that several other editor behaviors that should be reusable are still mixed into Godot host code. Most of that pressure collects in `src/viewport/quader_viewport_control.cpp`, which currently owns input routing, modeling commands, picking, overlay planning, box construction, mesh presentation, transform gizmo orchestration, dirty flags, and Godot node/resource lifetimes. `src/gizmo` is a root-level package but is not engine-neutral: its public APIs expose Godot vectors, cameras, keys, materials, and meshes.

## 2. Prioritized Smell And Risk List

### Must Fix

1. `QuaderViewportControl` is a God object and the highest regression risk.
   - References: `src/viewport/quader_viewport_control.cpp:88`, `src/viewport/quader_viewport_control.cpp:167`, `src/viewport/quader_viewport_control.cpp:595`, `src/viewport/quader_viewport_control.cpp:911`, `src/viewport/quader_viewport_control.cpp:1158`, `src/viewport/quader_viewport_control.cpp:1191`, `src/viewport/quader_viewport_control.cpp:1404`, `src/viewport/quader_viewport_control.cpp:1541`, `src/viewport/quader_viewport_control.cpp:1722`.
   - It mixes keyboard mapping, modifiers, box tool geometry, selection target helpers, ray picking, Godot overlay nodes, input event routing, mesh sync, overlay composition, hover/select policy, transform gizmo drag, and grid preset state.
   - Risk: every viewport feature change can accidentally affect selection, camera navigation, overlays, mesh revisions, or tool state. This violates SRP in a practical way and makes behavior hard to test without Godot.

2. `src/gizmo` is placed like a reusable package but is Godot-coupled.
   - References: `src/gizmo/gizmo.h:5`, `src/gizmo/gizmo_drag_session.h:6`, `src/gizmo/gizmo_mutation.h:3`, `src/gizmo/gizmo_registry.h:5`, `src/gizmo/gizmo.cpp:61`, `src/gizmo/gizmo.cpp:325`, `src/gizmo/gizmo.cpp:512`, `src/gizmo/move_gizmo.cpp:196`, `src/gizmo/rotate_gizmo.cpp:235`, `src/gizmo/scale_gizmo.cpp:338`.
   - Public gizmo contracts expose `godot::Camera3D`, `godot::Vector2`, `godot::Vector3`, `godot::ArrayMesh`, `godot::Material`, and `godot::Key`.
   - Risk: transform tool behavior cannot be reused outside Godot and cannot be tested as plain Quader editor behavior. This conflicts with the repository rule that reusable root-level packages should be plain C++.

3. Modeling selection has parallel truth that is not isolated or directly tested.
   - References: `src/modeling/quader_modeling_adapter.h:83`, `src/modeling/quader_modeling_adapter.cpp:58`, `src/modeling/quader_modeling_adapter.cpp:178`, `src/modeling/quader_modeling_adapter.cpp:261`, `src/modeling/quader_modeling_adapter.cpp:288`, `src/modeling/quader_modeling_adapter.cpp:317`.
   - The adapter owns `mesh_selection_` and `active_object_` beside the SDK selection summary. This may be necessary because mesh selection and component selection have different editor meanings, but the invariant is implicit.
   - Risk: overlays, transform gizmos, flip normals, and component source selection can drift if SDK selection and adapter mesh selection are updated in different orders.

4. The SDK boundary is build-system porous.
   - References: `CMakeLists.txt:55`, `CMakeLists.txt:66`, `CMakeLists.txt:70`, `CMakeLists.txt:71`, `SConstruct:40`, `SConstruct:43`, `SConstruct:44`, `SConstruct:57`.
   - The plugin target recursively compiles all `src/*.cpp` files and exposes `src/quader_modeling_sdk/internal` and `internal/thirdparty` globally.
   - Risk: plugin host code can include SDK internals by accident, and SDK portability can regress without a local failure. No current non-SDK file appears to include SDK internals, but the build allows it.

### Should Improve

5. Picking and overlay planning are embedded in the viewport host.
   - References: `src/viewport/quader_viewport_control.cpp:528`, `src/viewport/quader_viewport_control.cpp:567`, `src/viewport/quader_viewport_control.cpp:595`, `src/viewport/quader_viewport_control.cpp:667`, `src/viewport/quader_viewport_control.cpp:769`, `src/viewport/quader_viewport_control.cpp:810`, `src/viewport/quader_viewport_control.cpp:1191`.
   - Risk: selection hit testing and overlay visibility policy cannot be covered with fast unit tests, and future parity fixes will require editing Godot host code.

6. Settings fields are repeated across four owners.
   - References: `src/viewport/quader_viewport_visual_settings.h:7`, `src/viewport/quader_viewport_visual_settings.cpp:7`, `src/settings/quader_viewport_settings_store.cpp:53`, `src/settings/quader_viewport_settings_store.cpp:137`, `src/ui/quader_viewport_settings_window.cpp:83`, `src/ui/quader_viewport_settings_window.cpp:114`, `src/editor/quader_editor_window.cpp:37`, `src/editor/quader_editor_window.cpp:220`.
   - Risk: adding, renaming, clamping, or migrating a visual setting requires edits in the state struct, defaults, load path, save path, settings UI, and `QuaderEditorWindow` setter bindings. This is duplication, not useful separation.

7. Render code depends on the modeling adapter for SDK payload types.
   - Reference: `src/render/quader_godot_render_utils.h:3`.
   - `render` should consume SDK render payloads or small presenter inputs, not the plugin workflow adapter. The current include direction makes render depend on a higher-level modeling facade.

8. Shortcut and input policy is split across viewport, camera bridge, and gizmo registry.
   - References: `src/viewport/quader_viewport_control.cpp:104`, `src/viewport/quader_viewport_control.cpp:131`, `src/viewport/quader_viewport_control.cpp:911`, `src/viewport/godot_editor_camera_bridge.cpp:44`, `src/gizmo/gizmo_registry.cpp:21`.
   - Risk: Quader parity shortcuts can conflict or drift because raw Godot keys are interpreted in several places. The camera bridge pattern is good, but the semantic command mapping is not centralized.

9. Godot resource caches are static globals spread across render and gizmo code.
   - References: `src/render/quader_godot_selection_overlay.cpp:48`, `src/render/quader_godot_selection_overlay.cpp:331`, `src/gizmo/gizmo.cpp:52`, `src/gizmo/gizmo.cpp:557`, `src/register_types.cpp:30`.
   - Risk: editor reload lifetime is handled manually today. It works only if every cache is remembered during GDExtension teardown.

10. Build/test ownership does not match package ownership.
   - References: `CMakeLists.txt:121`, `tests/reusable_package_guardrail_tests.cpp:18`.
   - Tests currently compile `src/camera` and `src/selection` only. New root reusable packages can be added without automatically being compiled or protected by guardrails.

### Optional

11. Visual style constants are scattered.
   - References: `src/viewport/quader_viewport_visual_settings.cpp:7`, `src/gizmo/gizmo.cpp:35`, `src/gizmo/rotate_gizmo.cpp:29`, `src/gizmo/scale_gizmo.cpp:37`, `src/ui/components/organism/quader_top_bar.cpp:39`, `src/ui/components/organism/quader_bottom_bar.cpp:28`.
   - This is not an immediate design failure, but parity-sensitive colors and sizes should eventually have a single named home per domain.

12. UI component helpers are functional but not yet descriptor-driven.
   - References: `src/ui/quader_viewport_settings_window.cpp:39`, `src/ui/quader_viewport_settings_window.cpp:68`.
   - Current UI factories are acceptable for now. Avoid turning them into a UI framework; only deduplicate where settings drift is already real.

## 3. Proposed Target Package Structure

Keep the Godot plugin simple: plain C++ reusable cores plus concrete Godot bridges. Do not add generic engine interfaces, DI containers, abstract factories, or host-agnostic renderer layers before a second host exists.

Proposed structure:

```text
src/
  camera/                      Existing plain C++ camera core.
  selection/                   Existing plain C++ selection/component-source policy.
  modeling/                    Plain C++ plugin workflow adapter over the SDK.
    mesh_selection_state.*     Explicit adapter-level mesh selection and active-object invariant.
  picking/                     Plain C++ ray, hit-test, occlusion, and SelectionTarget picking.
  box_tool/                    Plain C++ box construction plane, footprint, snapping, and commit payload prep.
  overlay/                     Plain C++ overlay plan builder from snapshots, selection mode, hover, and settings values.
  gizmo/                       Plain C++ transform gizmo handles, frame math, picking, drag math, draw plan, mutations.
  viewport/                    Godot Control, SubViewport, input adapters, camera bridge, tool orchestration.
  render/                      Godot ArrayMesh/material/environment presenters and resource caches.
    godot_overlay_renderer.*   Converts overlay plans to Godot overlay meshes/materials.
    godot_gizmo_renderer.*     Converts gizmo draw plans to Godot meshes/materials.
    godot_mesh_presenter.*     Owns MeshInstance3D scene mesh sync.
  settings/                    Godot settings persistence plus a narrow viewport setting schema.
  ui/                          Godot UI construction only.
  editor/                      Godot editor plugin host and window shell.
  quader_modeling_sdk/         Copied SDK, built and guarded as a separate portable boundary.
```

Package rules:

- Root reusable packages may depend on the C++ standard library and public `quader/modeling` headers when needed.
- Root reusable packages must not include `godot_cpp`, use `godot::` types, allocate Godot resources, register Godot classes, or include Godot host packages.
- `viewport`, `render`, `ui`, `settings`, and `editor` remain Godot host packages and may use Godot APIs directly.
- `modeling` stays engine-neutral but host-facing. It should not become a second SDK, and it should not expose SDK internals.

## 4. Refactor Slices In Order

### Slice 1: Lock Baseline Guardrails Before Moving Behavior

Scope:
Add tests that describe the intended boundaries before any large file moves.

Files likely touched:
`tests/reusable_package_guardrail_tests.cpp`, `CMakeLists.txt`, possibly a new `tests/sdk_boundary_guardrail_tests.cpp`.

Expected outcome:
The test suite explicitly protects `src/camera` and `src/selection` as reusable packages, and adds guardrails for future reusable packages without changing runtime behavior.

Verification/tests:

- Build `quader_godot_tests`.
- Run `ReusablePackageGuardrailTests.*`.
- Add a guardrail that scans non-SDK files and fails if they include `src/quader_modeling_sdk/internal`, `internal/mesh`, `internal/io`, or SDK third-party headers.

### Slice 2: Extract Viewport Picking Into `src/picking`

Scope:
Move ray and selection-hit logic out of `QuaderViewportControl` while preserving existing hit ordering and occlusion behavior.

Files likely touched:
`src/viewport/quader_viewport_control.cpp`, `src/viewport/quader_viewport_control.h`, new `src/picking/viewport_picking.h`, new `src/picking/viewport_picking.cpp`, new tests under `tests/`.

Extract candidates:
`Ray`, `PickHit`, `ray_triangle_intersection`, `point_ray_distance`, `segment_ray_distance`, `component_pick_occluded`, `pick_face_target`, `pick_vertex_target`, `pick_edge_target`, authored face/edge/center helpers currently around `src/viewport/quader_viewport_control.cpp:390` through `src/viewport/quader_viewport_control.cpp:831`.

Expected outcome:
`QuaderViewportControl` provides camera-derived ray inputs and receives a `SelectionTarget` result. The core decides target priority and occlusion without Godot types.

Verification/tests:

- Face picking returns the nearest visible face.
- Vertex and edge picking respect source-object filtering and surface occlusion.
- Inside-out/authored face cases preserve current behavior.
- Remove-preview picking still allows selected components to be targeted.

### Slice 3: Extract Box Tool Construction Into `src/box_tool`

Scope:
Move box construction plane, footprint snapping, height policy, and corner creation out of the viewport host.

Files likely touched:
`src/viewport/quader_viewport_control.cpp`, `src/viewport/quader_viewport_control.h`, new `src/box_tool/box_tool.h`, new `src/box_tool/box_tool.cpp`, new tests.

Extract candidates:
`BoxConstructionPlane`, `BoxToolFootprint`, `safe_box_grid_size`, `directional_snap_pair`, `project_point_to_plane`, `make_box_construction_plane`, `box_plane_point`, `intersect_ray_plane`, `make_box_tool_footprint`, currently around `src/viewport/quader_viewport_control.cpp:167` through `src/viewport/quader_viewport_control.cpp:314`.

Expected outcome:
Viewport code only decides when the box tool is active and which ray/seed surface is used. The reusable core owns snap math, plane basis, valid footprint detection, and corner order.

Verification/tests:

- Dragging a zero-size footprint is invalid.
- Footprints snap directionally around the drag start.
- Box height equals the active grid size.
- Construction on a face uses the face normal and produces stable corner ordering.

### Slice 4: Make Modeling Adapter Selection State Explicit

Scope:
Keep behavior stable but isolate `mesh_selection_` and `active_object_` into a small plain C++ state helper with direct tests.

Files likely touched:
`src/modeling/quader_modeling_adapter.h`, `src/modeling/quader_modeling_adapter.cpp`, new `src/modeling/mesh_selection_state.h`, new `src/modeling/mesh_selection_state.cpp`, new tests.

Expected outcome:
The adapter still delegates operations to `ModelingApi`, but object-level editor selection is an explicit concept instead of incidental vector manipulation. The active-object rule is documented by tests.

Verification/tests:

- Replace/add/remove/toggle object selection updates tracked mesh selection and active object exactly as current behavior.
- Creating a box selects and activates the created mesh.
- Component selection keeps the source object active as today.
- Removing the last component keeps or clears object selection according to current behavior.
- Transform/flip selected mesh calls fail predictably when no mesh is selected.

### Slice 5: Extract Overlay Plan Composition Into `src/overlay`

Scope:
Move overlay semantic planning out of `QuaderViewportControl::refresh_overlays`; keep Godot mesh/material creation in `src/render`.

Files likely touched:
`src/viewport/quader_viewport_control.cpp`, `src/render/quader_godot_selection_overlay.*`, new `src/overlay/viewport_overlay_plan.h`, new `src/overlay/viewport_overlay_plan.cpp`, tests.

Expected outcome:
The overlay core receives mesh snapshots, selection mode, hover target, component source candidate, box preview, and visual size/color values or style IDs. It returns semantic arrays such as selected faces, selected wires, hover wires, vertex points, source wires, and box preview segments. Render code converts those arrays to Godot meshes.

Verification/tests:

- Mesh mode selected object emits selected face fill and wire records.
- Vertex mode emits source wire plus vertex handles for the same scenarios covered by component-source tests.
- Hover and remove-preview colors/styles select the correct overlay buckets.
- Box preview overlay emits dashed edge segments only when visible.

### Slice 6: Split Gizmo Core From Godot Rendering And Input Types

Scope:
Turn `src/gizmo` into a plain C++ transform gizmo core. Move Godot `ArrayMesh`, `Material`, `Camera3D`, `Key`, and `Color` usage into concrete Godot adapter/presenter files under `src/render` and `src/viewport`.

Files likely touched:
`src/gizmo/gizmo.h`, `src/gizmo/gizmo_drag_session.h`, `src/gizmo/gizmo_mutation.h`, `src/gizmo/gizmo_registry.*`, `src/gizmo/gizmo.cpp`, `src/gizmo/move_gizmo.cpp`, `src/gizmo/rotate_gizmo.cpp`, `src/gizmo/scale_gizmo.cpp`, new `src/render/godot_gizmo_renderer.*`, new `src/viewport/godot_gizmo_input_adapter.*`, new tests.

Expected outcome:
`src/gizmo` exposes plain structs such as `GizmoInput`, `GizmoCameraFrame`, `GizmoDrawPlan`, `GizmoLine`, `GizmoTriangle`, `GizmoPickHit`, and `GizmoMutation` using plain numeric vectors. Godot code adapts camera pose/projection and converts the draw plan into meshes/materials.

Verification/tests:

- Move, rotate, and scale picking chooses the same handle for representative screen points.
- Move snapping applies grid steps and preserves unsnapped accumulated drag.
- Rotate snapping uses the current 15-degree step.
- Scale snapping respects minimum factors and selected bounds.
- Guardrail test adds `src/gizmo` to reusable packages and rejects all Godot symbols there.

### Slice 7: Slim `QuaderViewportControl` To Host Orchestration

Scope:
After picking, box tool, overlay planning, modeling selection state, and gizmo core are extracted, reduce `QuaderViewportControl` to Godot lifecycle and orchestration.

Files likely touched:
`src/viewport/quader_viewport_control.h`, `src/viewport/quader_viewport_control.cpp`, new `src/render/godot_mesh_presenter.*`, possibly new `src/viewport/quader_viewport_tool_state.*`.

Expected outcome:
The viewport control owns:

- Godot notifications and `_gui_input`.
- SubViewport/scene root setup.
- Calling camera bridge, tool cores, modeling adapter, and render presenters.
- Dirty flags and redraw scheduling.

It should not own:

- Ray triangle math.
- Overlay list composition.
- Box footprint math.
- Gizmo drag math.
- SDK selection invariants.
- Godot mesh resource construction details beyond presenter calls.

Verification/tests:

- Existing camera and selection tests still pass.
- New picking, box, overlay, modeling adapter, and gizmo tests pass.
- `scons platform=windows target=editor arch=x86_64`.
- Godot headless editor-load check after C++ host changes.

### Slice 8: Replace Repeated Settings Wiring With A Narrow Schema

Scope:
Introduce a small viewport settings schema that enumerates each setting once with key, label, kind, getter/setter, limits, and alpha policy. Keep it local to viewport settings, not a generic settings framework.

Files likely touched:
`src/viewport/quader_viewport_visual_settings.*`, `src/settings/quader_viewport_settings_store.*`, `src/ui/quader_viewport_settings_window.*`, `src/editor/quader_editor_window.*`, tests.

Expected outcome:
Adding a visual setting requires one descriptor entry plus the field/default itself. Load, save, UI row creation, and editor callbacks stop duplicating every field manually.

Verification/tests:

- A settings schema coverage test verifies every persisted field is described.
- Load/save round trip preserves all fields.
- Background alpha remains forced to 1.0.
- Legacy version migration for vertex sizes remains covered.

### Slice 9: Tighten Build Targets Around Package Boundaries

Scope:
Stop treating all of `src` as one undifferentiated source pile. Separate the copied SDK and reusable core tests from Godot host code where practical.

Files likely touched:
`CMakeLists.txt`, `SConstruct`, possibly small source-list helper files.

Expected outcome:
The SDK can be compiled as its own target or source group, plugin host code no longer gets broad SDK internal include directories by default, and reusable package tests compile plain C++ packages without linking Godot.

Verification/tests:

- `quader_godot_tests` builds without Godot host packages for reusable package coverage.
- Plugin target still builds with SCons.
- Guardrail confirms non-SDK files do not include SDK internals.
- Source registration is explicit enough that adding a new package requires intentional ownership.

## 5. Guardrail Tests To Add Or Update

1. Expand `ReusablePackageGuardrailTests`.
   - Add `src/modeling` once `mesh_selection_state` is plain and does not require Godot.
   - Add `src/picking`, `src/box_tool`, `src/overlay`, and `src/gizmo` as each package becomes engine-neutral.
   - Keep `src/camera` and `src/selection` as current positive examples.

2. Add an SDK boundary guardrail.
   - Scan all non-SDK source files.
   - Fail on includes of `src/quader_modeling_sdk/internal`, `internal/mesh`, `internal/io`, `internal/diagnostics`, `internal/math`, or SDK third-party headers.
   - Allow only public `#include <quader/modeling/...>` outside the SDK subtree.

3. Add a render dependency guardrail.
   - Fail if `src/render` includes `modeling/quader_modeling_adapter.h`.
   - Allow render to include public SDK payload headers or a renderer-neutral presenter input.

4. Add reusable package build coverage.
   - The test target should compile every `.cpp` in reusable packages, not only `src/camera/editor_camera.cpp` and `src/selection/component_source_policy.cpp`.

5. Add behavior tests for extracted cores.
   - `EditorCameraTests` already covers camera behavior.
   - Add `PickingTests`, `BoxToolTests`, `OverlayPlanTests`, `MeshSelectionStateTests`, and `GizmoCoreTests`.

6. Add settings schema coverage after slice 8.
   - Verify every `ViewportVisualSettings` field with persistence is represented in the schema.
   - Verify load/save round trips include all schema fields.

7. Add an input routing parity guardrail after shortcut extraction.
   - Verify Quader shortcuts for selection modes `1` to `4`, box `B`, transform `W/R/S/Q`, grid `F1` to `F10`, flip `F`, and navigation capture are registered in one semantic mapping table.

## 6. Explicit Non-Goals

- Do not rewrite the plugin or replace the current Godot editor window.
- Do not introduce a generic engine abstraction layer, DI container, abstract renderer, service locator, or factory hierarchy.
- Do not move all viewport behavior at once. Extract one tested behavior slice at a time.
- Do not change Quader modeling behavior unless a test proves current behavior is wrong.
- Do not invent tool, shortcut, grid, overlay, material, or selection semantics without checking the native Quader app.
- Do not expose SDK internals to plugin host packages to make a refactor easier.
- Do not replace the copied SDK with a different dependency strategy as part of this refactor plan.
- Do not add broad GDScript systems. Keep C++ as the implementation language.
- Do not turn UI factories into a general UI framework. Use descriptors only where repeated settings wiring already creates drift risk.
- Do not optimize file size by hiding behavior behind vague abstractions. Prefer small concrete owners with direct tests.

