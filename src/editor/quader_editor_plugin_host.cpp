#include "editor/quader_editor_plugin_host.h"

#include "editor/quader_editor_window.h"
#include "render/quader_godot_render_utils.h"

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string_name.hpp>

namespace quader_godot::editor {

void QuaderEditorPluginHost::_bind_methods() {
	godot::ClassDB::bind_method(godot::D_METHOD("initialize", "plugin"), &QuaderEditorPluginHost::initialize);
	godot::ClassDB::bind_method(godot::D_METHOD("shutdown"), &QuaderEditorPluginHost::shutdown);
	godot::ClassDB::bind_method(godot::D_METHOD("open_editor_window"), &QuaderEditorPluginHost::open_editor_window);
}

void QuaderEditorPluginHost::initialize(godot::EditorPlugin *plugin) {
	if (plugin_ == plugin && toolbar_button_ != nullptr) {
		return;
	}
	plugin_ = plugin;
	create_toolbar_button();
	ensure_window();
}

void QuaderEditorPluginHost::shutdown() {
	if (toolbar_button_ != nullptr && plugin_ != nullptr) {
		plugin_->remove_control_from_container(godot::EditorPlugin::CONTAINER_TOOLBAR, toolbar_button_);
		toolbar_button_->queue_free();
		toolbar_button_ = nullptr;
	}
	if (window_ != nullptr) {
		window_->hide();
		window_->queue_free();
		window_ = nullptr;
	}
	plugin_ = nullptr;
}

void QuaderEditorPluginHost::open_editor_window() {
	ensure_window();
	if (window_ == nullptr) {
		return;
	}
	window_->popup_centered({1280, 800});
	window_->grab_focus();
	window_->focus_viewport();
}

void QuaderEditorPluginHost::create_toolbar_button() {
	if (plugin_ == nullptr || toolbar_button_ != nullptr) {
		return;
	}

	toolbar_button_ = memnew(godot::Button);
	toolbar_button_->set_name("QuaderToolbarButton");
	toolbar_button_->set_text("");
	toolbar_button_->set_flat(true);
	toolbar_button_->set_expand_icon(true);
	toolbar_button_->set_custom_minimum_size({48.0f, 48.0f});
	toolbar_button_->set_tooltip_text("Open Quader");
	toolbar_button_->set_button_icon(load_toolbar_icon());
	toolbar_button_->connect("pressed", godot::Callable(this, "open_editor_window"));

	plugin_->add_control_to_container(godot::EditorPlugin::CONTAINER_TOOLBAR, toolbar_button_);
	place_toolbar_button_before_run_bar();
}

void QuaderEditorPluginHost::place_toolbar_button_before_run_bar() {
	if (toolbar_button_ == nullptr) {
		return;
	}
	godot::Node *toolbar_parent = toolbar_button_->get_parent();
	if (toolbar_parent == nullptr) {
		return;
	}
	godot::Node *run_bar = toolbar_parent->find_child("RunBar", true, false);
	if (run_bar == nullptr) {
		return;
	}
	godot::Node *target = run_bar;
	while (target != nullptr && target->get_parent() != toolbar_parent) {
		target = target->get_parent();
	}
	if (target != nullptr && target->get_parent() == toolbar_parent) {
		toolbar_parent->move_child(toolbar_button_, target->get_index());
	}
}

void QuaderEditorPluginHost::ensure_window() {
	if (window_ != nullptr) {
		return;
	}
	window_ = memnew(QuaderEditorWindow);
	add_child(window_);
	window_->hide();
}

godot::Ref<godot::Texture2D> QuaderEditorPluginHost::load_toolbar_icon() const {
	return render::load_texture("res://addons/quader/icons/quader-logo@2x.png");
}

} // namespace quader_godot::editor
