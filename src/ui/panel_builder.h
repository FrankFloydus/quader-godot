#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/style_box.hpp>
#include <godot_cpp/classes/style_box_empty.hpp>
#include <godot_cpp/classes/style_box_flat.hpp>
#include <godot_cpp/classes/window.hpp>

namespace quader_godot::ui {

using godot::Control;
using godot::Ref;
using godot::StyleBox;
using godot::Window;

struct PanelInsets {
	float left = 0.0f;
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;

	[[nodiscard]] static constexpr PanelInsets all(float value) {
		return {value, value, value, value};
	}

	[[nodiscard]] static constexpr PanelInsets symmetric(float horizontal, float vertical) {
		return {horizontal, vertical, horizontal, vertical};
	}
};

class PanelBuilder {
public:
	PanelBuilder() = default;
	explicit PanelBuilder(Control *target);
	explicit PanelBuilder(Window *target);

	PanelBuilder *target(Control *target);
	PanelBuilder *target(Window *target);
	PanelBuilder *override(const char *style_override);
	PanelBuilder *make_empty();
	PanelBuilder *make_flat();
	PanelBuilder *set_background(const char *background_html);
	PanelBuilder *set_margins(PanelInsets insets);
	PanelBuilder *set_margins(float all);
	PanelBuilder *set_margins(float horizontal, float vertical);
	PanelBuilder *set_border_width(int width);
	PanelBuilder *set_corner_radius(int radius);
	PanelBuilder *apply();

	[[nodiscard]] Ref<StyleBox> build() const;

private:
	enum class Kind {
		Empty,
		Flat,
	};

	Control *control_target_ = nullptr;
	Window *window_target_ = nullptr;
	const char *style_override_ = nullptr;
	Kind kind_ = Kind::Empty;
	const char *background_html_ = "#000000";
	PanelInsets margins_;
	int border_width_ = 0;
	int corner_radius_ = 0;
};

} // namespace quader_godot::ui
