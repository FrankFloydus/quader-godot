#pragma once

#include <godot_cpp/classes/control.hpp>

namespace quader_godot::ui {

using godot::Control;

class BaseUIComponent {
public:
	virtual ~BaseUIComponent() = default;

	[[nodiscard]] virtual Control *render() const = 0;
};

} // namespace quader_godot::ui
