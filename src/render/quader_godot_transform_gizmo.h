#pragma once

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace quader_godot::render {

enum class TransformGizmoTool {
	None,
	Move,
	Rotate,
	Scale,
};

enum class TransformGizmoAxis {
	None,
	X,
	Y,
	Z,
	XY,
	XZ,
	YZ,
	All,
};

struct TransformGizmoInput {
	TransformGizmoTool tool = TransformGizmoTool::None;
	TransformGizmoAxis hover_axis = TransformGizmoAxis::None;
	TransformGizmoAxis active_axis = TransformGizmoAxis::None;
	bool has_selection = false;
	bool has_pivot = false;
	godot::Vector3 pivot;
	const godot::Camera3D *camera = nullptr;
	godot::Vector2 viewport_size;
	float gizmo_scale = 1.0f;
};

struct TransformGizmoPickHit {
	bool hit = false;
	TransformGizmoAxis axis = TransformGizmoAxis::None;
	godot::Vector2 screen_tip;
	float distance = 0.0f;
};

struct TransformGizmoMeshes {
	godot::Ref<godot::ArrayMesh> lines;
	godot::Ref<godot::ArrayMesh> triangles;
};

[[nodiscard]] TransformGizmoPickHit pick_transform_gizmo(const TransformGizmoInput &input, godot::Vector2 screen_position);
[[nodiscard]] TransformGizmoMeshes make_transform_gizmo_meshes(const TransformGizmoInput &input);
[[nodiscard]] godot::Ref<godot::Material> make_transform_gizmo_line_material();
[[nodiscard]] godot::Ref<godot::Material> make_transform_gizmo_triangle_material();
void clear_transform_gizmo_material_cache();

} // namespace quader_godot::render
