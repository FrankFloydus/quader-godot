#pragma once

#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {
class Camera3D;
} // namespace godot

namespace quader_godot::viewport {

class QuaderCameraController {
public:
	void orbit(godot::Vector2 mouse_delta);
	void pan(godot::Vector2 screen_delta, float viewport_height);
	void fly_look(godot::Vector2 mouse_delta);
	void fly_move(godot::Vector3 local_direction, double delta_seconds, bool fast, bool slow);
	void zoom(float wheel_steps);
	void apply_to(class godot::Camera3D *camera) const;
	[[nodiscard]] godot::Vector3 eye() const;
	[[nodiscard]] godot::Vector3 forward() const;
	[[nodiscard]] godot::Vector3 right() const;
	[[nodiscard]] godot::Vector3 up() const;

private:
	void clamp_pitch();

	godot::Vector3 target_{0.0f, 0.0f, 0.0f};
	float yaw_radians_ = -0.6435011f;
	float pitch_radians_ = -0.4636476f;
	float distance_ = 11.18034f;
	float fly_speed_ = 6.0f;
};

} // namespace quader_godot::viewport
