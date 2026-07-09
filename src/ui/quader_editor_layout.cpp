#include "ui/quader_editor_layout.h"

#include "ui/components/organism/quader_sidebar.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_split_container.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>

namespace quader_godot::ui {
namespace {

using godot::Control;
using godot::Color;
using godot::HSplitContainer;
using godot::Ref;
using godot::SplitContainer;
using godot::String;
using godot::StyleBoxFlat;

constexpr int kSidebarSplitterWidth = 4;
constexpr char kSidebarSplitterSurfaceColor[] = "#282828";

} // namespace

Control *make_quader_editor_body(Control *viewport) {
	HSplitContainer *body = memnew(HSplitContainer);
	body->set_name("QuaderEditorBody");
	body->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	body->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	body->set_dragging_enabled(true);
	body->set_dragger_visibility(SplitContainer::DRAGGER_VISIBLE);
	body->add_theme_constant_override("separation", kSidebarSplitterWidth);
	body->add_theme_constant_override("minimum_grab_thickness", kSidebarSplitterWidth);
	body->add_theme_constant_override("autohide", 0);

	Ref<StyleBoxFlat> split_bar_style;
	split_bar_style.instantiate();
	split_bar_style->set_bg_color(Color(String(kSidebarSplitterSurfaceColor)));
	body->add_theme_stylebox_override("split_bar_background", split_bar_style);

	if (viewport != nullptr) {
		viewport->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		viewport->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		body->add_child(viewport);
	}

	QuaderSidebar sidebar;
	body->add_child(sidebar.render());
	return body;
}

} // namespace quader_godot::ui
