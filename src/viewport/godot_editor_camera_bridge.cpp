#include "viewport/godot_editor_camera_bridge.h"

#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <algorithm>
#include <cmath>

namespace quader_godot::viewport {
namespace {

using quader::editor::camera::EditorCameraMoveInput;
using quader::editor::camera::EditorCameraPose;
using quader::editor::camera::Vec2;
using quader::editor::camera::Vec3;

constexpr float kRadiansToDegrees = 57.29577951308232f;
constexpr float kMinimumViewportHeight = 1.0f;

godot::Vector3 to_godot(Vec3 value) {
	return {value.x, value.y, value.z};
}

Vec2 to_camera_vec2(godot::Vector2 value) {
	return {value.x, value.y};
}

Vec2 to_quader_look_delta(godot::Vector2 godot_relative) {
	return {-godot_relative.x, godot_relative.y};
}

bool any_move_pressed(const EditorCameraMoveInput &input) {
	return input.forward || input.backward || input.right || input.left || input.up || input.down;
}

EditorCameraMoveInput fly_move_input() {
	EditorCameraMoveInput move;
	godot::Input *input = godot::Input::get_singleton();
	if (input == nullptr) {
		return move;
	}
	move.forward = input->is_physical_key_pressed(godot::KEY_W);
	move.backward = input->is_physical_key_pressed(godot::KEY_S);
	move.right = input->is_physical_key_pressed(godot::KEY_D);
	move.left = input->is_physical_key_pressed(godot::KEY_A);
	move.up = input->is_physical_key_pressed(godot::KEY_E);
	move.down = input->is_physical_key_pressed(godot::KEY_Q);
	move.fast = input->is_key_pressed(godot::KEY_SHIFT);
	move.slow = input->is_key_pressed(godot::KEY_CTRL);
	return move;
}

void capture_mouse() {
	godot::Input *input = godot::Input::get_singleton();
	if (input != nullptr) {
		input->set_mouse_mode(godot::Input::MOUSE_MODE_CAPTURED);
	}
}

void release_mouse_if_captured() {
	godot::Input *input = godot::Input::get_singleton();
	if (input != nullptr && input->get_mouse_mode() == godot::Input::MOUSE_MODE_CAPTURED) {
		input->set_mouse_mode(godot::Input::MOUSE_MODE_VISIBLE);
	}
}

} // namespace

void GodotEditorCameraBridge::build(godot::Node3D *parent) {
	if (camera_ == nullptr) {
		camera_ = memnew(godot::Camera3D);
		camera_->set_name(godot::String("QuaderCamera"));
		if (parent != nullptr) {
			parent->add_child(camera_);
		}
	}
	const EditorCameraPose pose = core_.pose();
	camera_->set_perspective(pose.vertical_fov_radians * kRadiansToDegrees, pose.near_plane, pose.far_plane);
	camera_->set_current(true);
	apply_pose();
}

void GodotEditorCameraBridge::release_mouse_capture() {
	core_.end_navigation();
	release_mouse_if_captured();
}

godot::Camera3D *GodotEditorCameraBridge::camera() const {
	return camera_;
}

const EditorCamera &GodotEditorCameraBridge::core() const {
	return core_;
}

CameraInputResult GodotEditorCameraBridge::handle_mouse_button(
		const godot::Ref<godot::InputEventMouseButton> &event, bool keyboard_shift_pressed) {
	if (event.is_null()) {
		return {};
	}
	const godot::MouseButton button = event->get_button_index();
	if (button == godot::MOUSE_BUTTON_MIDDLE) {
		if (event->is_pressed()) {
			if (event->is_shift_pressed() || keyboard_shift_pressed) {
				core_.begin_pan();
			} else {
				core_.begin_orbit();
			}
		} else if (core_.is_orbiting() || core_.is_panning()) {
			core_.end_navigation();
		}
		return {.consumed = true};
	}
	if (button == godot::MOUSE_BUTTON_RIGHT) {
		if (event->is_pressed()) {
			core_.begin_fly();
			capture_mouse();
		} else if (core_.is_flying()) {
			core_.end_navigation();
			release_mouse_if_captured();
		}
		return {.consumed = true};
	}
	if (button == godot::MOUSE_BUTTON_WHEEL_UP && event->is_pressed()) {
		core_.zoom(1.0f);
		apply_pose();
		return {.consumed = true, .changed = true};
	}
	if (button == godot::MOUSE_BUTTON_WHEEL_DOWN && event->is_pressed()) {
		core_.zoom(-1.0f);
		apply_pose();
		return {.consumed = true, .changed = true};
	}
	return {};
}

CameraInputResult GodotEditorCameraBridge::handle_mouse_motion(
		const godot::Ref<godot::InputEventMouseMotion> &event, float viewport_height, bool keyboard_shift_pressed) {
	if (event.is_null()) {
		return {};
	}
	const godot::Vector2 relative = event->get_relative();
	if (core_.is_panning() || (core_.is_orbiting() && (event->is_shift_pressed() || keyboard_shift_pressed))) {
		core_.begin_pan();
		core_.pan(to_camera_vec2(relative), std::max(viewport_height, kMinimumViewportHeight));
		apply_pose();
		return {.consumed = true, .changed = true};
	}
	if (core_.is_orbiting()) {
		core_.orbit(to_quader_look_delta(relative));
		apply_pose();
		return {.consumed = true, .changed = true};
	}
	if (core_.is_flying()) {
		core_.fly_look(to_quader_look_delta(relative));
		apply_pose();
		return {.consumed = true, .changed = true};
	}
	return {};
}

bool GodotEditorCameraBridge::handle_escape() {
	if (!core_.is_flying()) {
		return false;
	}
	core_.end_navigation();
	release_mouse_if_captured();
	return true;
}

bool GodotEditorCameraBridge::update(double delta_seconds) {
	if (!core_.is_flying()) {
		return false;
	}
	const EditorCameraMoveInput move = fly_move_input();
	if (!any_move_pressed(move)) {
		return false;
	}
	core_.fly_move(move, delta_seconds);
	apply_pose();
	return true;
}

void GodotEditorCameraBridge::apply_pose() {
	if (camera_ == nullptr) {
		return;
	}
	const EditorCameraPose camera_pose = core_.pose();
	camera_->look_at_from_position(to_godot(camera_pose.eye), to_godot(camera_pose.target), to_godot(camera_pose.up));
}

} // namespace quader_godot::viewport
