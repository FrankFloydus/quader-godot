#pragma once

namespace godot {
class Control;
} // namespace godot

namespace quader_godot::ui {

using godot::Control;

class QuaderSidebar final {
public:
	[[nodiscard]] Control *render() const;
};

} // namespace quader_godot::ui
