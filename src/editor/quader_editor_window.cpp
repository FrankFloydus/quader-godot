#include "editor/quader_editor_window.h"

#include "settings/quader_viewport_settings_store.h"
#include "ui/components/organism/quader_bottom_bar.h"
#include "ui/quader_editor_layout.h"
#include "ui/components/organism/quader_top_bar.h"
#include "ui/quader_viewport_settings_window.h"
#include "ui/ui_tokens.h"
#include "viewport/quader_viewport_control.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

namespace quader_godot::editor {
namespace {

using godot::ClassDB;
using godot::Color;
using godot::Control;
using godot::D_METHOD;
using godot::VBoxContainer;
using settings::ViewportSettingsState;
using settings::clamp_grid_preset;
using settings::load_viewport_settings;
using settings::save_viewport_settings;
using ui::QuaderBottomBar;
using ui::QuaderTopBar;
using ui::kEditMenuSettingsId;
using ui::make_quader_editor_body;
using ui::make_quader_viewport_settings_window;
using viewport::QuaderViewportControl;

namespace ConstantOverride = ui::ConstantOverride;

} // namespace

void QuaderEditorWindow::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_settings_window"), &QuaderEditorWindow::open_settings_window);
	ClassDB::bind_method(D_METHOD("hide_settings_window"), &QuaderEditorWindow::hide_settings_window);
	ClassDB::bind_method(D_METHOD("focus_viewport"), &QuaderEditorWindow::focus_viewport);
	ClassDB::bind_method(D_METHOD("on_edit_menu_id", "id"), &QuaderEditorWindow::on_edit_menu_id);
	ClassDB::bind_method(D_METHOD("set_grid_minor_color", "color"),
			&QuaderEditorWindow::set_grid_minor_color);
	ClassDB::bind_method(D_METHOD("set_grid_major_color", "color"),
			&QuaderEditorWindow::set_grid_major_color);
	ClassDB::bind_method(D_METHOD("set_grid_x_axis_color", "color"),
			&QuaderEditorWindow::set_grid_x_axis_color);
	ClassDB::bind_method(D_METHOD("set_grid_z_axis_color", "color"),
			&QuaderEditorWindow::set_grid_z_axis_color);
	ClassDB::bind_method(D_METHOD("set_mesh_grid_minor_color", "color"),
			&QuaderEditorWindow::set_mesh_grid_minor_color);
	ClassDB::bind_method(D_METHOD("set_mesh_grid_major_color", "color"),
			&QuaderEditorWindow::set_mesh_grid_major_color);
	ClassDB::bind_method(D_METHOD("set_background_color", "color"),
			&QuaderEditorWindow::set_background_color);
	ClassDB::bind_method(D_METHOD("set_selection_face_color", "color"),
			&QuaderEditorWindow::set_selection_face_color);
	ClassDB::bind_method(D_METHOD("set_selection_wire_color", "color"),
			&QuaderEditorWindow::set_selection_wire_color);
	ClassDB::bind_method(D_METHOD("set_source_wire_color", "color"),
			&QuaderEditorWindow::set_source_wire_color);
	ClassDB::bind_method(D_METHOD("set_open_edge_color", "color"),
			&QuaderEditorWindow::set_open_edge_color);
	ClassDB::bind_method(D_METHOD("set_diagnostic_edge_color", "color"),
			&QuaderEditorWindow::set_diagnostic_edge_color);
	ClassDB::bind_method(D_METHOD("set_hover_face_color", "color"),
			&QuaderEditorWindow::set_hover_face_color);
	ClassDB::bind_method(D_METHOD("set_hover_wire_color", "color"),
			&QuaderEditorWindow::set_hover_wire_color);
	ClassDB::bind_method(D_METHOD("set_remove_face_color", "color"),
			&QuaderEditorWindow::set_remove_face_color);
	ClassDB::bind_method(D_METHOD("set_remove_wire_color", "color"),
			&QuaderEditorWindow::set_remove_wire_color);
	ClassDB::bind_method(D_METHOD("set_vertex_color", "color"),
			&QuaderEditorWindow::set_vertex_color);
	ClassDB::bind_method(D_METHOD("set_selected_vertex_color", "color"),
			&QuaderEditorWindow::set_selected_vertex_color);
	ClassDB::bind_method(D_METHOD("set_hover_vertex_color", "color"),
			&QuaderEditorWindow::set_hover_vertex_color);
	ClassDB::bind_method(D_METHOD("set_remove_vertex_color", "color"),
			&QuaderEditorWindow::set_remove_vertex_color);
	ClassDB::bind_method(D_METHOD("set_vertex_outline_color", "color"),
			&QuaderEditorWindow::set_vertex_outline_color);
	ClassDB::bind_method(D_METHOD("set_minor_line_size", "value"),
			&QuaderEditorWindow::set_minor_line_size);
	ClassDB::bind_method(D_METHOD("set_major_line_size", "value"),
			&QuaderEditorWindow::set_major_line_size);
	ClassDB::bind_method(D_METHOD("set_axis_line_size", "value"),
			&QuaderEditorWindow::set_axis_line_size);
	ClassDB::bind_method(D_METHOD("set_source_wire_line_size", "value"),
			&QuaderEditorWindow::set_source_wire_line_size);
	ClassDB::bind_method(D_METHOD("set_selection_face_wire_line_size", "value"),
			&QuaderEditorWindow::set_selection_face_wire_line_size);
	ClassDB::bind_method(D_METHOD("set_selection_edge_line_size", "value"),
			&QuaderEditorWindow::set_selection_edge_line_size);
	ClassDB::bind_method(D_METHOD("set_hover_wire_line_size", "value"),
			&QuaderEditorWindow::set_hover_wire_line_size);
	ClassDB::bind_method(D_METHOD("set_open_edge_line_size", "value"),
			&QuaderEditorWindow::set_open_edge_line_size);
	ClassDB::bind_method(D_METHOD("set_diagnostic_edge_line_size", "value"),
			&QuaderEditorWindow::set_diagnostic_edge_line_size);
	ClassDB::bind_method(D_METHOD("set_vertex_size", "value"),
			&QuaderEditorWindow::set_vertex_size);
	ClassDB::bind_method(D_METHOD("set_selected_vertex_growth", "value"),
			&QuaderEditorWindow::set_selected_vertex_growth);
	ClassDB::bind_method(D_METHOD("set_hover_vertex_growth", "value"),
			&QuaderEditorWindow::set_hover_vertex_growth);
	ClassDB::bind_method(D_METHOD("set_vertex_outline_size", "value"),
			&QuaderEditorWindow::set_vertex_outline_size);
	ClassDB::bind_method(D_METHOD("set_pick_vertex_radius", "value"),
			&QuaderEditorWindow::set_pick_vertex_radius);
	ClassDB::bind_method(D_METHOD("set_pick_edge_radius", "value"),
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

	const ViewportSettingsState persisted_settings = load_viewport_settings();
	visual_settings_ = persisted_settings.visual_settings;
	grid_preset_ = persisted_settings.grid_preset;

	set_title("Quader");
	set_size({1280, 800});
	set_min_size({640, 360});
	set_wrap_controls(true);

	auto *root = memnew(VBoxContainer);
	root->set_anchors_preset(Control::PRESET_FULL_RECT);
	root->add_theme_constant_override(ConstantOverride::Separation, 0);
	add_child(root);
	QuaderTopBar top_bar{this};
	root->add_child(top_bar.render());

	viewport_ = memnew(QuaderViewportControl);
	viewport_->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	viewport_->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	viewport_->set_visual_settings(visual_settings_);
	viewport_->set_grid_preset(grid_preset_);
	visual_settings_.grid_world_size = viewport_->visual_settings().grid_world_size;
	viewport_->set_grid_preset_changed_callback([this](int preset) { on_grid_preset_changed(preset); });
	root->add_child(make_quader_editor_body(viewport_));
	QuaderBottomBar bottom_bar;
	root->add_child(bottom_bar.render());
	ensure_settings_window();
}

void QuaderEditorWindow::ensure_settings_window() {
	if (settings_window_ != nullptr) {
		return;
	}

	settings_window_ = make_quader_viewport_settings_window(this, visual_settings_);
	add_child(settings_window_);
	settings_window_->hide();
}

void QuaderEditorWindow::open_settings_window() {
	ensure_settings_window();
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

void QuaderEditorWindow::set_grid_minor_color(Color color) {
	visual_settings_.grid_minor_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_grid_major_color(Color color) {
	visual_settings_.grid_major_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_grid_x_axis_color(Color color) {
	visual_settings_.grid_x_axis_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_grid_z_axis_color(Color color) {
	visual_settings_.grid_z_axis_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_mesh_grid_minor_color(Color color) {
	visual_settings_.mesh_grid_minor_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_mesh_grid_major_color(Color color) {
	visual_settings_.mesh_grid_major_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_background_color(Color color) {
	color.a = 1.0f;
	visual_settings_.background_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_selection_face_color(Color color) {
	visual_settings_.selection_face_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_selection_wire_color(Color color) {
	visual_settings_.selection_wire_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_source_wire_color(Color color) {
	visual_settings_.source_wire_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_open_edge_color(Color color) {
	visual_settings_.open_edge_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_diagnostic_edge_color(Color color) {
	visual_settings_.diagnostic_edge_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_hover_face_color(Color color) {
	visual_settings_.hover_face_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_hover_wire_color(Color color) {
	visual_settings_.hover_wire_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_remove_face_color(Color color) {
	visual_settings_.remove_face_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_remove_wire_color(Color color) {
	visual_settings_.remove_wire_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_vertex_color(Color color) {
	visual_settings_.vertex_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_selected_vertex_color(Color color) {
	visual_settings_.selected_vertex_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_hover_vertex_color(Color color) {
	visual_settings_.hover_vertex_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_remove_vertex_color(Color color) {
	visual_settings_.remove_vertex_color = color;
	apply_visual_settings();
}

void QuaderEditorWindow::set_vertex_outline_color(Color color) {
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
	save_settings();
}

void QuaderEditorWindow::save_settings() const {
	ViewportSettingsState state;
	state.visual_settings = visual_settings_;
	state.grid_preset = grid_preset_;
	save_viewport_settings(state);
}

void QuaderEditorWindow::on_grid_preset_changed(int preset) {
	grid_preset_ = clamp_grid_preset(preset);
	if (viewport_ != nullptr) {
		visual_settings_ = viewport_->visual_settings();
	}
	save_settings();
}

} // namespace quader_godot::editor
