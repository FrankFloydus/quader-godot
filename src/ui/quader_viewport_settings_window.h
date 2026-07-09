#pragma once

#include "viewport/quader_viewport_visual_settings.h"

namespace godot {
class Object;
class Window;
} // namespace godot

namespace quader_godot::ui {

using godot::Object;
using godot::Window;
using viewport::ViewportVisualSettings;

[[nodiscard]] Window *make_quader_viewport_settings_window(
		Object *target, const ViewportVisualSettings &settings);

} // namespace quader_godot::ui
