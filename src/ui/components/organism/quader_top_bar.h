#pragma once

#include <cstdint>

namespace godot {
class Control;
class Object;
} // namespace godot

namespace quader_godot::ui {

inline constexpr int32_t kEditMenuSettingsId = 1;

using godot::Control;
using godot::Object;

class QuaderTopBar final {
public:
	explicit QuaderTopBar(Object *target);

	[[nodiscard]] Control *render() const;

private:
	Object *target_ = nullptr;
};

} // namespace quader_godot::ui
