#pragma once

#include "modeling/quader_modeling_adapter.h"
#include "viewport/quader_viewport_visual_settings.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>

namespace quader_godot::render {

godot::Ref<godot::Texture2D> load_texture(const godot::String &path);
godot::Ref<godot::ArrayMesh> make_array_mesh(const modeling::MeshPayload &payload);
godot::Ref<godot::ShaderMaterial> make_default_quader_material();
godot::Ref<godot::ShaderMaterial> make_default_quader_material(const viewport::ViewportVisualSettings &settings);
void apply_default_quader_material_settings(const godot::Ref<godot::ShaderMaterial> &material,
		const viewport::ViewportVisualSettings &settings);
godot::Ref<godot::ShaderMaterial> make_ground_grid_material();
godot::Ref<godot::ShaderMaterial> make_ground_grid_material(const viewport::ViewportVisualSettings &settings);
void apply_ground_grid_settings(const godot::Ref<godot::ShaderMaterial> &material,
		const viewport::ViewportVisualSettings &settings);
godot::MeshInstance3D *make_ground_grid();
godot::MeshInstance3D *make_ground_grid(const godot::Ref<godot::ShaderMaterial> &material);
godot::Ref<godot::Environment> make_environment();
godot::Ref<godot::Environment> make_environment(const viewport::ViewportVisualSettings &settings);
void apply_environment_settings(const godot::Ref<godot::Environment> &environment,
		const viewport::ViewportVisualSettings &settings);
godot::Ref<godot::World3D> make_world();
godot::Ref<godot::World3D> make_world(const godot::Ref<godot::Environment> &environment);
godot::WorldEnvironment *make_world_environment();
godot::WorldEnvironment *make_world_environment(const godot::Ref<godot::Environment> &environment);

} // namespace quader_godot::render
