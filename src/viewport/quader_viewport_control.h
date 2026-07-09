#pragma once

#include "gizmo/gizmo_drag_session.h"
#include "gizmo/gizmo.h"
#include "modeling/quader_modeling_adapter.h"
#include "viewport/godot_editor_camera_bridge.h"
#include "viewport/quader_viewport_selection_mode.h"
#include "viewport/quader_viewport_visual_settings.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace godot {
class Camera3D;
class Environment;
class MeshInstance3D;
class Node3D;
class ShaderMaterial;
class SubViewport;
class SubViewportContainer;
class WorldEnvironment;
} // namespace godot

namespace quader_godot::viewport {

using quader::modeling::ObjectId;
using quader::modeling::SelectionEdit;

struct BoxConstructionPlane {
	godot::Vector3 origin;
	godot::Vector3 snap_origin;
	godot::Vector3 normal{0.0f, 1.0f, 0.0f};
	godot::Vector3 axis_u{1.0f, 0.0f, 0.0f};
	godot::Vector3 axis_v{0.0f, 0.0f, -1.0f};
};

struct BoxToolFootprint {
	bool valid = false;
	godot::Vector3 corners[8];
};

class QuaderViewportControl : public godot::Control {
	GDCLASS(QuaderViewportControl, godot::Control)

public:
	QuaderViewportControl() = default;
	~QuaderViewportControl() override = default;

	void release_mouse_capture();
	void _notification(int what);
	void _gui_input(const godot::Ref<godot::InputEvent> &event) override;
	void _process(double delta) override;
	[[nodiscard]] const ViewportVisualSettings &visual_settings() const;
	[[nodiscard]] int grid_preset() const;
	void set_visual_settings(const ViewportVisualSettings &settings);
	void set_grid_preset(int preset);
	void set_grid_preset_changed_callback(std::function<void(int)> callback);

protected:
	static void _bind_methods();

private:
	struct SceneMeshNode {
		ObjectId object;
		std::uint64_t content_revision = 0;
		godot::MeshInstance3D *instance = nullptr;
	};

	void build_viewport();
	void update_subviewport_size();
	void update_camera();
	void refresh_scene_meshes();
	void refresh_overlays();
	void refresh_overlays_if_dirty();
	void invalidate_overlays();
	void request_viewport_redraw();
	void clear_hover();
	void update_hover(godot::Vector2 position, bool remove_preview);
	bool select_at(godot::Vector2 position, SelectionEdit edit);
	void set_active_gizmo(const gizmo::Gizmo *gizmo);
	[[nodiscard]] gizmo::GizmoMutationResult apply_gizmo_mutation(const gizmo::GizmoMutation &mutation);
	[[nodiscard]] gizmo::GizmoInput transform_gizmo_input(
			std::span<const modeling::MeshObjectSnapshot> objects) const;
	[[nodiscard]] std::optional<godot::Vector3> selected_mesh_pivot(
			std::span<const modeling::MeshObjectSnapshot> objects) const;
	[[nodiscard]] gizmo::GizmoSelectionBounds selected_mesh_bounds(
			std::span<const modeling::MeshObjectSnapshot> objects) const;
	bool begin_transform_drag(godot::Vector2 position);
	void update_transform_drag(godot::Vector2 position);
	void end_transform_drag();
	void update_transform_gizmo_hover(godot::Vector2 position);
	void activate_box_tool();
	void cancel_box_tool();
	bool begin_box_drag(godot::Vector2 position);
	void update_box_drag(godot::Vector2 position);
	void update_box_hover(godot::Vector2 position);
	void commit_box_drag(godot::Vector2 position);
	[[nodiscard]] std::optional<godot::Vector3> box_construction_point(godot::Vector2 position, bool seed_plane);
	[[nodiscard]] bool update_box_preview(godot::Vector3 raw_start, godot::Vector3 raw_end);

	bool built_ = false;
	int grid_preset_ = 6;
	SelectionMode selection_mode_ = SelectionMode::Mesh;
	bool has_hover_ = false;
	bool hover_remove_preview_ = false;
	bool overlays_dirty_ = true;
	bool box_tool_active_ = false;
	bool box_drag_active_ = false;
	bool box_preview_visible_ = false;
	modeling::SelectionTarget hover_target_;
	ObjectId component_source_candidate_;
	godot::Vector3 box_raw_start_;
	godot::Vector3 box_raw_end_;
	BoxConstructionPlane box_plane_;
	BoxToolFootprint box_preview_;
	const gizmo::Gizmo *active_gizmo_ = nullptr;
	gizmo::GizmoHandle hovered_gizmo_handle_ = gizmo::GizmoHandle::None;
	std::unique_ptr<gizmo::GizmoDragSession> active_gizmo_drag_;
	ViewportVisualSettings visual_settings_ = default_viewport_visual_settings();
	godot::Ref<godot::Environment> environment_;
	godot::Ref<godot::ShaderMaterial> grid_material_;
	godot::Ref<godot::ShaderMaterial> mesh_material_;
	godot::SubViewportContainer *viewport_container_ = nullptr;
	godot::SubViewport *subviewport_ = nullptr;
	godot::WorldEnvironment *world_environment_ = nullptr;
	godot::Node3D *scene_root_ = nullptr;
	godot::Node3D *overlay_root_ = nullptr;
	godot::MeshInstance3D *selection_face_overlay_ = nullptr;
	godot::MeshInstance3D *hover_face_overlay_ = nullptr;
	godot::MeshInstance3D *source_wire_overlay_ = nullptr;
	godot::MeshInstance3D *selection_wire_overlay_ = nullptr;
	godot::MeshInstance3D *hover_wire_overlay_ = nullptr;
	godot::MeshInstance3D *vertex_overlay_ = nullptr;
	godot::MeshInstance3D *selected_vertex_outline_overlay_ = nullptr;
	godot::MeshInstance3D *selected_vertex_overlay_ = nullptr;
	godot::MeshInstance3D *hover_vertex_outline_overlay_ = nullptr;
	godot::MeshInstance3D *hover_vertex_overlay_ = nullptr;
	godot::MeshInstance3D *box_preview_wire_overlay_ = nullptr;
	godot::MeshInstance3D *transform_gizmo_line_overlay_ = nullptr;
	godot::MeshInstance3D *transform_gizmo_triangle_overlay_ = nullptr;
	GodotEditorCameraBridge camera_bridge_;
	modeling::QuaderModelingAdapter modeling_;
	std::vector<SceneMeshNode> scene_meshes_;
	std::function<void(int)> grid_preset_changed_callback_;
};

} // namespace quader_godot::viewport
