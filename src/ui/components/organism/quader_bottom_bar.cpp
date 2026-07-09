#include "ui/components/organism/quader_bottom_bar.h"

#include "ui/components/atoms/surface.h"
#include "ui/ui_tokens.h"

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

namespace quader_godot::ui {
namespace {

using godot::Color;
using godot::ColorRect;
using godot::Control;
using godot::PanelContainer;
using godot::String;
using godot::VBoxContainer;

constexpr float kBottomBarHeight = 28.0f;
constexpr float kBottomBarDividerHeight = 1.0f;
constexpr float kBottomBarAccentHeight = 3.0f;
constexpr char kBottomBarSurfaceColor[] = "#121316";
constexpr char kBottomBarDividerColor[] = "#484a52";
constexpr char kBottomBarAccentColor[] = "#765ca8";

ColorRect *make_strip(const char *name, const char *color_html, float height) {
	auto *strip = memnew(ColorRect);
	strip->set_name(name);
	strip->set_color(Color(String(color_html)));
	strip->set_custom_minimum_size({0.0f, height});
	strip->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	return strip;
}

} // namespace

Control *QuaderBottomBar::render() const {
	PanelContainer *bottom_bar_panel = Surface{kBottomBarSurfaceColor}.render();
	bottom_bar_panel->set_name(UiNodeName::QuaderBottomBar);
	bottom_bar_panel->set_custom_minimum_size({0.0f, kBottomBarHeight});
	bottom_bar_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	auto *stack = memnew(VBoxContainer);
	stack->set_name(UiNodeName::QuaderBottomBarStack);
	stack->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	stack->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	stack->add_theme_constant_override(ConstantOverride::Separation, 0);
	bottom_bar_panel->add_child(stack);

	stack->add_child(make_strip(UiNodeName::QuaderBottomBarDivider, kBottomBarDividerColor, kBottomBarDividerHeight));

	PanelContainer *status_surface = Surface{kBottomBarSurfaceColor}.render();
	status_surface->set_name(UiNodeName::QuaderBottomBarStatusSurface);
	status_surface->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	status_surface->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	stack->add_child(status_surface);

	stack->add_child(make_strip(UiNodeName::QuaderBottomBarAccent, kBottomBarAccentColor, kBottomBarAccentHeight));

	return bottom_bar_panel;
}

} // namespace quader_godot::ui
