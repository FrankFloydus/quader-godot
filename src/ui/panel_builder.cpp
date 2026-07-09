#include "ui/panel_builder.h"

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/string.hpp>

namespace quader_godot::ui {
namespace {

using godot::Color;
using godot::Control;
using godot::Ref;
using godot::SIDE_BOTTOM;
using godot::SIDE_LEFT;
using godot::SIDE_RIGHT;
using godot::SIDE_TOP;
using godot::String;
using godot::StyleBox;
using godot::StyleBoxEmpty;
using godot::StyleBoxFlat;
using godot::Window;

} // namespace

PanelBuilder::PanelBuilder(Control *target) :
		control_target_(target) {
}

PanelBuilder::PanelBuilder(Window *target) :
		window_target_(target) {
}

PanelBuilder *PanelBuilder::target(Control *target) {
	control_target_ = target;
	window_target_ = nullptr;
	return this;
}

PanelBuilder *PanelBuilder::target(Window *target) {
	window_target_ = target;
	control_target_ = nullptr;
	return this;
}

PanelBuilder *PanelBuilder::override(const char *style_override) {
	style_override_ = style_override;
	return this;
}

PanelBuilder *PanelBuilder::make_empty() {
	kind_ = Kind::Empty;
	background_html_ = "#000000";
	margins_ = {};
	border_width_ = 0;
	corner_radius_ = 0;
	return this;
}

PanelBuilder *PanelBuilder::make_flat() {
	kind_ = Kind::Flat;
	background_html_ = "#000000";
	margins_ = {};
	border_width_ = 0;
	corner_radius_ = 0;
	return this;
}

PanelBuilder *PanelBuilder::set_background(const char *background_html) {
	background_html_ = background_html;
	return this;
}

PanelBuilder *PanelBuilder::set_margins(PanelInsets insets) {
	margins_ = insets;
	return this;
}

PanelBuilder *PanelBuilder::set_margins(float all) {
	margins_ = PanelInsets::all(all);
	return this;
}

PanelBuilder *PanelBuilder::set_margins(float horizontal, float vertical) {
	margins_ = PanelInsets::symmetric(horizontal, vertical);
	return this;
}

PanelBuilder *PanelBuilder::set_border_width(int width) {
	border_width_ = width;
	return this;
}

PanelBuilder *PanelBuilder::set_corner_radius(int radius) {
	corner_radius_ = radius;
	return this;
}

PanelBuilder *PanelBuilder::apply() {
	if (style_override_ == nullptr) {
		return this;
	}
	if (control_target_ != nullptr) {
		control_target_->add_theme_stylebox_override(style_override_, build());
	} else if (window_target_ != nullptr) {
		window_target_->add_theme_stylebox_override(style_override_, build());
	}
	return this;
}

Ref<StyleBox> PanelBuilder::build() const {
	if (kind_ == Kind::Empty) {
		Ref<StyleBoxEmpty> style;
		style.instantiate();
		return style;
	}

	Ref<StyleBoxFlat> style;
	style.instantiate();
	style->set_bg_color(Color(String(background_html_)));
	style->set_border_width_all(border_width_);
	style->set_corner_radius_all(corner_radius_);
	style->set_content_margin(SIDE_LEFT, margins_.left);
	style->set_content_margin(SIDE_TOP, margins_.top);
	style->set_content_margin(SIDE_RIGHT, margins_.right);
	style->set_content_margin(SIDE_BOTTOM, margins_.bottom);
	return style;
}

} // namespace quader_godot::ui
