#pragma once

#include "viewport/quader_viewport_visual_settings.h"

namespace quader_godot::settings {

struct ViewportSettingsState {
	viewport::ViewportVisualSettings visual_settings = viewport::default_viewport_visual_settings();
	int grid_preset = 6;
};

[[nodiscard]] int clamp_grid_preset(int preset);
[[nodiscard]] ViewportSettingsState load_viewport_settings(ViewportSettingsState fallback = {});
void save_viewport_settings(const ViewportSettingsState &state);

} // namespace quader_godot::settings
