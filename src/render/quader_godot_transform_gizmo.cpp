#include "render/quader_godot_transform_gizmo.h"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace quader_godot::render {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = kPi * 2.0f;
constexpr float kGizmoCircleSize = 1.1f;
constexpr float kGizmoPlaneSize = 0.2f;
constexpr float kGizmoPlaneDistance = 0.3f;
constexpr float kGizmoArrowSize = 0.35f;
constexpr float kGizmoArrowOffset = kGizmoCircleSize + 0.3f;
constexpr float kGizmoArrowTip = kGizmoArrowOffset + kGizmoArrowSize;
constexpr float kGizmoArrowConeRadius = 0.065f;
constexpr float kGizmoScaleOffset = kGizmoCircleSize + 0.3f;
constexpr float kGizmoScaleTip = kGizmoScaleOffset * 1.11f;
constexpr float kGizmoScaleHalfWidth = 0.07f;
constexpr float kGizmoViewRotationScale = 1.14f;
constexpr float kTransformGizmoAxisHideScore = 0.02f;
constexpr float kTransformGizmoAxisFullScore = 0.1f;
constexpr float kTransformGizmoSizePixels = 80.0f;
constexpr float kTransformGizmoPickRadiusPixels = 12.0f;
constexpr float kTransformGizmoScaleCenterSizePixels = 10.0f;
constexpr float kTransformGizmoAxisLineWidthPixels = 1.6f;
constexpr int kConeSegments = 16;
constexpr int kRingSegments = 64;

constexpr char kAxisXColor[] = "#f53352e6";
constexpr char kAxisYColor[] = "#87d603e6";
constexpr char kAxisZColor[] = "#298cf5e6";
constexpr char kAxisMixedColor[] = "#eef2f7e6";
constexpr char kAxisXHighlightColor[] = "#ffccd3ff";
constexpr char kAxisYHighlightColor[] = "#e6ffc0ff";
constexpr char kAxisZHighlightColor[] = "#cae4ffff";
constexpr char kAxisHighlightColor[] = "#fffaccff";
constexpr char kTrackballColor[] = "#d2d6db8a";
constexpr char kScaleCenterOutlineColor[] = "#fff680b4";
constexpr char kWhiteColor[] = "#ffffffff";

struct GizmoAxisPrimitive {
	TransformGizmoAxis axis = TransformGizmoAxis::None;
	godot::Vector3 world_start;
	godot::Vector3 world_tip;
	godot::Vector2 screen_tip;
	float alpha = 1.0f;
};

struct GizmoPlanePrimitive {
	TransformGizmoAxis axis = TransformGizmoAxis::None;
	std::array<godot::Vector3, 4> world;
	std::array<godot::Vector2, 4> screen;
};

struct GizmoRingSegment {
	TransformGizmoAxis axis = TransformGizmoAxis::None;
	godot::Vector3 world_start;
	godot::Vector3 world_end;
	godot::Vector2 screen_start;
	godot::Vector2 screen_end;
	bool front_facing = true;
};

struct GizmoFrame {
	bool ok = false;
	TransformGizmoTool tool = TransformGizmoTool::None;
	TransformGizmoAxis hover_axis = TransformGizmoAxis::None;
	TransformGizmoAxis active_axis = TransformGizmoAxis::None;
	godot::Vector3 pivot;
	godot::Vector2 screen_pivot;
	godot::Vector2 viewport_size;
	float gizmo_scale = 1.0f;
	std::vector<GizmoAxisPrimitive> axes;
	std::vector<GizmoPlanePrimitive> planes;
	std::vector<GizmoRingSegment> ring_segments;
	std::vector<GizmoRingSegment> trackball_segments;
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

struct GizmoMaterialCache {
	godot::Ref<godot::Shader> line_shader;
	godot::Ref<godot::Shader> triangle_shader;
	godot::Ref<godot::Material> line_material;
	godot::Ref<godot::Material> triangle_material;
};

GizmoMaterialCache &material_cache() {
	static GizmoMaterialCache cache;
	return cache;
}

float clamped_gizmo_scale(float scale) {
	return std::clamp(scale, 0.5f, 2.0f);
}

godot::Vector3 camera_right(const godot::Camera3D *camera) {
	return camera->get_global_transform().basis.get_column(0).normalized();
}

godot::Vector3 camera_up(const godot::Camera3D *camera) {
	return camera->get_global_transform().basis.get_column(1).normalized();
}

godot::Vector3 camera_forward(const godot::Camera3D *camera) {
	return -camera->get_global_transform().basis.get_column(2).normalized();
}

float world_units_per_pixel_at(const godot::Camera3D *camera, godot::Vector2 viewport_size,
		const godot::Vector3 &position) {
	const float height = std::max(viewport_size.y, 1.0f);
	if (camera->get_projection() == godot::Camera3D::PROJECTION_ORTHOGONAL) {
		return static_cast<float>(camera->get_size()) / height;
	}

	const godot::Vector3 camera_position = camera->get_global_transform().origin;
	const float view_depth = std::max((position - camera_position).dot(camera_forward(camera)), 0.001f);
	const float fov_radians = static_cast<float>(camera->get_fov()) * kPi / 180.0f;
	return 2.0f * view_depth * std::tan(fov_radians * 0.5f) / height;
}

godot::Vector3 axis_vector(TransformGizmoAxis axis) {
	if (axis == TransformGizmoAxis::X) {
		return {1.0f, 0.0f, 0.0f};
	}
	if (axis == TransformGizmoAxis::Y) {
		return {0.0f, 1.0f, 0.0f};
	}
	if (axis == TransformGizmoAxis::Z) {
		return {0.0f, 0.0f, 1.0f};
	}
	return {};
}

godot::Vector3 fallback_perpendicular(godot::Vector3 axis) {
	godot::Vector3 side = axis.cross({0.0f, 1.0f, 0.0f});
	if (side.length_squared() <= 0.000001f) {
		side = axis.cross({1.0f, 0.0f, 0.0f});
	}
	return side.length_squared() <= 0.000001f ? godot::Vector3{0.0f, 0.0f, 1.0f} : side.normalized();
}

godot::Color html_color(const char *color) {
	return godot::Color(godot::String(color));
}

float scaled_channel(float value, float scale) {
	return std::clamp(value * scale, 0.0f, 1.0f);
}

godot::Color multiply_color(godot::Color color, float rgb_scale, float alpha_scale = 1.0f) {
	return {scaled_channel(color.r, rgb_scale), scaled_channel(color.g, rgb_scale),
			scaled_channel(color.b, rgb_scale), scaled_channel(color.a, alpha_scale)};
}

godot::Color color_for_axis(TransformGizmoAxis axis) {
	if (axis == TransformGizmoAxis::X) {
		return html_color(kAxisXColor);
	}
	if (axis == TransformGizmoAxis::Y) {
		return html_color(kAxisYColor);
	}
	if (axis == TransformGizmoAxis::Z) {
		return html_color(kAxisZColor);
	}
	return html_color(kAxisMixedColor);
}

godot::Color color_for_plane(TransformGizmoAxis axis) {
	if (axis == TransformGizmoAxis::XY) {
		return html_color(kAxisZColor);
	}
	if (axis == TransformGizmoAxis::XZ) {
		return html_color(kAxisYColor);
	}
	if (axis == TransformGizmoAxis::YZ) {
		return html_color(kAxisXColor);
	}
	return html_color(kWhiteColor);
}

godot::Color highlight_color_for_axis(TransformGizmoAxis axis) {
	if (axis == TransformGizmoAxis::X || axis == TransformGizmoAxis::YZ) {
		return html_color(kAxisXHighlightColor);
	}
	if (axis == TransformGizmoAxis::Y || axis == TransformGizmoAxis::XZ) {
		return html_color(kAxisYHighlightColor);
	}
	if (axis == TransformGizmoAxis::Z || axis == TransformGizmoAxis::XY) {
		return html_color(kAxisZHighlightColor);
	}
	return html_color(kAxisHighlightColor);
}

bool axis_matches(TransformGizmoAxis expected, TransformGizmoAxis actual) {
	return expected != TransformGizmoAxis::None && expected == actual;
}

bool highlighted(const GizmoFrame &frame, TransformGizmoAxis axis) {
	return axis_matches(frame.active_axis, axis) || axis_matches(frame.hover_axis, axis);
}

godot::Color authored_axis_color(const GizmoFrame &frame, TransformGizmoAxis axis, float alpha = 1.0f) {
	return highlighted(frame, axis) ? highlight_color_for_axis(axis) : multiply_color(color_for_axis(axis), 1.0f, alpha);
}

bool drag_axis_includes(TransformGizmoAxis active_axis, TransformGizmoAxis axis) {
	if (active_axis == TransformGizmoAxis::None || active_axis == axis) {
		return true;
	}
	if (active_axis == TransformGizmoAxis::XY) {
		return axis == TransformGizmoAxis::X || axis == TransformGizmoAxis::Y;
	}
	if (active_axis == TransformGizmoAxis::XZ) {
		return axis == TransformGizmoAxis::X || axis == TransformGizmoAxis::Z;
	}
	if (active_axis == TransformGizmoAxis::YZ) {
		return axis == TransformGizmoAxis::Y || axis == TransformGizmoAxis::Z;
	}
	return false;
}

bool drag_plane_includes(TransformGizmoAxis active_axis, TransformGizmoAxis plane_axis) {
	return active_axis == TransformGizmoAxis::None || active_axis == plane_axis;
}

float axis_alpha(const godot::Camera3D *camera, godot::Vector3 center, godot::Vector3 world_axis) {
	if (world_axis.length_squared() <= 0.000001f) {
		return 0.0f;
	}
	const godot::Vector3 view_direction = camera->get_global_transform().origin - center;
	if (view_direction.length_squared() <= 0.000001f) {
		return 1.0f;
	}
	const float score = std::clamp(1.0f - std::abs(world_axis.normalized().dot(view_direction.normalized())), 0.0f, 1.0f);
	if (score <= kTransformGizmoAxisHideScore) {
		return 0.0f;
	}
	if (score >= kTransformGizmoAxisFullScore) {
		return 1.0f;
	}
	return (score - kTransformGizmoAxisHideScore) / (kTransformGizmoAxisFullScore - kTransformGizmoAxisHideScore);
}

bool project_point(const godot::Camera3D *camera, godot::Vector3 point, godot::Vector2 &screen) {
	if (camera == nullptr || camera->is_position_behind(point)) {
		return false;
	}
	screen = camera->unproject_position(point);
	return true;
}

void add_axis(GizmoFrame &frame, const godot::Camera3D *camera, TransformGizmoAxis axis, godot::Vector3 center,
		float axis_length) {
	const godot::Vector3 direction = axis_vector(axis);
	if (direction.length_squared() <= 0.000001f) {
		return;
	}
	const godot::Vector3 tip = center + direction * axis_length;
	godot::Vector2 screen_start;
	godot::Vector2 screen_tip;
	if (!project_point(camera, center, screen_start) || !project_point(camera, tip, screen_tip)) {
		return;
	}
	if (screen_start.distance_squared_to(screen_tip) <= 0.0001f) {
		return;
	}
	frame.axes.push_back({
			.axis = axis,
			.world_start = center,
			.world_tip = tip,
			.screen_tip = screen_tip,
			.alpha = axis_alpha(camera, center, direction),
	});
}

void add_plane(GizmoFrame &frame, const godot::Camera3D *camera, TransformGizmoAxis axis, godot::Vector3 center,
		godot::Vector3 axis_a, godot::Vector3 axis_b, float offset, float half_size) {
	const godot::Vector3 a = axis_a.length_squared() <= 0.000001f ? godot::Vector3{1.0f, 0.0f, 0.0f} : axis_a.normalized();
	const godot::Vector3 b = axis_b.length_squared() <= 0.000001f ? godot::Vector3{0.0f, 1.0f, 0.0f} : axis_b.normalized();
	const godot::Vector3 handle_center = center + (a + b) * offset;
	GizmoPlanePrimitive plane;
	plane.axis = axis;
	plane.world = {
			handle_center - a * half_size - b * half_size,
			handle_center + a * half_size - b * half_size,
			handle_center + a * half_size + b * half_size,
			handle_center - a * half_size + b * half_size,
	};
	for (std::size_t index = 0; index < plane.world.size(); ++index) {
		if (!project_point(camera, plane.world[index], plane.screen[index])) {
			return;
		}
	}
	frame.planes.push_back(plane);
}

void add_ring(GizmoFrame &frame, const godot::Camera3D *camera, TransformGizmoAxis axis, godot::Vector3 center,
		float screen_size) {
	const float radius = std::max(0.001f, world_units_per_pixel_at(camera, frame.viewport_size, center) * std::max(24.0f, screen_size));
	godot::Vector3 ring_a;
	godot::Vector3 ring_b;
	if (axis == TransformGizmoAxis::X) {
		ring_a = {0.0f, 1.0f, 0.0f};
		ring_b = {0.0f, 0.0f, 1.0f};
	} else if (axis == TransformGizmoAxis::Y) {
		ring_a = {1.0f, 0.0f, 0.0f};
		ring_b = {0.0f, 0.0f, 1.0f};
	} else {
		ring_a = {1.0f, 0.0f, 0.0f};
		ring_b = {0.0f, 1.0f, 0.0f};
	}
	const godot::Vector3 view_direction = (camera->get_global_transform().origin - center).normalized();
	for (int index = 0; index < kRingSegments; ++index) {
		const float a0 = kTau * static_cast<float>(index) / static_cast<float>(kRingSegments);
		const float a1 = kTau * static_cast<float>(index + 1) / static_cast<float>(kRingSegments);
		const float mid_angle = (a0 + a1) * 0.5f;
		const godot::Vector3 p0 = center + (ring_a * std::cos(a0) + ring_b * std::sin(a0)) * radius;
		const godot::Vector3 p1 = center + (ring_a * std::cos(a1) + ring_b * std::sin(a1)) * radius;
		const godot::Vector3 pm = center + (ring_a * std::cos(mid_angle) + ring_b * std::sin(mid_angle)) * radius;
		godot::Vector2 screen0;
		godot::Vector2 screen1;
		if (!project_point(camera, p0, screen0) || !project_point(camera, p1, screen1)) {
			continue;
		}
		frame.ring_segments.push_back({
				.axis = axis,
				.world_start = p0,
				.world_end = p1,
				.screen_start = screen0,
				.screen_end = screen1,
				.front_facing = view_direction.length_squared() <= 0.000001f ||
						(pm - center).normalized().dot(view_direction) >= -0.01f,
		});
	}
}

void add_trackball_ring(GizmoFrame &frame, const godot::Camera3D *camera, godot::Vector3 center, float screen_size) {
	const float radius = std::max(0.001f, world_units_per_pixel_at(camera, frame.viewport_size, center) *
					std::max(24.0f, screen_size * kGizmoViewRotationScale));
	godot::Vector3 right = camera_right(camera);
	godot::Vector3 up = camera_up(camera);
	up = (up - right * up.dot(right)).normalized();
	for (int index = 0; index < kRingSegments; ++index) {
		const float a0 = kTau * static_cast<float>(index) / static_cast<float>(kRingSegments);
		const float a1 = kTau * static_cast<float>(index + 1) / static_cast<float>(kRingSegments);
		const godot::Vector3 p0 = center + (right * std::cos(a0) + up * std::sin(a0)) * radius;
		const godot::Vector3 p1 = center + (right * std::cos(a1) + up * std::sin(a1)) * radius;
		godot::Vector2 screen0;
		godot::Vector2 screen1;
		if (!project_point(camera, p0, screen0) || !project_point(camera, p1, screen1)) {
			continue;
		}
		frame.trackball_segments.push_back({
				.axis = TransformGizmoAxis::All,
				.world_start = p0,
				.world_end = p1,
				.screen_start = screen0,
				.screen_end = screen1,
		});
	}
}

GizmoFrame build_frame(const TransformGizmoInput &input) {
	GizmoFrame frame;
	frame.tool = input.tool;
	frame.hover_axis = input.hover_axis;
	frame.active_axis = input.active_axis;
	frame.pivot = input.pivot;
	frame.gizmo_scale = clamped_gizmo_scale(input.gizmo_scale);
	frame.viewport_size = input.viewport_size;
	if (!input.has_selection || !input.has_pivot || input.camera == nullptr || input.tool == TransformGizmoTool::None) {
		return frame;
	}
	if (!project_point(input.camera, input.pivot, frame.screen_pivot)) {
		return frame;
	}

	const float unit_screen_size = std::max(1.0f, kTransformGizmoSizePixels) * frame.gizmo_scale;
	frame.ok = true;
	if (input.tool == TransformGizmoTool::Rotate) {
		const float ring_screen_size = unit_screen_size * kGizmoCircleSize;
		add_trackball_ring(frame, input.camera, input.pivot, ring_screen_size);
		add_ring(frame, input.camera, TransformGizmoAxis::X, input.pivot, ring_screen_size);
		add_ring(frame, input.camera, TransformGizmoAxis::Y, input.pivot, ring_screen_size);
		add_ring(frame, input.camera, TransformGizmoAxis::Z, input.pivot, ring_screen_size);
		return frame;
	}

	const float pixel_world = world_units_per_pixel_at(input.camera, input.viewport_size, input.pivot);
	const float axis_length = pixel_world * unit_screen_size *
			(input.tool == TransformGizmoTool::Scale ? kGizmoScaleTip : kGizmoArrowTip);
	const float plane_offset = pixel_world * unit_screen_size * (kGizmoPlaneDistance + kGizmoPlaneSize * 0.5f);
	const float plane_half_size = pixel_world * unit_screen_size * (kGizmoPlaneSize * 0.5f);
	add_axis(frame, input.camera, TransformGizmoAxis::X, input.pivot, axis_length);
	add_axis(frame, input.camera, TransformGizmoAxis::Y, input.pivot, axis_length);
	add_axis(frame, input.camera, TransformGizmoAxis::Z, input.pivot, axis_length);
	add_plane(frame, input.camera, TransformGizmoAxis::XY, input.pivot, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
			plane_offset, plane_half_size);
	add_plane(frame, input.camera, TransformGizmoAxis::XZ, input.pivot, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
			plane_offset, plane_half_size);
	add_plane(frame, input.camera, TransformGizmoAxis::YZ, input.pivot, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
			plane_offset, plane_half_size);
	return frame;
}

float distance_to_segment_squared(godot::Vector2 point, godot::Vector2 a, godot::Vector2 b) {
	const godot::Vector2 segment = b - a;
	const float length_squared = segment.length_squared();
	if (length_squared <= 0.0001f) {
		return point.distance_squared_to(a);
	}
	const float t = std::clamp((point - a).dot(segment) / length_squared, 0.0f, 1.0f);
	return point.distance_squared_to(a + segment * t);
}

bool point_in_convex_quad(godot::Vector2 point, const std::array<godot::Vector2, 4> &quad) {
	float sign = 0.0f;
	for (std::size_t index = 0; index < quad.size(); ++index) {
		const godot::Vector2 a = quad[index];
		const godot::Vector2 b = quad[(index + 1U) % quad.size()];
		const godot::Vector2 edge = b - a;
		const godot::Vector2 to_point = point - a;
		const float cross = edge.x * to_point.y - edge.y * to_point.x;
		if (std::abs(cross) <= 0.001f) {
			continue;
		}
		if (sign == 0.0f) {
			sign = cross > 0.0f ? 1.0f : -1.0f;
		} else if ((cross > 0.0f ? 1.0f : -1.0f) != sign) {
			return false;
		}
	}
	return true;
}

TransformGizmoPickHit pick_move_axis(godot::Vector2 screen_position, godot::Vector2 screen_center,
		const GizmoFrame &frame, float pick_radius) {
	TransformGizmoPickHit hit;
	const float radius_squared = pick_radius * pick_radius;
	float best_distance = std::numeric_limits<float>::max();
	const GizmoAxisPrimitive *best = nullptr;
	for (const GizmoAxisPrimitive &axis : frame.axes) {
		const float distance = screen_position.distance_squared_to(axis.screen_tip);
		if (distance <= radius_squared && distance < best_distance) {
			best_distance = distance;
			best = &axis;
		}
	}
	for (const GizmoAxisPrimitive &axis : frame.axes) {
		if (best != nullptr) {
			break;
		}
		const float distance = distance_to_segment_squared(screen_position, screen_center, axis.screen_tip);
		if (distance <= radius_squared && distance < best_distance) {
			best_distance = distance;
			best = &axis;
		}
	}
	if (best == nullptr) {
		return hit;
	}
	hit.hit = true;
	hit.axis = best->axis;
	hit.screen_tip = best->screen_tip;
	hit.distance = std::sqrt(best_distance);
	return hit;
}

TransformGizmoPickHit pick_scale_axis(godot::Vector2 screen_position, const GizmoFrame &frame, float pick_radius) {
	const float screen_size = std::max(1.0f, kTransformGizmoSizePixels) * frame.gizmo_scale;
	const float center_safe_radius =
			std::max(pick_radius, std::min(std::max(24.0f, screen_size) * 0.35f, std::max(pick_radius * 1.75f, 12.0f)));
	const float center_distance_squared = screen_position.distance_squared_to(frame.screen_pivot);
	if (center_distance_squared <= center_safe_radius * center_safe_radius) {
		return {
				.hit = true,
				.axis = TransformGizmoAxis::All,
				.screen_tip = {frame.screen_pivot.x + std::max(24.0f, screen_size), frame.screen_pivot.y},
				.distance = std::sqrt(center_distance_squared),
		};
	}
	return pick_move_axis(screen_position, frame.screen_pivot, frame, pick_radius);
}

TransformGizmoPickHit pick_rotate_axis(godot::Vector2 screen_position, const GizmoFrame &frame, float pick_radius) {
	TransformGizmoPickHit hit;
	float best_distance = pick_radius * pick_radius;
	for (const GizmoRingSegment &segment : frame.ring_segments) {
		const float distance = distance_to_segment_squared(screen_position, segment.screen_start, segment.screen_end);
		if (distance < best_distance) {
			best_distance = distance;
			hit.hit = true;
			hit.axis = segment.axis;
			hit.screen_tip = segment.screen_start;
			hit.distance = std::sqrt(distance);
		}
	}
	return hit;
}

void append_line(GizmoMeshBuilder &builder, godot::Vector3 a, godot::Vector3 b, godot::Color color,
		float width_pixels) {
	builder.lines.push_back({
			.a = a,
			.b = b,
			.color = color,
			.width_pixels = std::max(width_pixels, 1.0f),
	});
}

void append_triangle(GizmoMeshBuilder &builder, godot::Vector3 a, godot::Vector3 b, godot::Vector3 c,
		godot::Color color) {
	builder.triangles.push_back({
			.a = a,
			.b = b,
			.c = c,
			.color = color,
	});
}

void append_quad(GizmoMeshBuilder &builder, godot::Vector3 a, godot::Vector3 b, godot::Vector3 c, godot::Vector3 d,
		godot::Color color) {
	append_triangle(builder, a, b, c, color);
	append_triangle(builder, a, c, d, color);
}

void append_plane(GizmoMeshBuilder &builder, const GizmoPlanePrimitive &plane, godot::Color color) {
	append_quad(builder, plane.world[0], plane.world[1], plane.world[2], plane.world[3], color);
	append_line(builder, plane.world[0], plane.world[1], color, 1.2f);
	append_line(builder, plane.world[1], plane.world[2], color, 1.2f);
	append_line(builder, plane.world[2], plane.world[3], color, 1.2f);
	append_line(builder, plane.world[3], plane.world[0], color, 1.2f);
}

void append_cone_handle(GizmoMeshBuilder &builder, godot::Vector3 base_center, godot::Vector3 tip,
		godot::Color color, float radius_world) {
	godot::Vector3 axis = tip - base_center;
	const float cone_length = axis.length();
	if (cone_length <= 0.000001f || radius_world <= 0.000001f) {
		return;
	}
	axis /= cone_length;
	godot::Vector3 u = fallback_perpendicular(axis);
	godot::Vector3 v = axis.cross(u);
	if (v.length_squared() <= 0.000001f) {
		v = fallback_perpendicular(u);
	} else {
		v.normalize();
	}
	u = v.cross(axis).normalized();
	std::array<godot::Vector3, kConeSegments> base_points{};
	for (int index = 0; index < kConeSegments; ++index) {
		const float angle = kTau * static_cast<float>(index) / static_cast<float>(kConeSegments);
		base_points[static_cast<std::size_t>(index)] = base_center + (u * std::cos(angle) + v * std::sin(angle)) * radius_world;
	}
	for (int index = 0; index < kConeSegments; ++index) {
		const int next = (index + 1) % kConeSegments;
		const godot::Vector3 a = base_points[static_cast<std::size_t>(index)];
		const godot::Vector3 b = base_points[static_cast<std::size_t>(next)];
		append_triangle(builder, tip, a, b, color);
		append_triangle(builder, base_center, b, a, color);
	}
}

void append_arrow_handle(GizmoMeshBuilder &builder, godot::Vector3 start, godot::Vector3 tip, godot::Color color,
		float line_width_pixels) {
	godot::Vector3 direction = tip - start;
	const float axis_length = direction.length();
	if (axis_length <= 0.000001f) {
		return;
	}
	direction /= axis_length;
	const float unit_world = axis_length / kGizmoArrowTip;
	const godot::Vector3 cone_base = start + direction * (unit_world * kGizmoArrowOffset);
	append_line(builder, start, cone_base, color, line_width_pixels);
	append_cone_handle(builder, cone_base, tip, color, unit_world * kGizmoArrowConeRadius);
}

void append_square_prism(GizmoMeshBuilder &builder, godot::Vector3 start, godot::Vector3 end, float half_width_world,
		godot::Color color) {
	godot::Vector3 axis = end - start;
	const float length_world = axis.length();
	if (length_world <= 0.000001f || half_width_world <= 0.000001f) {
		return;
	}
	axis /= length_world;
	godot::Vector3 side_a = fallback_perpendicular(axis);
	godot::Vector3 side_b = axis.cross(side_a);
	if (side_b.length_squared() <= 0.000001f) {
		side_b = {0.0f, 1.0f, 0.0f};
	} else {
		side_b.normalize();
	}
	side_a = side_b.cross(axis).normalized();
	const std::array<godot::Vector3, 4> start_corners{
			start + side_a * half_width_world + side_b * half_width_world,
			start - side_a * half_width_world + side_b * half_width_world,
			start - side_a * half_width_world - side_b * half_width_world,
			start + side_a * half_width_world - side_b * half_width_world,
	};
	const std::array<godot::Vector3, 4> end_corners{
			end + side_a * half_width_world + side_b * half_width_world,
			end - side_a * half_width_world + side_b * half_width_world,
			end - side_a * half_width_world - side_b * half_width_world,
			end + side_a * half_width_world - side_b * half_width_world,
	};
	for (int index = 0; index < 4; ++index) {
		const int next = (index + 1) % 4;
		append_quad(builder, start_corners[static_cast<std::size_t>(index)],
				start_corners[static_cast<std::size_t>(next)], end_corners[static_cast<std::size_t>(next)],
				end_corners[static_cast<std::size_t>(index)], color);
	}
	append_quad(builder, start_corners[0], start_corners[1], start_corners[2], start_corners[3], color);
	append_quad(builder, end_corners[3], end_corners[2], end_corners[1], end_corners[0], color);
}

void append_scale_handle(GizmoMeshBuilder &builder, godot::Vector3 start, godot::Vector3 tip, godot::Color color,
		float line_width_pixels) {
	godot::Vector3 direction = tip - start;
	const float axis_length = direction.length();
	if (axis_length <= 0.000001f) {
		return;
	}
	direction /= axis_length;
	const float unit_world = axis_length / kGizmoScaleTip;
	const godot::Vector3 block_start = start + direction * (unit_world * kGizmoScaleOffset);
	append_line(builder, start, block_start, color, line_width_pixels);
	append_square_prism(builder, block_start, tip, unit_world * kGizmoScaleHalfWidth, color);
}

void append_cube_handle(GizmoMeshBuilder &builder, godot::Vector3 center, float world_size, godot::Color color) {
	const float half = world_size * 0.5f;
	const std::array<godot::Vector3, 8> corners{
			center + godot::Vector3{-half, -half, -half}, center + godot::Vector3{half, -half, -half},
			center + godot::Vector3{half, half, -half}, center + godot::Vector3{-half, half, -half},
			center + godot::Vector3{-half, -half, half}, center + godot::Vector3{half, -half, half},
			center + godot::Vector3{half, half, half}, center + godot::Vector3{-half, half, half},
	};
	constexpr std::array<std::array<int, 4>, 6> kFaces{
			std::array<int, 4>{1, 5, 6, 2}, std::array<int, 4>{4, 0, 3, 7}, std::array<int, 4>{3, 2, 6, 7},
			std::array<int, 4>{4, 5, 1, 0}, std::array<int, 4>{5, 4, 7, 6}, std::array<int, 4>{0, 1, 2, 3},
	};
	for (const std::array<int, 4> &face : kFaces) {
		append_quad(builder, corners[static_cast<std::size_t>(face[0])], corners[static_cast<std::size_t>(face[1])],
				corners[static_cast<std::size_t>(face[2])], corners[static_cast<std::size_t>(face[3])], color);
		append_line(builder, corners[static_cast<std::size_t>(face[0])], corners[static_cast<std::size_t>(face[1])],
				html_color(kScaleCenterOutlineColor), 1.0f);
		append_line(builder, corners[static_cast<std::size_t>(face[1])], corners[static_cast<std::size_t>(face[2])],
				html_color(kScaleCenterOutlineColor), 1.0f);
		append_line(builder, corners[static_cast<std::size_t>(face[2])], corners[static_cast<std::size_t>(face[3])],
				html_color(kScaleCenterOutlineColor), 1.0f);
		append_line(builder, corners[static_cast<std::size_t>(face[3])], corners[static_cast<std::size_t>(face[0])],
				html_color(kScaleCenterOutlineColor), 1.0f);
	}
}

void append_rotate_gizmo(GizmoMeshBuilder &builder, const GizmoFrame &frame) {
	const float width_scale = std::clamp(frame.gizmo_scale, 0.75f, 1.5f);
	for (const GizmoRingSegment &segment : frame.trackball_segments) {
		append_line(builder, segment.world_start, segment.world_end, html_color(kTrackballColor), 2.25f * width_scale);
	}
	for (const GizmoRingSegment &segment : frame.ring_segments) {
		if (!segment.front_facing) {
			continue;
		}
		append_line(builder, segment.world_start, segment.world_end, authored_axis_color(frame, segment.axis),
				3.0f * width_scale);
	}
}

void append_move_or_scale_gizmo(GizmoMeshBuilder &builder, const GizmoFrame &frame) {
	const float width_scale = std::clamp(frame.gizmo_scale, 0.75f, 1.5f);
	for (const GizmoPlanePrimitive &plane : frame.planes) {
		if (!drag_plane_includes(frame.active_axis, plane.axis)) {
			continue;
		}
		const godot::Color color =
				highlighted(frame, plane.axis) ? highlight_color_for_axis(plane.axis) : color_for_plane(plane.axis);
		append_plane(builder, plane, color);
	}
	for (const GizmoAxisPrimitive &axis : frame.axes) {
		if (!drag_axis_includes(frame.active_axis, axis.axis)) {
			continue;
		}
		const godot::Color color = highlighted(frame, axis.axis) ? highlight_color_for_axis(axis.axis)
																 : multiply_color(color_for_axis(axis.axis), 1.0f, axis.alpha);
		const float line_width = kTransformGizmoAxisLineWidthPixels * width_scale;
		if (frame.tool == TransformGizmoTool::Scale) {
			append_scale_handle(builder, axis.world_start, axis.world_tip, color, line_width);
		} else {
			append_arrow_handle(builder, axis.world_start, axis.world_tip, color, line_width);
		}
	}
	if (frame.tool == TransformGizmoTool::Scale &&
			(frame.hover_axis == TransformGizmoAxis::All || frame.active_axis == TransformGizmoAxis::All) &&
			!frame.axes.empty()) {
		const float axis_length = frame.axes.front().world_start.distance_to(frame.axes.front().world_tip);
		const float world_size = std::max(0.0001f, axis_length * (kTransformGizmoScaleCenterSizePixels / kTransformGizmoSizePixels));
		append_cube_handle(builder, frame.pivot, world_size, html_color(kAxisHighlightColor));
	}
}

godot::Ref<godot::ArrayMesh> make_line_mesh(const std::vector<GizmoLine> &lines, const godot::Camera3D *camera,
		godot::Vector2 viewport_size) {
	godot::PackedVector3Array vertices;
	godot::PackedVector2Array uvs;
	godot::PackedColorArray colors;
	godot::PackedInt32Array indices;
	if (camera == nullptr || lines.empty()) {
		godot::Ref<godot::ArrayMesh> empty;
		empty.instantiate();
		return empty;
	}
	const godot::Vector3 right = camera_right(camera);
	const godot::Vector3 up = camera_up(camera);
	for (const GizmoLine &line : lines) {
		if (camera->is_position_behind(line.a) || camera->is_position_behind(line.b)) {
			continue;
		}
		const godot::Vector2 screen_a = camera->unproject_position(line.a);
		const godot::Vector2 screen_b = camera->unproject_position(line.b);
		const godot::Vector2 screen_delta = screen_b - screen_a;
		if (screen_delta.length_squared() <= 0.0001f) {
			continue;
		}
		const godot::Vector2 screen_normal = godot::Vector2(-screen_delta.y, screen_delta.x).normalized();
		const float world_per_pixel = world_units_per_pixel_at(camera, viewport_size, (line.a + line.b) * 0.5f);
		const godot::Vector3 offset = (right * screen_normal.x - up * screen_normal.y) *
				((std::max(line.width_pixels, 1.0f) + 1.0f) * 0.5f * world_per_pixel);
		const std::int32_t base = static_cast<std::int32_t>(vertices.size());
		vertices.push_back(line.a + offset);
		vertices.push_back(line.b + offset);
		vertices.push_back(line.b - offset);
		vertices.push_back(line.a - offset);
		uvs.push_back({0.0f, 1.0f});
		uvs.push_back({1.0f, 1.0f});
		uvs.push_back({1.0f, -1.0f});
		uvs.push_back({0.0f, -1.0f});
		for (int index = 0; index < 4; ++index) {
			colors.push_back(line.color);
		}
		indices.push_back(base);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
		indices.push_back(base);
		indices.push_back(base + 2);
		indices.push_back(base + 3);
	}
	godot::Ref<godot::ArrayMesh> mesh;
	mesh.instantiate();
	if (!vertices.is_empty()) {
		godot::Array arrays;
		arrays.resize(godot::Mesh::ARRAY_MAX);
		arrays[godot::Mesh::ARRAY_VERTEX] = vertices;
		arrays[godot::Mesh::ARRAY_TEX_UV] = uvs;
		arrays[godot::Mesh::ARRAY_COLOR] = colors;
		arrays[godot::Mesh::ARRAY_INDEX] = indices;
		mesh->add_surface_from_arrays(godot::Mesh::PRIMITIVE_TRIANGLES, arrays);
	}
	return mesh;
}

godot::Ref<godot::ArrayMesh> make_triangle_mesh(const std::vector<GizmoTriangle> &triangles) {
	godot::PackedVector3Array vertices;
	godot::PackedColorArray colors;
	godot::PackedInt32Array indices;
	for (const GizmoTriangle &triangle : triangles) {
		const std::int32_t base = static_cast<std::int32_t>(vertices.size());
		vertices.push_back(triangle.a);
		vertices.push_back(triangle.b);
		vertices.push_back(triangle.c);
		colors.push_back(triangle.color);
		colors.push_back(triangle.color);
		colors.push_back(triangle.color);
		indices.push_back(base);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
	}
	godot::Ref<godot::ArrayMesh> mesh;
	mesh.instantiate();
	if (!vertices.is_empty()) {
		godot::Array arrays;
		arrays.resize(godot::Mesh::ARRAY_MAX);
		arrays[godot::Mesh::ARRAY_VERTEX] = vertices;
		arrays[godot::Mesh::ARRAY_COLOR] = colors;
		arrays[godot::Mesh::ARRAY_INDEX] = indices;
		mesh->add_surface_from_arrays(godot::Mesh::PRIMITIVE_TRIANGLES, arrays);
	}
	return mesh;
}

godot::Ref<godot::Shader> make_shader(const char *code) {
	godot::Ref<godot::Shader> shader;
	shader.instantiate();
	shader->set_code(godot::String(code));
	return shader;
}

} // namespace

TransformGizmoPickHit pick_transform_gizmo(const TransformGizmoInput &input, godot::Vector2 screen_position) {
	const GizmoFrame frame = build_frame(input);
	if (!frame.ok) {
		return {};
	}
	const float pick_radius = std::max(8.0f, kTransformGizmoPickRadiusPixels * frame.gizmo_scale);
	if (input.tool == TransformGizmoTool::Move) {
		TransformGizmoPickHit axis_hit = pick_move_axis(screen_position, frame.screen_pivot, frame, pick_radius);
		if (axis_hit.hit) {
			return axis_hit;
		}
		for (const GizmoPlanePrimitive &plane : frame.planes) {
			if (point_in_convex_quad(screen_position, plane.screen)) {
				return {
						.hit = true,
						.axis = plane.axis,
						.screen_tip = (plane.screen[0] + plane.screen[1] + plane.screen[2] + plane.screen[3]) * 0.25f,
				};
			}
		}
		return {};
	}
	if (input.tool == TransformGizmoTool::Scale) {
		TransformGizmoPickHit axis_hit = pick_scale_axis(screen_position, frame, pick_radius);
		if (axis_hit.hit) {
			return axis_hit;
		}
		for (const GizmoPlanePrimitive &plane : frame.planes) {
			if (point_in_convex_quad(screen_position, plane.screen)) {
				return {
						.hit = true,
						.axis = plane.axis,
						.screen_tip = (plane.screen[0] + plane.screen[1] + plane.screen[2] + plane.screen[3]) * 0.25f,
				};
			}
		}
		return {};
	}
	if (input.tool == TransformGizmoTool::Rotate) {
		return pick_rotate_axis(screen_position, frame, pick_radius);
	}
	return {};
}

TransformGizmoMeshes make_transform_gizmo_meshes(const TransformGizmoInput &input) {
	TransformGizmoMeshes meshes;
	const GizmoFrame frame = build_frame(input);
	GizmoMeshBuilder builder;
	if (frame.ok) {
		if (input.tool == TransformGizmoTool::Rotate) {
			append_rotate_gizmo(builder, frame);
		} else {
			append_move_or_scale_gizmo(builder, frame);
		}
	}
	meshes.lines = make_line_mesh(builder.lines, input.camera, input.viewport_size);
	meshes.triangles = make_triangle_mesh(builder.triangles);
	return meshes;
}

godot::Ref<godot::Material> make_transform_gizmo_line_material() {
	GizmoMaterialCache &cache = material_cache();
	if (cache.line_material.is_valid()) {
		return cache.line_material;
	}
	if (cache.line_shader.is_null()) {
		cache.line_shader = make_shader(R"(shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, depth_test_disabled;
void fragment() {
	float edge = abs(UV.y);
	float feather = max(fwidth(edge) * 1.5, 0.001);
	float alpha = 1.0 - smoothstep(1.0 - feather, 1.0, edge);
	ALBEDO = COLOR.rgb;
	ALPHA = COLOR.a * alpha;
})");
	}
	godot::Ref<godot::ShaderMaterial> material;
	material.instantiate();
	material->set_shader(cache.line_shader);
	material->set_render_priority(godot::Material::RENDER_PRIORITY_MAX);
	cache.line_material = material;
	return cache.line_material;
}

godot::Ref<godot::Material> make_transform_gizmo_triangle_material() {
	GizmoMaterialCache &cache = material_cache();
	if (cache.triangle_material.is_valid()) {
		return cache.triangle_material;
	}
	if (cache.triangle_shader.is_null()) {
		cache.triangle_shader = make_shader(R"(shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, depth_test_disabled;
void fragment() {
	ALBEDO = COLOR.rgb;
	ALPHA = COLOR.a;
})");
	}
	godot::Ref<godot::ShaderMaterial> material;
	material.instantiate();
	material->set_shader(cache.triangle_shader);
	material->set_render_priority(godot::Material::RENDER_PRIORITY_MAX - 1);
	cache.triangle_material = material;
	return cache.triangle_material;
}

void clear_transform_gizmo_material_cache() {
	GizmoMaterialCache &cache = material_cache();
	cache.line_material.unref();
	cache.triangle_material.unref();
	cache.line_shader.unref();
	cache.triangle_shader.unref();
}

} // namespace quader_godot::render
