#include "ui/components/organism/quader_top_bar.h"

#include "ui/components/atoms/surface.h"
#include "ui/panel_builder.h"
#include "ui/ui_tokens.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/menu_button.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
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
using godot::String;

constexpr float kTopBarHeight = 24.0f;
constexpr char kTopBarSurfaceColor[] = "#191919";
constexpr char kPopupSurfaceColor[] = "#0f0f0f";
constexpr char kMenuHoverSurfaceColor[] = "#2e2e2e";
constexpr char kMenuPressedSurfaceColor[] = "#3c3c3c";
constexpr char kMenuTextColor[] = "#d8d8d8";

void apply_edit_menu_style(MenuButton *edit_menu) {
	PanelBuilder panel_builder{edit_menu};
	edit_menu->add_theme_font_size_override(FontSizeOverride::FontSize, 14);
	edit_menu->add_theme_color_override(ColorOverride::FontColor, Color(String(kMenuTextColor)));
	edit_menu->add_theme_color_override(ColorOverride::FontHoverColor, Color(String(kMenuTextColor)));
	edit_menu->add_theme_color_override(ColorOverride::FontPressedColor, Color(String(kMenuTextColor)));
	panel_builder.override(StyleOverride::Normal)
			->make_flat()
			->set_background(kTopBarSurfaceColor)
			->set_margins(6.0f, 2.0f)
			->apply();
	panel_builder.override(StyleOverride::Hover)
			->make_flat()
			->set_background(kMenuHoverSurfaceColor)
			->set_margins(6.0f, 2.0f)
			->apply();
	panel_builder.override(StyleOverride::Pressed)
			->make_flat()
			->set_background(kMenuPressedSurfaceColor)
			->set_margins(6.0f, 2.0f)
			->apply();
	panel_builder.override(StyleOverride::Focus)
			->make_empty()
			->apply();
}

void configure_edit_menu_popup(PopupMenu *popup, Object *target) {
	if (popup == nullptr) {
		return;
	}
	PanelBuilder panel_builder{popup};
	popup->add_theme_font_size_override(FontSizeOverride::FontSize, 14);
	popup->add_theme_color_override(ColorOverride::FontColor, Color(String(kMenuTextColor)));
	panel_builder.override(StyleOverride::Panel)
			->make_flat()
			->set_background(kPopupSurfaceColor)
			->set_margins(4.0f)
			->apply();
	popup->add_item("Settings", kEditMenuSettingsId);
	popup->connect(SignalName::IdPressed, Callable(target, "on_edit_menu_id"));
}

} // namespace

QuaderTopBar::QuaderTopBar(Object *target) :
		target_(target) {
}

Control *QuaderTopBar::render() const {
	PanelContainer *top_bar_panel = Surface{kTopBarSurfaceColor}.render();
	top_bar_panel->set_name(UiNodeName::QuaderTopBar);
	top_bar_panel->set_custom_minimum_size({0.0f, kTopBarHeight});
	top_bar_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	auto *top_bar = memnew(HBoxContainer);
	top_bar->set_custom_minimum_size({0.0f, kTopBarHeight});
	top_bar->add_theme_constant_override(ConstantOverride::Separation, 0);
	top_bar_panel->add_child(top_bar);

	auto *edit_menu = memnew(MenuButton);
	edit_menu->set_name(UiNodeName::QuaderEditMenu);
	edit_menu->set_text("Edit");
	edit_menu->set_flat(true);
	edit_menu->set_custom_minimum_size({42.0f, kTopBarHeight});
	apply_edit_menu_style(edit_menu);
	top_bar->add_child(edit_menu);

	configure_edit_menu_popup(edit_menu->get_popup(), target_);

	return top_bar_panel;
}

} // namespace quader_godot::ui
