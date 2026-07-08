#pragma once

#include "modeling/quader_modeling_adapter.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/variant/color.hpp>

namespace quader_godot::render {

struct ViewportVisualSettings {
	godot::Color grid_minor_color;
	godot::Color grid_major_color;
	godot::Color grid_x_axis_color;
	godot::Color grid_z_axis_color;
	godot::Color mesh_grid_minor_color;
	godot::Color mesh_grid_major_color;
	godot::Color background_color;
	godot::Color selection_face_color;
	godot::Color selection_wire_color;
	godot::Color source_wire_color;
	godot::Color open_edge_color;
	godot::Color diagnostic_edge_color;
	godot::Color hover_face_color;
	godot::Color hover_wire_color;
	godot::Color remove_face_color;
	godot::Color remove_wire_color;
	godot::Color vertex_color;
	godot::Color selected_vertex_color;
	godot::Color hover_vertex_color;
	godot::Color remove_vertex_color;
	godot::Color vertex_outline_color;
	float grid_world_size = 1.0f;
	float minor_line_size = 0.25f;
	float major_line_size = 0.25f;
	float axis_line_size = 1.0f;
	float source_wire_line_size = 1.5f;
	float selection_face_wire_line_size = 2.5f;
	float selection_edge_line_size = 2.5f;
	float hover_wire_line_size = 2.5f;
	float open_edge_line_size = 1.0f;
	float diagnostic_edge_line_size = 4.0f;
	float vertex_size = 8.0f;
	float selected_vertex_growth = 1.0f;
	float hover_vertex_growth = 1.0f;
	float vertex_outline_size = 1.0f;
	float pick_vertex_radius = 0.3f;
	float pick_edge_radius = 0.2f;
};

[[nodiscard]] ViewportVisualSettings default_viewport_visual_settings();
godot::Ref<godot::Texture2D> load_texture(const godot::String &path);
godot::Ref<godot::ArrayMesh> make_array_mesh(const modeling::MeshPayload &payload);
godot::Ref<godot::ShaderMaterial> make_default_quader_material();
godot::Ref<godot::ShaderMaterial> make_default_quader_material(const ViewportVisualSettings &settings);
void apply_default_quader_material_settings(const godot::Ref<godot::ShaderMaterial> &material,
		const ViewportVisualSettings &settings);
godot::Ref<godot::ShaderMaterial> make_ground_grid_material();
godot::Ref<godot::ShaderMaterial> make_ground_grid_material(const ViewportVisualSettings &settings);
void apply_ground_grid_settings(const godot::Ref<godot::ShaderMaterial> &material,
		const ViewportVisualSettings &settings);
godot::MeshInstance3D *make_ground_grid();
godot::MeshInstance3D *make_ground_grid(const godot::Ref<godot::ShaderMaterial> &material);
godot::Ref<godot::Environment> make_environment();
godot::Ref<godot::Environment> make_environment(const ViewportVisualSettings &settings);
void apply_environment_settings(const godot::Ref<godot::Environment> &environment,
		const ViewportVisualSettings &settings);
godot::Ref<godot::World3D> make_world();
godot::Ref<godot::World3D> make_world(const godot::Ref<godot::Environment> &environment);
godot::WorldEnvironment *make_world_environment();
godot::WorldEnvironment *make_world_environment(const godot::Ref<godot::Environment> &environment);
godot::Camera3D *make_camera();

} // namespace quader_godot::render
