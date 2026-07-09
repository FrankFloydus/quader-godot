#include "ui/components/organism/quader_top_bar.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/menu_button.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>

namespace quader_godot::ui {
namespace {

using godot::Callable;
using godot::Color;
using godot::Control;
using godot::HBoxContainer;
using godot::MenuButton;
using godot::Object;
using godot::PanelContainer;
using godot::PopupMenu;
using godot::Ref;
using godot::SIDE_BOTTOM;
using godot::SIDE_LEFT;
using godot::SIDE_RIGHT;
using godot::SIDE_TOP;
using godot::String;
using godot::StyleBoxEmpty;
using godot::StyleBoxFlat;

constexpr float kTopBarHeight = 24.0f;
constexpr char kTopBarSurfaceColor[] = "#191919";
constexpr char kPopupSurfaceColor[] = "#0f0f0f";
constexpr char kMenuHoverSurfaceColor[] = "#2e2e2e";
constexpr char kMenuPressedSurfaceColor[] = "#3c3c3c";
constexpr char kMenuTextColor[] = "#d8d8d8";

} // namespace

QuaderTopBar::QuaderTopBar(Object *target) :
		target_(target) {
}

Control *QuaderTopBar::render() const {
	PanelContainer *top_bar_panel = memnew(PanelContainer);
	top_bar_panel->set_name("QuaderTopBar");
	top_bar_panel->set_custom_minimum_size({0.0f, kTopBarHeight});
	top_bar_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	Ref<StyleBoxFlat> panel_style;
	panel_style.instantiate();
	panel_style->set_bg_color(Color(String(kTopBarSurfaceColor)));
	top_bar_panel->add_theme_stylebox_override("panel", panel_style);

	HBoxContainer *top_bar = memnew(HBoxContainer);
	top_bar->set_custom_minimum_size({0.0f, kTopBarHeight});
	top_bar->add_theme_constant_override("separation", 0);
	top_bar_panel->add_child(top_bar);

	MenuButton *edit_menu = memnew(MenuButton);
	edit_menu->set_name("QuaderEditMenu");
	edit_menu->set_text("Edit");
	edit_menu->set_flat(true);
	edit_menu->set_custom_minimum_size({42.0f, kTopBarHeight});
	edit_menu->add_theme_font_size_override("font_size", 14);
	edit_menu->add_theme_color_override("font_color", Color(String(kMenuTextColor)));
	edit_menu->add_theme_color_override("font_hover_color", Color(String(kMenuTextColor)));
	edit_menu->add_theme_color_override("font_pressed_color", Color(String(kMenuTextColor)));

	Ref<StyleBoxFlat> menu_normal_style;
	menu_normal_style.instantiate();
	menu_normal_style->set_bg_color(Color(String(kTopBarSurfaceColor)));
	menu_normal_style->set_content_margin(SIDE_LEFT, 6.0f);
	menu_normal_style->set_content_margin(SIDE_TOP, 2.0f);
	menu_normal_style->set_content_margin(SIDE_RIGHT, 6.0f);
	menu_normal_style->set_content_margin(SIDE_BOTTOM, 2.0f);
	edit_menu->add_theme_stylebox_override("normal", menu_normal_style);

	Ref<StyleBoxFlat> menu_hover_style;
	menu_hover_style.instantiate();
	menu_hover_style->set_bg_color(Color(String(kMenuHoverSurfaceColor)));
	menu_hover_style->set_content_margin(SIDE_LEFT, 6.0f);
	menu_hover_style->set_content_margin(SIDE_TOP, 2.0f);
	menu_hover_style->set_content_margin(SIDE_RIGHT, 6.0f);
	menu_hover_style->set_content_margin(SIDE_BOTTOM, 2.0f);
	edit_menu->add_theme_stylebox_override("hover", menu_hover_style);

	Ref<StyleBoxFlat> menu_pressed_style;
	menu_pressed_style.instantiate();
	menu_pressed_style->set_bg_color(Color(String(kMenuPressedSurfaceColor)));
	menu_pressed_style->set_content_margin(SIDE_LEFT, 6.0f);
	menu_pressed_style->set_content_margin(SIDE_TOP, 2.0f);
	menu_pressed_style->set_content_margin(SIDE_RIGHT, 6.0f);
	menu_pressed_style->set_content_margin(SIDE_BOTTOM, 2.0f);
	edit_menu->add_theme_stylebox_override("pressed", menu_pressed_style);

	Ref<StyleBoxEmpty> menu_focus_style;
	menu_focus_style.instantiate();
	edit_menu->add_theme_stylebox_override("focus", menu_focus_style);
	top_bar->add_child(edit_menu);

	PopupMenu *popup = edit_menu->get_popup();
	if (popup != nullptr) {
		popup->add_theme_font_size_override("font_size", 14);
		popup->add_theme_color_override("font_color", Color(String(kMenuTextColor)));

		Ref<StyleBoxFlat> popup_style;
		popup_style.instantiate();
		popup_style->set_bg_color(Color(String(kPopupSurfaceColor)));
		popup_style->set_content_margin(SIDE_LEFT, 4.0f);
		popup_style->set_content_margin(SIDE_TOP, 4.0f);
		popup_style->set_content_margin(SIDE_RIGHT, 4.0f);
		popup_style->set_content_margin(SIDE_BOTTOM, 4.0f);
		popup->add_theme_stylebox_override("panel", popup_style);

		popup->add_item("Settings", kEditMenuSettingsId);
		popup->connect("id_pressed", Callable(target_, "on_edit_menu_id"));
	}

	return top_bar_panel;
}

} // namespace quader_godot::ui
