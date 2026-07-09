#include "ui/components/organism/quader_sidebar.h"

#include "ui/components/atoms/surface.h"
#include "ui/panel_builder.h"
#include "ui/ui_tokens.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel_container.hpp>

namespace quader_godot::ui {
namespace {

using godot::Control;
using godot::PanelContainer;

constexpr float kSidebarMinimumWidth = 340.0f;
constexpr char kSidebarSurfaceColor[] = "#0f0f0f";

} // namespace

Control *QuaderSidebar::render() const {
	PanelContainer *sidebar = Surface{kSidebarSurfaceColor, PanelInsets::all(8.0f)}.render();
	sidebar->set_name(UiNodeName::QuaderSidebar);
	sidebar->set_custom_minimum_size({kSidebarMinimumWidth, 0.0f});
	sidebar->set_h_size_flags(Control::SIZE_FILL);
	sidebar->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	return sidebar;
}

} // namespace quader_godot::ui
