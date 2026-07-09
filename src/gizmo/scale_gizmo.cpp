#include "gizmo/scale_gizmo.h"

#include "gizmo/gizmo_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace quader_godot::gizmo {
namespace {

constexpr float kScaleSizePixels = 80.0f;
constexpr float kScalePlaneSize = 0.2f;
constexpr float kScalePlaneDistance = 0.3f;
constexpr float kScaleHandleOffset = 1.4f;
constexpr float kScaleHandleTip = kScaleHandleOffset * 1.11f;
constexpr float kScaleHandleHalfWidth = 0.07f;
constexpr float kScaleAxisLineWidthPixels = 1.6f;
constexpr float kScaleCenterSizePixels = 16.0f;
constexpr float kScalePickRadiusPixels = 7.0f;
constexpr float kScaleMinimumPickRadiusPixels = 8.0f;
constexpr float kScaleMinimumScreenSizePixels = 1.0f;
constexpr float kScaleCenterMinimumScreenSizePixels = 24.0f;
constexpr float kScaleCenterSafeRadiusFactor = 0.35f;
constexpr float kScaleCenterPickRadiusFactor = 1.75f;
constexpr float kScaleCenterMinimumPickRadiusPixels = 12.0f;
constexpr float kScaleHandleMinWidthScale = 0.75f;
constexpr float kScaleHandleMaxWidthScale = 1.5f;
constexpr float kScalePixelsPerFactor = 96.0f;
constexpr float kMinScaleFactor = 0.01f;
constexpr float kScaleHalfFactor = 0.5f;
constexpr float kScaleDoubleFactor = 2.0f;
constexpr float kScaleQuadCentroidFactor = 0.25f;
constexpr float kScaleColorFullIntensity = 1.0f;
constexpr float kScaleCenterOutlineLineWidthPixels = 1.0f;
constexpr char kScaleCenterOutlineColor[] = "#fff680b4";
constexpr char kScaleCenterFillColor[] = "#fffaccff";

GizmoFrame make_frame(const GizmoInput &input) {
	GizmoFrame frame;
	if (!prepare_frame(input, frame)) {
		return frame;
	}
	const float unit_screen_size = std::max(kScaleMinimumScreenSizePixels, kScaleSizePixels) * frame.scale;
	const float pixel_world = world_units_per_pixel_at(input.camera, input.viewport_size, input.pivot);
	const float axis_length = pixel_world * unit_screen_size * kScaleHandleTip;
	const float plane_offset = pixel_world * unit_screen_size * (kScalePlaneDistance + kScalePlaneSize * kScaleHalfFactor);
	const float plane_half_size = pixel_world * unit_screen_size * (kScalePlaneSize * kScaleHalfFactor);
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

GizmoPickHit pick_center_or_axis(const GizmoFrame &frame, godot::Vector2 screen_position, float pick_radius) {
	const float screen_size = std::max(kScaleMinimumScreenSizePixels, kScaleSizePixels) * frame.scale;
	const float center_safe_radius =
			std::max(pick_radius,
					std::min(std::max(kScaleCenterMinimumScreenSizePixels, screen_size) * kScaleCenterSafeRadiusFactor,
							std::max(pick_radius * kScaleCenterPickRadiusFactor,
									kScaleCenterMinimumPickRadiusPixels)));
	const float center_distance_squared = screen_position.distance_squared_to(frame.screen_pivot);
	if (center_distance_squared <= center_safe_radius * center_safe_radius) {
		return {
				.hit = true,
				.handle = GizmoHandle::All,
				.screen_anchor = {frame.screen_pivot.x + std::max(kScaleCenterMinimumScreenSizePixels, screen_size),
						frame.screen_pivot.y},
				.distance = std::sqrt(center_distance_squared),
		};
	}
	return pick_axis_handle(frame, screen_position, pick_radius);
}

void append_square_prism(GizmoMeshBuilder &builder, godot::Vector3 start, godot::Vector3 end, float half_width_world,
		godot::Color color) {
	godot::Vector3 axis = end - start;
	const float length_world = axis.length();
	if (length_world <= kGizmoEpsilon || half_width_world <= kGizmoEpsilon) {
		return;
	}
	axis /= length_world;
	godot::Vector3 side_a = fallback_perpendicular(axis);
	godot::Vector3 side_b = axis.cross(side_a);
	if (side_b.length_squared() <= kGizmoEpsilon) {
		side_b = {0.0f, 1.0f, 0.0f};
	} else {
		side_b.normalize();
	}
	side_a = side_b.cross(axis).normalized();
	const std::array<godot::Vector3, 4> start_corners{
			start + side_a * half_width_world + side_b * half_width_world,
			start - side_a * half_width_world + side_b * half_width_world,
			start - side_a * half_width_world - side_b * half_width_world,
			start + side_a * half_width_world - side_b * half_width_world,
	};
	const std::array<godot::Vector3, 4> end_corners{
			end + side_a * half_width_world + side_b * half_width_world,
			end - side_a * half_width_world + side_b * half_width_world,
			end - side_a * half_width_world - side_b * half_width_world,
			end + side_a * half_width_world - side_b * half_width_world,
	};
	for (int index = 0; index < 4; ++index) {
		const int next = (index + 1) % 4;
		append_quad(builder, start_corners[static_cast<std::size_t>(index)],
				start_corners[static_cast<std::size_t>(next)], end_corners[static_cast<std::size_t>(next)],
				end_corners[static_cast<std::size_t>(index)], color);
	}
	append_quad(builder, start_corners[0], start_corners[1], start_corners[2], start_corners[3], color);
	append_quad(builder, end_corners[3], end_corners[2], end_corners[1], end_corners[0], color);
}

void append_scale_handle(GizmoMeshBuilder &builder, godot::Vector3 start, godot::Vector3 tip, godot::Color color,
		float line_width_pixels) {
	godot::Vector3 direction = tip - start;
	const float axis_length = direction.length();
	if (axis_length <= kGizmoEpsilon) {
		return;
	}
	direction /= axis_length;
	const float unit_world = axis_length / kScaleHandleTip;
	const godot::Vector3 block_start = start + direction * (unit_world * kScaleHandleOffset);
	append_line(builder, start, block_start, color, line_width_pixels);
	append_square_prism(builder, block_start, tip, unit_world * kScaleHandleHalfWidth, color);
}

void append_cube_handle(GizmoMeshBuilder &builder, godot::Vector3 center, float world_size, godot::Color color) {
	const float half = world_size * kScaleHalfFactor;
	const std::array<godot::Vector3, 8> corners{
			center + godot::Vector3{-half, -half, -half}, center + godot::Vector3{half, -half, -half},
			center + godot::Vector3{half, half, -half}, center + godot::Vector3{-half, half, -half},
			center + godot::Vector3{-half, -half, half}, center + godot::Vector3{half, -half, half},
			center + godot::Vector3{half, half, half}, center + godot::Vector3{-half, half, half},
	};
	constexpr std::array<std::array<int, 4>, 6> kFaces{
			std::array<int, 4>{1, 5, 6, 2}, std::array<int, 4>{4, 0, 3, 7}, std::array<int, 4>{3, 2, 6, 7},
			std::array<int, 4>{4, 5, 1, 0}, std::array<int, 4>{5, 4, 7, 6}, std::array<int, 4>{0, 1, 2, 3},
	};
	for (const std::array<int, 4> &face : kFaces) {
		append_quad(builder, corners[static_cast<std::size_t>(face[0])], corners[static_cast<std::size_t>(face[1])],
				corners[static_cast<std::size_t>(face[2])], corners[static_cast<std::size_t>(face[3])], color);
		append_line(builder, corners[static_cast<std::size_t>(face[0])], corners[static_cast<std::size_t>(face[1])],
				html_color(kScaleCenterOutlineColor), kScaleCenterOutlineLineWidthPixels);
		append_line(builder, corners[static_cast<std::size_t>(face[1])], corners[static_cast<std::size_t>(face[2])],
				html_color(kScaleCenterOutlineColor), kScaleCenterOutlineLineWidthPixels);
		append_line(builder, corners[static_cast<std::size_t>(face[2])], corners[static_cast<std::size_t>(face[3])],
				html_color(kScaleCenterOutlineColor), kScaleCenterOutlineLineWidthPixels);
		append_line(builder, corners[static_cast<std::size_t>(face[3])], corners[static_cast<std::size_t>(face[0])],
				html_color(kScaleCenterOutlineColor), kScaleCenterOutlineLineWidthPixels);
	}
}

constexpr std::array<GizmoComponent, 3> kScaleComponents{
		GizmoComponent::X,
		GizmoComponent::Y,
		GizmoComponent::Z,
};

float bounds_extent_component(godot::Vector3 min, godot::Vector3 max, GizmoComponent component) {
	if (component == GizmoComponent::X) {
		return std::abs(max.x - min.x);
	}
	if (component == GizmoComponent::Y) {
		return std::abs(max.y - min.y);
	}
	return std::abs(max.z - min.z);
}

float max_abs(float left, float right) {
	return std::max(std::abs(left), std::abs(right));
}

float base_scale_distance(GizmoHandle handle, godot::Vector3 min, godot::Vector3 max, godot::Vector3 pivot) {
	float distance = 0.0f;
	if (handle_includes_component(handle, GizmoComponent::X)) {
		distance = std::max(distance, max_abs(max.x - pivot.x, min.x - pivot.x));
	}
	if (handle_includes_component(handle, GizmoComponent::Y)) {
		distance = std::max(distance, max_abs(max.y - pivot.y, min.y - pivot.y));
	}
	if (handle_includes_component(handle, GizmoComponent::Z)) {
		distance = std::max(distance, max_abs(max.z - pivot.z, min.z - pivot.z));
	}
	return distance;
}

float minimum_scale_factor_for_grid(GizmoHandle handle, godot::Vector3 min, godot::Vector3 max,
		float grid_size, bool shrinking) {
	float factor = kMinScaleFactor;
	for (GizmoComponent component : kScaleComponents) {
		if (!handle_includes_component(handle, component)) {
			continue;
		}
		const float extent = bounds_extent_component(min, max, component);
		if (extent <= kGizmoScreenEpsilon) {
			continue;
		}
		float axis_factor = grid_size / extent;
		if (shrinking && axis_factor > 1.0f) {
			axis_factor = 1.0f;
		}
		factor = std::max(factor, axis_factor);
	}
	return factor;
}

float snapped_scale_factor(GizmoHandle handle, float factor, const GizmoSelectionBounds &bounds,
		godot::Vector3 pivot, bool grid_enabled, float grid_size) {
	if (!bounds.has_bounds || !grid_enabled || grid_size <= kGizmoScreenEpsilon) {
		return factor;
	}
	if (std::abs(factor - 1.0f) <= kGizmoEpsilon) {
		return 1.0f;
	}
	const float base_distance_value = base_scale_distance(handle, bounds.min, bounds.max, pivot);
	if (base_distance_value <= kGizmoScreenEpsilon) {
		return factor;
	}
	const float base_extent = base_distance_value * kScaleDoubleFactor;
	const float raw_extent = std::max(kGizmoEpsilon, base_extent * factor);
	const float grid = safe_grid_size(grid_size);
	const bool shrinking = factor < 1.0f;
	const float snapped_units = shrinking ? std::floor(raw_extent / grid) : std::round(raw_extent / grid);
	const float snapped_extent = snapped_units * grid;
	const float snapped_factor = snapped_extent <= kGizmoScreenEpsilon ? kMinScaleFactor : snapped_extent / base_extent;
	const float minimum_factor = minimum_scale_factor_for_grid(handle, bounds.min, bounds.max, grid, shrinking);
	return std::max(minimum_factor, snapped_factor);
}

godot::Vector3 axis_scale(GizmoHandle handle, float factor) {
	godot::Vector3 scale{1.0f, 1.0f, 1.0f};
	if (handle == GizmoHandle::X || handle == GizmoHandle::XY || handle == GizmoHandle::XZ) {
		scale.x = factor;
	}
	if (handle == GizmoHandle::Y || handle == GizmoHandle::XY || handle == GizmoHandle::YZ) {
		scale.y = factor;
	}
	if (handle == GizmoHandle::Z || handle == GizmoHandle::XZ || handle == GizmoHandle::YZ) {
		scale.z = factor;
	}
	if (handle == GizmoHandle::All) {
		scale = {factor, factor, factor};
	}
	return scale;
}

class ScaleGizmoDragSession final : public GizmoDragSession {
public:
	explicit ScaleGizmoDragSession(const GizmoDragStart &start)
			: GizmoDragSession(start.hit.handle, start.screen_position, start.pivot),
			  bounds_(start.selection_bounds) {}

	[[nodiscard]] GizmoDragOperation update_drag(const GizmoDragContext &context) override;

private:
	GizmoSelectionBounds bounds_;
	float unsnapped_scale_amount_ = 0.0f;
	float applied_scale_factor_ = 1.0f;
};

} // namespace

std::string_view ScaleGizmo::id() const {
	return kId;
}

GizmoPickHit ScaleGizmo::pick(const GizmoInput &input, godot::Vector2 screen_position) const {
	const GizmoFrame frame = make_frame(input);
	if (!frame.ok) {
		return {};
	}
	const float pick_radius = std::max(kScaleMinimumPickRadiusPixels, kScalePickRadiusPixels * frame.scale);
	GizmoPickHit axis_hit = pick_center_or_axis(frame, screen_position, pick_radius);
	if (axis_hit.hit) {
		return axis_hit;
	}
	for (const GizmoPlanePrimitive &plane : frame.planes) {
		if (point_in_convex_quad(screen_position, plane.screen)) {
			return {
					.hit = true,
					.handle = plane.handle,
					.screen_anchor = (plane.screen[0] + plane.screen[1] + plane.screen[2] + plane.screen[3]) *
							kScaleQuadCentroidFactor,
			};
		}
	}
	return {};
}

GizmoMeshes ScaleGizmo::draw(const GizmoInput &input) const {
	const GizmoFrame frame = make_frame(input);
	GizmoMeshBuilder builder;
	if (frame.ok) {
		const float width_scale = std::clamp(frame.scale, kScaleHandleMinWidthScale, kScaleHandleMaxWidthScale);
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
																			   kScaleColorFullIntensity, axis.alpha);
			append_scale_handle(builder, axis.world_start, axis.world_tip, color, kScaleAxisLineWidthPixels * width_scale);
		}
		if ((frame.hovered_handle == GizmoHandle::All || frame.active_handle == GizmoHandle::All) && !frame.axes.empty()) {
			const float axis_length = frame.axes.front().world_start.distance_to(frame.axes.front().world_tip);
			const float world_size = std::max(kGizmoEpsilon, axis_length * (kScaleCenterSizePixels / kScaleSizePixels));
			append_cube_handle(builder, frame.pivot, world_size, html_color(kScaleCenterFillColor));
		}
	}
	return {
			.lines = make_line_mesh(builder.lines, input.camera, input.viewport_size),
			.triangles = make_triangle_mesh(builder.triangles),
	};
}

std::unique_ptr<GizmoDragSession> ScaleGizmo::begin_drag(const GizmoDragStart &start) const {
	if (!has_gizmo_handle(start.hit.handle)) {
		return nullptr;
	}
	return std::make_unique<ScaleGizmoDragSession>(start);
}

GizmoDragOperation ScaleGizmoDragSession::update_drag(const GizmoDragContext &context) {
	GizmoDragOperation operation;
	if (context.camera == nullptr || !has_gizmo_handle(handle())) {
		return operation;
	}
	const godot::Vector2 delta = context.position - last_position();
	if (delta.length_squared() <= kGizmoScreenEpsilon) {
		return operation;
	}
	operation.dragged = true;
	float amount = 0.0f;
	if (handle() == GizmoHandle::All) {
		amount = delta.x - delta.y;
	} else if (handle_is_plane(handle())) {
		const std::array<godot::Vector3, 2> axes = plane_axes(handle());
		const godot::Vector2 screen_pivot = context.camera->unproject_position(pivot());
		const godot::Vector2 screen_a = context.camera->unproject_position(pivot() + axes[0]) - screen_pivot;
		const godot::Vector2 screen_b = context.camera->unproject_position(pivot() + axes[1]) - screen_pivot;
		godot::Vector2 screen_axis = screen_a.normalized() + screen_b.normalized();
		if (screen_axis.length_squared() > kGizmoScreenEpsilon) {
			amount = delta.dot(screen_axis.normalized());
		}
	} else {
		const godot::Vector3 axis = axis_vector(handle());
		const godot::Vector2 screen_axis =
				context.camera->unproject_position(pivot() + axis) - context.camera->unproject_position(pivot());
		if (screen_axis.length_squared() > kGizmoScreenEpsilon) {
			amount = delta.dot(screen_axis.normalized());
		}
	}
	unsnapped_scale_amount_ += amount;
	float factor = std::max(kMinScaleFactor, 1.0f + unsnapped_scale_amount_ / kScalePixelsPerFactor);
	factor = snapped_scale_factor(handle(), factor, bounds_, pivot(), context.snap_enabled, context.grid_size);
	const float relative_factor = factor / std::max(applied_scale_factor_, kMinScaleFactor);
	if (std::abs(relative_factor - 1.0f) > kGizmoEpsilon) {
		if (context.apply_mutation) {
			const GizmoMutationResult result = context.apply_mutation({
					.kind = GizmoMutationKind::ScaleSelection,
					.value = axis_scale(handle(), relative_factor),
					.pivot = pivot(),
			});
			operation.changed = result.success && result.changed;
			if (operation.changed) {
				applied_scale_factor_ = factor;
			}
		}
	}
	set_last_position(context.position);
	return operation;
}

const ScaleGizmo &scale_gizmo() {
	static const ScaleGizmo gizmo;
	return gizmo;
}

} // namespace quader_godot::gizmo
