#pragma once

#include "camera/editor_camera.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace quader_godot::viewport {

using quader::editor::camera::EditorCamera;

struct CameraInputResult {
	bool consumed = false;
	bool changed = false;
};

class GodotEditorCameraBridge {
public:
	void build(godot::Node3D *parent);
	void release_mouse_capture();

	[[nodiscard]] godot::Camera3D *camera() const;
	[[nodiscard]] const EditorCamera &core() const;

	[[nodiscard]] CameraInputResult handle_mouse_button(
			const godot::Ref<godot::InputEventMouseButton> &event, bool keyboard_shift_pressed);
	[[nodiscard]] CameraInputResult handle_mouse_motion(const godot::Ref<godot::InputEventMouseMotion> &event,
			float viewport_height, bool keyboard_shift_pressed);
	[[nodiscard]] bool handle_escape();
	[[nodiscard]] bool update(double delta_seconds);

private:
	void apply_pose();

	EditorCamera core_;
	godot::Camera3D *camera_ = nullptr;
};

} // namespace quader_godot::viewport
