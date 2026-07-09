#include "ui/components/organism/quader_sidebar.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>

namespace quader_godot::ui {
namespace {

using godot::Color;
using godot::Control;
using godot::PanelContainer;
using godot::Ref;
using godot::SIDE_BOTTOM;
using godot::SIDE_LEFT;
using godot::SIDE_RIGHT;
using godot::SIDE_TOP;
using godot::String;
using godot::StyleBoxFlat;

constexpr float kSidebarMinimumWidth = 340.0f;
constexpr char kSidebarSurfaceColor[] = "#0f0f0f";

} // namespace

Control *QuaderSidebar::render() const {
	PanelContainer *sidebar = memnew(PanelContainer);
	sidebar->set_name("QuaderSidebar");
	sidebar->set_custom_minimum_size({kSidebarMinimumWidth, 0.0f});
	sidebar->set_h_size_flags(Control::SIZE_FILL);
	sidebar->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	Ref<StyleBoxFlat> style;
	style.instantiate();
	style->set_bg_color(Color(String(kSidebarSurfaceColor)));
	style->set_content_margin(SIDE_LEFT, 8.0f);
	style->set_content_margin(SIDE_TOP, 8.0f);
	style->set_content_margin(SIDE_RIGHT, 8.0f);
	style->set_content_margin(SIDE_BOTTOM, 8.0f);
	sidebar->add_theme_stylebox_override("panel", style);

	return sidebar;
}

} // namespace quader_godot::ui
