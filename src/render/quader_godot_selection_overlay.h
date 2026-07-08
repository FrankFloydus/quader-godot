#pragma once

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <span>

namespace quader_godot::render {

struct OverlaySegment {
	godot::Vector3 a;
	godot::Vector3 b;
};

struct OverlayTriangle {
	godot::Vector3 a;
	godot::Vector3 b;
	godot::Vector3 c;
};

[[nodiscard]] godot::Ref<godot::Material> make_overlay_face_material(godot::Color color, bool draw_on_top = true,
		int render_priority = godot::Material::RENDER_PRIORITY_MAX);
[[nodiscard]] godot::Ref<godot::Material> make_overlay_line_material(godot::Color color, bool draw_on_top = true,
		int render_priority = godot::Material::RENDER_PRIORITY_MAX, float clip_depth_bias = 0.0f);
[[nodiscard]] godot::Ref<godot::Material> make_overlay_point_material(godot::Color color, bool draw_on_top = true,
		int render_priority = godot::Material::RENDER_PRIORITY_MAX, float clip_depth_bias = 0.0f);
void clear_overlay_material_cache();
[[nodiscard]] godot::Ref<godot::ArrayMesh> make_overlay_line_mesh(std::span<const OverlaySegment> segments,
		const godot::Camera3D *camera, godot::Vector2 viewport_size, float width_pixels, float depth_bias_pixels = 0.0f);
[[nodiscard]] godot::Ref<godot::ArrayMesh> make_overlay_point_mesh(std::span<const godot::Vector3> points,
		const godot::Camera3D *camera, godot::Vector2 viewport_size, float size_pixels, float depth_bias_pixels = 0.0f);
[[nodiscard]] godot::Ref<godot::ArrayMesh> make_overlay_face_mesh(std::span<const OverlayTriangle> triangles);

} // namespace quader_godot::render
