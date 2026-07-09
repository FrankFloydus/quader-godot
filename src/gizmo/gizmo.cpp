#include "gizmo/gizmo_internal.h"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace quader_godot::gizmo {

namespace {

constexpr float kDegreesPerHalfTurn = 180.0f;
constexpr float kAxisHideScore = 0.02f;
constexpr float kAxisFullScore = 0.1f;
constexpr float kMinimumViewportHeight = 1.0f;
constexpr float kMinimumViewDepth = 0.001f;
constexpr float kConvexQuadEdgeTolerance = 0.001f;
constexpr float kDefaultLineWidthPixels = 1.0f;
constexpr float kLineMeshExtraWidthPixels = 1.0f;
constexpr float kLineMeshUvTop = 1.0f;
constexpr float kLineMeshUvBottom = -1.0f;
constexpr float kPlaneBorderLineWidthPixels = 1.2f;
constexpr int kQuadVertexCount = 4;
constexpr char kAxisXColor[] = "#f53352e6";
constexpr char kAxisYColor[] = "#87d603e6";
constexpr char kAxisZColor[] = "#298cf5e6";
constexpr char kAxisMixedColor[] = "#eef2f7e6";
constexpr char kAxisXHighlightColor[] = "#ffccd3ff";
constexpr char kAxisYHighlightColor[] = "#e6ffc0ff";
constexpr char kAxisZHighlightColor[] = "#cae4ffff";
constexpr char kAxisHighlightColor[] = "#fffaccff";
constexpr char kWhiteColor[] = "#ffffffff";

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

float scaled_channel(float value, float scale) {
	return std::clamp(value * scale, 0.0f, 1.0f);
}

godot::Ref<godot::Shader> make_shader(const char *code) {
	godot::Ref<godot::Shader> shader;
	shader.instantiate();
	shader->set_code(godot::String(code));
	return shader;
}

} // namespace

float clamped_gizmo_scale(float scale) {
	return std::clamp(scale, 0.5f, 2.0f);
}

bool prepare_frame(const GizmoInput &input, GizmoFrame &frame) {
	frame.hovered_handle = input.hovered_handle;
	frame.active_handle = input.active_handle;
	frame.pivot = input.pivot;
	frame.scale = clamped_gizmo_scale(input.scale);
	frame.viewport_size = input.viewport_size;
	if (!input.has_selection || !input.has_pivot || input.camera == nullptr) {
		return false;
	}
	if (!project_point(input.camera, input.pivot, frame.screen_pivot)) {
		return false;
	}
	frame.ok = true;
	return true;
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

godot::Vector3 axis_vector(GizmoHandle handle) {
	if (handle == GizmoHandle::X) {
		return {1.0f, 0.0f, 0.0f};
	}
	if (handle == GizmoHandle::Y) {
		return {0.0f, 1.0f, 0.0f};
	}
	if (handle == GizmoHandle::Z) {
		return {0.0f, 0.0f, 1.0f};
	}
	return {};
}

godot::Vector3 fallback_perpendicular(godot::Vector3 axis) {
	godot::Vector3 side = axis.cross({0.0f, 1.0f, 0.0f});
	if (side.length_squared() <= kGizmoEpsilon) {
		side = axis.cross({1.0f, 0.0f, 0.0f});
	}
	return side.length_squared() <= kGizmoEpsilon ? godot::Vector3{0.0f, 0.0f, 1.0f} : side.normalized();
}

float world_units_per_pixel_at(const godot::Camera3D *camera, godot::Vector2 viewport_size,
		const godot::Vector3 &position) {
	const float height = std::max(viewport_size.y, kMinimumViewportHeight);
	if (camera->get_projection() == godot::Camera3D::PROJECTION_ORTHOGONAL) {
		return static_cast<float>(camera->get_size()) / height;
	}
	const godot::Vector3 camera_position = camera->get_global_transform().origin;
	const float view_depth = std::max((position - camera_position).dot(camera_forward(camera)), kMinimumViewDepth);
	const float fov_radians = static_cast<float>(camera->get_fov()) * std::numbers::pi_v<float> / kDegreesPerHalfTurn;
	return 2.0f * view_depth * std::tan(fov_radians * 0.5f) / height;
}

godot::Color html_color(const char *color) {
	return godot::Color(godot::String(color));
}

godot::Color multiply_color(godot::Color color, float rgb_scale, float alpha_scale) {
	return {scaled_channel(color.r, rgb_scale), scaled_channel(color.g, rgb_scale),
			scaled_channel(color.b, rgb_scale), scaled_channel(color.a, alpha_scale)};
}

godot::Color color_for_axis(GizmoHandle handle) {
	if (handle == GizmoHandle::X) {
		return html_color(kAxisXColor);
	}
	if (handle == GizmoHandle::Y) {
		return html_color(kAxisYColor);
	}
	if (handle == GizmoHandle::Z) {
		return html_color(kAxisZColor);
	}
	return html_color(kAxisMixedColor);
}

godot::Color color_for_plane(GizmoHandle handle) {
	if (handle == GizmoHandle::XY) {
		return html_color(kAxisZColor);
	}
	if (handle == GizmoHandle::XZ) {
		return html_color(kAxisYColor);
	}
	if (handle == GizmoHandle::YZ) {
		return html_color(kAxisXColor);
	}
	return html_color(kWhiteColor);
}

godot::Color highlight_color_for_axis(GizmoHandle handle) {
	if (handle == GizmoHandle::X || handle == GizmoHandle::YZ) {
		return html_color(kAxisXHighlightColor);
	}
	if (handle == GizmoHandle::Y || handle == GizmoHandle::XZ) {
		return html_color(kAxisYHighlightColor);
	}
	if (handle == GizmoHandle::Z || handle == GizmoHandle::XY) {
		return html_color(kAxisZHighlightColor);
	}
	return html_color(kAxisHighlightColor);
}

bool highlighted(const GizmoFrame &frame, GizmoHandle handle) {
	return frame.active_handle == handle || frame.hovered_handle == handle;
}

float axis_alpha(const godot::Camera3D *camera, godot::Vector3 center, godot::Vector3 world_axis) {
	if (world_axis.length_squared() <= kGizmoEpsilon) {
		return 0.0f;
	}
	const godot::Vector3 view_direction = camera->get_global_transform().origin - center;
	if (view_direction.length_squared() <= kGizmoEpsilon) {
		return 1.0f;
	}
	const float score = std::clamp(
			1.0f - std::abs(world_axis.normalized().dot(view_direction.normalized())), 0.0f, 1.0f);
	if (score <= kAxisHideScore) {
		return 0.0f;
	}
	if (score >= kAxisFullScore) {
		return 1.0f;
	}
	return (score - kAxisHideScore) / (kAxisFullScore - kAxisHideScore);
}

bool project_point(const godot::Camera3D *camera, godot::Vector3 point, godot::Vector2 &screen) {
	if (camera == nullptr || camera->is_position_behind(point)) {
		return false;
	}
	screen = camera->unproject_position(point);
	return true;
}

void add_axis(GizmoFrame &frame, const godot::Camera3D *camera, GizmoHandle handle, godot::Vector3 center,
		float axis_length) {
	const godot::Vector3 direction = axis_vector(handle);
	if (direction.length_squared() <= kGizmoEpsilon) {
		return;
	}
	const godot::Vector3 tip = center + direction * axis_length;
	godot::Vector2 screen_start;
	godot::Vector2 screen_tip;
	if (!project_point(camera, center, screen_start) || !project_point(camera, tip, screen_tip)) {
		return;
	}
	if (screen_start.distance_squared_to(screen_tip) <= kGizmoScreenEpsilon) {
		return;
	}
	frame.axes.push_back({
			.handle = handle,
			.world_start = center,
			.world_tip = tip,
			.screen_anchor = screen_tip,
			.alpha = axis_alpha(camera, center, direction),
	});
}

void add_plane(GizmoFrame &frame, const godot::Camera3D *camera, GizmoHandle handle, godot::Vector3 center,
		godot::Vector3 axis_a, godot::Vector3 axis_b, float offset, float half_size) {
	const godot::Vector3 a = axis_a.length_squared() <= kGizmoEpsilon ? godot::Vector3{1.0f, 0.0f, 0.0f}
																	  : axis_a.normalized();
	const godot::Vector3 b = axis_b.length_squared() <= kGizmoEpsilon ? godot::Vector3{0.0f, 1.0f, 0.0f}
																	  : axis_b.normalized();
	const godot::Vector3 handle_center = center + (a + b) * offset;
	GizmoPlanePrimitive plane;
	plane.handle = handle;
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

float distance_to_segment_squared(godot::Vector2 point, godot::Vector2 a, godot::Vector2 b) {
	const godot::Vector2 segment = b - a;
	const float length_squared = segment.length_squared();
	if (length_squared <= kGizmoScreenEpsilon) {
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
		if (std::abs(cross) <= kConvexQuadEdgeTolerance) {
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

void append_line(GizmoMeshBuilder &builder, godot::Vector3 a, godot::Vector3 b, godot::Color color,
		float width_pixels) {
	builder.lines.push_back({
			.a = a,
			.b = b,
			.color = color,
			.width_pixels = std::max(width_pixels, kDefaultLineWidthPixels),
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

void append_quad(GizmoMeshBuilder &builder, godot::Vector3 a, godot::Vector3 b, godot::Vector3 c,
		godot::Vector3 d, godot::Color color) {
	append_triangle(builder, a, b, c, color);
	append_triangle(builder, a, c, d, color);
}

void append_plane(GizmoMeshBuilder &builder, const GizmoPlanePrimitive &plane, godot::Color color) {
	append_quad(builder, plane.world[0], plane.world[1], plane.world[2], plane.world[3], color);
	append_line(builder, plane.world[0], plane.world[1], color, kPlaneBorderLineWidthPixels);
	append_line(builder, plane.world[1], plane.world[2], color, kPlaneBorderLineWidthPixels);
	append_line(builder, plane.world[2], plane.world[3], color, kPlaneBorderLineWidthPixels);
	append_line(builder, plane.world[3], plane.world[0], color, kPlaneBorderLineWidthPixels);
}

godot::Ref<godot::ArrayMesh> make_line_mesh(const std::vector<GizmoLine> &lines,
		const godot::Camera3D *camera, godot::Vector2 viewport_size) {
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
		if (screen_delta.length_squared() <= kGizmoScreenEpsilon) {
			continue;
		}
		const godot::Vector2 screen_normal = godot::Vector2(-screen_delta.y, screen_delta.x).normalized();
		const float world_per_pixel = world_units_per_pixel_at(camera, viewport_size, (line.a + line.b) * 0.5f);
		const godot::Vector3 offset = (right * screen_normal.x - up * screen_normal.y) *
				((std::max(line.width_pixels, kDefaultLineWidthPixels) + kLineMeshExtraWidthPixels) * 0.5f *
						world_per_pixel);
		const std::int32_t base = static_cast<std::int32_t>(vertices.size());
		vertices.push_back(line.a + offset);
		vertices.push_back(line.b + offset);
		vertices.push_back(line.b - offset);
		vertices.push_back(line.a - offset);
		uvs.push_back({0.0f, kLineMeshUvTop});
		uvs.push_back({1.0f, kLineMeshUvTop});
		uvs.push_back({1.0f, kLineMeshUvBottom});
		uvs.push_back({0.0f, kLineMeshUvBottom});
		for (int index = 0; index < kQuadVertexCount; ++index) {
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

bool handle_is_plane(GizmoHandle handle) {
	return handle == GizmoHandle::XY || handle == GizmoHandle::XZ || handle == GizmoHandle::YZ;
}

std::array<godot::Vector3, 2> plane_axes(GizmoHandle handle) {
	if (handle == GizmoHandle::XY) {
		return {godot::Vector3{1.0f, 0.0f, 0.0f}, godot::Vector3{0.0f, 1.0f, 0.0f}};
	}
	if (handle == GizmoHandle::XZ) {
		return {godot::Vector3{1.0f, 0.0f, 0.0f}, godot::Vector3{0.0f, 0.0f, 1.0f}};
	}
	return {godot::Vector3{0.0f, 1.0f, 0.0f}, godot::Vector3{0.0f, 0.0f, 1.0f}};
}

bool handle_includes_component(GizmoHandle handle, GizmoComponent component) {
	if (handle == GizmoHandle::All) {
		return true;
	}
	if (component == GizmoComponent::X) {
		return handle == GizmoHandle::X || handle == GizmoHandle::XY || handle == GizmoHandle::XZ;
	}
	if (component == GizmoComponent::Y) {
		return handle == GizmoHandle::Y || handle == GizmoHandle::XY || handle == GizmoHandle::YZ;
	}
	return handle == GizmoHandle::Z || handle == GizmoHandle::XZ || handle == GizmoHandle::YZ;
}

bool drag_axis_includes(GizmoHandle active_handle, GizmoHandle handle) {
	if (active_handle == GizmoHandle::None || active_handle == handle) {
		return true;
	}
	if (active_handle == GizmoHandle::XY) {
		return handle == GizmoHandle::X || handle == GizmoHandle::Y;
	}
	if (active_handle == GizmoHandle::XZ) {
		return handle == GizmoHandle::X || handle == GizmoHandle::Z;
	}
	if (active_handle == GizmoHandle::YZ) {
		return handle == GizmoHandle::Y || handle == GizmoHandle::Z;
	}
	return false;
}

bool drag_plane_includes(GizmoHandle active_handle, GizmoHandle plane_handle) {
	return active_handle == GizmoHandle::None || active_handle == plane_handle;
}

GizmoPickHit pick_axis_handle(const GizmoFrame &frame, godot::Vector2 screen_position, float pick_radius) {
	GizmoPickHit hit;
	const float radius_squared = pick_radius * pick_radius;
	float best_distance = std::numeric_limits<float>::max();
	const GizmoAxisPrimitive *best = nullptr;
	for (const GizmoAxisPrimitive &axis : frame.axes) {
		const float distance = screen_position.distance_squared_to(axis.screen_anchor);
		if (distance <= radius_squared && distance < best_distance) {
			best_distance = distance;
			best = &axis;
		}
	}
	for (const GizmoAxisPrimitive &axis : frame.axes) {
		if (best != nullptr) {
			break;
		}
		const float distance = distance_to_segment_squared(screen_position, frame.screen_pivot, axis.screen_anchor);
		if (distance <= radius_squared && distance < best_distance) {
			best_distance = distance;
			best = &axis;
		}
	}
	if (best == nullptr) {
		return hit;
	}
	hit.hit = true;
	hit.handle = best->handle;
	hit.screen_anchor = best->screen_anchor;
	hit.distance = std::sqrt(best_distance);
	return hit;
}

float safe_grid_size(float grid_size) {
	return std::isfinite(grid_size) && grid_size > kGizmoEpsilon ? grid_size : 1.0f;
}

float snap_to_step(float value, float step) {
	if (!std::isfinite(step) || step <= kGizmoEpsilon) {
		return value;
	}
	return std::round(value / step) * step;
}

GizmoMeshes make_empty_gizmo_meshes() {
	GizmoMeshes meshes;
	meshes.lines.instantiate();
	meshes.triangles.instantiate();
	return meshes;
}

godot::Ref<godot::Material> make_gizmo_line_material() {
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

godot::Ref<godot::Material> make_gizmo_triangle_material() {
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

void clear_gizmo_material_cache() {
	GizmoMaterialCache &cache = material_cache();
	cache.line_material.unref();
	cache.triangle_material.unref();
	cache.line_shader.unref();
	cache.triangle_shader.unref();
}

} // namespace quader_godot::gizmo
