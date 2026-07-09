#include "ui/components/atoms/surface.h"

#include "ui/ui_tokens.h"

#include <godot_cpp/core/memory.hpp>

namespace quader_godot::ui {
namespace {

using godot::PanelContainer;

} // namespace

Surface::Surface(const char *background_html, PanelInsets content_insets) :
		background_html_(background_html),
		content_insets_(content_insets) {
}

PanelContainer *Surface::render() const {
	auto *panel = memnew(PanelContainer);
	PanelBuilder panel_builder{panel};
	panel_builder.override(StyleOverride::Panel)
			->make_flat()
			->set_background(background_html_)
			->set_margins(content_insets_)
			->apply();
	return panel;
}

} // namespace quader_godot::ui
