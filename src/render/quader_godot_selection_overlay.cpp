#include "render/quader_godot_selection_overlay.h"

#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace quader_godot::render {
namespace {

constexpr float kPi = 3.14159265358979323846f;

enum class OverlayMaterialKind {
	Face,
	Line,
	Point,
};

struct OverlayMaterialCacheEntry {
	OverlayMaterialKind kind = OverlayMaterialKind::Face;
	godot::Color color;
	bool draw_on_top = true;
	int render_priority = godot::Material::RENDER_PRIORITY_MAX;
	float clip_depth_bias = 0.0f;
	godot::Ref<godot::Material> material;
};

struct OverlayResourceCache {
	godot::Ref<godot::Shader> depth_tested_line_shader;
	godot::Ref<godot::Shader> draw_on_top_line_shader;
	godot::Ref<godot::Shader> depth_tested_point_shader;
	godot::Ref<godot::Shader> draw_on_top_point_shader;
	std::vector<OverlayMaterialCacheEntry> materials;
};

OverlayResourceCache &overlay_resource_cache() {
	static OverlayResourceCache cache;
	return cache;
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

const char *line_shader_code(bool draw_on_top) {
	return draw_on_top
			? R"(shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, fog_disabled, depth_test_disabled;
uniform vec4 overlay_color : source_color = vec4(1.0);
uniform float overlay_depth_bias = 0.0;
void vertex() {
	vec4 clip = PROJECTION_MATRIX * MODELVIEW_MATRIX * vec4(VERTEX, 1.0);
	clip.z += overlay_depth_bias * clip.w;
	clip.z = min(clip.z, clip.w * 0.999999);
	POSITION = clip;
}
void fragment() {
	float distance_pixels = abs(UV.y);
	float core_half_width = max(UV.x, 0.5);
	float feather_pixels = max(fwidth(distance_pixels) * 1.5, 1.0);
	float alpha = 1.0 - smoothstep(core_half_width, core_half_width + feather_pixels, distance_pixels);
	ALBEDO = overlay_color.rgb;
	ALPHA = overlay_color.a * alpha;
})"
			: R"(shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, fog_disabled;
uniform vec4 overlay_color : source_color = vec4(1.0);
uniform float overlay_depth_bias = 0.0;
void vertex() {
	vec4 clip = PROJECTION_MATRIX * MODELVIEW_MATRIX * vec4(VERTEX, 1.0);
	clip.z += overlay_depth_bias * clip.w;
	clip.z = min(clip.z, clip.w * 0.999999);
	POSITION = clip;
}
void fragment() {
	float distance_pixels = abs(UV.y);
	float core_half_width = max(UV.x, 0.5);
	float feather_pixels = max(fwidth(distance_pixels) * 1.5, 1.0);
	float alpha = 1.0 - smoothstep(core_half_width, core_half_width + feather_pixels, distance_pixels);
	ALBEDO = overlay_color.rgb;
	ALPHA = overlay_color.a * alpha;
})";
}

const char *point_shader_code(bool draw_on_top) {
	return draw_on_top
			? R"(shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, fog_disabled, depth_test_disabled;
uniform vec4 overlay_color : source_color = vec4(1.0);
uniform float overlay_depth_bias = 0.0;
void vertex() {
	vec4 clip = PROJECTION_MATRIX * MODELVIEW_MATRIX * vec4(VERTEX, 1.0);
	clip.z += overlay_depth_bias * clip.w;
	clip.z = min(clip.z, clip.w * 0.999999);
	POSITION = clip;
}
void fragment() {
	float edge = max(abs(UV.x), abs(UV.y));
	float feather = max(fwidth(edge), 0.001);
	float alpha = 1.0 - smoothstep(1.0 - feather, 1.0, edge);
	ALBEDO = overlay_color.rgb;
	ALPHA = overlay_color.a * alpha;
})"
			: R"(shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled, fog_disabled;
uniform vec4 overlay_color : source_color = vec4(1.0);
uniform float overlay_depth_bias = 0.0;
void vertex() {
	vec4 clip = PROJECTION_MATRIX * MODELVIEW_MATRIX * vec4(VERTEX, 1.0);
	clip.z += overlay_depth_bias * clip.w;
	clip.z = min(clip.z, clip.w * 0.999999);
	POSITION = clip;
}
void fragment() {
	float edge = max(abs(UV.x), abs(UV.y));
	float feather = max(fwidth(edge), 0.001);
	float alpha = 1.0 - smoothstep(1.0 - feather, 1.0, edge);
	ALBEDO = overlay_color.rgb;
	ALPHA = overlay_color.a * alpha;
})";
}

godot::Ref<godot::Shader> make_shader(const char *shader_code) {
	godot::Ref<godot::Shader> shader;
	shader.instantiate();
	shader->set_code(godot::String(shader_code));
	return shader;
}

const godot::Ref<godot::Shader> &line_shader(bool draw_on_top) {
	OverlayResourceCache &cache = overlay_resource_cache();
	godot::Ref<godot::Shader> &shader = draw_on_top ? cache.draw_on_top_line_shader : cache.depth_tested_line_shader;
	if (shader.is_null()) {
		shader = make_shader(line_shader_code(draw_on_top));
	}
	return shader;
}

const godot::Ref<godot::Shader> &point_shader(bool draw_on_top) {
	OverlayResourceCache &cache = overlay_resource_cache();
	godot::Ref<godot::Shader> &shader = draw_on_top ? cache.draw_on_top_point_shader : cache.depth_tested_point_shader;
	if (shader.is_null()) {
		shader = make_shader(point_shader_code(draw_on_top));
	}
	return shader;
}

godot::Ref<godot::ShaderMaterial> make_feather_material(const godot::Ref<godot::Shader> &shader, godot::Color color,
		int render_priority, float clip_depth_bias) {
	godot::Ref<godot::ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	material->set_shader_parameter(godot::StringName("overlay_color"), color);
	material->set_shader_parameter(godot::StringName("overlay_depth_bias"), clip_depth_bias);
	material->set_render_priority(render_priority);
	return material;
}

bool same_color(godot::Color a, godot::Color b) {
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

godot::Ref<godot::Material> make_uncached_overlay_material(OverlayMaterialKind kind, godot::Color color,
		bool draw_on_top, int render_priority, float clip_depth_bias) {
	if (kind == OverlayMaterialKind::Face) {
		godot::Ref<godot::StandardMaterial3D> material;
		material.instantiate();
		material->set_albedo(color);
		material->set_shading_mode(godot::StandardMaterial3D::SHADING_MODE_UNSHADED);
		material->set_transparency(godot::StandardMaterial3D::TRANSPARENCY_ALPHA);
		material->set_cull_mode(godot::StandardMaterial3D::CULL_DISABLED);
		material->set_render_priority(render_priority);
		material->set_flag(godot::StandardMaterial3D::FLAG_DISABLE_FOG, true);
		material->set_flag(godot::StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, draw_on_top);
		return material;
	}
	if (kind == OverlayMaterialKind::Line) {
		return make_feather_material(line_shader(draw_on_top), color, render_priority, clip_depth_bias);
	}
	return make_feather_material(point_shader(draw_on_top), color, render_priority, clip_depth_bias);
}

godot::Ref<godot::Material> cached_overlay_material(OverlayMaterialKind kind, godot::Color color, bool draw_on_top,
		int render_priority, float clip_depth_bias = 0.0f) {
	std::vector<OverlayMaterialCacheEntry> &materials = overlay_resource_cache().materials;
	const std::vector<OverlayMaterialCacheEntry>::iterator found =
			std::find_if(materials.begin(), materials.end(), [&](const OverlayMaterialCacheEntry &entry) {
				return entry.kind == kind && entry.draw_on_top == draw_on_top &&
						entry.render_priority == render_priority &&
						std::abs(entry.clip_depth_bias - clip_depth_bias) <= 0.0000001f &&
						same_color(entry.color, color);
			});
	if (found != materials.end()) {
		return found->material;
	}

	godot::Ref<godot::Material> material =
			make_uncached_overlay_material(kind, color, draw_on_top, render_priority, clip_depth_bias);
	materials.push_back({
			.kind = kind,
			.color = color,
			.draw_on_top = draw_on_top,
			.render_priority = render_priority,
			.clip_depth_bias = clip_depth_bias,
			.material = material,
	});
	return material;
}

void append_quad(godot::PackedVector3Array &vertices, godot::PackedVector2Array *uvs,
		godot::PackedInt32Array &indices, const godot::Vector3 &a, const godot::Vector3 &b,
		const godot::Vector3 &c, const godot::Vector3 &d, godot::Vector2 uv_a = {},
		godot::Vector2 uv_b = {}, godot::Vector2 uv_c = {}, godot::Vector2 uv_d = {}) {
	const std::int32_t base = static_cast<std::int32_t>(vertices.size());
	vertices.push_back(a);
	vertices.push_back(b);
	vertices.push_back(c);
	vertices.push_back(d);
	if (uvs != nullptr) {
		uvs->push_back(uv_a);
		uvs->push_back(uv_b);
		uvs->push_back(uv_c);
		uvs->push_back(uv_d);
	}
	indices.push_back(base);
	indices.push_back(base + 1);
	indices.push_back(base + 2);
	indices.push_back(base);
	indices.push_back(base + 2);
	indices.push_back(base + 3);
}

bool point_is_in_front_of_near_plane(const godot::Camera3D *camera, godot::Vector3 point) {
	const godot::Vector3 forward = camera_forward(camera);
	const float depth = (point - camera->get_global_transform().origin).dot(forward);
	return depth >= std::max(static_cast<float>(camera->get_near()), 0.0001f);
}

float view_depth_at(const godot::Camera3D *camera, godot::Vector3 point) {
	const godot::Vector3 forward = camera_forward(camera);
	return (point - camera->get_global_transform().origin).dot(forward);
}

float near_plane_guard(const godot::Camera3D *camera) {
	return std::max(static_cast<float>(camera->get_near()) * 0.05f, 0.0001f);
}

float clamped_toward_camera_bias(const godot::Camera3D *camera, godot::Vector2 viewport_size,
		godot::Vector3 reference_point, float min_segment_depth, float depth_bias_pixels) {
	const float raw_bias = std::max(depth_bias_pixels, 0.0f) *
			world_units_per_pixel_at(camera, viewport_size, reference_point);
	const float near_plane = std::max(static_cast<float>(camera->get_near()), 0.0001f);
	const float max_bias = std::max(min_segment_depth - near_plane - near_plane_guard(camera), 0.0f);
	return std::min(raw_bias, max_bias);
}

bool clip_segment_to_near_plane(const godot::Camera3D *camera, godot::Vector3 &a, godot::Vector3 &b) {
	const godot::Vector3 forward = camera_forward(camera);
	const godot::Vector3 eye = camera->get_global_transform().origin;
	const float near_plane = std::max(static_cast<float>(camera->get_near()), 0.0001f);
	const float a_near = (a - eye).dot(forward) - near_plane;
	const float b_near = (b - eye).dot(forward) - near_plane;
	if (a_near < 0.0f && b_near < 0.0f) {
		return false;
	}
	if (a_near < 0.0f || b_near < 0.0f) {
		const float t = std::clamp(a_near / (a_near - b_near), 0.0f, 1.0f);
		const godot::Vector3 clipped = a + (b - a) * t;
		if (a_near < 0.0f) {
			a = clipped;
		} else {
			b = clipped;
		}
	}
	return true;
}

godot::Ref<godot::ArrayMesh> make_triangle_array_mesh(const godot::PackedVector3Array &vertices,
		const godot::PackedInt32Array &indices, const godot::PackedVector2Array *uvs = nullptr) {
	godot::Array arrays;
	arrays.resize(godot::Mesh::ARRAY_MAX);
	arrays[godot::Mesh::ARRAY_VERTEX] = vertices;
	if (uvs != nullptr) {
		arrays[godot::Mesh::ARRAY_TEX_UV] = *uvs;
	}
	arrays[godot::Mesh::ARRAY_INDEX] = indices;

	godot::Ref<godot::ArrayMesh> mesh;
	mesh.instantiate();
	if (!vertices.is_empty()) {
		mesh->add_surface_from_arrays(godot::Mesh::PRIMITIVE_TRIANGLES, arrays);
	}
	return mesh;
}

} // namespace

void clear_overlay_material_cache() {
	OverlayResourceCache &cache = overlay_resource_cache();
	cache.materials.clear();
	cache.depth_tested_line_shader.unref();
	cache.draw_on_top_line_shader.unref();
	cache.depth_tested_point_shader.unref();
	cache.draw_on_top_point_shader.unref();
}

godot::Ref<godot::Material> make_overlay_face_material(godot::Color color, bool draw_on_top, int render_priority) {
	return cached_overlay_material(OverlayMaterialKind::Face, color, draw_on_top, render_priority, 0.0f);
}

godot::Ref<godot::Material> make_overlay_line_material(godot::Color color, bool draw_on_top, int render_priority,
		float clip_depth_bias) {
	return cached_overlay_material(OverlayMaterialKind::Line, color, draw_on_top, render_priority,
			draw_on_top ? 0.0f : clip_depth_bias);
}

godot::Ref<godot::Material> make_overlay_point_material(godot::Color color, bool draw_on_top, int render_priority,
		float clip_depth_bias) {
	return cached_overlay_material(OverlayMaterialKind::Point, color, draw_on_top, render_priority,
			draw_on_top ? 0.0f : clip_depth_bias);
}

godot::Ref<godot::ArrayMesh> make_overlay_line_mesh(std::span<const OverlaySegment> segments,
		const godot::Camera3D *camera, godot::Vector2 viewport_size, float width_pixels, float depth_bias_pixels) {
	godot::PackedVector3Array vertices;
	godot::PackedVector2Array uvs;
	godot::PackedInt32Array indices;
	if (camera == nullptr || segments.empty()) {
		return make_triangle_array_mesh(vertices, indices);
	}

	const godot::Vector3 right = camera_right(camera);
	const godot::Vector3 up = camera_up(camera);
	const godot::Vector3 toward_camera = -camera_forward(camera);
	const float core_half_width = std::max(width_pixels, 1.0f) * 0.5f;
	const float side_distance = core_half_width + 1.0f;
	for (const OverlaySegment &segment : segments) {
		godot::Vector3 clipped_a = segment.a;
		godot::Vector3 clipped_b = segment.b;
		if (!clip_segment_to_near_plane(camera, clipped_a, clipped_b)) {
			continue;
		}
		const godot::Vector3 midpoint = (clipped_a + clipped_b) * 0.5f;
		const float world_per_pixel = world_units_per_pixel_at(camera, viewport_size, midpoint);
		const float min_depth = std::min(view_depth_at(camera, clipped_a), view_depth_at(camera, clipped_b));
		const godot::Vector3 depth_bias = toward_camera *
				clamped_toward_camera_bias(camera, viewport_size, midpoint, min_depth, depth_bias_pixels);
		const godot::Vector3 biased_a = clipped_a + depth_bias;
		const godot::Vector3 biased_b = clipped_b + depth_bias;
		const godot::Vector2 screen_a = camera->unproject_position(biased_a);
		const godot::Vector2 screen_b = camera->unproject_position(biased_b);
		const godot::Vector2 screen_delta = screen_b - screen_a;
		if (screen_delta.length_squared() <= 0.0001f) {
			continue;
		}
		const godot::Vector2 screen_direction = screen_delta.normalized();
		const godot::Vector2 screen_normal = godot::Vector2(-screen_direction.y, screen_direction.x);
		const godot::Vector3 offset =
				(right * screen_normal.x - up * screen_normal.y) * (side_distance * world_per_pixel);
		const godot::Vector3 cap =
				(right * screen_direction.x - up * screen_direction.y) * (side_distance * world_per_pixel);
		const godot::Vector3 a = biased_a - cap;
		const godot::Vector3 b = biased_b + cap;
		append_quad(vertices, &uvs, indices, a - offset, b - offset, b + offset, a + offset,
				{core_half_width, -side_distance}, {core_half_width, -side_distance},
				{core_half_width, side_distance}, {core_half_width, side_distance});
	}
	return make_triangle_array_mesh(vertices, indices, &uvs);
}

godot::Ref<godot::ArrayMesh> make_overlay_point_mesh(std::span<const godot::Vector3> points,
		const godot::Camera3D *camera, godot::Vector2 viewport_size, float size_pixels, float depth_bias_pixels) {
	godot::PackedVector3Array vertices;
	godot::PackedVector2Array uvs;
	godot::PackedInt32Array indices;
	if (camera == nullptr || points.empty()) {
		return make_triangle_array_mesh(vertices, indices);
	}

	const godot::Vector3 right = camera_right(camera);
	const godot::Vector3 up = camera_up(camera);
	const godot::Vector3 toward_camera = -camera_forward(camera);
	const float half_size = (std::max(size_pixels, 1.0f) + 1.0f) * 0.5f;
	for (const godot::Vector3 &point : points) {
		if (!point_is_in_front_of_near_plane(camera, point)) {
			continue;
		}
		const float world_per_pixel = world_units_per_pixel_at(camera, viewport_size, point);
		const godot::Vector3 biased_point = point + toward_camera *
				clamped_toward_camera_bias(camera, viewport_size, point, view_depth_at(camera, point),
						depth_bias_pixels);
		const godot::Vector3 x = right * (half_size * world_per_pixel);
		const godot::Vector3 y = up * (half_size * world_per_pixel);
		append_quad(vertices, &uvs, indices, biased_point - x + y, biased_point + x + y, biased_point + x - y,
				biased_point - x - y,
				{-1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, -1.0f});
	}
	return make_triangle_array_mesh(vertices, indices, &uvs);
}

godot::Ref<godot::ArrayMesh> make_overlay_face_mesh(std::span<const OverlayTriangle> triangles) {
	godot::PackedVector3Array vertices;
	godot::PackedInt32Array indices;
	for (const OverlayTriangle &triangle : triangles) {
		const std::int32_t base = static_cast<std::int32_t>(vertices.size());
		vertices.push_back(triangle.a);
		vertices.push_back(triangle.b);
		vertices.push_back(triangle.c);
		indices.push_back(base);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
	}
	return make_triangle_array_mesh(vertices, indices);
}

} // namespace quader_godot::render
