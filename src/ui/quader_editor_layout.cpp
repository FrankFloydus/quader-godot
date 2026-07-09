#include "ui/quader_editor_layout.h"

#include "ui/panel_builder.h"
#include "ui/components/organism/quader_sidebar.h"
#include "ui/ui_tokens.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/h_split_container.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/core/memory.hpp>

namespace quader_godot::ui {
namespace {

using godot::Control;
using godot::HSplitContainer;
using godot::SplitContainer;

constexpr int kSidebarSplitterWidth = 4;
constexpr char kSidebarSplitterSurfaceColor[] = "#282828";

void style_sidebar_splitter(HSplitContainer *splitter) {
	PanelBuilder panel_builder{splitter};
	splitter->set_name(UiNodeName::QuaderEditorBody);
	splitter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	splitter->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	splitter->set_dragging_enabled(true);
	splitter->set_dragger_visibility(SplitContainer::DRAGGER_VISIBLE);
	splitter->add_theme_constant_override(ConstantOverride::Separation, kSidebarSplitterWidth);
	splitter->add_theme_constant_override(ConstantOverride::MinimumGrabThickness, kSidebarSplitterWidth);
	splitter->add_theme_constant_override(ConstantOverride::Autohide, 0);
	panel_builder.override(StyleOverride::SplitBarBackground)
			->make_flat()
			->set_background(kSidebarSplitterSurfaceColor)
			->apply();
}

} // namespace

Control *make_quader_editor_body(Control *viewport) {
	auto *body = memnew(HSplitContainer);
	style_sidebar_splitter(body);

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
