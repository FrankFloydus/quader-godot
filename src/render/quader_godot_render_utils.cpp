#include "render/quader_godot_render_utils.h"

#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/string_name.hpp>

#include <algorithm>
#include <cstdint>

namespace quader_godot::render {
namespace {

constexpr float kGridPlaneSize = 4096.0f;
constexpr float kParentGridWorldSize = 2.0f;
constexpr float kMeshSurfaceMajorGridMultiplier = 4.0f;

godot::Color rgba8(int red, int green, int blue, int alpha = 255) {
	return {static_cast<float>(red) / 255.0f, static_cast<float>(green) / 255.0f,
			static_cast<float>(blue) / 255.0f, static_cast<float>(alpha) / 255.0f};
}

godot::Color abgr8(std::uint32_t abgr) {
	return {static_cast<float>(abgr & 0xffU) / 255.0f,
			static_cast<float>((abgr >> 8U) & 0xffU) / 255.0f,
			static_cast<float>((abgr >> 16U) & 0xffU) / 255.0f,
			static_cast<float>((abgr >> 24U) & 0xffU) / 255.0f};
}

godot::String ground_grid_shader_code() {
	return R"(
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled;

uniform vec4 grid_color : source_color = vec4(0.588235, 0.588235, 0.588235, 1.0);
uniform vec4 major_grid_color : source_color = vec4(0.823529, 0.823529, 0.823529, 1.0);
uniform vec4 x_axis_color : source_color = vec4(1.0, 0.239216, 0.0, 0.721569);
uniform vec4 z_axis_color : source_color = vec4(0.058824, 0.611765, 1.0, 0.721569);
uniform float grid_size = 1.0;
uniform float major_grid_size = 2.0;
uniform float line_thickness = 0.250;
uniform float major_line_thickness = 0.250;
uniform float axis_line_thickness = 1.0;
uniform float fade_start = 96.0;
uniform float fade_end = 1536.0;

varying vec3 world_position;

void vertex() {
	world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

float grid_line(float coord, float spacing, float width_scale, float thickness_value) {
	float safe_spacing = max(spacing, 0.0001);
	float wrapped = abs(fract(coord / safe_spacing + 0.5) - 0.5) * safe_spacing;
	float thickness = clamp(thickness_value, 0.05, 8.0);
	float antialias = max(fwidth(coord), safe_spacing * 0.001);
	float line_width = antialias * max(width_scale, 0.05) * thickness;
	float coverage = 1.0 - smoothstep(line_width, line_width + antialias, wrapped);
	return coverage * min(thickness, 1.0);
}

float axis_line(float coord) {
	float thickness = clamp(axis_line_thickness, 0.05, 8.0);
	float antialias = max(fwidth(coord), 0.0001);
	float line_width = antialias * 0.9 * thickness;
	float coverage = 1.0 - smoothstep(line_width, line_width + antialias, abs(coord));
	return coverage * min(thickness, 1.0);
}

void fragment() {
	vec2 coords = world_position.xz;
	float minor = max(
		grid_line(coords.x, grid_size, 0.65, line_thickness),
		grid_line(coords.y, grid_size, 0.65, line_thickness));
	float major = max(
		grid_line(coords.x, max(major_grid_size, grid_size), 0.85, major_line_thickness),
		grid_line(coords.y, max(major_grid_size, grid_size), 0.85, major_line_thickness));
	float distance_to_camera = length(world_position - CAMERA_POSITION_WORLD);
	float fade = 1.0 - smoothstep(fade_start, fade_end, distance_to_camera);
	float major_coverage = major * major_grid_color.a;
	float minor_coverage = minor * grid_color.a * (1.0 - clamp(major_coverage, 0.0, 1.0));
	float alpha = minor_coverage + major_coverage;
	vec3 color = grid_color.rgb;
	if (alpha > 0.0) {
		color = (grid_color.rgb * minor_coverage + major_grid_color.rgb * major_coverage) / alpha;
	}
	float x_axis = axis_line(coords.y);
	float z_axis = axis_line(coords.x);
	if (x_axis > 0.0) {
		color = mix(color, x_axis_color.rgb, x_axis);
		alpha = max(alpha, x_axis * x_axis_color.a);
	}
	if (z_axis > 0.0) {
		color = mix(color, z_axis_color.rgb, z_axis);
		alpha = max(alpha, z_axis * z_axis_color.a);
	}
	ALBEDO = color;
	ALPHA = clamp(alpha * fade, 0.0, 1.0);
}
)";
}

godot::String default_quader_mesh_shader_code() {
	return R"(
shader_type spatial;
render_mode unshaded, cull_back, depth_draw_opaque;

uniform sampler2D base_color_texture : source_color, filter_linear_mipmap_anisotropic, repeat_enable;
uniform vec4 base_color_factor : source_color = vec4(1.0, 1.0, 1.0, 1.0);
uniform vec4 surface_grid_minor_color : source_color = vec4(1.0, 1.0, 1.0, 0.050980);
uniform vec4 surface_grid_major_color : source_color = vec4(1.0, 1.0, 1.0, 0.152941);
uniform float surface_grid_size = 1.0;
uniform float surface_grid_major_size = 4.0;
uniform float surface_grid_minor_line_thickness = 0.250;
uniform float surface_grid_major_line_thickness = 0.250;
uniform float surface_grid_enabled = 1.0;

varying vec3 world_position;
varying vec3 world_normal;

void vertex() {
	world_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

float surface_grid_floor(float spacing) {
	return exp2(floor(log2(max(spacing, 0.0001))));
}

float surface_grid_line_coverage(float coord, float spacing, float thickness_value) {
	float safe_spacing = max(spacing, 0.0001);
	float wrapped = abs(fract(coord / safe_spacing + 0.5) - 0.5) * safe_spacing;
	float thickness = clamp(thickness_value, 0.05, 8.0);
	float derivative_width = fwidth(coord);
	float antialias = clamp(derivative_width, safe_spacing * 0.0005, safe_spacing * 0.125);
	float line_width = min(antialias * 0.75 * thickness, safe_spacing * 0.125);
	float feather = min(antialias, safe_spacing * 0.125);
	return 1.0 - smoothstep(line_width, line_width + feather, wrapped);
}

float surface_grid_line(float coord, float spacing, float thickness_value, float world_pixel_width) {
	float safe_spacing = max(spacing, 0.0001);
	float min_renderable_spacing = max(world_pixel_width, 0.0) * 2.0;
	float lod_spacing = max(surface_grid_floor(max(min_renderable_spacing, safe_spacing)), safe_spacing);
	float next_spacing = max(lod_spacing * 2.0, lod_spacing + 0.0001);
	float blend = smoothstep(lod_spacing, next_spacing, min_renderable_spacing);
	float current_coverage = surface_grid_line_coverage(coord, lod_spacing, thickness_value);
	float next_coverage = surface_grid_line_coverage(coord, next_spacing, thickness_value);
	return mix(current_coverage, next_coverage, blend);
}

vec2 surface_grid_coordinates(vec3 position, vec3 normal) {
	vec3 dominant = abs(normal);
	if (dominant.x >= dominant.y && dominant.x >= dominant.z) {
		return position.yz;
	}
	if (dominant.y >= dominant.z) {
		return position.xz;
	}
	return position.xy;
}

vec3 apply_surface_grid(vec3 base_color) {
	if (surface_grid_enabled <= 0.5) {
		return base_color;
	}

	vec2 coords = surface_grid_coordinates(world_position, normalize(world_normal));
	float world_pixel_width = max(length(dFdx(world_position)), length(dFdy(world_position)));
	float base_spacing = max(surface_grid_size, 0.0001);
	float major_spacing = max(surface_grid_major_size, base_spacing);
	float minor_line = max(
		surface_grid_line(coords.x, base_spacing, surface_grid_minor_line_thickness, world_pixel_width),
		surface_grid_line(coords.y, base_spacing, surface_grid_minor_line_thickness, world_pixel_width));
	float major_line = max(
		surface_grid_line(coords.x, major_spacing, surface_grid_major_line_thickness, world_pixel_width),
		surface_grid_line(coords.y, major_spacing, surface_grid_major_line_thickness, world_pixel_width));

	float major_coverage = major_line * surface_grid_major_color.a;
	float minor_coverage = minor_line * surface_grid_minor_color.a * (1.0 - clamp(major_coverage, 0.0, 1.0));
	float alpha = clamp(minor_coverage + major_coverage, 0.0, 1.0);
	if (alpha <= 0.000001) {
		return base_color;
	}
	vec3 grid_color =
		(surface_grid_minor_color.rgb * minor_coverage + surface_grid_major_color.rgb * major_coverage) / alpha;
	return mix(base_color, grid_color, alpha);
}

void fragment() {
	vec2 uv = vec2(UV.x, 1.0 - UV.y);
	vec4 base_sample = texture(base_color_texture, uv);
	vec3 base_color = base_sample.rgb * base_color_factor.rgb;
	base_color = apply_surface_grid(base_color);

	ALBEDO = base_color;
}
)";
}

} // namespace

ViewportVisualSettings default_viewport_visual_settings() {
	return {
			.grid_minor_color = rgba8(255, 255, 255, 0x16),
			.grid_major_color = rgba8(255, 255, 255, 0x2e),
			.grid_x_axis_color = godot::Color::html("ff0000"),
			.grid_z_axis_color = godot::Color::html("006cff"),
			.mesh_grid_minor_color = rgba8(255, 255, 255, 0x0a),
			.mesh_grid_major_color = rgba8(255, 255, 255, 0x15),
			.background_color = rgba8(0x15, 0x15, 0x15),
			.selection_face_color = abgr8(0x081facffU),
			.selection_wire_color = abgr8(0xff1fd3ffU),
			.source_wire_color = abgr8(0xebffe05cU),
			.open_edge_color = abgr8(0xff00ffffU),
			.diagnostic_edge_color = abgr8(0xff0000ffU),
			.hover_face_color = abgr8(0x161fff66U),
			.hover_wire_color = abgr8(0xff00ff51U),
			.remove_face_color = abgr8(0x52ff6b1fU),
			.remove_wire_color = abgr8(0xffff6b1fU),
			.vertex_color = abgr8(0xffffe66bU),
			.selected_vertex_color = abgr8(0xff1fadffU),
			.hover_vertex_color = abgr8(0xff38ff00U),
			.remove_vertex_color = abgr8(0xffff6b1fU),
			.vertex_outline_color = abgr8(0xe6000000U),
			.grid_world_size = 1.0f,
			.minor_line_size = 0.25f,
			.major_line_size = 0.25f,
			.axis_line_size = 1.0f,
			.source_wire_line_size = 1.0f,
			.selection_face_wire_line_size = 1.0f,
			.selection_edge_line_size = 2.0f,
			.hover_wire_line_size = 2.0f,
			.open_edge_line_size = 1.0f,
			.diagnostic_edge_line_size = 4.0f,
			.vertex_size = 10.0f,
			.selected_vertex_growth = 1.0f,
			.hover_vertex_growth = 1.0f,
			.vertex_outline_size = 1.0f,
			.pick_vertex_radius = 0.07f,
			.pick_edge_radius = 0.07f,
	};
}

godot::Ref<godot::Texture2D> load_texture(const godot::String &path) {
	godot::Ref<godot::Texture2D> texture;
	godot::ResourceLoader *loader = godot::ResourceLoader::get_singleton();
	if (loader != nullptr) {
		godot::Ref<godot::Resource> resource = loader->load(path);
		texture = resource;
		if (texture.is_valid()) {
			return texture;
		}
	}

	godot::Ref<godot::Image> image;
	image.instantiate();
	if (image.is_valid() && image->load(path) == godot::OK) {
		texture = godot::ImageTexture::create_from_image(image);
	}
	return texture;
}

godot::Ref<godot::ArrayMesh> make_array_mesh(const modeling::MeshPayload &payload) {
	godot::PackedVector3Array vertices;
	godot::PackedVector3Array normals;
	godot::PackedVector2Array uvs;
	for (const quader::modeling::MeshVertexPayload &vertex : payload.vertices) {
		vertices.push_back({vertex.position.x, vertex.position.y, vertex.position.z});
		normals.push_back({vertex.normal.x, vertex.normal.y, vertex.normal.z});
		uvs.push_back({vertex.uv0.x, vertex.uv0.y});
	}

	godot::PackedInt32Array indices;
	for (std::size_t index = 0; index + 2 < payload.indices.size(); index += 3) {
		indices.push_back(static_cast<std::int32_t>(payload.indices[index]));
		indices.push_back(static_cast<std::int32_t>(payload.indices[index + 2]));
		indices.push_back(static_cast<std::int32_t>(payload.indices[index + 1]));
	}

	godot::Array arrays;
	arrays.resize(godot::Mesh::ARRAY_MAX);
	arrays[godot::Mesh::ARRAY_VERTEX] = vertices;
	arrays[godot::Mesh::ARRAY_NORMAL] = normals;
	arrays[godot::Mesh::ARRAY_TEX_UV] = uvs;
	arrays[godot::Mesh::ARRAY_INDEX] = indices;

	godot::Ref<godot::ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(godot::Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

godot::Ref<godot::ShaderMaterial> make_default_quader_material() {
	return make_default_quader_material(default_viewport_visual_settings());
}

godot::Ref<godot::ShaderMaterial> make_default_quader_material(const ViewportVisualSettings &settings) {
	godot::Ref<godot::Shader> shader;
	shader.instantiate();
	shader->set_code(default_quader_mesh_shader_code());

	godot::Ref<godot::ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	material->set_shader_parameter("base_color_texture",
			load_texture("res://addons/quader/materials/default/default_albedo.png"));
	apply_default_quader_material_settings(material, settings);
	return material;
}

void apply_default_quader_material_settings(const godot::Ref<godot::ShaderMaterial> &material,
		const ViewportVisualSettings &settings) {
	if (material.is_null()) {
		return;
	}
	material->set_shader_parameter("base_color_factor", godot::Color(1.0f, 1.0f, 1.0f, 1.0f));
	material->set_shader_parameter("surface_grid_minor_color", settings.mesh_grid_minor_color);
	material->set_shader_parameter("surface_grid_major_color", settings.mesh_grid_major_color);
	const float spacing = std::max(settings.grid_world_size, 0.0001f);
	material->set_shader_parameter("surface_grid_size", spacing);
	material->set_shader_parameter("surface_grid_major_size",
			std::max(spacing * kMeshSurfaceMajorGridMultiplier, spacing));
	material->set_shader_parameter("surface_grid_minor_line_thickness", settings.minor_line_size);
	material->set_shader_parameter("surface_grid_major_line_thickness", settings.major_line_size);
	material->set_shader_parameter("surface_grid_enabled", 1.0f);
}

godot::Ref<godot::ShaderMaterial> make_ground_grid_material() {
	return make_ground_grid_material(default_viewport_visual_settings());
}

godot::Ref<godot::ShaderMaterial> make_ground_grid_material(const ViewportVisualSettings &settings) {
	godot::Ref<godot::Shader> shader;
	shader.instantiate();
	shader->set_code(ground_grid_shader_code());

	godot::Ref<godot::ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);
	apply_ground_grid_settings(material, settings);
	return material;
}

void apply_ground_grid_settings(const godot::Ref<godot::ShaderMaterial> &material,
		const ViewportVisualSettings &settings) {
	if (material.is_null()) {
		return;
	}
	material->set_shader_parameter("grid_color", settings.grid_minor_color);
	material->set_shader_parameter("major_grid_color", settings.grid_major_color);
	material->set_shader_parameter("x_axis_color", settings.grid_x_axis_color);
	material->set_shader_parameter("z_axis_color", settings.grid_z_axis_color);
	const float spacing = std::max(settings.grid_world_size, 0.0001f);
	material->set_shader_parameter("grid_size", spacing);
	material->set_shader_parameter("major_grid_size", std::max(kParentGridWorldSize, spacing));
	material->set_shader_parameter("line_thickness", settings.minor_line_size);
	material->set_shader_parameter("major_line_thickness", settings.major_line_size);
	material->set_shader_parameter("axis_line_thickness", settings.axis_line_size);
}

godot::MeshInstance3D *make_ground_grid() {
	return make_ground_grid(make_ground_grid_material());
}

godot::MeshInstance3D *make_ground_grid(const godot::Ref<godot::ShaderMaterial> &material) {
	godot::Ref<godot::PlaneMesh> plane;
	plane.instantiate();
	plane->set_size({kGridPlaneSize, kGridPlaneSize});

	auto *instance = memnew(godot::MeshInstance3D);
	instance->set_name("QuaderGroundGrid");
	instance->set_mesh(plane);
	instance->set_surface_override_material(0, material);
	return instance;
}

godot::Ref<godot::Environment> make_environment() {
	return make_environment(default_viewport_visual_settings());
}

godot::Ref<godot::Environment> make_environment(const ViewportVisualSettings &settings) {
	godot::Ref<godot::Environment> environment;
	environment.instantiate();
	apply_environment_settings(environment, settings);
	return environment;
}

void apply_environment_settings(const godot::Ref<godot::Environment> &environment,
		const ViewportVisualSettings &settings) {
	if (environment.is_null()) {
		return;
	}
	environment->set_background(godot::Environment::BG_COLOR);
	environment->set_bg_color(settings.background_color);
	environment->set_ambient_source(godot::Environment::AMBIENT_SOURCE_COLOR);
	environment->set_ambient_light_color({0.75f, 0.75f, 0.75f, 1.0f});
	environment->set_ambient_light_energy(0.35f);
	environment->set_ambient_light_sky_contribution(0.0f);
	environment->set_reflection_source(godot::Environment::REFLECTION_SOURCE_DISABLED);
}

godot::Ref<godot::World3D> make_world() {
	return make_world(make_environment());
}

godot::Ref<godot::World3D> make_world(const godot::Ref<godot::Environment> &environment) {
	godot::Ref<godot::World3D> world;
	world.instantiate();
	world->set_environment(environment);
	world->set_fallback_environment(environment);
	return world;
}

godot::WorldEnvironment *make_world_environment() {
	return make_world_environment(make_environment());
}

godot::WorldEnvironment *make_world_environment(const godot::Ref<godot::Environment> &environment) {
	auto *world = memnew(godot::WorldEnvironment);
	world->set_name("QuaderEnvironment");
	world->set_environment(environment);
	return world;
}

godot::Camera3D *make_camera() {
	auto *camera = memnew(godot::Camera3D);
	camera->set_name("QuaderCamera");
	camera->set_perspective(60.0f, 0.05f, 1000.0f);
	camera->set_current(true);
	return camera;
}

} // namespace quader_godot::render
