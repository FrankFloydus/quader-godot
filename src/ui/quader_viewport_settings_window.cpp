#include "ui/quader_viewport_settings_window.h"

#include "ui/ui_tokens.h"

#include <godot_cpp/classes/color_picker_button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable.hpp>

namespace quader_godot::ui {
namespace {

using godot::Callable;
using godot::Color;
using godot::ColorPickerButton;
using godot::Control;
using godot::GridContainer;
using godot::Label;
using godot::MarginContainer;
using godot::Object;
using godot::ScrollContainer;
using godot::SpinBox;
using godot::VBoxContainer;
using godot::Window;
using viewport::ViewportVisualSettings;

Label *make_label(const char *text) {
	auto *label = memnew(Label);
	label->set_text(text);
	label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	return label;
}

void add_section_label(VBoxContainer *root, const char *text) {
	auto *label = make_label(text);
	label->add_theme_constant_override(ConstantOverride::MarginTop, 8);
	root->add_child(label);
}

void add_color_row(GridContainer *grid, Object *target, const char *label_text,
		const Color &value, const char *method, bool edit_alpha) {
	grid->add_child(make_label(label_text));

	auto *button = memnew(ColorPickerButton);
	button->set_pick_color(value);
	button->set_edit_alpha(edit_alpha);
	button->set_custom_minimum_size({160.0f, 28.0f});
	button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	button->connect(SignalName::ColorChanged, Callable(target, method));
	grid->add_child(button);
}

void add_spin_row(GridContainer *grid, Object *target, const char *label_text,
		double value, double minimum, double maximum, double step, const char *method) {
	grid->add_child(make_label(label_text));

	auto *spin = memnew(SpinBox);
	spin->set_min(minimum);
	spin->set_max(maximum);
	spin->set_step(step);
	spin->set_value(value);
	spin->set_custom_minimum_size({160.0f, 28.0f});
	spin->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	spin->connect(SignalName::ValueChanged, Callable(target, method));
	grid->add_child(spin);
}

GridContainer *make_settings_grid() {
	auto *grid = memnew(GridContainer);
	grid->set_columns(2);
	grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	return grid;
}

} // namespace

Window *make_quader_viewport_settings_window(
		Object *target, const ViewportVisualSettings &settings) {
	auto *window = memnew(Window);
	window->set_title("Quader Settings");
	window->set_size({480, 720});
	window->set_min_size({420, 520});
	window->set_wrap_controls(true);
	window->set_transient(true);
	window->connect(SignalName::CloseRequested, Callable(target, "hide_settings_window"));

	auto *margin = memnew(MarginContainer);
	margin->set_anchors_preset(Control::PRESET_FULL_RECT);
	margin->add_theme_constant_override(ConstantOverride::MarginLeft, 12);
	margin->add_theme_constant_override(ConstantOverride::MarginTop, 12);
	margin->add_theme_constant_override(ConstantOverride::MarginRight, 12);
	margin->add_theme_constant_override(ConstantOverride::MarginBottom, 12);
	window->add_child(margin);

	auto *scroll = memnew(ScrollContainer);
	scroll->set_anchors_preset(Control::PRESET_FULL_RECT);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	margin->add_child(scroll);

	auto *root = memnew(VBoxContainer);
	root->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->add_child(root);

	add_section_label(root, "Grid Colors");
	GridContainer *color_grid = make_settings_grid();
	root->add_child(color_grid);
	add_color_row(color_grid, target, "Minor", settings.grid_minor_color, "set_grid_minor_color", true);
	add_color_row(color_grid, target, "Major", settings.grid_major_color, "set_grid_major_color", true);
	add_color_row(color_grid, target, "X Axis", settings.grid_x_axis_color, "set_grid_x_axis_color", true);
	add_color_row(color_grid, target, "Z Axis", settings.grid_z_axis_color, "set_grid_z_axis_color", true);
	add_color_row(color_grid, target, "Mesh Minor", settings.mesh_grid_minor_color, "set_mesh_grid_minor_color", true);
	add_color_row(color_grid, target, "Mesh Major", settings.mesh_grid_major_color, "set_mesh_grid_major_color", true);

	add_section_label(root, "Selection Colors");
	GridContainer *selection_color_grid = make_settings_grid();
	root->add_child(selection_color_grid);
	add_color_row(selection_color_grid, target, "Face Fill", settings.selection_face_color, "set_selection_face_color",
			true);
	add_color_row(selection_color_grid, target, "Wire", settings.selection_wire_color, "set_selection_wire_color", true);

	add_section_label(root, "Overlay Colors");
	GridContainer *overlay_color_grid = make_settings_grid();
	root->add_child(overlay_color_grid);
	add_color_row(overlay_color_grid, target, "Source Wire", settings.source_wire_color, "set_source_wire_color", true);
	add_color_row(overlay_color_grid, target, "Open Edge", settings.open_edge_color, "set_open_edge_color", true);
	add_color_row(overlay_color_grid, target, "Diagnostic Edge", settings.diagnostic_edge_color,
			"set_diagnostic_edge_color", true);
	add_color_row(overlay_color_grid, target, "Hover Face", settings.hover_face_color, "set_hover_face_color", true);
	add_color_row(overlay_color_grid, target, "Hover Wire", settings.hover_wire_color, "set_hover_wire_color", true);
	add_color_row(overlay_color_grid, target, "Remove Face", settings.remove_face_color, "set_remove_face_color", true);
	add_color_row(overlay_color_grid, target, "Remove Wire", settings.remove_wire_color, "set_remove_wire_color", true);

	add_section_label(root, "Vertex Colors");
	GridContainer *vertex_color_grid = make_settings_grid();
	root->add_child(vertex_color_grid);
	add_color_row(vertex_color_grid, target, "Vertex", settings.vertex_color, "set_vertex_color", true);
	add_color_row(vertex_color_grid, target, "Selected Vertex", settings.selected_vertex_color,
			"set_selected_vertex_color", true);
	add_color_row(vertex_color_grid, target, "Hover Vertex", settings.hover_vertex_color, "set_hover_vertex_color", true);
	add_color_row(vertex_color_grid, target, "Remove Vertex", settings.remove_vertex_color, "set_remove_vertex_color",
			true);
	add_color_row(vertex_color_grid, target, "Outline", settings.vertex_outline_color, "set_vertex_outline_color", true);

	add_section_label(root, "Grid Line Sizes");
	GridContainer *size_grid = make_settings_grid();
	root->add_child(size_grid);
	add_spin_row(size_grid, target, "Minor Line", settings.minor_line_size, 0.05, 8.0, 0.01, "set_minor_line_size");
	add_spin_row(size_grid, target, "Major Line", settings.major_line_size, 0.05, 8.0, 0.01, "set_major_line_size");
	add_spin_row(size_grid, target, "Axis Line", settings.axis_line_size, 0.05, 8.0, 0.01, "set_axis_line_size");

	add_section_label(root, "Overlay Sizes");
	GridContainer *overlay_size_grid = make_settings_grid();
	root->add_child(overlay_size_grid);
	add_spin_row(overlay_size_grid, target, "Cyan Component Wire", settings.source_wire_line_size, 0.25, 8.0, 0.01,
			"set_source_wire_line_size");
	add_spin_row(overlay_size_grid, target, "Orange Mesh/Face Wire", settings.selection_face_wire_line_size, 0.25,
			8.0, 0.01, "set_selection_face_wire_line_size");
	add_spin_row(overlay_size_grid, target, "Orange Edge Wire", settings.selection_edge_line_size, 0.25, 8.0, 0.01,
			"set_selection_edge_line_size");
	add_spin_row(overlay_size_grid, target, "Hover/Remove Wire", settings.hover_wire_line_size, 0.25, 8.0, 0.01,
			"set_hover_wire_line_size");
	add_spin_row(overlay_size_grid, target, "Open Boundary Wire", settings.open_edge_line_size, 0.25, 8.0, 0.01,
			"set_open_edge_line_size");
	add_spin_row(overlay_size_grid, target, "Diagnostic Wire", settings.diagnostic_edge_line_size, 0.25, 8.0, 0.01,
			"set_diagnostic_edge_line_size");

	add_section_label(root, "Vertex Sizes");
	GridContainer *vertex_size_grid = make_settings_grid();
	root->add_child(vertex_size_grid);
	add_spin_row(vertex_size_grid, target, "Base Vertex Quad", settings.vertex_size, 2.0, 32.0, 0.1,
			"set_vertex_size");
	add_spin_row(vertex_size_grid, target, "Selected Vertex Growth", settings.selected_vertex_growth, 0.0, 12.0, 0.1,
			"set_selected_vertex_growth");
	add_spin_row(vertex_size_grid, target, "Hover Vertex Growth", settings.hover_vertex_growth, 0.0, 12.0, 0.1,
			"set_hover_vertex_growth");
	add_spin_row(vertex_size_grid, target, "Selected/Hover Outline", settings.vertex_outline_size, 0.0, 12.0, 0.1,
			"set_vertex_outline_size");

	add_section_label(root, "Picking");
	GridContainer *picking_grid = make_settings_grid();
	root->add_child(picking_grid);
	add_spin_row(picking_grid, target, "Vertex Pick Radius", settings.pick_vertex_radius, 0.001, 2.0, 0.001,
			"set_pick_vertex_radius");
	add_spin_row(picking_grid, target, "Edge Pick Radius", settings.pick_edge_radius, 0.001, 2.0, 0.001,
			"set_pick_edge_radius");

	add_section_label(root, "Viewport");
	GridContainer *viewport_grid = make_settings_grid();
	root->add_child(viewport_grid);
	add_color_row(viewport_grid, target, "Background", settings.background_color, "set_background_color", false);

	return window;
}

} // namespace quader_godot::ui
