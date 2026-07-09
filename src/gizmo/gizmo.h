#pragma once

#include "gizmo/gizmo_drag_session.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <memory>
#include <string_view>

namespace quader_godot::gizmo {

struct GizmoMeshes {
	godot::Ref<godot::ArrayMesh> lines;
	godot::Ref<godot::ArrayMesh> triangles;
};

struct GizmoInput {
	GizmoHandle hovered_handle = GizmoHandle::None;
	GizmoHandle active_handle = GizmoHandle::None;
	bool has_selection = false;
	bool has_pivot = false;
	godot::Vector3 pivot;
	const godot::Camera3D *camera = nullptr;
	godot::Vector2 viewport_size;
	float scale = 1.0f;
};

class Gizmo {
public:
	virtual ~Gizmo() = default;

	[[nodiscard]] virtual std::string_view id() const = 0;
	[[nodiscard]] virtual GizmoPickHit pick(const GizmoInput &input, godot::Vector2 screen_position) const = 0;
	[[nodiscard]] virtual GizmoMeshes draw(const GizmoInput &input) const = 0;
	[[nodiscard]] virtual std::unique_ptr<GizmoDragSession> begin_drag(const GizmoDragStart &start) const = 0;
};

[[nodiscard]] GizmoMeshes make_empty_gizmo_meshes();
[[nodiscard]] godot::Ref<godot::Material> make_gizmo_line_material();
[[nodiscard]] godot::Ref<godot::Material> make_gizmo_triangle_material();
void clear_gizmo_material_cache();

} // namespace quader_godot::gizmo
