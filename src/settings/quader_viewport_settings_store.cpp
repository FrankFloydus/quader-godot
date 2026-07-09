#include "settings/quader_viewport_settings_store.h"

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <algorithm>
#include <cmath>

namespace quader_godot::settings {
namespace {

constexpr char kSettingsPath[] = "user://quader_viewport_settings.cfg";
constexpr char kSettingsSection[] = "viewport";
constexpr int kSettingsVersion = 3;
constexpr int kMinGridPreset = 1;
constexpr int kMaxGridPreset = 10;

godot::Color read_color_setting(const godot::Ref<godot::ConfigFile> &config, const char *key,
		const godot::Color &fallback) {
	return static_cast<godot::Color>(config->get_value(kSettingsSection, key, godot::Variant(fallback)));
}

float read_float_setting(const godot::Ref<godot::ConfigFile> &config, const char *key, float fallback) {
	return static_cast<float>(
			static_cast<double>(config->get_value(kSettingsSection, key, godot::Variant(fallback))));
}

int read_int_setting(const godot::Ref<godot::ConfigFile> &config, const char *key, int fallback) {
	return static_cast<int>(static_cast<int64_t>(config->get_value(kSettingsSection, key, godot::Variant(fallback))));
}

bool float_near(float a, float b) {
	return std::abs(a - b) <= 0.0001f;
}

} // namespace

int clamp_grid_preset(int preset) {
	return std::clamp(preset, kMinGridPreset, kMaxGridPreset);
}

ViewportSettingsState load_viewport_settings(ViewportSettingsState fallback) {
	godot::Ref<godot::ConfigFile> config;
	config.instantiate();
	if (config.is_null() || config->load(kSettingsPath) != godot::OK) {
		return fallback;
	}
	const int settings_version = read_int_setting(config, "settings_version", 0);
	if (settings_version <= 0 || settings_version > kSettingsVersion) {
		return fallback;
	}

	viewport::ViewportVisualSettings &visual_settings = fallback.visual_settings;
	visual_settings.grid_minor_color =
			read_color_setting(config, "grid_minor_color", visual_settings.grid_minor_color);
	visual_settings.grid_major_color =
			read_color_setting(config, "grid_major_color", visual_settings.grid_major_color);
	visual_settings.grid_x_axis_color =
			read_color_setting(config, "grid_x_axis_color", visual_settings.grid_x_axis_color);
	visual_settings.grid_z_axis_color =
			read_color_setting(config, "grid_z_axis_color", visual_settings.grid_z_axis_color);
	visual_settings.mesh_grid_minor_color =
			read_color_setting(config, "mesh_grid_minor_color", visual_settings.mesh_grid_minor_color);
	visual_settings.mesh_grid_major_color =
			read_color_setting(config, "mesh_grid_major_color", visual_settings.mesh_grid_major_color);
	visual_settings.background_color =
			read_color_setting(config, "background_color", visual_settings.background_color);
	visual_settings.background_color.a = 1.0f;
	visual_settings.selection_face_color =
			read_color_setting(config, "selection_face_color", visual_settings.selection_face_color);
	visual_settings.selection_wire_color =
			read_color_setting(config, "selection_wire_color", visual_settings.selection_wire_color);
	visual_settings.source_wire_color =
			read_color_setting(config, "source_wire_color", visual_settings.source_wire_color);
	visual_settings.open_edge_color = read_color_setting(config, "open_edge_color", visual_settings.open_edge_color);
	visual_settings.diagnostic_edge_color =
			read_color_setting(config, "diagnostic_edge_color", visual_settings.diagnostic_edge_color);
	visual_settings.hover_face_color =
			read_color_setting(config, "hover_face_color", visual_settings.hover_face_color);
	visual_settings.hover_wire_color =
			read_color_setting(config, "hover_wire_color", visual_settings.hover_wire_color);
	visual_settings.remove_face_color =
			read_color_setting(config, "remove_face_color", visual_settings.remove_face_color);
	visual_settings.remove_wire_color =
			read_color_setting(config, "remove_wire_color", visual_settings.remove_wire_color);
	visual_settings.vertex_color = read_color_setting(config, "vertex_color", visual_settings.vertex_color);
	visual_settings.selected_vertex_color =
			read_color_setting(config, "selected_vertex_color", visual_settings.selected_vertex_color);
	visual_settings.hover_vertex_color =
			read_color_setting(config, "hover_vertex_color", visual_settings.hover_vertex_color);
	visual_settings.remove_vertex_color =
			read_color_setting(config, "remove_vertex_color", visual_settings.remove_vertex_color);
	visual_settings.vertex_outline_color =
			read_color_setting(config, "vertex_outline_color", visual_settings.vertex_outline_color);
	visual_settings.minor_line_size =
			read_float_setting(config, "minor_line_size", visual_settings.minor_line_size);
	visual_settings.major_line_size =
			read_float_setting(config, "major_line_size", visual_settings.major_line_size);
	visual_settings.axis_line_size = read_float_setting(config, "axis_line_size", visual_settings.axis_line_size);
	visual_settings.source_wire_line_size =
			read_float_setting(config, "source_wire_line_size", visual_settings.source_wire_line_size);
	visual_settings.selection_face_wire_line_size = read_float_setting(config, "selection_face_wire_line_size",
			visual_settings.selection_face_wire_line_size);
	visual_settings.selection_edge_line_size =
			read_float_setting(config, "selection_edge_line_size", visual_settings.selection_edge_line_size);
	visual_settings.hover_wire_line_size =
			read_float_setting(config, "hover_wire_line_size", visual_settings.hover_wire_line_size);
	visual_settings.open_edge_line_size =
			read_float_setting(config, "open_edge_line_size", visual_settings.open_edge_line_size);
	visual_settings.diagnostic_edge_line_size =
			read_float_setting(config, "diagnostic_edge_line_size", visual_settings.diagnostic_edge_line_size);
	visual_settings.vertex_size = read_float_setting(config, "vertex_size", visual_settings.vertex_size);
	visual_settings.selected_vertex_growth =
			read_float_setting(config, "selected_vertex_growth", visual_settings.selected_vertex_growth);
	visual_settings.hover_vertex_growth =
			read_float_setting(config, "hover_vertex_growth", visual_settings.hover_vertex_growth);
	visual_settings.vertex_outline_size =
			read_float_setting(config, "vertex_outline_size", visual_settings.vertex_outline_size);
	visual_settings.pick_vertex_radius =
			read_float_setting(config, "pick_vertex_radius", visual_settings.pick_vertex_radius);
	visual_settings.pick_edge_radius = read_float_setting(config, "pick_edge_radius", visual_settings.pick_edge_radius);
	fallback.grid_preset = clamp_grid_preset(read_int_setting(config, "grid_preset", fallback.grid_preset));
	if (settings_version < 3) {
		if (float_near(visual_settings.vertex_size, 7.0f)) {
			visual_settings.vertex_size = 8.0f;
		}
		if (float_near(visual_settings.selected_vertex_growth, 0.5f)) {
			visual_settings.selected_vertex_growth = 1.0f;
		}
		if (float_near(visual_settings.hover_vertex_growth, 0.5f)) {
			visual_settings.hover_vertex_growth = 1.0f;
		}
	}
	return fallback;
}

void save_viewport_settings(const ViewportSettingsState &state) {
	godot::Ref<godot::ConfigFile> config;
	config.instantiate();
	if (config.is_null()) {
		return;
	}

	const viewport::ViewportVisualSettings &visual_settings = state.visual_settings;
	config->set_value(kSettingsSection, "settings_version", kSettingsVersion);
	config->set_value(kSettingsSection, "grid_minor_color", visual_settings.grid_minor_color);
	config->set_value(kSettingsSection, "grid_major_color", visual_settings.grid_major_color);
	config->set_value(kSettingsSection, "grid_x_axis_color", visual_settings.grid_x_axis_color);
	config->set_value(kSettingsSection, "grid_z_axis_color", visual_settings.grid_z_axis_color);
	config->set_value(kSettingsSection, "mesh_grid_minor_color", visual_settings.mesh_grid_minor_color);
	config->set_value(kSettingsSection, "mesh_grid_major_color", visual_settings.mesh_grid_major_color);
	config->set_value(kSettingsSection, "background_color", visual_settings.background_color);
	config->set_value(kSettingsSection, "selection_face_color", visual_settings.selection_face_color);
	config->set_value(kSettingsSection, "selection_wire_color", visual_settings.selection_wire_color);
	config->set_value(kSettingsSection, "source_wire_color", visual_settings.source_wire_color);
	config->set_value(kSettingsSection, "open_edge_color", visual_settings.open_edge_color);
	config->set_value(kSettingsSection, "diagnostic_edge_color", visual_settings.diagnostic_edge_color);
	config->set_value(kSettingsSection, "hover_face_color", visual_settings.hover_face_color);
	config->set_value(kSettingsSection, "hover_wire_color", visual_settings.hover_wire_color);
	config->set_value(kSettingsSection, "remove_face_color", visual_settings.remove_face_color);
	config->set_value(kSettingsSection, "remove_wire_color", visual_settings.remove_wire_color);
	config->set_value(kSettingsSection, "vertex_color", visual_settings.vertex_color);
	config->set_value(kSettingsSection, "selected_vertex_color", visual_settings.selected_vertex_color);
	config->set_value(kSettingsSection, "hover_vertex_color", visual_settings.hover_vertex_color);
	config->set_value(kSettingsSection, "remove_vertex_color", visual_settings.remove_vertex_color);
	config->set_value(kSettingsSection, "vertex_outline_color", visual_settings.vertex_outline_color);
	config->set_value(kSettingsSection, "minor_line_size", visual_settings.minor_line_size);
	config->set_value(kSettingsSection, "major_line_size", visual_settings.major_line_size);
	config->set_value(kSettingsSection, "axis_line_size", visual_settings.axis_line_size);
	config->set_value(kSettingsSection, "source_wire_line_size", visual_settings.source_wire_line_size);
	config->set_value(kSettingsSection, "selection_face_wire_line_size",
			visual_settings.selection_face_wire_line_size);
	config->set_value(kSettingsSection, "selection_edge_line_size", visual_settings.selection_edge_line_size);
	config->set_value(kSettingsSection, "hover_wire_line_size", visual_settings.hover_wire_line_size);
	config->set_value(kSettingsSection, "open_edge_line_size", visual_settings.open_edge_line_size);
	config->set_value(kSettingsSection, "diagnostic_edge_line_size", visual_settings.diagnostic_edge_line_size);
	config->set_value(kSettingsSection, "vertex_size", visual_settings.vertex_size);
	config->set_value(kSettingsSection, "selected_vertex_growth", visual_settings.selected_vertex_growth);
	config->set_value(kSettingsSection, "hover_vertex_growth", visual_settings.hover_vertex_growth);
	config->set_value(kSettingsSection, "vertex_outline_size", visual_settings.vertex_outline_size);
	config->set_value(kSettingsSection, "pick_vertex_radius", visual_settings.pick_vertex_radius);
	config->set_value(kSettingsSection, "pick_edge_radius", visual_settings.pick_edge_radius);
	config->set_value(kSettingsSection, "grid_preset", clamp_grid_preset(state.grid_preset));
	config->save(kSettingsPath);
}

} // namespace quader_godot::settings
