#include "viewport/quader_viewport_visual_settings.h"

#include <godot_cpp/variant/string.hpp>

namespace quader_godot::viewport {

ViewportVisualSettings default_viewport_visual_settings() {
	return {
			.grid_minor_color = godot::Color(godot::String("#ffffff16")),
			.grid_major_color = godot::Color(godot::String("#ffffff2e")),
			.grid_x_axis_color = godot::Color(godot::String("#ff0000")),
			.grid_z_axis_color = godot::Color(godot::String("#006cff")),
			.mesh_grid_minor_color = godot::Color(godot::String("#ffffff0a")),
			.mesh_grid_major_color = godot::Color(godot::String("#ffffff15")),
			.background_color = godot::Color(godot::String("#151515")),
			.selection_face_color = godot::Color(godot::String("#ffac1f08")),
			.selection_wire_color = godot::Color(godot::String("#ffd31fff")),
			.source_wire_color = godot::Color(godot::String("#5ce0ffeb")),
			.open_edge_color = godot::Color(godot::String("#ffff00ff")),
			.diagnostic_edge_color = godot::Color(godot::String("#ff0000ff")),
			.hover_face_color = godot::Color(godot::String("#66ff1f16")),
			.hover_wire_color = godot::Color(godot::String("#51ff00ff")),
			.remove_face_color = godot::Color(godot::String("#1f6bff52")),
			.remove_wire_color = godot::Color(godot::String("#1f6bffff")),
			.vertex_color = godot::Color(godot::String("#6be6ffff")),
			.selected_vertex_color = godot::Color(godot::String("#ffad1fff")),
			.hover_vertex_color = godot::Color(godot::String("#00ff38ff")),
			.remove_vertex_color = godot::Color(godot::String("#1f6bffff")),
			.vertex_outline_color = godot::Color(godot::String("#000000e6")),
	};
}

} // namespace quader_godot::viewport
