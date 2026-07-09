#include "ui/components/organism/quader_bottom_bar.h"

#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>

namespace quader_godot::ui {
namespace {

using godot::Color;
using godot::ColorRect;
using godot::Control;
using godot::PanelContainer;
using godot::Ref;
using godot::String;
using godot::StyleBoxFlat;
using godot::VBoxContainer;

constexpr float kBottomBarHeight = 28.0f;
constexpr float kBottomBarDividerHeight = 1.0f;
constexpr float kBottomBarAccentHeight = 3.0f;
constexpr char kBottomBarSurfaceColor[] = "#121316";
constexpr char kBottomBarDividerColor[] = "#484a52";
constexpr char kBottomBarAccentColor[] = "#765ca8";

} // namespace

Control *QuaderBottomBar::render() const {
	PanelContainer *bottom_bar_panel = memnew(PanelContainer);
	bottom_bar_panel->set_name("QuaderBottomBar");
	bottom_bar_panel->set_custom_minimum_size({0.0f, kBottomBarHeight});
	bottom_bar_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	Ref<StyleBoxFlat> bottom_bar_style;
	bottom_bar_style.instantiate();
	bottom_bar_style->set_bg_color(Color(String(kBottomBarSurfaceColor)));
	bottom_bar_panel->add_theme_stylebox_override("panel", bottom_bar_style);

	VBoxContainer *stack = memnew(VBoxContainer);
	stack->set_name("QuaderBottomBarStack");
	stack->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	stack->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	stack->add_theme_constant_override("separation", 0);
	bottom_bar_panel->add_child(stack);

	ColorRect *divider = memnew(ColorRect);
	divider->set_name("QuaderBottomBarDivider");
	divider->set_color(Color(String(kBottomBarDividerColor)));
	divider->set_custom_minimum_size({0.0f, kBottomBarDividerHeight});
	divider->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	stack->add_child(divider);

	PanelContainer *status_surface = memnew(PanelContainer);
	status_surface->set_name("QuaderBottomBarStatusSurface");
	status_surface->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	status_surface->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	Ref<StyleBoxFlat> status_style;
	status_style.instantiate();
	status_style->set_bg_color(Color(String(kBottomBarSurfaceColor)));
	status_surface->add_theme_stylebox_override("panel", status_style);
	stack->add_child(status_surface);

	ColorRect *accent = memnew(ColorRect);
	accent->set_name("QuaderBottomBarAccent");
	accent->set_color(Color(String(kBottomBarAccentColor)));
	accent->set_custom_minimum_size({0.0f, kBottomBarAccentHeight});
	accent->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	stack->add_child(accent);

	return bottom_bar_panel;
}

} // namespace quader_godot::ui
