#pragma once

namespace quader::editor::camera {

// Coordinate convention: Y-up world, radians for angles, vertical FOV.
// Engine adapters convert this pose into engine-specific camera transforms.
struct Vec2 {
	float x = 0.0f;
	float y = 0.0f;
};

struct Vec3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct EditorCameraPose {
	Vec3 eye;
	Vec3 target;
	Vec3 forward;
	Vec3 right;
	Vec3 up;
	float vertical_fov_radians = 1.0471976f;
	float near_plane = 0.05f;
	float far_plane = 1000.0f;
};

struct EditorCameraMoveInput {
	bool forward = false;
	bool backward = false;
	bool right = false;
	bool left = false;
	bool up = false;
	bool down = false;
	bool fast = false;
	bool slow = false;
};

} // namespace quader::editor::camera
