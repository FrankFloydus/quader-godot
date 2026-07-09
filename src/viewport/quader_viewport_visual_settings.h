#pragma once

#include <godot_cpp/variant/color.hpp>

namespace quader_godot::viewport {

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
	float source_wire_line_size = 1.0f;
	float selection_face_wire_line_size = 1.0f;
	float selection_edge_line_size = 2.0f;
	float hover_wire_line_size = 2.0f;
	float open_edge_line_size = 1.0f;
	float diagnostic_edge_line_size = 4.0f;
	float vertex_size = 10.0f;
	float selected_vertex_growth = 1.0f;
	float hover_vertex_growth = 1.0f;
	float vertex_outline_size = 1.0f;
	float pick_vertex_radius = 0.07f;
	float pick_edge_radius = 0.07f;
};

[[nodiscard]] ViewportVisualSettings default_viewport_visual_settings();

} // namespace quader_godot::viewport
