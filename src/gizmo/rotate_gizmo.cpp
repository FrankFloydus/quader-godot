#include "gizmo/rotate_gizmo.h"

#include "gizmo/gizmo_internal.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

namespace quader_godot::gizmo {
namespace {

constexpr float kRotateSizePixels = 80.0f;
constexpr float kRotateCircleSize = 1.1f;
constexpr float kRotatePickRadiusPixels = 7.0f;
constexpr float kRotateMinimumPickRadiusPixels = 8.0f;
constexpr float kRotateMinimumScreenSizePixels = 1.0f;
constexpr float kRotateHandleMinWidthScale = 0.75f;
constexpr float kRotateHandleMaxWidthScale = 1.5f;
constexpr float kRotateTrackballLineWidthPixels = 2.25f;
constexpr float kRotateAxisLineWidthPixels = 3.0f;
constexpr float kRotateSnapRadians = std::numbers::pi_v<float> / 12.0f;
constexpr float kScreenRingViewMultiplier = 1.14f;
constexpr float kMinimumRingRadius = 0.001f;
constexpr float kMinimumRingScreenPixels = 24.0f;
constexpr float kRingFrontFacingTolerance = -0.01f;
constexpr int kRingSegments = 64;
constexpr char kTrackballColor[] = "#d2d6db8a";

struct RotateRingSegment {
	GizmoHandle handle = GizmoHandle::None;
	godot::Vector3 world_start;
	godot::Vector3 world_end;
	godot::Vector2 screen_start;
	godot::Vector2 screen_end;
	bool front_facing = true;
};

struct RotateFrame {
	bool ok = false;
	GizmoHandle hovered_handle = GizmoHandle::None;
	GizmoHandle active_handle = GizmoHandle::None;
	godot::Vector3 pivot;
	godot::Vector2 viewport_size;
	float scale = 1.0f;
	std::vector<RotateRingSegment> rings;
	std::vector<RotateRingSegment> screen_rings;
};

void add_ring(RotateFrame &frame, const godot::Camera3D *camera, GizmoHandle handle, godot::Vector3 center,
		float screen_size) {
	const float radius = std::max(kMinimumRingRadius,
			world_units_per_pixel_at(camera, frame.viewport_size, center) *
					std::max(kMinimumRingScreenPixels, screen_size));
	godot::Vector3 ring_a;
	godot::Vector3 ring_b;
	if (handle == GizmoHandle::X) {
		ring_a = {0.0f, 1.0f, 0.0f};
		ring_b = {0.0f, 0.0f, 1.0f};
	} else if (handle == GizmoHandle::Y) {
		ring_a = {1.0f, 0.0f, 0.0f};
		ring_b = {0.0f, 0.0f, 1.0f};
	} else {
		ring_a = {1.0f, 0.0f, 0.0f};
		ring_b = {0.0f, 1.0f, 0.0f};
	}
	const godot::Vector3 view_direction = (camera->get_global_transform().origin - center).normalized();
	for (int index = 0; index < kRingSegments; ++index) {
		const float start_angle = std::numbers::pi_v<float> * 2.0f * static_cast<float>(index) /
				static_cast<float>(kRingSegments);
		const float end_angle = std::numbers::pi_v<float> * 2.0f * static_cast<float>(index + 1) /
				static_cast<float>(kRingSegments);
		const float mid_angle = (start_angle + end_angle) * 0.5f;
		const godot::Vector3 start = center +
				(ring_a * std::cos(start_angle) + ring_b * std::sin(start_angle)) * radius;
		const godot::Vector3 end = center + (ring_a * std::cos(end_angle) + ring_b * std::sin(end_angle)) * radius;
		const godot::Vector3 mid = center + (ring_a * std::cos(mid_angle) + ring_b * std::sin(mid_angle)) * radius;
		godot::Vector2 screen_start;
		godot::Vector2 screen_end;
		if (!project_point(camera, start, screen_start) || !project_point(camera, end, screen_end)) {
			continue;
		}
		frame.rings.push_back({
				.handle = handle,
				.world_start = start,
				.world_end = end,
				.screen_start = screen_start,
				.screen_end = screen_end,
				.front_facing = view_direction.length_squared() <= kGizmoEpsilon ||
						(mid - center).normalized().dot(view_direction) >= kRingFrontFacingTolerance,
		});
	}
}

void add_screen_ring(RotateFrame &frame, const godot::Camera3D *camera, godot::Vector3 center, float screen_size) {
	const float radius = std::max(kMinimumRingRadius,
			world_units_per_pixel_at(camera, frame.viewport_size, center) *
					std::max(kMinimumRingScreenPixels, screen_size * kScreenRingViewMultiplier));
	godot::Vector3 right = camera_right(camera);
	godot::Vector3 up = camera_up(camera);
	up = (up - right * up.dot(right)).normalized();
	for (int index = 0; index < kRingSegments; ++index) {
		const float start_angle = std::numbers::pi_v<float> * 2.0f * static_cast<float>(index) /
				static_cast<float>(kRingSegments);
		const float end_angle = std::numbers::pi_v<float> * 2.0f * static_cast<float>(index + 1) /
				static_cast<float>(kRingSegments);
		const godot::Vector3 start = center +
				(right * std::cos(start_angle) + up * std::sin(start_angle)) * radius;
		const godot::Vector3 end = center + (right * std::cos(end_angle) + up * std::sin(end_angle)) * radius;
		godot::Vector2 screen_start;
		godot::Vector2 screen_end;
		if (!project_point(camera, start, screen_start) || !project_point(camera, end, screen_end)) {
			continue;
		}
		frame.screen_rings.push_back({
				.handle = GizmoHandle::All,
				.world_start = start,
				.world_end = end,
				.screen_start = screen_start,
				.screen_end = screen_end,
		});
	}
}

RotateFrame make_frame(const GizmoInput &input) {
	RotateFrame frame;
	GizmoFrame base_frame;
	if (!prepare_frame(input, base_frame)) {
		return frame;
	}
	frame.ok = true;
	frame.hovered_handle = base_frame.hovered_handle;
	frame.active_handle = base_frame.active_handle;
	frame.pivot = base_frame.pivot;
	frame.viewport_size = base_frame.viewport_size;
	frame.scale = base_frame.scale;
	const float unit_screen_size = std::max(kRotateMinimumScreenSizePixels, kRotateSizePixels) * frame.scale;
	const float ring_screen_size = unit_screen_size * kRotateCircleSize;
	add_screen_ring(frame, input.camera, input.pivot, ring_screen_size);
	add_ring(frame, input.camera, GizmoHandle::X, input.pivot, ring_screen_size);
	add_ring(frame, input.camera, GizmoHandle::Y, input.pivot, ring_screen_size);
	add_ring(frame, input.camera, GizmoHandle::Z, input.pivot, ring_screen_size);
	return frame;
}

GizmoPickHit pick_ring(const RotateFrame &frame, godot::Vector2 screen_position, float pick_radius) {
	GizmoPickHit hit;
	float best_distance = pick_radius * pick_radius;
	for (const RotateRingSegment &segment : frame.rings) {
		const float distance = distance_to_segment_squared(screen_position, segment.screen_start, segment.screen_end);
		if (distance < best_distance) {
			best_distance = distance;
			hit.hit = true;
			hit.handle = segment.handle;
			hit.screen_anchor = segment.screen_start;
			hit.distance = std::sqrt(distance);
		}
	}
	return hit;
}

godot::Vector3 axis_radians(GizmoHandle handle, float radians) {
	if (handle == GizmoHandle::X) {
		return {radians, 0.0f, 0.0f};
	}
	if (handle == GizmoHandle::Y) {
		return {0.0f, radians, 0.0f};
	}
	if (handle == GizmoHandle::Z) {
		return {0.0f, 0.0f, radians};
	}
	return {};
}

class RotateGizmoDragSession final : public GizmoDragSession {
public:
	explicit RotateGizmoDragSession(const GizmoDragStart &start)
			: GizmoDragSession(start.hit.handle, start.screen_position, start.pivot) {}

	[[nodiscard]] GizmoDragOperation update_drag(const GizmoDragContext &context) override;

private:
	float unsnapped_angle_ = 0.0f;
	float applied_angle_ = 0.0f;
};

} // namespace

std::string_view RotateGizmo::id() const {
	return kId;
}

GizmoPickHit RotateGizmo::pick(const GizmoInput &input, godot::Vector2 screen_position) const {
	const RotateFrame frame = make_frame(input);
	if (!frame.ok) {
		return {};
	}
	const float pick_radius = std::max(kRotateMinimumPickRadiusPixels, kRotatePickRadiusPixels * frame.scale);
	return pick_ring(frame, screen_position, pick_radius);
}

GizmoMeshes RotateGizmo::draw(const GizmoInput &input) const {
	const RotateFrame frame = make_frame(input);
	GizmoMeshBuilder builder;
	if (frame.ok) {
		const float width_scale = std::clamp(frame.scale, kRotateHandleMinWidthScale, kRotateHandleMaxWidthScale);
		for (const RotateRingSegment &segment : frame.screen_rings) {
			append_line(builder, segment.world_start, segment.world_end, html_color(kTrackballColor),
					kRotateTrackballLineWidthPixels * width_scale);
		}
		for (const RotateRingSegment &segment : frame.rings) {
			if (!segment.front_facing) {
				continue;
			}
			const bool is_highlighted = frame.active_handle == segment.handle || frame.hovered_handle == segment.handle;
			const godot::Color color =
					is_highlighted ? highlight_color_for_axis(segment.handle) : color_for_axis(segment.handle);
			append_line(builder, segment.world_start, segment.world_end, color, kRotateAxisLineWidthPixels * width_scale);
		}
	}
	return {
			.lines = make_line_mesh(builder.lines, input.camera, input.viewport_size),
			.triangles = make_triangle_mesh(builder.triangles),
	};
}

std::unique_ptr<GizmoDragSession> RotateGizmo::begin_drag(const GizmoDragStart &start) const {
	if (!has_gizmo_handle(start.hit.handle)) {
		return nullptr;
	}
	return std::make_unique<RotateGizmoDragSession>(start);
}

GizmoDragOperation RotateGizmoDragSession::update_drag(const GizmoDragContext &context) {
	GizmoDragOperation operation;
	if (context.camera == nullptr || !has_gizmo_handle(handle())) {
		return operation;
	}
	const godot::Vector2 delta = context.position - last_position();
	if (delta.length_squared() <= kGizmoScreenEpsilon) {
		return operation;
	}
	operation.dragged = true;
	const godot::Vector2 screen_pivot = context.camera->unproject_position(pivot());
	const godot::Vector2 before = last_position() - screen_pivot;
	const godot::Vector2 after = context.position - screen_pivot;
	if (before.length_squared() <= kGizmoScreenEpsilon || after.length_squared() <= kGizmoScreenEpsilon) {
		set_last_position(context.position);
		return operation;
	}
	const float cross = before.x * after.y - before.y * after.x;
	const float dot = before.dot(after);
	unsnapped_angle_ += -std::atan2(cross, dot);
	const float target_angle = context.snap_enabled ? snap_to_step(unsnapped_angle_, kRotateSnapRadians)
													: unsnapped_angle_;
	const float apply_angle = target_angle - applied_angle_;
	if (std::abs(apply_angle) > kGizmoEpsilon) {
		if (context.apply_mutation) {
			const GizmoMutationResult result = context.apply_mutation({
					.kind = GizmoMutationKind::RotateSelection,
					.value = axis_radians(handle(), apply_angle),
					.pivot = pivot(),
			});
			operation.changed = result.success && result.changed;
			if (operation.changed) {
				applied_angle_ = target_angle;
			}
		}
	}
	set_last_position(context.position);
	return operation;
}

const RotateGizmo &rotate_gizmo() {
	static const RotateGizmo gizmo;
	return gizmo;
}

} // namespace quader_godot::gizmo
