#pragma once

#include "ui/components/atoms/base_ui_component.h"
#include "ui/panel_builder.h"

#include <godot_cpp/classes/panel_container.hpp>

namespace quader_godot::ui {

using godot::PanelContainer;

class Surface : public BaseUIComponent {
public:
	explicit Surface(const char *background_html, PanelInsets content_insets = {});

	[[nodiscard]] PanelContainer *render() const override;

private:
	const char *background_html_ = "#000000";
	PanelInsets content_insets_;
};

} // namespace quader_godot::ui
