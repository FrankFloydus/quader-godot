#include "editor/quader_editor_window.h"

#include "viewport/quader_viewport_control.h"

#include <godot_cpp/classes/color_picker_button.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/menu_button.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace quader_godot::editor {
namespace {

constexpr int32_t kEditMenuSettingsId = 1;
constexpr float kTopBarHeight = 24.0f;
constexpr char kSettingsPath[] = "user://quader_viewport_settings.cfg";
constexpr char kSettingsSection[] = "viewport";
constexpr int kSettingsVersion = 3;
constexpr int kMinGridPreset = 1;
constexpr int kMaxGridPreset = 10;

godot::Color ui_color(int red, int green, int blue, float alpha = 1.0f) {
	return {static_cast<float>(red) / 255.0f, static_cast<float>(green) / 255.0f,
			static_cast<float>(blue) / 255.0f, alpha};
}

godot::Ref<godot::StyleBoxFlat> make_style_box(godot::Color color, float horizontal_margin, float vertical_margin) {
	godot::Ref<godot::StyleBoxFlat> style;
	style.instantiate();
	style->set_bg_color(color);
	style->set_border_width_all(0);
	style->set_corner_radius_all(0);
	style->set_content_margin(godot::SIDE_LEFT, horizontal_margin);
	style->set_content_margin(godot::SIDE_RIGHT, horizontal_margin);
	style->set_content_margin(godot::SIDE_TOP, vertical_margin);
	style->set_content_margin(godot::SIDE_BOTTOM, vertical_margin);
	return style;
}

godot::Ref<godot::StyleBoxEmpty> make_empty_style_box() {
	godot::Ref<godot::StyleBoxEmpty> style;
	style.instantiate();
	return style;
}

godot::Label *make_label(const char *text) {
	auto *label = memnew(godot::Label);
	label->set_text(text);
	label->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
	return label;
}

bool float_near(float a, float b) {
	return std::abs(a - b) <= 0.0001f;
}

void add_section_label(godot::VBoxContainer *root, const char *text) {
	auto *label = make_label(text);
	label->add_theme_constant_override(godot::StringName("margin_top"), 8);
	root->add_child(label);
}

void add_color_row(godot::GridContainer *grid, QuaderEditorWindow *target, const char *label_text,
		const godot::Color &value, const char *method, bool edit_alpha) {
	grid->add_child(make_label(label_text));

	auto *button = memnew(godot::ColorPickerButton);
	button->set_pick_color(value);
	button->set_edit_alpha(edit_alpha);
	button->set_custom_minimum_size({160.0f, 28.0f});
	button->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
	button->connect("color_changed", godot::Callable(target, method));
	grid->add_child(button);
}

void add_spin_row(godot::GridContainer *grid, QuaderEditorWindow *target, const char *label_text,
		double value, double minimum, double maximum, double step, const char *method) {
	grid->add_child(make_label(label_text));

	auto *spin = memnew(godot::SpinBox);
	spin->set_min(minimum);
	spin->set_max(maximum);
	spin->set_step(step);
	spin->set_value(value);
	spin->set_custom_minimum_size({160.0f, 28.0f});
	spin->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
	spin->connect("value_changed", godot::Callable(target, method));
	grid->add_child(spin);
}

godot::GridContainer *make_settings_grid() {
	auto *grid = memnew(godot::GridContainer);
	grid->set_columns(2);
	grid->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
	return grid;
}

godot::Color read_color_setting(const godot::Ref<godot::ConfigFile> &config, const char *key,
		const godot::Color &fallback) {
	return static_cast<godot::Color>(config->get_value(kSettingsSection, key, godot::Variant(fallback)));
}

float read_float_setting(const godot::Ref<godot::ConfigFile> &config, const char *key, float fallback) {
	return static_cast<float>(
			static_cast<double>(config->get_value(kSettingsSection, key, godot::Variant(fallback))));
}

int read_int_setting(const godot::Ref<godot::ConfigFile> &config, const char *key, int fallback) {
	return static_cast<int>(static_cast<int64_t>(config->get_value(kSettingsSection, key, godot::Variant(fallback))));
}

} // namespace

void QuaderEditorWindow::_bind_methods() {
	godot::ClassDB::bind_method(godot::D_METHOD("open_settings_window"), &QuaderEditorWindow::open_settings_window);
	godot::ClassDB::bind_method(godot::D_METHOD("hide_settings_window"), &QuaderEditorWindow::hide_settings_window);
	godot::ClassDB::bind_method(godot::D_METHOD("focus_viewport"), &QuaderEditorWindow::focus_viewport);
	godot::ClassDB::bind_method(godot::D_METHOD("on_edit_menu_id", "id"), &QuaderEditorWindow::on_edit_menu_id);
	godot::ClassDB::bind_method(godot::D_METHOD("set_grid_minor_color", "color"),
			&QuaderEditorWindow::set_grid_minor_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_grid_major_color", "color"),
			&QuaderEditorWindow::set_grid_major_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_grid_x_axis_color", "color"),
			&QuaderEditorWindow::set_grid_x_axis_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_grid_z_axis_color", "color"),
			&QuaderEditorWindow::set_grid_z_axis_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_mesh_grid_minor_color", "color"),
			&QuaderEditorWindow::set_mesh_grid_minor_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_mesh_grid_major_color", "color"),
			&QuaderEditorWindow::set_mesh_grid_major_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_background_color", "color"),
			&QuaderEditorWindow::set_background_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_selection_face_color", "color"),
			&QuaderEditorWindow::set_selection_face_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_selection_wire_color", "color"),
			&QuaderEditorWindow::set_selection_wire_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_source_wire_color", "color"),
			&QuaderEditorWindow::set_source_wire_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_open_edge_color", "color"),
			&QuaderEditorWindow::set_open_edge_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_diagnostic_edge_color", "color"),
			&QuaderEditorWindow::set_diagnostic_edge_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_hover_face_color", "color"),
			&QuaderEditorWindow::set_hover_face_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_hover_wire_color", "color"),
			&QuaderEditorWindow::set_hover_wire_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_remove_face_color", "color"),
			&QuaderEditorWindow::set_remove_face_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_remove_wire_color", "color"),
			&QuaderEditorWindow::set_remove_wire_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_vertex_color", "color"),
			&QuaderEditorWindow::set_vertex_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_selected_vertex_color", "color"),
			&QuaderEditorWindow::set_selected_vertex_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_hover_vertex_color", "color"),
			&QuaderEditorWindow::set_hover_vertex_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_remove_vertex_color", "color"),
			&QuaderEditorWindow::set_remove_vertex_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_vertex_outline_color", "color"),
			&QuaderEditorWindow::set_vertex_outline_color);
	godot::ClassDB::bind_method(godot::D_METHOD("set_minor_line_size", "value"),
			&QuaderEditorWindow::set_minor_line_size);
	godot::ClassDB::bind_method(godot::D_METHOD("set_major_line_size", "value"),
			&QuaderEditorWindow::set_major_line_size);
	godot::ClassDB::bind_method(godot::D_METHOD("set_axis_line_size", "value"),
			&QuaderEditorWindow::set_axis_line_size);
	godot::ClassDB::bind_method(godot::D_METHOD("set_source_wire_line_size", "value"),
			&QuaderEditorWindow::set_source_wire_line_size);
	godot::ClassDB::bind_method(godot::D_METHOD("set_selection_face_wire_line_size", "value"),
			&QuaderEditorWindow::set_selection_face_wire_line_size);
	godot::ClassDB::bind_method(godot::D_METHOD("set_selection_edge_line_size", "value"),
			&QuaderEditorWindow::set_selection_edge_line_size);
	godot::ClassDB::bind_method(godot::D_METHOD("set_hover_wire_line_size", "value"),
			&QuaderEditorWindow::set_hover_wire_line_size);
	godot::ClassDB::bind_method(godot::D_METHOD("set_open_edge_line_size", "value"),
			&QuaderEditorWindow::set_open_edge_line_size);
	godot::ClassDB::bind_method(godot::D_METHOD("set_diagnostic_edge_line_size", "value"),
			&QuaderEditorWindow::set_diagnostic_edge_line_size);
	godot::ClassDB::bind_method(godot::D_METHOD("set_vertex_size", "value"),
			&QuaderEditorWindow::set_vertex_size);
	godot::ClassDB::bind_method(godot::D_METHOD("set_selected_vertex_growth", "value"),
			&QuaderEditorWindow::set_selected_vertex_growth);
	godot::ClassDB::bind_method(godot::D_METHOD("set_hover_vertex_growth", "value"),
			&QuaderEditorWindow::set_hover_vertex_growth);
	godot::ClassDB::bind_method(godot::D_METHOD("set_vertex_outline_size", "value"),
			&QuaderEditorWindow::set_vertex_outline_size);
	godot::ClassDB::bind_method(godot::D_METHOD("set_pick_vertex_radius", "value"),
			&QuaderEditorWindow::set_pick_vertex_radius);
	godot::ClassDB::bind_method(godot::D_METHOD("set_pick_edge_radius", "value"),
			&QuaderEditorWindow::set_pick_edge_radius);
}

void QuaderEditorWindow::_notification(int what) {
	if (what == NOTIFICATION_READY) {
		build_content();
		focus_viewport();
		return;
	}
	if (what == NOTIFICATION_WM_WINDOW_FOCUS_IN || what == NOTIFICATION_APPLICATION_FOCUS_IN) {
		focus_viewport();
		return;
	}
	if (what == NOTIFICATION_WM_CLOSE_REQUEST) {
		if (viewport_ != nullptr) {
			viewport_->release_mouse_capture();
		}
		if (settings_window_ != nullptr) {
			settings_window_->hide();
		}
		hide();
		return;
	}
	if (what == NOTIFICATION_VISIBILITY_CHANGED && viewport_ != nullptr) {
		if (is_visible()) {
			focus_viewport();
		} else {
			viewport_->release_mouse_capture();
		}
	}
}

void QuaderEditorWindow::build_content() {
	if (built_) {
		return;
	}
	built_ = true;

	load_persisted_settings();

	set_title("Quader");
	set_size({1280, 800});
	set_min_size({640, 360});
	set_wrap_controls(true);

	auto *root = memnew(godot::VBoxContainer);
	root->set_anchors_preset(godot::Control::PRESET_FULL_RECT);
	add_child(root);
	build_menu_bar(root);

	viewport_ = memnew(viewport::QuaderViewportControl);
	viewport_->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
	viewport_->set_v_size_flags(godot::Control::SIZE_EXPAND_FILL);
	viewport_->set_visual_settings(visual_settings_);
	viewport_->set_grid_preset(grid_preset_);
	visual_settings_.grid_world_size = viewport_->visual_settings().grid_world_size;
	viewport_->set_grid_preset_changed_callback([this](int preset) { on_grid_preset_changed(preset); });
	root->add_child(viewport_);
	build_settings_window();
}

void QuaderEditorWindow::build_menu_bar(godot::VBoxContainer *root) {
	auto *top_bar_panel = memnew(godot::PanelContainer);
	top_bar_panel->set_name("QuaderTopBar");
	top_bar_panel->set_custom_minimum_size({0.0f, kTopBarHeight});
	top_bar_panel->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
	top_bar_panel->add_theme_stylebox_override(godot::StringName("panel"), make_style_box(ui_color(25, 25, 25), 0.0f, 0.0f));
	root->add_child(top_bar_panel);

	auto *top_bar = memnew(godot::HBoxContainer);
	top_bar->set_custom_minimum_size({0.0f, kTopBarHeight});
	top_bar->add_theme_constant_override(godot::StringName("separation"), 0);
	top_bar_panel->add_child(top_bar);

	auto *edit_menu = memnew(godot::MenuButton);
	edit_menu->set_name("QuaderEditMenu");
	edit_menu->set_text("Edit");
	edit_menu->set_flat(true);
	edit_menu->set_custom_minimum_size({42.0f, kTopBarHeight});
	edit_menu->add_theme_font_size_override(godot::StringName("font_size"), 14);
	edit_menu->add_theme_color_override(godot::StringName("font_color"), ui_color(216, 216, 216));
	edit_menu->add_theme_color_override(godot::StringName("font_hover_color"), ui_color(216, 216, 216));
	edit_menu->add_theme_color_override(godot::StringName("font_pressed_color"), ui_color(216, 216, 216));
	edit_menu->add_theme_stylebox_override(godot::StringName("normal"), make_style_box(ui_color(25, 25, 25), 6.0f, 2.0f));
	edit_menu->add_theme_stylebox_override(godot::StringName("hover"), make_style_box(ui_color(46, 46, 46), 6.0f, 2.0f));
	edit_menu->add_theme_stylebox_override(godot::StringName("pressed"), make_style_box(ui_color(60, 60, 60), 6.0f, 2.0f));
	edit_menu->add_theme_stylebox_override(godot::StringName("focus"), make_empty_style_box());
	top_bar->add_child(edit_menu);

	godot::PopupMenu *popup = edit_menu->get_popup();
	if (popup != nullptr) {
		popup->add_theme_font_size_override(godot::StringName("font_size"), 14);
		popup->add_theme_color_override(godot::StringName("font_color"), ui_color(216, 216, 216));
		popup->add_theme_stylebox_override(godot::StringName("panel"), make_style_box(ui_color(15, 15, 15), 4.0f, 4.0f));
		popup->add_item("Settings", kEditMenuSettingsId);
		popup->connect("id_pressed", godot::Callable(this, "on_edit_menu_id"));
	}
}

void QuaderEditorWindow::build_settings_window() {
	if (settings_window_ != nullptr) {
		return;
	}

	settings_window_ = memnew(godot::Window);
	settings_window_->set_title("Quader Settings");
	settings_window_->set_size({480, 720});
	settings_window_->set_min_size({420, 520});
	settings_window_->set_wrap_controls(true);
	settings_window_->set_transient(true);
	settings_window_->connect("close_requested", godot::Callable(this, "hide_settings_window"));
	add_child(settings_window_);
	settings_window_->hide();

	auto *margin = memnew(godot::MarginContainer);
	margin->set_anchors_preset(godot::Control::PRESET_FULL_RECT);
	margin->add_theme_constant_override(godot::StringName("margin_left"), 12);
	margin->add_theme_constant_override(godot::StringName("margin_top"), 12);
	margin->add_theme_constant_override(godot::StringName("margin_right"), 12);
	margin->add_theme_constant_override(godot::StringName("margin_bottom"), 12);
	settings_window_->add_child(margin);

	auto *scroll = memnew(godot::ScrollContainer);
	scroll->set_anchors_preset(godot::Control::PRESET_FULL_RECT);
	scroll->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(godot::Control::SIZE_EXPAND_FILL);
	margin->add_child(scroll);

	auto *root = memnew(godot::VBoxContainer);
	root->set_h_size_flags(godot::Control::SIZE_EXPAND_FILL);
	scroll->add_child(root);

	add_section_label(root, "Grid Colors");
	godot::GridContainer *color_grid = make_settings_grid();
	root->add_child(color_grid);
	add_color_row(color_grid, this, "Minor", visual_settings_.grid_minor_color, "set_grid_minor_color", true);
	add_color_row(color_grid, this, "Major", visual_settings_.grid_major_color, "set_grid_major_color", true);
	add_color_row(color_grid, this, "X Axis", visual_settings_.grid_x_axis_color, "set_grid_x_axis_color", true);
	add_color_row(color_grid, this, "Z Axis", visual_settings_.grid_z_axis_color, "set_grid_z_axis_color", true);
	add_color_row(color_grid, this, "Mesh Minor", visual_settings_.mesh_grid_minor_color, "set_mesh_grid_minor_color",
			true);
	add_color_row(color_grid, this, "Mesh Major", visual_settings_.mesh_grid_major_color, "set_mesh_grid_major_color",
			true);

	add_section_label(root, "Selection Colors");
	godot::GridContainer *selection_color_grid = make_settings_grid();
	root->add_child(selection_color_grid);
	add_color_row(selection_color_grid, this, "Face Fill", visual_settings_.selection_face_color,
			"set_selection_face_color", true);
	add_color_row(selection_color_grid, this, "Wire", visual_settings_.selection_wire_color, "set_selection_wire_color",
			true);

	add_section_label(root, "Overlay Colors");
	godot::GridContainer *overlay_color_grid = make_settings_grid();
	root->add_child(overlay_color_grid);
	add_color_row(overlay_color_grid, this, "Source Wire", visual_settings_.source_wire_color, "set_source_wire_color",
			true);
	add_color_row(overlay_color_grid, this, "Open Edge", visual_settings_.open_edge_color, "set_open_edge_color",
			true);
	add_color_row(overlay_color_grid, this, "Diagnostic Edge", visual_settings_.diagnostic_edge_color,
			"set_diagnostic_edge_color", true);
	add_color_row(overlay_color_grid, this, "Hover Face", visual_settings_.hover_face_color, "set_hover_face_color",
			true);
	add_color_row(overlay_color_grid, this, "Hover Wire", visual_settings_.hover_wire_color, "set_hover_wire_color",
			true);
	add_color_row(overlay_color_grid, this, "Remove Face", visual_settings_.remove_face_color, "set_remove_face_color",
			true);
	add_color_row(overlay_color_grid, this, "Remove Wire", visual_settings_.remove_wire_color, "set_remove_wire_color",
			true);

	add_section_label(root, "Vertex Colors");
	godot::GridContainer *vertex_color_grid = make_settings_grid();
	root->add_child(vertex_color_grid);
	add_color_row(vertex_color_grid, this, "Vertex", visual_settings_.vertex_color, "set_vertex_color", true);
	add_color_row(vertex_color_grid, this, "Selected Vertex", visual_settings_.selected_vertex_color,
			"set_selected_vertex_color", true);
	add_color_row(vertex_color_grid, this, "Hover Vertex", visual_settings_.hover_vertex_color,
			"set_hover_vertex_color", true);
	add_color_row(vertex_color_grid, this, "Remove Vertex", visual_settings_.remove_vertex_color,
			"set_remove_vertex_color", true);
	add_color_row(vertex_color_grid, this, "Outline", visual_settings_.vertex_outline_color,
			"set_vertex_outline_color", true);

	add_section_label(root, "Grid Line Sizes");
	godot::GridContainer *size_grid = make_settings_grid();
	root->add_child(size_grid);
	add_spin_row(size_grid, this, "Minor Line", visual_settings_.minor_line_size, 0.05, 8.0, 0.01,
			"set_minor_line_size");
	add_spin_row(size_grid, this, "Major Line", visual_settings_.major_line_size, 0.05, 8.0, 0.01,
			"set_major_line_size");
	add_spin_row(size_grid, this, "Axis Line", visual_settings_.axis_line_size, 0.05, 8.0, 0.01,
			"set_axis_line_size");

	add_section_label(root, "Overlay Sizes");
	godot::GridContainer *overlay_size_grid = make_settings_grid();
	root->add_child(overlay_size_grid);
	add_spin_row(overlay_size_grid, this, "Cyan Component Wire", visual_settings_.source_wire_line_size, 0.25,
			8.0, 0.01, "set_source_wire_line_size");
	add_spin_row(overlay_size_grid, this, "Orange Mesh/Face Wire", visual_settings_.selection_face_wire_line_size,
			0.25, 8.0, 0.01, "set_selection_face_wire_line_size");
	add_spin_row(overlay_size_grid, this, "Orange Edge Wire", visual_settings_.selection_edge_line_size, 0.25, 8.0,
			0.01, "set_selection_edge_line_size");
	add_spin_row(overlay_size_grid, this, "Hover/Remove Wire", visual_settings_.hover_wire_line_size, 0.25, 8.0, 0.01,
			"set_hover_wire_line_size");
	add_spin_row(overlay_size_grid, this, "Open Boundary Wire", visual_settings_.open_edge_line_size, 0.25, 8.0, 0.01,
			"set_open_edge_line_size");
	add_spin_row(overlay_size_grid, this, "Diagnostic Wire", visual_settings_.diagnostic_edge_line_size, 0.25, 8.0,
			0.01, "set_diagnostic_edge_line_size");

	add_section_label(root, "Vertex Sizes");
	godot::GridContainer *vertex_size_grid = make_settings_grid();
	root->add_child(vertex_size_grid);
	add_spin_row(vertex_size_grid, this, "Base Vertex Quad", visual_settings_.vertex_size, 2.0, 32.0, 0.1,
			"set_vertex_size");
	add_spin_row(vertex_size_grid, this, "Selected Vertex Growth", visual_settings_.selected_vertex_growth, 0.0,
			12.0, 0.1, "set_selected_vertex_growth");
	add_spin_row(vertex_size_grid, this, "Hover Vertex Growth", visual_settings_.hover_vertex_growth, 0.0, 12.0, 0.1,
			"set_hover_vertex_growth");
	add_spin_row(vertex_size_grid, this, "Selected/Hover Outline", visual_settings_.vertex_outline_size, 0.0, 12.0, 0.1,
			"set_vertex_outline_size");

	add_section_label(root, "Picking");
	godot::GridContainer *picking_grid = make_settings_grid();
	root->add_child(picking_grid);
	add_spin_row(picking_grid, this, "Vertex Pick Radius", visual_settings_.pick_vertex_radius, 0.001, 2.0, 0.001,
			"set_pick_vertex_radius");
	add_spin_row(picking_grid, this, "Edge Pick Radius", visual_settings_.pick_edge_radius, 0.001, 2.0, 0.001,
			"set_pick_edge_radius");

	add_section_label(root, "Viewport");
	godot::GridContainer *viewport_grid = make_settings_grid();
	root->add_child(viewport_grid);
	add_color_row(viewport_grid, this, "Background", visual_settings_.background_color, "set_background_color", false);
}

void QuaderEditorWindow::open_settings_window() {
	build_settings_window();
	if (settings_window_ == nullptr) {
		return;
	}
	settings_window_->popup_centered({480, 720});
	settings_window_->grab_focus();
}

void QuaderEditorWindow::hide_settings_window() {
	if (settings_window_ != nullptr) {
		settings_window_->hide();
	}
	focus_viewport();
}

void QuaderEditorWindow::focus_viewport() {
	if (viewport_ == nullptr || !is_visible()) {
		return;
	}
	grab_focus();
	viewport_->grab_focus();
}

void QuaderEditorWindow::on_edit_menu_id(int32_t id) {
	if (id == kEditMenuSettingsId) {
		open_settings_window();
	}
}

void QuaderEditorWindow::set_grid_minor_color(godot::Color color) {
	visual_settings_.grid_minor_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_grid_major_color(godot::Color color) {
	visual_settings_.grid_major_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_grid_x_axis_color(godot::Color color) {
	visual_settings_.grid_x_axis_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_grid_z_axis_color(godot::Color color) {
	visual_settings_.grid_z_axis_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_mesh_grid_minor_color(godot::Color color) {
	visual_settings_.mesh_grid_minor_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_mesh_grid_major_color(godot::Color color) {
	visual_settings_.mesh_grid_major_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_background_color(godot::Color color) {
	color.a = 1.0f;
	visual_settings_.background_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_selection_face_color(godot::Color color) {
	visual_settings_.selection_face_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_selection_wire_color(godot::Color color) {
	visual_settings_.selection_wire_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_source_wire_color(godot::Color color) {
	visual_settings_.source_wire_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_open_edge_color(godot::Color color) {
	visual_settings_.open_edge_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_diagnostic_edge_color(godot::Color color) {
	visual_settings_.diagnostic_edge_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_hover_face_color(godot::Color color) {
	visual_settings_.hover_face_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_hover_wire_color(godot::Color color) {
	visual_settings_.hover_wire_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_remove_face_color(godot::Color color) {
	visual_settings_.remove_face_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_remove_wire_color(godot::Color color) {
	visual_settings_.remove_wire_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_vertex_color(godot::Color color) {
	visual_settings_.vertex_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_selected_vertex_color(godot::Color color) {
	visual_settings_.selected_vertex_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_hover_vertex_color(godot::Color color) {
	visual_settings_.hover_vertex_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_remove_vertex_color(godot::Color color) {
	visual_settings_.remove_vertex_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_vertex_outline_color(godot::Color color) {
	visual_settings_.vertex_outline_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_minor_line_size(double value) {
	visual_settings_.minor_line_size = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_major_line_size(double value) {
	visual_settings_.major_line_size = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_axis_line_size(double value) {
	visual_settings_.axis_line_size = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_source_wire_line_size(double value) {
	visual_settings_.source_wire_line_size = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_selection_face_wire_line_size(double value) {
	visual_settings_.selection_face_wire_line_size = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_selection_edge_line_size(double value) {
	visual_settings_.selection_edge_line_size = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_hover_wire_line_size(double value) {
	visual_settings_.hover_wire_line_size = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_open_edge_line_size(double value) {
	visual_settings_.open_edge_line_size = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_diagnostic_edge_line_size(double value) {
	visual_settings_.diagnostic_edge_line_size = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_vertex_size(double value) {
	visual_settings_.vertex_size = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_selected_vertex_growth(double value) {
	visual_settings_.selected_vertex_growth = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_hover_vertex_growth(double value) {
	visual_settings_.hover_vertex_growth = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_vertex_outline_size(double value) {
	visual_settings_.vertex_outline_size = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_pick_vertex_radius(double value) {
	visual_settings_.pick_vertex_radius = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::set_pick_edge_radius(double value) {
	visual_settings_.pick_edge_radius = static_cast<float>(value);
	apply_visual_settings();
}

void QuaderEditorWindow::apply_visual_settings() {
	if (viewport_ != nullptr) {
		visual_settings_.grid_world_size = viewport_->visual_settings().grid_world_size;
		viewport_->set_visual_settings(visual_settings_);
	}
	save_persisted_settings();
}

void QuaderEditorWindow::load_persisted_settings() {
	godot::Ref<godot::ConfigFile> config;
	config.instantiate();
	if (config.is_null() || config->load(kSettingsPath) != godot::OK) {
		return;
	}
	const int settings_version = read_int_setting(config, "settings_version", 0);
	if (settings_version <= 0 || settings_version > kSettingsVersion) {
		return;
	}

	visual_settings_.grid_minor_color =
			read_color_setting(config, "grid_minor_color", visual_settings_.grid_minor_color);
	visual_settings_.grid_major_color =
			read_color_setting(config, "grid_major_color", visual_settings_.grid_major_color);
	visual_settings_.grid_x_axis_color =
			read_color_setting(config, "grid_x_axis_color", visual_settings_.grid_x_axis_color);
	visual_settings_.grid_z_axis_color =
			read_color_setting(config, "grid_z_axis_color", visual_settings_.grid_z_axis_color);
	visual_settings_.mesh_grid_minor_color =
			read_color_setting(config, "mesh_grid_minor_color", visual_settings_.mesh_grid_minor_color);
	visual_settings_.mesh_grid_major_color =
			read_color_setting(config, "mesh_grid_major_color", visual_settings_.mesh_grid_major_color);
	visual_settings_.background_color =
			read_color_setting(config, "background_color", visual_settings_.background_color);
	visual_settings_.background_color.a = 1.0f;
	visual_settings_.selection_face_color =
			read_color_setting(config, "selection_face_color", visual_settings_.selection_face_color);
	visual_settings_.selection_wire_color =
			read_color_setting(config, "selection_wire_color", visual_settings_.selection_wire_color);
	visual_settings_.source_wire_color =
			read_color_setting(config, "source_wire_color", visual_settings_.source_wire_color);
	visual_settings_.open_edge_color = read_color_setting(config, "open_edge_color", visual_settings_.open_edge_color);
	visual_settings_.diagnostic_edge_color =
			read_color_setting(config, "diagnostic_edge_color", visual_settings_.diagnostic_edge_color);
	visual_settings_.hover_face_color =
			read_color_setting(config, "hover_face_color", visual_settings_.hover_face_color);
	visual_settings_.hover_wire_color =
			read_color_setting(config, "hover_wire_color", visual_settings_.hover_wire_color);
	visual_settings_.remove_face_color =
			read_color_setting(config, "remove_face_color", visual_settings_.remove_face_color);
	visual_settings_.remove_wire_color =
			read_color_setting(config, "remove_wire_color", visual_settings_.remove_wire_color);
	visual_settings_.vertex_color = read_color_setting(config, "vertex_color", visual_settings_.vertex_color);
	visual_settings_.selected_vertex_color =
			read_color_setting(config, "selected_vertex_color", visual_settings_.selected_vertex_color);
	visual_settings_.hover_vertex_color =
			read_color_setting(config, "hover_vertex_color", visual_settings_.hover_vertex_color);
	visual_settings_.remove_vertex_color =
			read_color_setting(config, "remove_vertex_color", visual_settings_.remove_vertex_color);
	visual_settings_.vertex_outline_color =
			read_color_setting(config, "vertex_outline_color", visual_settings_.vertex_outline_color);
	visual_settings_.minor_line_size =
			read_float_setting(config, "minor_line_size", visual_settings_.minor_line_size);
	visual_settings_.major_line_size =
			read_float_setting(config, "major_line_size", visual_settings_.major_line_size);
	visual_settings_.axis_line_size = read_float_setting(config, "axis_line_size", visual_settings_.axis_line_size);
	visual_settings_.source_wire_line_size =
			read_float_setting(config, "source_wire_line_size", visual_settings_.source_wire_line_size);
	visual_settings_.selection_face_wire_line_size = read_float_setting(config, "selection_face_wire_line_size",
			visual_settings_.selection_face_wire_line_size);
	visual_settings_.selection_edge_line_size =
			read_float_setting(config, "selection_edge_line_size", visual_settings_.selection_edge_line_size);
	visual_settings_.hover_wire_line_size =
			read_float_setting(config, "hover_wire_line_size", visual_settings_.hover_wire_line_size);
	visual_settings_.open_edge_line_size =
			read_float_setting(config, "open_edge_line_size", visual_settings_.open_edge_line_size);
	visual_settings_.diagnostic_edge_line_size =
			read_float_setting(config, "diagnostic_edge_line_size", visual_settings_.diagnostic_edge_line_size);
	visual_settings_.vertex_size = read_float_setting(config, "vertex_size", visual_settings_.vertex_size);
	visual_settings_.selected_vertex_growth =
			read_float_setting(config, "selected_vertex_growth", visual_settings_.selected_vertex_growth);
	visual_settings_.hover_vertex_growth =
			read_float_setting(config, "hover_vertex_growth", visual_settings_.hover_vertex_growth);
	visual_settings_.vertex_outline_size =
			read_float_setting(config, "vertex_outline_size", visual_settings_.vertex_outline_size);
	visual_settings_.pick_vertex_radius =
			read_float_setting(config, "pick_vertex_radius", visual_settings_.pick_vertex_radius);
	visual_settings_.pick_edge_radius = read_float_setting(config, "pick_edge_radius", visual_settings_.pick_edge_radius);
	grid_preset_ = std::clamp(read_int_setting(config, "grid_preset", grid_preset_), kMinGridPreset, kMaxGridPreset);
	if (settings_version < 3) {
		if (float_near(visual_settings_.vertex_size, 7.0f)) {
			visual_settings_.vertex_size = 8.0f;
		}
		if (float_near(visual_settings_.selected_vertex_growth, 0.5f)) {
			visual_settings_.selected_vertex_growth = 1.0f;
		}
		if (float_near(visual_settings_.hover_vertex_growth, 0.5f)) {
			visual_settings_.hover_vertex_growth = 1.0f;
		}
	}
}

void QuaderEditorWindow::save_persisted_settings() const {
	godot::Ref<godot::ConfigFile> config;
	config.instantiate();
	if (config.is_null()) {
		return;
	}

	config->set_value(kSettingsSection, "settings_version", kSettingsVersion);
	config->set_value(kSettingsSection, "grid_minor_color", visual_settings_.grid_minor_color);
	config->set_value(kSettingsSection, "grid_major_color", visual_settings_.grid_major_color);
	config->set_value(kSettingsSection, "grid_x_axis_color", visual_settings_.grid_x_axis_color);
	config->set_value(kSettingsSection, "grid_z_axis_color", visual_settings_.grid_z_axis_color);
	config->set_value(kSettingsSection, "mesh_grid_minor_color", visual_settings_.mesh_grid_minor_color);
	config->set_value(kSettingsSection, "mesh_grid_major_color", visual_settings_.mesh_grid_major_color);
	config->set_value(kSettingsSection, "background_color", visual_settings_.background_color);
	config->set_value(kSettingsSection, "selection_face_color", visual_settings_.selection_face_color);
	config->set_value(kSettingsSection, "selection_wire_color", visual_settings_.selection_wire_color);
	config->set_value(kSettingsSection, "source_wire_color", visual_settings_.source_wire_color);
	config->set_value(kSettingsSection, "open_edge_color", visual_settings_.open_edge_color);
	config->set_value(kSettingsSection, "diagnostic_edge_color", visual_settings_.diagnostic_edge_color);
	config->set_value(kSettingsSection, "hover_face_color", visual_settings_.hover_face_color);
	config->set_value(kSettingsSection, "hover_wire_color", visual_settings_.hover_wire_color);
	config->set_value(kSettingsSection, "remove_face_color", visual_settings_.remove_face_color);
	config->set_value(kSettingsSection, "remove_wire_color", visual_settings_.remove_wire_color);
	config->set_value(kSettingsSection, "vertex_color", visual_settings_.vertex_color);
	config->set_value(kSettingsSection, "selected_vertex_color", visual_settings_.selected_vertex_color);
	config->set_value(kSettingsSection, "hover_vertex_color", visual_settings_.hover_vertex_color);
	config->set_value(kSettingsSection, "remove_vertex_color", visual_settings_.remove_vertex_color);
	config->set_value(kSettingsSection, "vertex_outline_color", visual_settings_.vertex_outline_color);
	config->set_value(kSettingsSection, "minor_line_size", visual_settings_.minor_line_size);
	config->set_value(kSettingsSection, "major_line_size", visual_settings_.major_line_size);
	config->set_value(kSettingsSection, "axis_line_size", visual_settings_.axis_line_size);
	config->set_value(kSettingsSection, "source_wire_line_size", visual_settings_.source_wire_line_size);
	config->set_value(kSettingsSection, "selection_face_wire_line_size",
			visual_settings_.selection_face_wire_line_size);
	config->set_value(kSettingsSection, "selection_edge_line_size", visual_settings_.selection_edge_line_size);
	config->set_value(kSettingsSection, "hover_wire_line_size", visual_settings_.hover_wire_line_size);
	config->set_value(kSettingsSection, "open_edge_line_size", visual_settings_.open_edge_line_size);
	config->set_value(kSettingsSection, "diagnostic_edge_line_size", visual_settings_.diagnostic_edge_line_size);
	config->set_value(kSettingsSection, "vertex_size", visual_settings_.vertex_size);
	config->set_value(kSettingsSection, "selected_vertex_growth", visual_settings_.selected_vertex_growth);
	config->set_value(kSettingsSection, "hover_vertex_growth", visual_settings_.hover_vertex_growth);
	config->set_value(kSettingsSection, "vertex_outline_size", visual_settings_.vertex_outline_size);
	config->set_value(kSettingsSection, "pick_vertex_radius", visual_settings_.pick_vertex_radius);
	config->set_value(kSettingsSection, "pick_edge_radius", visual_settings_.pick_edge_radius);
	config->set_value(kSettingsSection, "grid_preset", grid_preset_);
	config->save(kSettingsPath);
}

void QuaderEditorWindow::on_grid_preset_changed(int preset) {
	grid_preset_ = std::clamp(preset, kMinGridPreset, kMaxGridPreset);
	if (viewport_ != nullptr) {
		visual_settings_ = viewport_->visual_settings();
	}
	save_persisted_settings();
}

} // namespace quader_godot::editor
