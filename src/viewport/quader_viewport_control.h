#pragma once

#include "modeling/quader_modeling_adapter.h"
#include "render/quader_godot_render_utils.h"
#include "render/quader_godot_transform_gizmo.h"
#include "viewport/quader_camera_controller.h"
#include "viewport/quader_viewport_selection_mode.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>

#include <cstdint>
#include <functional>
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

struct TransformDragBounds {
	bool has_bounds = false;
	godot::Vector3 min;
	godot::Vector3 max;
};

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
	[[nodiscard]] const render::ViewportVisualSettings &visual_settings() const;
	[[nodiscard]] int grid_preset() const;
	void set_visual_settings(const render::ViewportVisualSettings &settings);
	void set_grid_preset(int preset);
	void set_grid_preset_changed_callback(std::function<void(int)> callback);

protected:
	static void _bind_methods();

private:
	struct SceneMeshNode {
		quader::modeling::ObjectId object;
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
	bool select_at(godot::Vector2 position, quader::modeling::SelectionEdit edit);
	void set_transform_tool(render::TransformGizmoTool tool);
	[[nodiscard]] render::TransformGizmoInput transform_gizmo_input(
			std::span<const modeling::MeshObjectSnapshot> objects) const;
	[[nodiscard]] std::optional<godot::Vector3> selected_mesh_pivot(
			std::span<const modeling::MeshObjectSnapshot> objects) const;
	[[nodiscard]] TransformDragBounds selected_mesh_bounds(
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
	void handle_keyboard(double delta);
	void begin_fly();
	void end_fly();

	bool built_ = false;
	int grid_preset_ = 6;
	SelectionMode selection_mode_ = SelectionMode::Mesh;
	bool orbiting_ = false;
	bool panning_ = false;
	bool fly_active_ = false;
	bool has_hover_ = false;
	bool hover_remove_preview_ = false;
	bool overlays_dirty_ = true;
	bool transform_drag_active_ = false;
	bool box_tool_active_ = false;
	bool box_drag_active_ = false;
	bool box_preview_visible_ = false;
	modeling::SelectionTarget hover_target_;
	quader::modeling::ObjectId component_source_candidate_;
	godot::Vector3 box_raw_start_;
	godot::Vector3 box_raw_end_;
	BoxConstructionPlane box_plane_;
	BoxToolFootprint box_preview_;
	render::TransformGizmoTool transform_tool_ = render::TransformGizmoTool::None;
	render::TransformGizmoAxis gizmo_hover_axis_ = render::TransformGizmoAxis::None;
	render::TransformGizmoAxis gizmo_active_axis_ = render::TransformGizmoAxis::None;
	godot::Vector2 transform_drag_last_position_;
	godot::Vector3 transform_drag_start_pivot_;
	godot::Vector3 transform_drag_pivot_;
	godot::Vector3 transform_drag_unsnapped_move_;
	godot::Vector3 transform_drag_applied_move_;
	TransformDragBounds transform_drag_bounds_;
	float transform_drag_unsnapped_angle_ = 0.0f;
	float transform_drag_applied_angle_ = 0.0f;
	float transform_drag_unsnapped_scale_amount_ = 0.0f;
	float transform_drag_applied_scale_factor_ = 1.0f;
	render::ViewportVisualSettings visual_settings_ = render::default_viewport_visual_settings();
	godot::Ref<godot::Environment> environment_;
	godot::Ref<godot::ShaderMaterial> grid_material_;
	godot::Ref<godot::ShaderMaterial> mesh_material_;
	godot::SubViewportContainer *viewport_container_ = nullptr;
	godot::SubViewport *subviewport_ = nullptr;
	godot::WorldEnvironment *world_environment_ = nullptr;
	godot::Node3D *scene_root_ = nullptr;
	godot::Node3D *overlay_root_ = nullptr;
	godot::Camera3D *camera_ = nullptr;
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
	QuaderCameraController camera_controller_;
	modeling::QuaderModelingAdapter modeling_;
	std::vector<SceneMeshNode> scene_meshes_;
	std::function<void(int)> grid_preset_changed_callback_;
};

} // namespace quader_godot::viewport
