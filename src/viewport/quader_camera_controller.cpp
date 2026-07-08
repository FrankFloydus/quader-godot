#include "viewport/quader_camera_controller.h"

#include <godot_cpp/classes/camera3d.hpp>

#include <algorithm>
#include <cmath>

namespace quader_godot::viewport {
namespace {

constexpr float kOrbitRadiansPerPixel = 0.006f;
constexpr float kFlyLookRadiansPerPixel = 0.004f;
constexpr float kFlyFastMultiplier = 4.0f;
constexpr float kFlySlowMultiplier = 0.25f;
constexpr float kZoomStepFactor = 1.18f;
constexpr float kVerticalFovRadians = 1.0471976f;
constexpr float kMinCameraDistance = 0.05f;
constexpr float kMaxCameraDistance = 500.0f;
constexpr float kMaxFlyDeltaSeconds = 0.25f;
constexpr float kPitchLimit = 1.553343f;

godot::Vector3 normalized_or(godot::Vector3 value, godot::Vector3 fallback) {
	const float length = value.length();
	if (length <= 0.000001f) {
		return fallback;
	}
	return value / length;
}

} // namespace

void QuaderCameraController::orbit(godot::Vector2 mouse_delta) {
	yaw_radians_ += -mouse_delta.x * kOrbitRadiansPerPixel;
	pitch_radians_ += -mouse_delta.y * kOrbitRadiansPerPixel;
	clamp_pitch();
}

void QuaderCameraController::pan(godot::Vector2 screen_delta, float viewport_height) {
	const float safe_height = std::max(1.0f, viewport_height);
	const float vertical_span = 2.0f * std::max(kMinCameraDistance, distance_) * std::tan(kVerticalFovRadians * 0.5f);
	const float world_per_pixel = std::max(0.0001f, vertical_span) / safe_height;
	const godot::Vector3 move =
			right() * (-screen_delta.x * world_per_pixel) + up() * (screen_delta.y * world_per_pixel);
	target_ += move;
}

void QuaderCameraController::fly_look(godot::Vector2 mouse_delta) {
	const godot::Vector3 current_eye = eye();
	yaw_radians_ += -mouse_delta.x * kFlyLookRadiansPerPixel;
	pitch_radians_ += -mouse_delta.y * kFlyLookRadiansPerPixel;
	clamp_pitch();
	target_ = current_eye + forward() * distance_;
}

void QuaderCameraController::fly_move(godot::Vector3 local_direction, double delta_seconds, bool fast, bool slow) {
	if (local_direction.length_squared() <= 0.000001f) {
		return;
	}

	float speed = fly_speed_;
	if (fast) {
		speed *= kFlyFastMultiplier;
	}
	if (slow) {
		speed *= kFlySlowMultiplier;
	}
	const float clamped_delta = std::clamp(static_cast<float>(delta_seconds), 0.0f, kMaxFlyDeltaSeconds);
	const godot::Vector3 world_direction =
			right() * local_direction.x + up() * local_direction.y + forward() * local_direction.z;
	const godot::Vector3 move = normalized_or(world_direction, {}) * speed * clamped_delta;
	target_ += move;
}

void QuaderCameraController::zoom(float wheel_steps) {
	if (std::abs(wheel_steps) <= 0.000001f) {
		return;
	}
	const float factor = std::pow(kZoomStepFactor, wheel_steps);
	distance_ = std::clamp(distance_ / factor, kMinCameraDistance, kMaxCameraDistance);
}

void QuaderCameraController::apply_to(godot::Camera3D *camera) const {
	if (camera == nullptr) {
		return;
	}
	camera->look_at_from_position(eye(), target_, up());
}

godot::Vector3 QuaderCameraController::eye() const {
	return target_ - forward() * distance_;
}

godot::Vector3 QuaderCameraController::forward() const {
	const float cp = std::cos(pitch_radians_);
	return normalized_or({std::sin(yaw_radians_) * cp, std::sin(pitch_radians_), -std::cos(yaw_radians_) * cp},
			{0.0f, 0.0f, -1.0f});
}

godot::Vector3 QuaderCameraController::right() const {
	return normalized_or(forward().cross({0.0f, 1.0f, 0.0f}), {1.0f, 0.0f, 0.0f});
}

godot::Vector3 QuaderCameraController::up() const {
	return normalized_or(right().cross(forward()), {0.0f, 1.0f, 0.0f});
}

void QuaderCameraController::clamp_pitch() {
	pitch_radians_ = std::clamp(pitch_radians_, -kPitchLimit, kPitchLimit);
}

} // namespace quader_godot::viewport
