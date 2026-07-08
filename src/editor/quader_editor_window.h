#pragma once

#include "render/quader_godot_render_utils.h"

#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/color.hpp>

namespace quader_godot::viewport {
class QuaderViewportControl;
}

namespace godot {
class VBoxContainer;
class Window;
} // namespace godot

namespace quader_godot::editor {

class QuaderEditorWindow : public godot::Window {
	GDCLASS(QuaderEditorWindow, godot::Window)

public:
	QuaderEditorWindow() = default;
	~QuaderEditorWindow() override = default;

	void _notification(int what);
	void open_settings_window();
	void hide_settings_window();
	void focus_viewport();
	void on_edit_menu_id(int32_t id);
	void set_grid_minor_color(godot::Color color);
	void set_grid_major_color(godot::Color color);
	void set_grid_x_axis_color(godot::Color color);
	void set_grid_z_axis_color(godot::Color color);
	void set_mesh_grid_minor_color(godot::Color color);
	void set_mesh_grid_major_color(godot::Color color);
	void set_background_color(godot::Color color);
	void set_selection_face_color(godot::Color color);
	void set_selection_wire_color(godot::Color color);
	void set_source_wire_color(godot::Color color);
	void set_open_edge_color(godot::Color color);
	void set_diagnostic_edge_color(godot::Color color);
	void set_hover_face_color(godot::Color color);
	void set_hover_wire_color(godot::Color color);
	void set_remove_face_color(godot::Color color);
	void set_remove_wire_color(godot::Color color);
	void set_vertex_color(godot::Color color);
	void set_selected_vertex_color(godot::Color color);
	void set_hover_vertex_color(godot::Color color);
	void set_remove_vertex_color(godot::Color color);
	void set_vertex_outline_color(godot::Color color);
	void set_minor_line_size(double value);
	void set_major_line_size(double value);
	void set_axis_line_size(double value);
	void set_source_wire_line_size(double value);
	void set_selection_face_wire_line_size(double value);
	void set_selection_edge_line_size(double value);
	void set_hover_wire_line_size(double value);
	void set_open_edge_line_size(double value);
	void set_diagnostic_edge_line_size(double value);
	void set_vertex_size(double value);
	void set_selected_vertex_growth(double value);
	void set_hover_vertex_growth(double value);
	void set_vertex_outline_size(double value);
	void set_pick_vertex_radius(double value);
	void set_pick_edge_radius(double value);

protected:
	static void _bind_methods();

private:
	void build_content();
	void build_menu_bar(godot::VBoxContainer *root);
	void build_settings_window();
	void apply_visual_settings();
	void load_persisted_settings();
	void save_persisted_settings() const;
	void on_grid_preset_changed(int preset);

	bool built_ = false;
	int grid_preset_ = 6;
	render::ViewportVisualSettings visual_settings_ = render::default_viewport_visual_settings();
	godot::Window *settings_window_ = nullptr;
	viewport::QuaderViewportControl *viewport_ = nullptr;
};

} // namespace quader_godot::editor
