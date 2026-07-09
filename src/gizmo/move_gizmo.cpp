#include "gizmo/move_gizmo.h"

#include "gizmo/gizmo_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace quader_godot::gizmo {
namespace {

constexpr float kMoveSizePixels = 80.0f;
constexpr float kMovePlaneSize = 0.2f;
constexpr float kMovePlaneDistance = 0.3f;
constexpr float kMoveArrowSize = 0.35f;
constexpr float kMoveArrowOffset = 1.4f;
constexpr float kMoveArrowTip = kMoveArrowOffset + kMoveArrowSize;
constexpr float kMoveArrowConeRadius = 0.065f;
constexpr float kMoveAxisLineWidthPixels = 1.6f;
constexpr float kMovePickRadiusPixels = 7.0f;
constexpr float kMoveMinimumPickRadiusPixels = 8.0f;
constexpr float kMoveMinimumScreenSizePixels = 1.0f;
constexpr float kMoveFullCircleRadians = 6.28318530717958647692f;
constexpr float kMoveHandleMinWidthScale = 0.75f;
constexpr float kMoveHandleMaxWidthScale = 1.5f;
constexpr float kMoveHalfFactor = 0.5f;
constexpr float kMoveQuadCentroidFactor = 0.25f;
constexpr float kMoveColorFullIntensity = 1.0f;

GizmoFrame make_frame(const GizmoInput &input) {
	GizmoFrame frame;
	if (!prepare_frame(input, frame)) {
		return frame;
	}
	const float unit_screen_size = std::max(kMoveMinimumScreenSizePixels, kMoveSizePixels) * frame.scale;
	const float pixel_world = world_units_per_pixel_at(input.camera, input.viewport_size, input.pivot);
	const float axis_length = pixel_world * unit_screen_size * kMoveArrowTip;
	const float plane_offset = pixel_world * unit_screen_size * (kMovePlaneDistance + kMovePlaneSize * kMoveHalfFactor);
	const float plane_half_size = pixel_world * unit_screen_size * (kMovePlaneSize * kMoveHalfFactor);
	add_axis(frame, input.camera, GizmoHandle::X, input.pivot, axis_length);
	add_axis(frame, input.camera, GizmoHandle::Y, input.pivot, axis_length);
	add_axis(frame, input.camera, GizmoHandle::Z, input.pivot, axis_length);
	add_plane(frame, input.camera, GizmoHandle::XY, input.pivot, axis_vector(GizmoHandle::X), axis_vector(GizmoHandle::Y),
			plane_offset, plane_half_size);
	add_plane(frame, input.camera, GizmoHandle::XZ, input.pivot, axis_vector(GizmoHandle::X), axis_vector(GizmoHandle::Z),
			plane_offset, plane_half_size);
	add_plane(frame, input.camera, GizmoHandle::YZ, input.pivot, axis_vector(GizmoHandle::Y), axis_vector(GizmoHandle::Z),
			plane_offset, plane_half_size);
	return frame;
}

void append_cone_handle(GizmoMeshBuilder &builder, godot::Vector3 base_center, godot::Vector3 tip,
		godot::Color color, float radius_world) {
	constexpr int kConeSegments = 16;
	godot::Vector3 axis = tip - base_center;
	const float cone_length = axis.length();
	if (cone_length <= kGizmoEpsilon || radius_world <= kGizmoEpsilon) {
		return;
	}
	axis /= cone_length;
	godot::Vector3 u = fallback_perpendicular(axis);
	godot::Vector3 v = axis.cross(u);
	if (v.length_squared() <= kGizmoEpsilon) {
		v = fallback_perpendicular(u);
	} else {
		v.normalize();
	}
	u = v.cross(axis).normalized();
	std::array<godot::Vector3, kConeSegments> base_points{};
	for (int index = 0; index < kConeSegments; ++index) {
		const float angle = kMoveFullCircleRadians * static_cast<float>(index) / static_cast<float>(kConeSegments);
		base_points[static_cast<std::size_t>(index)] =
				base_center + (u * std::cos(angle) + v * std::sin(angle)) * radius_world;
	}
	for (int index = 0; index < kConeSegments; ++index) {
		const int next = (index + 1) % kConeSegments;
		const godot::Vector3 a = base_points[static_cast<std::size_t>(index)];
		const godot::Vector3 b = base_points[static_cast<std::size_t>(next)];
		append_triangle(builder, tip, a, b, color);
		append_triangle(builder, base_center, b, a, color);
	}
}

void append_arrow_handle(GizmoMeshBuilder &builder, godot::Vector3 start, godot::Vector3 tip, godot::Color color,
		float line_width_pixels) {
	godot::Vector3 direction = tip - start;
	const float axis_length = direction.length();
	if (axis_length <= kGizmoEpsilon) {
		return;
	}
	direction /= axis_length;
	const float unit_world = axis_length / kMoveArrowTip;
	const godot::Vector3 cone_base = start + direction * (unit_world * kMoveArrowOffset);
	append_line(builder, start, cone_base, color, line_width_pixels);
	append_cone_handle(builder, cone_base, tip, color, unit_world * kMoveArrowConeRadius);
}

float snap_world_value(float value, float grid_size) {
	const float grid = safe_grid_size(grid_size);
	return std::round(value / grid) * grid;
}

godot::Vector3 snap_center_drag_delta(GizmoHandle handle, godot::Vector3 center,
		godot::Vector3 raw_delta, float grid_size) {
	godot::Vector3 delta;
	if (handle_includes_component(handle, GizmoComponent::X)) {
		delta.x = snap_world_value(center.x + raw_delta.x, grid_size) - center.x;
	}
	if (handle_includes_component(handle, GizmoComponent::Y)) {
		delta.y = snap_world_value(center.y + raw_delta.y, grid_size) - center.y;
	}
	if (handle_includes_component(handle, GizmoComponent::Z)) {
		delta.z = snap_world_value(center.z + raw_delta.z, grid_size) - center.z;
	}
	return delta;
}

class MoveGizmoDragSession final : public GizmoDragSession {
public:
	explicit MoveGizmoDragSession(const GizmoDragStart &start)
			: GizmoDragSession(start.hit.handle, start.screen_position, start.pivot) {}

	[[nodiscard]] GizmoDragOperation update_drag(const GizmoDragContext &context) override;

private:
	godot::Vector3 unsnapped_move_;
	godot::Vector3 applied_move_;
};

} // namespace

std::string_view MoveGizmo::id() const {
	return kId;
}

GizmoPickHit MoveGizmo::pick(const GizmoInput &input, godot::Vector2 screen_position) const {
	const GizmoFrame frame = make_frame(input);
	if (!frame.ok) {
		return {};
	}
	const float pick_radius = std::max(kMoveMinimumPickRadiusPixels, kMovePickRadiusPixels * frame.scale);
	GizmoPickHit axis_hit = pick_axis_handle(frame, screen_position, pick_radius);
	if (axis_hit.hit) {
		return axis_hit;
	}
	for (const GizmoPlanePrimitive &plane : frame.planes) {
		if (point_in_convex_quad(screen_position, plane.screen)) {
			return {
					.hit = true,
					.handle = plane.handle,
					.screen_anchor = (plane.screen[0] + plane.screen[1] + plane.screen[2] + plane.screen[3]) *
							kMoveQuadCentroidFactor,
			};
		}
	}
	return {};
}

GizmoMeshes MoveGizmo::draw(const GizmoInput &input) const {
	const GizmoFrame frame = make_frame(input);
	GizmoMeshBuilder builder;
	if (frame.ok) {
		const float width_scale = std::clamp(frame.scale, kMoveHandleMinWidthScale, kMoveHandleMaxWidthScale);
		for (const GizmoPlanePrimitive &plane : frame.planes) {
			if (!drag_plane_includes(frame.active_handle, plane.handle)) {
				continue;
			}
			const godot::Color color =
					highlighted(frame, plane.handle) ? highlight_color_for_axis(plane.handle) : color_for_plane(plane.handle);
			append_plane(builder, plane, color);
		}
		for (const GizmoAxisPrimitive &axis : frame.axes) {
			if (!drag_axis_includes(frame.active_handle, axis.handle)) {
				continue;
			}
			const godot::Color color = highlighted(frame, axis.handle) ? highlight_color_for_axis(axis.handle)
																	 : multiply_color(color_for_axis(axis.handle),
																			   kMoveColorFullIntensity, axis.alpha);
			append_arrow_handle(builder, axis.world_start, axis.world_tip, color, kMoveAxisLineWidthPixels * width_scale);
		}
	}
	return {
			.lines = make_line_mesh(builder.lines, input.camera, input.viewport_size),
			.triangles = make_triangle_mesh(builder.triangles),
	};
}

std::unique_ptr<GizmoDragSession> MoveGizmo::begin_drag(const GizmoDragStart &start) const {
	if (!has_gizmo_handle(start.hit.handle)) {
		return nullptr;
	}
	return std::make_unique<MoveGizmoDragSession>(start);
}

GizmoDragOperation MoveGizmoDragSession::update_drag(const GizmoDragContext &context) {
	GizmoDragOperation operation;
	if (context.camera == nullptr || !has_gizmo_handle(handle())) {
		return operation;
	}
	const godot::Vector2 delta = context.position - last_position();
	if (delta.length_squared() <= kGizmoScreenEpsilon) {
		return operation;
	}
	operation.dragged = true;
	godot::Vector3 world_delta;
	if (handle_is_plane(handle())) {
		const std::array<godot::Vector3, 2> axes = plane_axes(handle());
		const godot::Vector2 screen_pivot = context.camera->unproject_position(pivot());
		const godot::Vector2 screen_a = context.camera->unproject_position(pivot() + axes[0]) - screen_pivot;
		const godot::Vector2 screen_b = context.camera->unproject_position(pivot() + axes[1]) - screen_pivot;
		const float determinant = screen_a.x * screen_b.y - screen_a.y * screen_b.x;
		if (std::abs(determinant) > kGizmoScreenEpsilon) {
			const float a = (delta.x * screen_b.y - delta.y * screen_b.x) / determinant;
			const float b = (screen_a.x * delta.y - screen_a.y * delta.x) / determinant;
			world_delta = axes[0] * a + axes[1] * b;
		}
	} else {
		const godot::Vector3 axis = axis_vector(handle());
		const godot::Vector2 screen_axis =
				context.camera->unproject_position(pivot() + axis) - context.camera->unproject_position(pivot());
		if (screen_axis.length_squared() > kGizmoScreenEpsilon) {
			const float world_units = world_units_per_pixel_at(context.camera, context.viewport_size, pivot());
			world_delta = axis * (delta.dot(screen_axis.normalized()) * world_units);
		}
	}
	unsnapped_move_ += world_delta;
	const godot::Vector3 target_move = context.snap_enabled
			? snap_center_drag_delta(handle(), start_pivot(), unsnapped_move_, context.grid_size)
			: unsnapped_move_;
	const godot::Vector3 apply_delta = target_move - applied_move_;
	if (apply_delta.length_squared() > kGizmoEpsilon * kGizmoEpsilon) {
		if (context.apply_mutation) {
			const GizmoMutationResult result = context.apply_mutation({
					.kind = GizmoMutationKind::TranslateSelection,
					.value = apply_delta,
			});
			operation.changed = result.success && result.changed;
			if (operation.changed) {
				applied_move_ = target_move;
				set_pivot(start_pivot() + target_move);
			}
		}
	}
	set_last_position(context.position);
	return operation;
}

const MoveGizmo &move_gizmo() {
	static const MoveGizmo gizmo;
	return gizmo;
}

} // namespace quader_godot::gizmo
