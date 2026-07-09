#include "camera/editor_camera.h"

#include <gtest/gtest.h>

#include <cmath>

namespace quader::editor::camera {
namespace {

constexpr float kExpectationTolerance = 0.0001f;
constexpr float kDistanceTolerance = 0.01f;

float length(Vec3 value) {
	return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

float distance(Vec3 left, Vec3 right) {
	const Vec3 delta{left.x - right.x, left.y - right.y, left.z - right.z};
	return length(delta);
}

float dot(Vec3 left, Vec3 right) {
	return left.x * right.x + left.y * right.y + left.z * right.z;
}

TEST(EditorCameraTests, NavigationStateTransitionsAreOwnedByCore) {
	EditorCamera camera;

	EXPECT_FALSE(camera.is_navigating());

	camera.begin_orbit();
	EXPECT_TRUE(camera.is_orbiting());
	EXPECT_FALSE(camera.is_panning());
	EXPECT_FALSE(camera.is_flying());

	camera.begin_pan();
	EXPECT_FALSE(camera.is_orbiting());
	EXPECT_TRUE(camera.is_panning());
	EXPECT_FALSE(camera.is_flying());

	camera.begin_fly();
	EXPECT_FALSE(camera.is_orbiting());
	EXPECT_FALSE(camera.is_panning());
	EXPECT_TRUE(camera.is_flying());

	camera.end_navigation();
	EXPECT_FALSE(camera.is_navigating());
}

TEST(EditorCameraTests, PoseBasisStaysNormalizedAndConsistentAfterPitchClamp) {
	EditorCamera camera;
	camera.orbit({0.0f, 100000.0f});

	const EditorCameraPose pose = camera.pose();

	EXPECT_NEAR(length(pose.forward), 1.0f, kExpectationTolerance);
	EXPECT_NEAR(length(pose.right), 1.0f, kExpectationTolerance);
	EXPECT_NEAR(length(pose.up), 1.0f, kExpectationTolerance);
	EXPECT_NEAR(dot(pose.forward, pose.right), 0.0f, kExpectationTolerance);
	EXPECT_NEAR(dot(pose.forward, pose.up), 0.0f, kExpectationTolerance);
	EXPECT_LT(std::abs(pose.forward.y), 1.0f);
}

TEST(EditorCameraTests, ZoomClampsDistance) {
	EditorCamera camera;

	for (int index = 0; index < 1000; ++index) {
		camera.zoom(1.0f);
	}
	EditorCameraPose near_pose = camera.pose();
	EXPECT_GE(distance(near_pose.eye, near_pose.target), 0.05f - kDistanceTolerance);

	for (int index = 0; index < 2000; ++index) {
		camera.zoom(-1.0f);
	}
	EditorCameraPose far_pose = camera.pose();
	EXPECT_LE(distance(far_pose.eye, far_pose.target), 500.0f + kDistanceTolerance);
}

TEST(EditorCameraTests, PanScalesByViewportHeight) {
	EditorCamera small_viewport_camera;
	EditorCamera large_viewport_camera;

	const EditorCameraPose small_start = small_viewport_camera.pose();
	const EditorCameraPose large_start = large_viewport_camera.pose();

	small_viewport_camera.pan({100.0f, 0.0f}, 100.0f);
	large_viewport_camera.pan({100.0f, 0.0f}, 1000.0f);

	const float small_move = distance(small_viewport_camera.pose().target, small_start.target);
	const float large_move = distance(large_viewport_camera.pose().target, large_start.target);

	EXPECT_GT(small_move, large_move);
}

TEST(EditorCameraTests, FlyMovementHonorsFastAndSlowModifiers) {
	EditorCamera normal_camera;
	EditorCamera fast_camera;
	EditorCamera slow_camera;
	EditorCameraMoveInput normal_input;
	normal_input.forward = true;
	EditorCameraMoveInput fast_input = normal_input;
	fast_input.fast = true;
	EditorCameraMoveInput slow_input = normal_input;
	slow_input.slow = true;

	const EditorCameraPose normal_start = normal_camera.pose();
	const EditorCameraPose fast_start = fast_camera.pose();
	const EditorCameraPose slow_start = slow_camera.pose();

	normal_camera.fly_move(normal_input, 1.0);
	fast_camera.fly_move(fast_input, 1.0);
	slow_camera.fly_move(slow_input, 1.0);

	const float normal_distance = distance(normal_camera.pose().target, normal_start.target);
	const float fast_distance = distance(fast_camera.pose().target, fast_start.target);
	const float slow_distance = distance(slow_camera.pose().target, slow_start.target);

	EXPECT_GT(fast_distance, normal_distance);
	EXPECT_LT(slow_distance, normal_distance);
}

} // namespace
} // namespace quader::editor::camera
