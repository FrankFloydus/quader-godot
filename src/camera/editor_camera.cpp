#include "camera/editor_camera.h"

#include <algorithm>
#include <cmath>

namespace quader::editor::camera {
namespace {

constexpr float kOrbitRadiansPerPixel = 0.006f;
constexpr float kFlyLookRadiansPerPixel = 0.004f;
constexpr float kFlyFastMultiplier = 4.0f;
constexpr float kFlySlowMultiplier = 0.25f;
constexpr float kZoomStepFactor = 1.18f;
constexpr float kVerticalFovRadians = 1.0471976f;
constexpr float kNearPlane = 0.05f;
constexpr float kFarPlane = 1000.0f;
constexpr float kMinCameraDistance = 0.05f;
constexpr float kMaxCameraDistance = 500.0f;
constexpr float kMaxFlyDeltaSeconds = 0.25f;
constexpr float kPitchLimit = 1.553343f;
constexpr float kVectorEpsilon = 0.000001f;
constexpr float kMinimumViewportHeight = 1.0f;
constexpr float kMinimumWorldPerPixelSpan = 0.0001f;

Vec3 add(Vec3 left, Vec3 right) {
	return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 subtract(Vec3 left, Vec3 right) {
	return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3 multiply(Vec3 value, float scalar) {
	return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float length_squared(Vec3 value) {
	return value.x * value.x + value.y * value.y + value.z * value.z;
}

float length(Vec3 value) {
	return std::sqrt(length_squared(value));
}

Vec3 cross(Vec3 left, Vec3 right) {
	return {
			left.y * right.z - left.z * right.y,
			left.z * right.x - left.x * right.z,
			left.x * right.y - left.y * right.x,
	};
}

Vec3 normalized_or(Vec3 value, Vec3 fallback) {
	const float value_length = length(value);
	if (value_length <= kVectorEpsilon) {
		return fallback;
	}
	return multiply(value, 1.0f / value_length);
}

} // namespace

void EditorCamera::begin_orbit() {
	orbiting_ = true;
	panning_ = false;
}

void EditorCamera::begin_pan() {
	panning_ = true;
	orbiting_ = false;
}

void EditorCamera::begin_fly() {
	flying_ = true;
	orbiting_ = false;
	panning_ = false;
}

void EditorCamera::end_navigation() {
	orbiting_ = false;
	panning_ = false;
	flying_ = false;
}

void EditorCamera::orbit(Vec2 mouse_delta) {
	yaw_radians_ += -mouse_delta.x * kOrbitRadiansPerPixel;
	pitch_radians_ += -mouse_delta.y * kOrbitRadiansPerPixel;
	clamp_pitch();
}

void EditorCamera::pan(Vec2 screen_delta, float viewport_height) {
	const float safe_height = std::max(kMinimumViewportHeight, viewport_height);
	const float vertical_span =
			2.0f * std::max(kMinCameraDistance, distance_) * std::tan(kVerticalFovRadians * 0.5f);
	const float world_per_pixel = std::max(kMinimumWorldPerPixelSpan, vertical_span) / safe_height;
	const Vec3 move = add(multiply(right(), -screen_delta.x * world_per_pixel),
			multiply(up(), screen_delta.y * world_per_pixel));
	target_ = add(target_, move);
}

void EditorCamera::fly_look(Vec2 mouse_delta) {
	const Vec3 current_eye = eye();
	yaw_radians_ += -mouse_delta.x * kFlyLookRadiansPerPixel;
	pitch_radians_ += -mouse_delta.y * kFlyLookRadiansPerPixel;
	clamp_pitch();
	target_ = add(current_eye, multiply(forward(), distance_));
}

void EditorCamera::fly_move(const EditorCameraMoveInput &input, double delta_seconds) {
	Vec3 local_direction;
	if (input.forward) {
		local_direction.z += 1.0f;
	}
	if (input.backward) {
		local_direction.z -= 1.0f;
	}
	if (input.right) {
		local_direction.x += 1.0f;
	}
	if (input.left) {
		local_direction.x -= 1.0f;
	}
	if (input.up) {
		local_direction.y += 1.0f;
	}
	if (input.down) {
		local_direction.y -= 1.0f;
	}
	if (length_squared(local_direction) <= kVectorEpsilon) {
		return;
	}

	float speed = fly_speed_;
	if (input.fast) {
		speed *= kFlyFastMultiplier;
	}
	if (input.slow) {
		speed *= kFlySlowMultiplier;
	}
	const float clamped_delta = std::clamp(static_cast<float>(delta_seconds), 0.0f, kMaxFlyDeltaSeconds);
	const Vec3 world_direction = add(add(multiply(right(), local_direction.x), multiply(up(), local_direction.y)),
			multiply(forward(), local_direction.z));
	const Vec3 move = multiply(normalized_or(world_direction, {}), speed * clamped_delta);
	target_ = add(target_, move);
}

void EditorCamera::zoom(float wheel_steps) {
	if (std::abs(wheel_steps) <= kVectorEpsilon) {
		return;
	}
	const float factor = std::pow(kZoomStepFactor, wheel_steps);
	distance_ = std::clamp(distance_ / factor, kMinCameraDistance, kMaxCameraDistance);
}

bool EditorCamera::is_orbiting() const {
	return orbiting_;
}

bool EditorCamera::is_panning() const {
	return panning_;
}

bool EditorCamera::is_flying() const {
	return flying_;
}

bool EditorCamera::is_navigating() const {
	return orbiting_ || panning_ || flying_;
}

EditorCameraPose EditorCamera::pose() const {
	return {
			.eye = eye(),
			.target = target_,
			.forward = forward(),
			.right = right(),
			.up = up(),
			.vertical_fov_radians = kVerticalFovRadians,
			.near_plane = kNearPlane,
			.far_plane = kFarPlane,
	};
}

void EditorCamera::clamp_pitch() {
	pitch_radians_ = std::clamp(pitch_radians_, -kPitchLimit, kPitchLimit);
}

Vec3 EditorCamera::eye() const {
	return subtract(target_, multiply(forward(), distance_));
}

Vec3 EditorCamera::forward() const {
	const float cos_pitch = std::cos(pitch_radians_);
	return normalized_or({
								 std::sin(yaw_radians_) * cos_pitch,
								 std::sin(pitch_radians_),
								 -std::cos(yaw_radians_) * cos_pitch,
						 },
			{0.0f, 0.0f, -1.0f});
}

Vec3 EditorCamera::right() const {
	return normalized_or(cross(forward(), {0.0f, 1.0f, 0.0f}), {1.0f, 0.0f, 0.0f});
}

Vec3 EditorCamera::up() const {
	return normalized_or(cross(right(), forward()), {0.0f, 1.0f, 0.0f});
}

} // namespace quader::editor::camera
