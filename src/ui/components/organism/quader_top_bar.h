#pragma once

#include "ui/components/atoms/base_ui_component.h"

#include <cstdint>

namespace godot {
class Control;
class Object;
} // namespace godot

namespace quader_godot::ui {

inline constexpr int32_t kEditMenuSettingsId = 1;

using godot::Control;
using godot::Object;

class QuaderTopBar final : public BaseUIComponent {
public:
	explicit QuaderTopBar(Object *target);

	[[nodiscard]] Control *render() const override;

private:
	Object *target_ = nullptr;
};

} // namespace quader_godot::ui
