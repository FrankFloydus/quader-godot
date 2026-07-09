#pragma once

#include "camera/editor_camera_types.h"

namespace quader::editor::camera {

class EditorCamera {
public:
	void begin_orbit();
	void begin_pan();
	void begin_fly();
	void end_navigation();

	void orbit(Vec2 mouse_delta);
	void pan(Vec2 screen_delta, float viewport_height);
	void fly_look(Vec2 mouse_delta);
	void fly_move(const EditorCameraMoveInput &input, double delta_seconds);
	void zoom(float wheel_steps);

	[[nodiscard]] bool is_orbiting() const;
	[[nodiscard]] bool is_panning() const;
	[[nodiscard]] bool is_flying() const;
	[[nodiscard]] bool is_navigating() const;
	[[nodiscard]] EditorCameraPose pose() const;

private:
	void clamp_pitch();
	[[nodiscard]] Vec3 eye() const;
	[[nodiscard]] Vec3 forward() const;
	[[nodiscard]] Vec3 right() const;
	[[nodiscard]] Vec3 up() const;

	Vec3 target_{0.0f, 0.0f, 0.0f};
	float yaw_radians_ = -0.6435011f;
	float pitch_radians_ = -0.4636476f;
	float distance_ = 11.18034f;
	float fly_speed_ = 6.0f;
	bool orbiting_ = false;
	bool panning_ = false;
	bool flying_ = false;
};

} // namespace quader::editor::camera
