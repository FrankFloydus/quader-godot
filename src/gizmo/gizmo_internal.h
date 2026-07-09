#pragma once

#include "gizmo/gizmo.h"

#include <godot_cpp/variant/color.hpp>

#include <array>
#include <vector>

namespace quader_godot::gizmo {

inline constexpr float kGizmoEpsilon = 0.000001f;
inline constexpr float kGizmoScreenEpsilon = 0.0001f;

enum class GizmoComponent {
	X,
	Y,
	Z,
};

struct GizmoAxisPrimitive {
	GizmoHandle handle = GizmoHandle::None;
	godot::Vector3 world_start;
	godot::Vector3 world_tip;
	godot::Vector2 screen_anchor;
	float alpha = 1.0f;
};

struct GizmoPlanePrimitive {
	GizmoHandle handle = GizmoHandle::None;
	std::array<godot::Vector3, 4> world;
	std::array<godot::Vector2, 4> screen;
};

struct GizmoFrame {
	bool ok = false;
	GizmoHandle hovered_handle = GizmoHandle::None;
	GizmoHandle active_handle = GizmoHandle::None;
	godot::Vector3 pivot;
	godot::Vector2 screen_pivot;
	godot::Vector2 viewport_size;
	float scale = 1.0f;
	std::vector<GizmoAxisPrimitive> axes;
	std::vector<GizmoPlanePrimitive> planes;
};

struct GizmoLine {
	godot::Vector3 a;
	godot::Vector3 b;
	godot::Color color;
	float width_pixels = 1.0f;
};

struct GizmoTriangle {
	godot::Vector3 a;
	godot::Vector3 b;
	godot::Vector3 c;
	godot::Color color;
};

struct GizmoMeshBuilder {
	std::vector<GizmoLine> lines;
	std::vector<GizmoTriangle> triangles;
};

[[nodiscard]] float clamped_gizmo_scale(float scale);
[[nodiscard]] bool prepare_frame(const GizmoInput &input, GizmoFrame &frame);
[[nodiscard]] godot::Vector3 camera_right(const godot::Camera3D *camera);
[[nodiscard]] godot::Vector3 camera_up(const godot::Camera3D *camera);
[[nodiscard]] godot::Vector3 camera_forward(const godot::Camera3D *camera);
[[nodiscard]] godot::Vector3 axis_vector(GizmoHandle handle);
[[nodiscard]] godot::Vector3 fallback_perpendicular(godot::Vector3 axis);
[[nodiscard]] float world_units_per_pixel_at(const godot::Camera3D *camera, godot::Vector2 viewport_size,
		const godot::Vector3 &position);
[[nodiscard]] godot::Color html_color(const char *color);
[[nodiscard]] godot::Color multiply_color(godot::Color color, float rgb_scale, float alpha_scale = 1.0f);
[[nodiscard]] godot::Color color_for_axis(GizmoHandle handle);
[[nodiscard]] godot::Color color_for_plane(GizmoHandle handle);
[[nodiscard]] godot::Color highlight_color_for_axis(GizmoHandle handle);
[[nodiscard]] bool highlighted(const GizmoFrame &frame, GizmoHandle handle);
[[nodiscard]] float axis_alpha(const godot::Camera3D *camera, godot::Vector3 center, godot::Vector3 world_axis);
[[nodiscard]] bool project_point(const godot::Camera3D *camera, godot::Vector3 point, godot::Vector2 &screen);
void add_axis(GizmoFrame &frame, const godot::Camera3D *camera, GizmoHandle handle, godot::Vector3 center,
		float axis_length);
void add_plane(GizmoFrame &frame, const godot::Camera3D *camera, GizmoHandle handle, godot::Vector3 center,
		godot::Vector3 axis_a, godot::Vector3 axis_b, float offset, float half_size);
[[nodiscard]] float distance_to_segment_squared(godot::Vector2 point, godot::Vector2 a, godot::Vector2 b);
[[nodiscard]] bool point_in_convex_quad(godot::Vector2 point, const std::array<godot::Vector2, 4> &quad);
void append_line(GizmoMeshBuilder &builder, godot::Vector3 a, godot::Vector3 b, godot::Color color,
		float width_pixels);
void append_triangle(GizmoMeshBuilder &builder, godot::Vector3 a, godot::Vector3 b, godot::Vector3 c,
		godot::Color color);
void append_quad(GizmoMeshBuilder &builder, godot::Vector3 a, godot::Vector3 b, godot::Vector3 c,
		godot::Vector3 d, godot::Color color);
void append_plane(GizmoMeshBuilder &builder, const GizmoPlanePrimitive &plane, godot::Color color);
[[nodiscard]] godot::Ref<godot::ArrayMesh> make_line_mesh(const std::vector<GizmoLine> &lines,
		const godot::Camera3D *camera, godot::Vector2 viewport_size);
[[nodiscard]] godot::Ref<godot::ArrayMesh> make_triangle_mesh(const std::vector<GizmoTriangle> &triangles);
[[nodiscard]] bool handle_is_plane(GizmoHandle handle);
[[nodiscard]] std::array<godot::Vector3, 2> plane_axes(GizmoHandle handle);
[[nodiscard]] bool handle_includes_component(GizmoHandle handle, GizmoComponent component);
[[nodiscard]] bool drag_axis_includes(GizmoHandle active_handle, GizmoHandle handle);
[[nodiscard]] bool drag_plane_includes(GizmoHandle active_handle, GizmoHandle plane_handle);
[[nodiscard]] GizmoPickHit pick_axis_handle(const GizmoFrame &frame, godot::Vector2 screen_position,
		float pick_radius);
[[nodiscard]] float snap_to_step(float value, float step);
[[nodiscard]] float safe_grid_size(float grid_size);

} // namespace quader_godot::gizmo
