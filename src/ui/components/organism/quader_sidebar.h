#pragma once

#include "ui/components/atoms/base_ui_component.h"

namespace godot {
class Control;
} // namespace godot

namespace quader_godot::ui {

using godot::Control;

class QuaderSidebar final : public BaseUIComponent {
public:
	[[nodiscard]] Control *render() const override;
};

} // namespace quader_godot::ui
