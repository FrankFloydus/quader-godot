#include "viewport/quader_viewport_control.h"

#include "render/quader_godot_render_utils.h"
#include "render/quader_godot_selection_overlay.h"
#include "render/quader_godot_transform_gizmo.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/sub_viewport_container.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <utility>

namespace quader_godot::viewport {
namespace {

using quader::modeling::AuthoredPolygonFacePayload;
using quader::modeling::AuthoredPolygonPayload;
using quader::modeling::EdgeKey;
using quader::modeling::FaceId;
using quader::modeling::ObjectId;
using quader::modeling::SelectionEdit;
using quader::modeling::SelectionKind;
using quader::modeling::Vec3;
using quader::modeling::VertexId;

struct Ray {
	godot::Vector3 origin;
	godot::Vector3 direction;
};

struct PickHit {
	bool hit = false;
	float distance = std::numeric_limits<float>::max();
	float depth = std::numeric_limits<float>::max();
	modeling::SelectionTarget target;
};

constexpr float kPickOcclusionTolerance = 0.01f;
constexpr float kOverlayLineDepthBiasPixels = 1.0f;
constexpr float kOverlayPointDepthBiasPixels = 1.5f;
constexpr float kToolEpsilon = 0.000001f;
constexpr float kScreenEpsilon = 0.0001f;
constexpr float kScalePixelsPerFactor = 96.0f;
constexpr float kMinScaleFactor = 0.01f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kRotateSnapRadians = kPi / 12.0f;
constexpr int kSelectedFaceRenderPriority = godot::Material::RENDER_PRIORITY_MAX - 20;
constexpr int kHoverFaceRenderPriority = godot::Material::RENDER_PRIORITY_MAX - 19;
constexpr int kSourceWireRenderPriority = godot::Material::RENDER_PRIORITY_MAX - 16;
constexpr int kSelectedWireRenderPriority = godot::Material::RENDER_PRIORITY_MAX - 14;
constexpr int kHoverRenderPriority = godot::Material::RENDER_PRIORITY_MAX - 12;
constexpr int kVertexBaseRenderPriority = godot::Material::RENDER_PRIORITY_MAX - 9;
constexpr int kSelectedVertexOutlineRenderPriority = godot::Material::RENDER_PRIORITY_MAX - 8;
constexpr int kVertexSelectedRenderPriority = godot::Material::RENDER_PRIORITY_MAX - 7;
constexpr int kHoverVertexOutlineRenderPriority = godot::Material::RENDER_PRIORITY_MAX - 6;
constexpr int kVertexHoverRenderPriority = godot::Material::RENDER_PRIORITY_MAX - 5;

bool keyboard_shift_pressed() {
	godot::Input *input = godot::Input::get_singleton();
	return input != nullptr && input->is_key_pressed(godot::KEY_SHIFT);
}

bool keyboard_remove_pressed() {
	godot::Input *input = godot::Input::get_singleton();
	return input != nullptr &&
			(input->is_key_pressed(godot::KEY_CTRL) || input->is_key_pressed(godot::KEY_META));
}

bool keyboard_snap_disabled() {
	godot::Input *input = godot::Input::get_singleton();
	return input != nullptr && input->is_key_pressed(godot::KEY_CTRL);
}

int grid_preset_for_key(godot::Key key) {
	switch (key) {
	case godot::KEY_F1:
		return 1;
	case godot::KEY_F2:
		return 2;
	case godot::KEY_F3:
		return 3;
	case godot::KEY_F4:
		return 4;
	case godot::KEY_F5:
		return 5;
	case godot::KEY_F6:
		return 6;
	case godot::KEY_F7:
		return 7;
	case godot::KEY_F8:
		return 8;
	case godot::KEY_F9:
		return 9;
	case godot::KEY_F10:
		return 10;
	default:
		return 0;
	}
}

std::optional<SelectionMode> selection_mode_for_key(godot::Key key) {
	switch (key) {
	case godot::KEY_1:
		return SelectionMode::Vertex;
	case godot::KEY_2:
		return SelectionMode::Edge;
	case godot::KEY_3:
		return SelectionMode::Face;
	case godot::KEY_4:
		return SelectionMode::Mesh;
	default:
		return std::nullopt;
	}
}

std::optional<render::TransformGizmoTool> transform_tool_for_key(godot::Key key) {
	switch (key) {
	case godot::KEY_Q:
		return render::TransformGizmoTool::None;
	case godot::KEY_W:
		return render::TransformGizmoTool::Move;
	case godot::KEY_R:
		return render::TransformGizmoTool::Rotate;
	case godot::KEY_S:
		return render::TransformGizmoTool::Scale;
	default:
		return std::nullopt;
	}
}

float grid_world_size_for_preset(int preset) {
	static constexpr std::array<float, 10> kPresetWorldSizes{
			1.0f / 32.0f,
			1.0f / 16.0f,
			1.0f / 8.0f,
			1.0f / 4.0f,
			1.0f / 2.0f,
			1.0f,
			2.0f,
			4.0f,
			8.0f,
			16.0f,
	};
	const int clamped = std::clamp(preset, 1, static_cast<int>(kPresetWorldSizes.size()));
	return kPresetWorldSizes[static_cast<std::size_t>(clamped - 1)];
}

bool same_object(ObjectId a, ObjectId b) {
	return a == b;
}

bool same_selection_target(const modeling::SelectionTarget &a, const modeling::SelectionTarget &b) {
	if (a.kind != b.kind || a.object != b.object) {
		return false;
	}
	if (a.kind == SelectionKind::Vertex) {
		return a.vertex == b.vertex;
	}
	if (a.kind == SelectionKind::Edge) {
		return a.edge == b.edge;
	}
	if (a.kind == SelectionKind::Face) {
		return a.face == b.face;
	}
	return true;
}

bool remove_modifier_key(godot::Key key) {
	return key == godot::KEY_CTRL || key == godot::KEY_META;
}

godot::Vector3 transform_axis_vector(render::TransformGizmoAxis axis) {
	if (axis == render::TransformGizmoAxis::X) {
		return {1.0f, 0.0f, 0.0f};
	}
	if (axis == render::TransformGizmoAxis::Y) {
		return {0.0f, 1.0f, 0.0f};
	}
	if (axis == render::TransformGizmoAxis::Z) {
		return {0.0f, 0.0f, 1.0f};
	}
	return {};
}

quader::modeling::Vec3 to_modeling(godot::Vector3 value) {
	return {value.x, value.y, value.z};
}

quader::modeling::Vec3 axis_radians(render::TransformGizmoAxis axis, float radians) {
	if (axis == render::TransformGizmoAxis::X) {
		return {radians, 0.0f, 0.0f};
	}
	if (axis == render::TransformGizmoAxis::Y) {
		return {0.0f, radians, 0.0f};
	}
	if (axis == render::TransformGizmoAxis::Z) {
		return {0.0f, 0.0f, radians};
	}
	return {};
}

quader::modeling::Vec3 axis_scale(render::TransformGizmoAxis axis, float factor) {
	quader::modeling::Vec3 scale{1.0f, 1.0f, 1.0f};
	if (axis == render::TransformGizmoAxis::X || axis == render::TransformGizmoAxis::XY ||
			axis == render::TransformGizmoAxis::XZ) {
		scale.x = factor;
	}
	if (axis == render::TransformGizmoAxis::Y || axis == render::TransformGizmoAxis::XY ||
			axis == render::TransformGizmoAxis::YZ) {
		scale.y = factor;
	}
	if (axis == render::TransformGizmoAxis::Z || axis == render::TransformGizmoAxis::XZ ||
			axis == render::TransformGizmoAxis::YZ) {
		scale.z = factor;
	}
	if (axis == render::TransformGizmoAxis::All) {
		scale = {factor, factor, factor};
	}
	return scale;
}

bool transform_axis_is_plane(render::TransformGizmoAxis axis) {
	return axis == render::TransformGizmoAxis::XY || axis == render::TransformGizmoAxis::XZ ||
			axis == render::TransformGizmoAxis::YZ;
}

std::array<godot::Vector3, 2> transform_plane_axes(render::TransformGizmoAxis axis) {
	if (axis == render::TransformGizmoAxis::XY) {
		return {godot::Vector3{1.0f, 0.0f, 0.0f}, godot::Vector3{0.0f, 1.0f, 0.0f}};
	}
	if (axis == render::TransformGizmoAxis::XZ) {
		return {godot::Vector3{1.0f, 0.0f, 0.0f}, godot::Vector3{0.0f, 0.0f, 1.0f}};
	}
	return {godot::Vector3{0.0f, 1.0f, 0.0f}, godot::Vector3{0.0f, 0.0f, 1.0f}};
}

float viewport_world_units_per_pixel_at(const godot::Camera3D *camera, godot::Vector2 viewport_size,
		godot::Vector3 position) {
	const float height = std::max(viewport_size.y, 1.0f);
	if (camera->get_projection() == godot::Camera3D::PROJECTION_ORTHOGONAL) {
		return static_cast<float>(camera->get_size()) / height;
	}
	const godot::Vector3 camera_forward = -camera->get_global_transform().basis.get_column(2).normalized();
	const float view_depth = std::max((position - camera->get_global_transform().origin).dot(camera_forward), 0.001f);
	const float fov_radians = static_cast<float>(camera->get_fov()) * 3.14159265358979323846f / 180.0f;
	return 2.0f * view_depth * std::tan(fov_radians * 0.5f) / height;
}

float safe_grid_size(float grid_size) {
	return std::isfinite(grid_size) && grid_size > kToolEpsilon ? grid_size : 1.0f;
}

float snap_world_value(float value, float grid_size) {
	const float grid = safe_grid_size(grid_size);
	return std::round(value / grid) * grid;
}

float snap_to_step(float value, float step) {
	if (!std::isfinite(step) || step <= kToolEpsilon) {
		return value;
	}
	return std::round(value / step) * step;
}

bool transform_axis_includes_component(render::TransformGizmoAxis axis, int component) {
	if (axis == render::TransformGizmoAxis::All) {
		return true;
	}
	if (component == 0) {
		return axis == render::TransformGizmoAxis::X || axis == render::TransformGizmoAxis::XY ||
				axis == render::TransformGizmoAxis::XZ;
	}
	if (component == 1) {
		return axis == render::TransformGizmoAxis::Y || axis == render::TransformGizmoAxis::XY ||
				axis == render::TransformGizmoAxis::YZ;
	}
	if (component == 2) {
		return axis == render::TransformGizmoAxis::Z || axis == render::TransformGizmoAxis::XZ ||
				axis == render::TransformGizmoAxis::YZ;
	}
	return false;
}

godot::Vector3 snap_center_drag_delta(render::TransformGizmoAxis axis, godot::Vector3 center,
		godot::Vector3 raw_delta, float grid_size) {
	godot::Vector3 delta;
	if (transform_axis_includes_component(axis, 0)) {
		delta.x = snap_world_value(center.x + raw_delta.x, grid_size) - center.x;
	}
	if (transform_axis_includes_component(axis, 1)) {
		delta.y = snap_world_value(center.y + raw_delta.y, grid_size) - center.y;
	}
	if (transform_axis_includes_component(axis, 2)) {
		delta.z = snap_world_value(center.z + raw_delta.z, grid_size) - center.z;
	}
	return delta;
}

float bounds_extent_component(godot::Vector3 min, godot::Vector3 max, int component) {
	if (component == 0) {
		return std::abs(max.x - min.x);
	}
	if (component == 1) {
		return std::abs(max.y - min.y);
	}
	if (component == 2) {
		return std::abs(max.z - min.z);
	}
	return 0.0f;
}

float base_scale_distance(render::TransformGizmoAxis axis, godot::Vector3 min, godot::Vector3 max,
		godot::Vector3 pivot) {
	const auto max_abs = [](float left, float right) { return std::max(std::abs(left), std::abs(right)); };
	float distance = 0.0f;
	if (transform_axis_includes_component(axis, 0)) {
		distance = std::max(distance, max_abs(max.x - pivot.x, min.x - pivot.x));
	}
	if (transform_axis_includes_component(axis, 1)) {
		distance = std::max(distance, max_abs(max.y - pivot.y, min.y - pivot.y));
	}
	if (transform_axis_includes_component(axis, 2)) {
		distance = std::max(distance, max_abs(max.z - pivot.z, min.z - pivot.z));
	}
	return distance;
}

float minimum_scale_factor_for_grid(render::TransformGizmoAxis axis, godot::Vector3 min, godot::Vector3 max,
		float grid_size, bool shrinking) {
	float factor = kMinScaleFactor;
	for (int component = 0; component < 3; ++component) {
		if (!transform_axis_includes_component(axis, component)) {
			continue;
		}
		const float extent = bounds_extent_component(min, max, component);
		if (extent <= kScreenEpsilon) {
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

float snapped_scale_factor(render::TransformGizmoAxis axis, float factor, const TransformDragBounds &bounds,
		godot::Vector3 pivot, bool grid_enabled, float grid_size) {
	if (!bounds.has_bounds || !grid_enabled || grid_size <= kScreenEpsilon) {
		return factor;
	}
	if (std::abs(factor - 1.0f) <= kToolEpsilon) {
		return 1.0f;
	}
	const float base_distance_value = base_scale_distance(axis, bounds.min, bounds.max, pivot);
	if (base_distance_value <= kScreenEpsilon) {
		return factor;
	}
	const float base_extent = base_distance_value * 2.0f;
	const float raw_extent = std::max(kToolEpsilon, base_extent * factor);
	const float grid = safe_grid_size(grid_size);
	const bool shrinking = factor < 1.0f;
	const float snapped_units = shrinking ? std::floor(raw_extent / grid) : std::round(raw_extent / grid);
	const float snapped_extent = snapped_units * grid;
	const float snapped_factor = snapped_extent <= kScreenEpsilon ? kMinScaleFactor : snapped_extent / base_extent;
	const float minimum_factor = minimum_scale_factor_for_grid(axis, bounds.min, bounds.max, grid, shrinking);
	return std::max(minimum_factor, snapped_factor);
}

template <typename T>
bool contains_id(std::span<const T> ids, T value) {
	return std::find(ids.begin(), ids.end(), value) != ids.end();
}

bool target_selected(const modeling::MeshObjectSnapshot &object, const modeling::SelectionTarget &target) {
	if (target.kind == SelectionKind::Object) {
		return object.mesh_selected;
	}
	if (target.kind == SelectionKind::Vertex) {
		return contains_id<VertexId>(object.selected_vertices, target.vertex);
	}
	if (target.kind == SelectionKind::Edge) {
		return contains_id<EdgeKey>(object.selected_edges, target.edge);
	}
	if (target.kind == SelectionKind::Face) {
		return contains_id<FaceId>(object.selected_faces, target.face);
	}
	return false;
}

bool has_component_selection(const modeling::MeshObjectSnapshot &object) {
	return !object.selected_vertices.empty() || !object.selected_edges.empty() || !object.selected_faces.empty();
}

bool component_selected(const modeling::MeshObjectSnapshot &object, SelectionKind kind, VertexId vertex, EdgeKey edge) {
	if (kind == SelectionKind::Vertex) {
		return contains_id<VertexId>(object.selected_vertices, vertex);
	}
	if (kind == SelectionKind::Edge) {
		return contains_id<EdgeKey>(object.selected_edges, edge);
	}
	return false;
}

godot::Vector3 to_godot(Vec3 value) {
	return {value.x, value.y, value.z};
}

std::optional<godot::Vector3> vertex_position(const AuthoredPolygonPayload &payload, VertexId vertex) {
	for (std::size_t index = 0; index < payload.vertices.size() && index < payload.positions.size(); ++index) {
		if (payload.vertices[index] == vertex) {
			return to_godot(payload.positions[index]);
		}
	}
	return std::nullopt;
}

const AuthoredPolygonFacePayload *find_face(const AuthoredPolygonPayload &payload, FaceId face) {
	for (const AuthoredPolygonFacePayload &candidate : payload.faces) {
		if (candidate.id == face) {
			return &candidate;
		}
	}
	return nullptr;
}

bool face_contains_vertex(const AuthoredPolygonFacePayload &face, VertexId vertex) {
	return std::find(face.vertices.begin(), face.vertices.end(), vertex) != face.vertices.end();
}

bool face_contains_edge(const AuthoredPolygonFacePayload &face, EdgeKey edge) {
	const EdgeKey normalized = quader::modeling::make_edge_key(edge.a, edge.b);
	if (!normalized.valid() || face.vertices.size() < 2U) {
		return false;
	}
	for (std::size_t index = 0; index < face.vertices.size(); ++index) {
		const VertexId a = face.vertices[index];
		const VertexId b = face.vertices[(index + 1U) % face.vertices.size()];
		if (quader::modeling::make_edge_key(a, b) == normalized) {
			return true;
		}
	}
	return false;
}

bool point_inside_authored_bounds(const AuthoredPolygonPayload &payload, godot::Vector3 point, float padding) {
	if (payload.positions.empty()) {
		return false;
	}

	godot::Vector3 min = to_godot(payload.positions.front());
	godot::Vector3 max = min;
	for (Vec3 position : payload.positions) {
		const godot::Vector3 value = to_godot(position);
		min.x = std::min(min.x, value.x);
		min.y = std::min(min.y, value.y);
		min.z = std::min(min.z, value.z);
		max.x = std::max(max.x, value.x);
		max.y = std::max(max.y, value.y);
		max.z = std::max(max.z, value.z);
	}

	return point.x >= min.x - padding && point.x <= max.x + padding && point.y >= min.y - padding &&
			point.y <= max.y + padding && point.z >= min.z - padding && point.z <= max.z + padding;
}

std::optional<godot::Vector3> authored_center(const AuthoredPolygonPayload &payload) {
	if (payload.positions.empty()) {
		return std::nullopt;
	}
	godot::Vector3 center;
	for (Vec3 position : payload.positions) {
		center += to_godot(position);
	}
	return center / static_cast<float>(payload.positions.size());
}

std::optional<godot::Vector3> face_center(const AuthoredPolygonPayload &payload,
		const AuthoredPolygonFacePayload &face) {
	if (face.vertices.empty()) {
		return std::nullopt;
	}
	godot::Vector3 center;
	std::size_t count = 0;
	for (VertexId vertex : face.vertices) {
		if (const std::optional<godot::Vector3> position = vertex_position(payload, vertex)) {
			center += *position;
			++count;
		}
	}
	if (count == 0U) {
		return std::nullopt;
	}
	return center / static_cast<float>(count);
}

std::optional<godot::Vector3> face_normal(const AuthoredPolygonPayload &payload,
		const AuthoredPolygonFacePayload &face) {
	if (face.vertices.size() < 3U) {
		return std::nullopt;
	}
	const std::optional<godot::Vector3> origin = vertex_position(payload, face.vertices.front());
	if (!origin.has_value()) {
		return std::nullopt;
	}
	for (std::size_t index = 1; index + 1U < face.vertices.size(); ++index) {
		const std::optional<godot::Vector3> b = vertex_position(payload, face.vertices[index]);
		const std::optional<godot::Vector3> c = vertex_position(payload, face.vertices[index + 1U]);
		if (!b.has_value() || !c.has_value()) {
			continue;
		}
		const godot::Vector3 normal = (*b - *origin).cross(*c - *origin);
		if (normal.length_squared() > kToolEpsilon * kToolEpsilon) {
			return normal.normalized();
		}
	}
	return std::nullopt;
}

bool authored_faces_are_inside_out(const AuthoredPolygonPayload &payload) {
	const std::optional<godot::Vector3> center = authored_center(payload);
	if (!center.has_value()) {
		return false;
	}
	float orientation_score = 0.0f;
	std::size_t count = 0;
	for (const AuthoredPolygonFacePayload &face : payload.faces) {
		const std::optional<godot::Vector3> normal = face_normal(payload, face);
		const std::optional<godot::Vector3> middle = face_center(payload, face);
		if (!normal.has_value() || !middle.has_value()) {
			continue;
		}
		const godot::Vector3 center_to_face = *middle - *center;
		if (center_to_face.length_squared() <= kToolEpsilon * kToolEpsilon) {
			continue;
		}
		orientation_score += normal->dot(center_to_face.normalized());
		++count;
	}
	return count > 0U && orientation_score < -kToolEpsilon;
}

void append_face_triangles(const AuthoredPolygonPayload &payload, const AuthoredPolygonFacePayload &face,
		std::vector<render::OverlayTriangle> &triangles) {
	if (face.vertices.size() < 3U) {
		return;
	}
	const std::optional<godot::Vector3> origin = vertex_position(payload, face.vertices.front());
	if (!origin.has_value()) {
		return;
	}
	for (std::size_t index = 1; index + 1U < face.vertices.size(); ++index) {
		const std::optional<godot::Vector3> b = vertex_position(payload, face.vertices[index]);
		const std::optional<godot::Vector3> c = vertex_position(payload, face.vertices[index + 1U]);
		if (b.has_value() && c.has_value()) {
			triangles.push_back({*origin, *b, *c});
		}
	}
}

void append_face_segments(const AuthoredPolygonPayload &payload, const AuthoredPolygonFacePayload &face,
		std::vector<render::OverlaySegment> &segments) {
	if (face.vertices.size() < 2U) {
		return;
	}
	for (std::size_t index = 0; index < face.vertices.size(); ++index) {
		const std::optional<godot::Vector3> a = vertex_position(payload, face.vertices[index]);
		const std::optional<godot::Vector3> b =
				vertex_position(payload, face.vertices[(index + 1U) % face.vertices.size()]);
		if (a.has_value() && b.has_value()) {
			segments.push_back({*a, *b});
		}
	}
}

void append_all_face_triangles(const AuthoredPolygonPayload &payload, std::vector<render::OverlayTriangle> &triangles) {
	for (const AuthoredPolygonFacePayload &face : payload.faces) {
		append_face_triangles(payload, face, triangles);
	}
}

void append_all_edge_segments(const modeling::MeshObjectSnapshot &object, std::vector<render::OverlaySegment> &segments) {
	for (const EdgeKey &edge : object.edges) {
		const std::optional<godot::Vector3> a = vertex_position(object.authored, edge.a);
		const std::optional<godot::Vector3> b = vertex_position(object.authored, edge.b);
		if (a.has_value() && b.has_value()) {
			segments.push_back({*a, *b});
		}
	}
}

void append_edge_segment(const modeling::MeshObjectSnapshot &object, EdgeKey edge,
		std::vector<render::OverlaySegment> &segments) {
	const std::optional<godot::Vector3> a = vertex_position(object.authored, edge.a);
	const std::optional<godot::Vector3> b = vertex_position(object.authored, edge.b);
	if (a.has_value() && b.has_value()) {
		segments.push_back({*a, *b});
	}
}

std::vector<godot::Vector3> all_vertex_points(const AuthoredPolygonPayload &payload) {
	std::vector<godot::Vector3> points;
	points.reserve(payload.positions.size());
	for (Vec3 position : payload.positions) {
		points.push_back(to_godot(position));
	}
	return points;
}

Ray make_ray(const godot::Camera3D *camera, godot::Vector2 position) {
	return {
			.origin = camera->project_ray_origin(position),
			.direction = camera->project_ray_normal(position).normalized(),
	};
}

bool ray_triangle_intersection(const Ray &ray, const godot::Vector3 &a, const godot::Vector3 &b,
		const godot::Vector3 &c, float &distance) {
	constexpr float kEpsilon = 0.000001f;
	const godot::Vector3 edge1 = b - a;
	const godot::Vector3 edge2 = c - a;
	const godot::Vector3 h = ray.direction.cross(edge2);
	const float determinant = edge1.dot(h);
	if (std::abs(determinant) < kEpsilon) {
		return false;
	}
	const float inverse_det = 1.0f / determinant;
	const godot::Vector3 s = ray.origin - a;
	const float u = inverse_det * s.dot(h);
	if (u < 0.0f || u > 1.0f) {
		return false;
	}
	const godot::Vector3 q = s.cross(edge1);
	const float v = inverse_det * ray.direction.dot(q);
	if (v < 0.0f || u + v > 1.0f) {
		return false;
	}
	const float t = inverse_det * edge2.dot(q);
	if (t <= kEpsilon) {
		return false;
	}
	distance = t;
	return true;
}

float point_ray_distance(const godot::Vector3 &point, const Ray &ray, float &ray_depth) {
	ray_depth = (point - ray.origin).dot(ray.direction);
	if (ray_depth < 0.0f) {
		return std::numeric_limits<float>::max();
	}
	return (point - (ray.origin + ray.direction * ray_depth)).length();
}

float segment_ray_distance(const godot::Vector3 &a, const godot::Vector3 &b, const Ray &ray, float &ray_depth) {
	const godot::Vector3 segment = b - a;
	const float segment_length_sq = segment.length_squared();
	if (segment_length_sq <= 0.000001f) {
		return point_ray_distance(a, ray, ray_depth);
	}

	const godot::Vector3 w0 = a - ray.origin;
	const float segment_ray_dot = segment.dot(ray.direction);
	const float segment_origin_dot = segment.dot(w0);
	const float ray_origin_dot = ray.direction.dot(w0);
	const float denominator = segment_length_sq - segment_ray_dot * segment_ray_dot;
	float segment_factor = 0.0f;
	float ray_factor = 0.0f;
	if (std::abs(denominator) > 0.000001f) {
		segment_factor = std::clamp((segment_ray_dot * ray_origin_dot - segment_origin_dot) / denominator, 0.0f, 1.0f);
	}
	ray_factor = segment_ray_dot * segment_factor + ray_origin_dot;
	if (ray_factor < 0.0f) {
		ray_factor = 0.0f;
		segment_factor = std::clamp(-segment_origin_dot / segment_length_sq, 0.0f, 1.0f);
	}
	const godot::Vector3 segment_point = a + segment * segment_factor;
	const godot::Vector3 ray_point = ray.origin + ray.direction * ray_factor;
	ray_depth = ray_factor;
	return segment_point.distance_to(ray_point);
}

PickHit pick_face_target(const std::vector<modeling::MeshObjectSnapshot> &objects, const Ray &ray,
		SelectionKind kind) {
	PickHit best;
	best.target.kind = kind;
	for (const modeling::MeshObjectSnapshot &object : objects) {
		for (const AuthoredPolygonFacePayload &face : object.authored.faces) {
			if (face.vertices.size() < 3U) {
				continue;
			}
			const std::optional<godot::Vector3> origin = vertex_position(object.authored, face.vertices.front());
			if (!origin.has_value()) {
				continue;
			}
			for (std::size_t index = 1; index + 1U < face.vertices.size(); ++index) {
				const std::optional<godot::Vector3> b = vertex_position(object.authored, face.vertices[index]);
				const std::optional<godot::Vector3> c = vertex_position(object.authored, face.vertices[index + 1U]);
				if (!b.has_value() || !c.has_value()) {
					continue;
				}
				float depth = 0.0f;
				if (ray_triangle_intersection(ray, *origin, *b, *c, depth) && depth < best.depth) {
					best.hit = true;
					best.depth = depth;
					best.distance = 0.0f;
					best.target.object = object.object;
					if (kind == SelectionKind::Object) {
						best.target.kind = SelectionKind::Object;
					} else {
						best.target.kind = SelectionKind::Face;
						best.target.face = face.id;
					}
				}
			}
		}
	}
	return best;
}

PickHit pick_face_target(const modeling::MeshObjectSnapshot &object, const Ray &ray, SelectionKind kind) {
	PickHit best;
	best.target.kind = kind;
	for (const AuthoredPolygonFacePayload &face : object.authored.faces) {
		if (face.vertices.size() < 3U) {
			continue;
		}
		const std::optional<godot::Vector3> origin = vertex_position(object.authored, face.vertices.front());
		if (!origin.has_value()) {
			continue;
		}
		for (std::size_t index = 1; index + 1U < face.vertices.size(); ++index) {
			const std::optional<godot::Vector3> b = vertex_position(object.authored, face.vertices[index]);
			const std::optional<godot::Vector3> c = vertex_position(object.authored, face.vertices[index + 1U]);
			if (!b.has_value() || !c.has_value()) {
				continue;
			}
			float depth = 0.0f;
			if (ray_triangle_intersection(ray, *origin, *b, *c, depth) && depth < best.depth) {
				best.hit = true;
				best.depth = depth;
				best.distance = 0.0f;
				best.target.object = object.object;
				if (kind == SelectionKind::Object) {
					best.target.kind = SelectionKind::Object;
				} else {
					best.target.kind = SelectionKind::Face;
					best.target.face = face.id;
				}
			}
		}
	}
	return best;
}

bool component_pick_occluded(float component_depth, const PickHit &surface_hit,
		const modeling::MeshObjectSnapshot &object, SelectionKind kind, VertexId vertex = {}, EdgeKey edge = {}) {
	if (component_selected(object, kind, vertex, edge)) {
		return false;
	}
	if (!surface_hit.hit || component_depth <= surface_hit.depth + kPickOcclusionTolerance) {
		return false;
	}
	if (same_object(surface_hit.target.object, object.object) && surface_hit.target.kind == SelectionKind::Face) {
		if (const AuthoredPolygonFacePayload *face = find_face(object.authored, surface_hit.target.face)) {
			if (kind == SelectionKind::Vertex && face_contains_vertex(*face, vertex)) {
				return false;
			}
			if (kind == SelectionKind::Edge && face_contains_edge(*face, edge)) {
				return false;
			}
		}
	}
	return true;
}

PickHit pick_vertex_target(const std::vector<modeling::MeshObjectSnapshot> &objects, const Ray &ray, float radius,
		ObjectId source_object) {
	PickHit best;
	best.target.kind = SelectionKind::Vertex;
	for (const modeling::MeshObjectSnapshot &object : objects) {
		const PickHit surface_hit = pick_face_target(object, ray, SelectionKind::Face);
		for (VertexId vertex : object.selected_vertices) {
			const std::optional<godot::Vector3> position = vertex_position(object.authored, vertex);
			if (!position.has_value()) {
				continue;
			}
			float depth = 0.0f;
			const float distance = point_ray_distance(*position, ray, depth);
			if (component_pick_occluded(depth, surface_hit, object, SelectionKind::Vertex, vertex)) {
				continue;
			}
			if (distance <= radius * 1.35f &&
					(distance < best.distance || (distance == best.distance && depth < best.depth))) {
				best.hit = true;
				best.distance = distance;
				best.depth = depth;
				best.target.object = object.object;
				best.target.vertex = vertex;
			}
		}
	}
	if (best.hit) {
		return best;
	}
	for (const modeling::MeshObjectSnapshot &object : objects) {
		const bool source_wire_object = same_object(object.object, source_object);
		if (source_object.valid() && !source_wire_object) {
			continue;
		}
		const PickHit surface_hit = pick_face_target(object, ray, SelectionKind::Face);
		for (std::size_t index = 0; index < object.authored.vertices.size() && index < object.authored.positions.size(); ++index) {
			float depth = 0.0f;
			const float distance = point_ray_distance(to_godot(object.authored.positions[index]), ray, depth);
			if (component_pick_occluded(depth, surface_hit, object, SelectionKind::Vertex,
						object.authored.vertices[index])) {
				continue;
			}
			if (distance <= radius && (distance < best.distance || (distance == best.distance && depth < best.depth))) {
				best.hit = true;
				best.distance = distance;
				best.depth = depth;
				best.target.object = object.object;
				best.target.vertex = object.authored.vertices[index];
			}
		}
	}
	return best;
}

PickHit pick_edge_target(const std::vector<modeling::MeshObjectSnapshot> &objects, const Ray &ray, float radius,
		ObjectId source_object) {
	PickHit best;
	best.target.kind = SelectionKind::Edge;
	for (const modeling::MeshObjectSnapshot &object : objects) {
		const PickHit surface_hit = pick_face_target(object, ray, SelectionKind::Face);
		for (const EdgeKey &edge : object.selected_edges) {
			const std::optional<godot::Vector3> a = vertex_position(object.authored, edge.a);
			const std::optional<godot::Vector3> b = vertex_position(object.authored, edge.b);
			if (!a.has_value() || !b.has_value()) {
				continue;
			}
			float depth = 0.0f;
			const float distance = segment_ray_distance(*a, *b, ray, depth);
			if (component_pick_occluded(depth, surface_hit, object, SelectionKind::Edge, {}, edge)) {
				continue;
			}
			if (distance <= radius * 1.35f &&
					(distance < best.distance || (distance == best.distance && depth < best.depth))) {
				best.hit = true;
				best.distance = distance;
				best.depth = depth;
				best.target.object = object.object;
				best.target.edge = edge;
			}
		}
	}
	if (best.hit) {
		return best;
	}
	for (const modeling::MeshObjectSnapshot &object : objects) {
		const bool source_wire_object = same_object(object.object, source_object);
		if (source_object.valid() && !source_wire_object) {
			continue;
		}
		const PickHit surface_hit = pick_face_target(object, ray, SelectionKind::Face);
		for (const EdgeKey &edge : object.edges) {
			const std::optional<godot::Vector3> a = vertex_position(object.authored, edge.a);
			const std::optional<godot::Vector3> b = vertex_position(object.authored, edge.b);
			if (!a.has_value() || !b.has_value()) {
				continue;
			}
			float depth = 0.0f;
			const float distance = segment_ray_distance(*a, *b, ray, depth);
			if (component_pick_occluded(depth, surface_hit, object, SelectionKind::Edge, {}, edge)) {
				continue;
			}
			if (distance <= radius && (distance < best.distance || (distance == best.distance && depth < best.depth))) {
				best.hit = true;
				best.distance = distance;
				best.depth = depth;
				best.target.object = object.object;
				best.target.edge = edge;
			}
		}
	}
	return best;
}

SelectionEdit edit_from_modifiers(bool shift, bool remove) {
	if (remove) {
		return SelectionEdit::Remove;
	}
	if (shift) {
		return SelectionEdit::Add;
	}
	return SelectionEdit::Replace;
}

ObjectId component_source_object(const std::vector<modeling::MeshObjectSnapshot> &objects,
		const std::optional<modeling::SelectionTarget> &hover_target) {
	for (const modeling::MeshObjectSnapshot &object : objects) {
		if (object.active && (object.selected || has_component_selection(object))) {
			return object.object;
		}
	}
	for (const modeling::MeshObjectSnapshot &object : objects) {
		if (has_component_selection(object)) {
			return object.object;
		}
	}
	if (hover_target.has_value() && hover_target->kind != SelectionKind::Object) {
		return hover_target->object;
	}
	for (const modeling::MeshObjectSnapshot &object : objects) {
		if (object.active) {
			return object.object;
		}
	}
	for (const modeling::MeshObjectSnapshot &object : objects) {
		if (object.selected) {
			return object.object;
		}
	}
	return {};
}

godot::MeshInstance3D *make_overlay_instance(const char *name, godot::Node3D *parent) {
	auto *instance = memnew(godot::MeshInstance3D);
	instance->set_name(name);
	parent->add_child(instance);
	return instance;
}

void set_overlay_mesh(godot::MeshInstance3D *instance, const godot::Ref<godot::ArrayMesh> &mesh,
		const godot::Ref<godot::Material> &material) {
	if (instance == nullptr) {
		return;
	}
	instance->set_mesh(mesh);
	if (mesh.is_valid() && mesh->get_surface_count() > 0) {
		instance->set_surface_override_material(0, material);
	}
}

} // namespace

void QuaderViewportControl::_bind_methods() {}

void QuaderViewportControl::release_mouse_capture() {
	orbiting_ = false;
	panning_ = false;
	end_transform_drag();
	if (fly_active_) {
		end_fly();
	}
}

void QuaderViewportControl::_notification(int what) {
	if (what == NOTIFICATION_READY) {
		build_viewport();
		set_process(true);
		update_camera();
		refresh_overlays_if_dirty();
		grab_focus();
		return;
	}
	if (what == NOTIFICATION_RESIZED) {
		update_subviewport_size();
		invalidate_overlays();
	}
	if (what == NOTIFICATION_EXIT_TREE) {
		release_mouse_capture();
	}
}

void QuaderViewportControl::_gui_input(const godot::Ref<godot::InputEvent> &event) {
	godot::Ref<godot::InputEventMouseButton> mouse_button = event;
	if (mouse_button.is_valid()) {
		const godot::MouseButton button = mouse_button->get_button_index();
		if (button == godot::MOUSE_BUTTON_LEFT) {
			if (mouse_button->is_pressed()) {
				grab_focus();
				if (begin_transform_drag(mouse_button->get_position())) {
					accept_event();
					return;
				}
				const bool remove = mouse_button->is_ctrl_pressed() || mouse_button->is_meta_pressed();
				const SelectionEdit edit = edit_from_modifiers(mouse_button->is_shift_pressed(), remove);
				static_cast<void>(select_at(mouse_button->get_position(), edit));
				update_transform_gizmo_hover(mouse_button->get_position());
				if (gizmo_hover_axis_ == render::TransformGizmoAxis::None) {
					update_hover(mouse_button->get_position(), remove);
				}
			} else {
				end_transform_drag();
			}
			accept_event();
			return;
		}
		if (button == godot::MOUSE_BUTTON_MIDDLE) {
			if (mouse_button->is_pressed()) {
				panning_ = mouse_button->is_shift_pressed() || keyboard_shift_pressed();
				orbiting_ = !panning_;
				grab_focus();
			} else {
				orbiting_ = false;
				panning_ = false;
			}
			accept_event();
			return;
		}
		if (button == godot::MOUSE_BUTTON_RIGHT) {
			if (mouse_button->is_pressed()) {
				begin_fly();
			} else {
				end_fly();
			}
			accept_event();
			return;
		}
		if (button == godot::MOUSE_BUTTON_WHEEL_UP && mouse_button->is_pressed()) {
			camera_controller_.zoom(1.0f);
			update_camera();
			accept_event();
			return;
		}
		if (button == godot::MOUSE_BUTTON_WHEEL_DOWN && mouse_button->is_pressed()) {
			camera_controller_.zoom(-1.0f);
			update_camera();
			accept_event();
			return;
		}
	}

	godot::Ref<godot::InputEventMouseMotion> mouse_motion = event;
	if (mouse_motion.is_valid()) {
		const godot::Vector2 relative = mouse_motion->get_relative();
		const godot::Vector2 quader_delta{-relative.x, relative.y};
		const bool shift_pressed = mouse_motion->is_shift_pressed() || keyboard_shift_pressed();
		if (transform_drag_active_) {
			update_transform_drag(mouse_motion->get_position());
			accept_event();
			return;
		}
		if (panning_ || (orbiting_ && shift_pressed)) {
			panning_ = true;
			orbiting_ = false;
			camera_controller_.pan(relative, std::max(get_size().y, 1.0f));
			update_camera();
			accept_event();
			return;
		}
		if (orbiting_) {
			camera_controller_.orbit(quader_delta);
			update_camera();
			accept_event();
			return;
		}
		if (fly_active_) {
			camera_controller_.fly_look(quader_delta);
			update_camera();
			accept_event();
			return;
		}
		update_transform_gizmo_hover(mouse_motion->get_position());
		if (gizmo_hover_axis_ != render::TransformGizmoAxis::None) {
			clear_hover();
			accept_event();
			return;
		}
		update_hover(mouse_motion->get_position(), mouse_motion->is_ctrl_pressed() || keyboard_remove_pressed());
	}

	godot::Ref<godot::InputEventKey> key = event;
	if (key.is_valid()) {
		if (remove_modifier_key(key->get_keycode())) {
			invalidate_overlays();
			accept_event();
			return;
		}
		if (!key->is_pressed()) {
			return;
		}
		if (fly_active_) {
			if (key->get_keycode() == godot::KEY_ESCAPE) {
				end_fly();
				accept_event();
			}
			return;
		}
		if (const std::optional<render::TransformGizmoTool> tool = transform_tool_for_key(key->get_keycode())) {
			set_transform_tool(*tool);
			accept_event();
			return;
		}
		if (const std::optional<SelectionMode> mode = selection_mode_for_key(key->get_keycode())) {
			selection_mode_ = *mode;
			clear_hover();
			gizmo_hover_axis_ = render::TransformGizmoAxis::None;
			invalidate_overlays();
			accept_event();
			return;
		}
		const int grid_preset = grid_preset_for_key(key->get_keycode());
		if (grid_preset != 0) {
			set_grid_preset(grid_preset);
			accept_event();
			return;
		}
		if (key->get_keycode() == godot::KEY_F && selection_mode_ == SelectionMode::Mesh) {
			const quader::modeling::OperationReceipt receipt = modeling_.flip_selected_mesh_normals();
			if (receipt.success && receipt.changed) {
				refresh_scene_meshes();
			}
			invalidate_overlays();
			accept_event();
			return;
		}
	}
}

void QuaderViewportControl::_process(double delta) {
	if (fly_active_) {
		handle_keyboard(delta);
		update_camera();
	}
	refresh_overlays_if_dirty();
}

const render::ViewportVisualSettings &QuaderViewportControl::visual_settings() const {
	return visual_settings_;
}

int QuaderViewportControl::grid_preset() const {
	return grid_preset_;
}

void QuaderViewportControl::set_visual_settings(const render::ViewportVisualSettings &settings) {
	visual_settings_ = settings;
	render::apply_ground_grid_settings(grid_material_, visual_settings_);
	render::apply_default_quader_material_settings(mesh_material_, visual_settings_);
	render::apply_environment_settings(environment_, visual_settings_);
	if (world_environment_ != nullptr) {
		world_environment_->set_environment(environment_);
	}
	invalidate_overlays();
}

void QuaderViewportControl::build_viewport() {
	if (built_) {
		return;
	}
	built_ = true;

	set_name("QuaderViewportControl");
	set_focus_mode(godot::Control::FOCUS_ALL);
	set_mouse_filter(godot::Control::MOUSE_FILTER_STOP);
	set_anchors_preset(godot::Control::PRESET_FULL_RECT);

	viewport_container_ = memnew(godot::SubViewportContainer);
	viewport_container_->set_name("ViewportContainer");
	viewport_container_->set_anchors_preset(godot::Control::PRESET_FULL_RECT);
	viewport_container_->set_stretch(false);
	viewport_container_->set_mouse_target(true);
	add_child(viewport_container_);

	subviewport_ = memnew(godot::SubViewport);
	subviewport_->set_name("QuaderSubViewport");
	subviewport_->set_update_mode(godot::SubViewport::UPDATE_ONCE);
	subviewport_->set_clear_mode(godot::SubViewport::CLEAR_MODE_ALWAYS);
	subviewport_->set_transparent_background(false);
	subviewport_->set_use_own_world_3d(true);
	subviewport_->set_msaa_3d(godot::Viewport::MSAA_4X);
	subviewport_->set_screen_space_aa(godot::Viewport::SCREEN_SPACE_AA_FXAA);
	subviewport_->set_use_taa(false);
	environment_ = render::make_environment(visual_settings_);
	subviewport_->set_world_3d(render::make_world(environment_));
	viewport_container_->add_child(subviewport_);
	update_subviewport_size();

	scene_root_ = memnew(godot::Node3D);
	scene_root_->set_name("QuaderScene");
	subviewport_->add_child(scene_root_);
	world_environment_ = render::make_world_environment(environment_);
	scene_root_->add_child(world_environment_);
	grid_material_ = render::make_ground_grid_material(visual_settings_);
	scene_root_->add_child(render::make_ground_grid(grid_material_));

	mesh_material_ = render::make_default_quader_material(visual_settings_);
	refresh_scene_meshes();

	overlay_root_ = memnew(godot::Node3D);
	overlay_root_->set_name("QuaderOverlays");
	scene_root_->add_child(overlay_root_);
	selection_face_overlay_ = make_overlay_instance("SelectionFaceOverlay", overlay_root_);
	hover_face_overlay_ = make_overlay_instance("HoverFaceOverlay", overlay_root_);
	source_wire_overlay_ = make_overlay_instance("SourceWireOverlay", overlay_root_);
	selection_wire_overlay_ = make_overlay_instance("SelectionWireOverlay", overlay_root_);
	hover_wire_overlay_ = make_overlay_instance("HoverWireOverlay", overlay_root_);
	vertex_overlay_ = make_overlay_instance("VertexOverlay", overlay_root_);
	selected_vertex_outline_overlay_ = make_overlay_instance("SelectedVertexOutlineOverlay", overlay_root_);
	selected_vertex_overlay_ = make_overlay_instance("SelectedVertexOverlay", overlay_root_);
	hover_vertex_outline_overlay_ = make_overlay_instance("HoverVertexOutlineOverlay", overlay_root_);
	hover_vertex_overlay_ = make_overlay_instance("HoverVertexOverlay", overlay_root_);
	transform_gizmo_triangle_overlay_ = make_overlay_instance("TransformGizmoTriangleOverlay", overlay_root_);
	transform_gizmo_line_overlay_ = make_overlay_instance("TransformGizmoLineOverlay", overlay_root_);

	camera_ = render::make_camera();
	scene_root_->add_child(camera_);
	invalidate_overlays();
	refresh_overlays_if_dirty();
}

void QuaderViewportControl::update_subviewport_size() {
	if (subviewport_ == nullptr) {
		return;
	}
	const godot::Vector2 size = get_size();
	subviewport_->set_size({static_cast<int>(std::max(size.x, 1.0f)), static_cast<int>(std::max(size.y, 1.0f))});
}

void QuaderViewportControl::update_camera() {
	camera_controller_.apply_to(camera_);
	invalidate_overlays();
}

void QuaderViewportControl::refresh_scene_meshes() {
	if (scene_root_ == nullptr || mesh_material_.is_null()) {
		return;
	}

	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.objects();
	bool changed = false;
	for (const modeling::MeshObjectSnapshot &object : objects) {
		auto found = std::find_if(scene_meshes_.begin(), scene_meshes_.end(),
				[&](const SceneMeshNode &node) { return same_object(node.object, object.object); });
		if (found == scene_meshes_.end()) {
			auto *instance = memnew(godot::MeshInstance3D);
			instance->set_name(godot::String(object.name.c_str()));
			scene_root_->add_child(instance);
			scene_meshes_.push_back({
					.object = object.object,
					.instance = instance,
			});
			found = std::prev(scene_meshes_.end());
		}
		if (found->content_revision != object.mesh.content_revision && found->instance != nullptr) {
			found->instance->set_mesh(render::make_array_mesh(object.mesh));
			found->instance->set_surface_override_material(0, mesh_material_);
			found->content_revision = object.mesh.content_revision;
			changed = true;
		}
	}
	if (changed) {
		request_viewport_redraw();
	}
}

void QuaderViewportControl::refresh_overlays() {
	if (!built_ || camera_ == nullptr || selection_face_overlay_ == nullptr) {
		return;
	}

	const godot::Vector2 viewport_size = get_size();
	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.overlay_objects();
	std::vector<render::OverlayTriangle> selection_faces;
	std::vector<render::OverlayTriangle> hover_faces;
	std::vector<render::OverlaySegment> source_wire;
	std::vector<render::OverlaySegment> selection_wire;
	std::vector<render::OverlaySegment> hover_wire;
	std::vector<godot::Vector3> vertex_points;
	std::vector<godot::Vector3> selected_vertex_points;
	std::vector<godot::Vector3> hover_vertex_points;
	bool hover_target_selected = false;
	const bool component_mode = selection_mode_ != SelectionMode::Mesh;
	const std::optional<modeling::SelectionTarget> source_hover = has_hover_ ? std::optional{hover_target_} : std::nullopt;
	const ObjectId source_object = component_mode ? component_source_object(objects, source_hover) : ObjectId{};
	bool source_component_draw_on_top = false;

	for (const modeling::MeshObjectSnapshot &object : objects) {
		if (selection_mode_ == SelectionMode::Mesh && object.mesh_selected) {
			append_all_face_triangles(object.authored, selection_faces);
			append_all_edge_segments(object, selection_wire);
		}

		if (component_mode && object.object == source_object) {
			append_all_edge_segments(object, source_wire);
			source_component_draw_on_top = authored_faces_are_inside_out(object.authored);
			if (selection_mode_ == SelectionMode::Vertex) {
				std::vector<godot::Vector3> object_vertices = all_vertex_points(object.authored);
				vertex_points.insert(vertex_points.end(), object_vertices.begin(), object_vertices.end());
			}
		}

		if (component_mode) {
			if (selection_mode_ == SelectionMode::Vertex) {
				for (VertexId vertex : object.selected_vertices) {
					if (const std::optional<godot::Vector3> position = vertex_position(object.authored, vertex)) {
						selected_vertex_points.push_back(*position);
					}
				}
			}
			for (EdgeKey edge : object.selected_edges) {
				append_edge_segment(object, edge, selection_wire);
			}
			for (FaceId face_id : object.selected_faces) {
				if (const AuthoredPolygonFacePayload *face = find_face(object.authored, face_id)) {
					append_face_triangles(object.authored, *face, selection_faces);
					append_face_segments(object.authored, *face, selection_wire);
				}
			}
		}

		if (has_hover_ && same_object(object.object, hover_target_.object)) {
			hover_target_selected = target_selected(object, hover_target_);
			if (hover_target_.kind == SelectionKind::Object && selection_mode_ != SelectionMode::Mesh) {
				append_all_face_triangles(object.authored, hover_faces);
				append_all_edge_segments(object, hover_wire);
			} else if (selection_mode_ == SelectionMode::Vertex && hover_target_.kind == SelectionKind::Vertex) {
				if (const std::optional<godot::Vector3> position = vertex_position(object.authored, hover_target_.vertex)) {
					hover_vertex_points.push_back(*position);
				}
			} else if (hover_target_.kind == SelectionKind::Edge) {
				append_edge_segment(object, hover_target_.edge, hover_wire);
			} else if (const AuthoredPolygonFacePayload *face = find_face(object.authored, hover_target_.face)) {
				append_face_triangles(object.authored, *face, hover_faces);
				append_face_segments(object.authored, *face, hover_wire);
			}
		}
	}

	const bool remove_preview = component_mode && hover_target_.kind != SelectionKind::Object &&
			(hover_remove_preview_ || keyboard_remove_pressed()) && hover_target_selected;
	const bool draw_on_top = !component_mode;
	const bool source_wire_draw_on_top = draw_on_top || source_component_draw_on_top;
	const bool vertex_draw_on_top = draw_on_top || source_component_draw_on_top;
	const float source_line_depth_bias =
			(component_mode && !source_component_draw_on_top) ? kOverlayLineDepthBiasPixels : 0.0f;
	const float selected_line_depth_bias = component_mode ? kOverlayLineDepthBiasPixels : 0.0f;
	const float hover_line_depth_bias = component_mode ? kOverlayLineDepthBiasPixels : 0.0f;
	const float source_point_depth_bias = component_mode ? kOverlayPointDepthBiasPixels : 0.0f;
	const float selected_point_depth_bias = component_mode ? kOverlayPointDepthBiasPixels : 0.0f;
	const float hover_point_depth_bias = component_mode ? kOverlayPointDepthBiasPixels : 0.0f;
	const float source_clip_depth_bias = 0.0f;
	const float selected_clip_depth_bias = 0.0f;
	const float hover_clip_depth_bias = 0.0f;
	const godot::Color hover_face_color = remove_preview ? visual_settings_.remove_face_color
																: visual_settings_.hover_face_color;
	const godot::Color hover_wire_color = remove_preview ? visual_settings_.remove_wire_color
																: visual_settings_.hover_wire_color;
	const godot::Color hover_vertex_color = remove_preview ? visual_settings_.remove_vertex_color
																  : visual_settings_.hover_vertex_color;

	set_overlay_mesh(selection_face_overlay_, render::make_overlay_face_mesh(selection_faces),
			render::make_overlay_face_material(visual_settings_.selection_face_color, draw_on_top,
					kSelectedFaceRenderPriority));
	set_overlay_mesh(hover_face_overlay_, render::make_overlay_face_mesh(hover_faces),
			render::make_overlay_face_material(hover_face_color, draw_on_top, kHoverFaceRenderPriority));
	set_overlay_mesh(source_wire_overlay_,
			render::make_overlay_line_mesh(source_wire, camera_, viewport_size, visual_settings_.source_wire_line_size,
					source_line_depth_bias),
			render::make_overlay_line_material(visual_settings_.source_wire_color, source_wire_draw_on_top,
					kSourceWireRenderPriority, source_clip_depth_bias));
	set_overlay_mesh(selection_wire_overlay_,
			render::make_overlay_line_mesh(selection_wire, camera_, viewport_size,
					selection_mode_ == SelectionMode::Edge ? visual_settings_.selection_edge_line_size
														   : visual_settings_.selection_face_wire_line_size,
					selected_line_depth_bias),
			render::make_overlay_line_material(visual_settings_.selection_wire_color, draw_on_top,
					kSelectedWireRenderPriority, selected_clip_depth_bias));
	set_overlay_mesh(hover_wire_overlay_,
			render::make_overlay_line_mesh(hover_wire, camera_, viewport_size, visual_settings_.hover_wire_line_size,
					hover_line_depth_bias),
			render::make_overlay_line_material(hover_wire_color, draw_on_top, kHoverRenderPriority, hover_clip_depth_bias));

	set_overlay_mesh(vertex_overlay_,
			render::make_overlay_point_mesh(vertex_points, camera_, viewport_size, visual_settings_.vertex_size,
					vertex_draw_on_top ? 0.0f : source_point_depth_bias),
			render::make_overlay_point_material(visual_settings_.vertex_color, vertex_draw_on_top,
					kVertexBaseRenderPriority,
					source_clip_depth_bias));
	set_overlay_mesh(selected_vertex_outline_overlay_,
			render::make_overlay_point_mesh(selected_vertex_points, camera_, viewport_size,
					visual_settings_.vertex_size + visual_settings_.selected_vertex_growth +
							visual_settings_.vertex_outline_size,
					vertex_draw_on_top ? 0.0f : selected_point_depth_bias),
			render::make_overlay_point_material(visual_settings_.vertex_outline_color, vertex_draw_on_top,
					kSelectedVertexOutlineRenderPriority, selected_clip_depth_bias));
	set_overlay_mesh(selected_vertex_overlay_,
			render::make_overlay_point_mesh(selected_vertex_points, camera_, viewport_size,
					visual_settings_.vertex_size + visual_settings_.selected_vertex_growth,
					vertex_draw_on_top ? 0.0f : selected_point_depth_bias),
			render::make_overlay_point_material(visual_settings_.selected_vertex_color, vertex_draw_on_top,
					kVertexSelectedRenderPriority, selected_clip_depth_bias));
	set_overlay_mesh(hover_vertex_outline_overlay_,
			render::make_overlay_point_mesh(hover_vertex_points, camera_, viewport_size,
					visual_settings_.vertex_size + visual_settings_.hover_vertex_growth +
							visual_settings_.vertex_outline_size,
					vertex_draw_on_top ? 0.0f : hover_point_depth_bias),
			render::make_overlay_point_material(visual_settings_.vertex_outline_color, vertex_draw_on_top,
					kHoverVertexOutlineRenderPriority, hover_clip_depth_bias));
	set_overlay_mesh(hover_vertex_overlay_,
			render::make_overlay_point_mesh(hover_vertex_points, camera_, viewport_size,
					visual_settings_.vertex_size + visual_settings_.hover_vertex_growth,
					vertex_draw_on_top ? 0.0f : hover_point_depth_bias),
			render::make_overlay_point_material(hover_vertex_color, vertex_draw_on_top, kVertexHoverRenderPriority,
					hover_clip_depth_bias));

	render::TransformGizmoMeshes gizmo_meshes = render::make_transform_gizmo_meshes(transform_gizmo_input(objects));
	set_overlay_mesh(transform_gizmo_triangle_overlay_, gizmo_meshes.triangles,
			render::make_transform_gizmo_triangle_material());
	set_overlay_mesh(transform_gizmo_line_overlay_, gizmo_meshes.lines, render::make_transform_gizmo_line_material());
}

void QuaderViewportControl::refresh_overlays_if_dirty() {
	if (!overlays_dirty_) {
		return;
	}
	if (!built_ || camera_ == nullptr || selection_face_overlay_ == nullptr) {
		return;
	}
	overlays_dirty_ = false;
	refresh_overlays();
}

void QuaderViewportControl::invalidate_overlays() {
	overlays_dirty_ = true;
	request_viewport_redraw();
}

void QuaderViewportControl::request_viewport_redraw() {
	if (subviewport_ != nullptr) {
		subviewport_->set_update_mode(godot::SubViewport::UPDATE_ONCE);
	}
}

void QuaderViewportControl::clear_hover() {
	const bool changed = has_hover_ || hover_remove_preview_;
	has_hover_ = false;
	hover_remove_preview_ = false;
	hover_target_ = {};
	if (changed) {
		invalidate_overlays();
	}
}

void QuaderViewportControl::update_hover(godot::Vector2 position, bool remove_preview) {
	if (camera_ == nullptr) {
		clear_hover();
		return;
	}
	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.overlay_objects();
	const Ray ray = make_ray(camera_, position);
	const PickHit surface_hit = pick_face_target(objects, ray, SelectionKind::Face);
	const ObjectId source_object =
			selection_mode_ == SelectionMode::Mesh ? ObjectId{} : component_source_object(objects, std::nullopt);
	PickHit hit;
	if (selection_mode_ == SelectionMode::Mesh) {
		hit = pick_face_target(objects, ray, SelectionKind::Object);
	} else if (selection_mode_ == SelectionMode::Vertex) {
		hit = pick_vertex_target(objects, ray, visual_settings_.pick_vertex_radius, source_object);
	} else if (selection_mode_ == SelectionMode::Edge) {
		hit = pick_edge_target(objects, ray, visual_settings_.pick_edge_radius, source_object);
	} else {
		hit = surface_hit;
	}
	if (!hit.hit) {
		clear_hover();
		return;
	}
	const bool changed = !has_hover_ || hover_remove_preview_ != remove_preview ||
			!same_selection_target(hover_target_, hit.target);
	has_hover_ = true;
	hover_remove_preview_ = remove_preview;
	hover_target_ = hit.target;
	if (changed) {
		invalidate_overlays();
	}
}

bool QuaderViewportControl::select_at(godot::Vector2 position, SelectionEdit edit) {
	if (camera_ == nullptr) {
		return false;
	}
	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.overlay_objects();
	const Ray ray = make_ray(camera_, position);
	const PickHit surface_hit = pick_face_target(objects, ray, SelectionKind::Face);
	const ObjectId source_object =
			selection_mode_ == SelectionMode::Mesh ? ObjectId{} : component_source_object(objects, std::nullopt);
	PickHit hit;
	if (selection_mode_ == SelectionMode::Mesh) {
		hit = pick_face_target(objects, ray, SelectionKind::Object);
	} else if (selection_mode_ == SelectionMode::Vertex) {
		hit = pick_vertex_target(objects, ray, visual_settings_.pick_vertex_radius, source_object);
	} else if (selection_mode_ == SelectionMode::Edge) {
		hit = pick_edge_target(objects, ray, visual_settings_.pick_edge_radius, source_object);
	} else {
		hit = surface_hit;
	}

	if (!hit.hit) {
		if ((selection_mode_ == SelectionMode::Vertex || selection_mode_ == SelectionMode::Edge) && surface_hit.hit &&
				edit == SelectionEdit::Replace) {
			static_cast<void>(modeling_.activate_component_source(surface_hit.target.object));
			clear_hover();
			invalidate_overlays();
			return true;
		}
		if (edit == SelectionEdit::Replace) {
			static_cast<void>(modeling_.clear_selection());
			clear_hover();
			invalidate_overlays();
			return true;
		}
		return false;
	}

	static_cast<void>(modeling_.apply_selection(hit.target, edit));
	invalidate_overlays();
	return true;
}

void QuaderViewportControl::set_transform_tool(render::TransformGizmoTool tool) {
	if (transform_tool_ == tool) {
		return;
	}
	transform_tool_ = tool;
	gizmo_hover_axis_ = render::TransformGizmoAxis::None;
	gizmo_active_axis_ = render::TransformGizmoAxis::None;
	transform_drag_active_ = false;
	invalidate_overlays();
}

std::optional<godot::Vector3> QuaderViewportControl::selected_mesh_pivot(
		std::span<const modeling::MeshObjectSnapshot> objects) const {
	godot::Vector3 sum;
	std::size_t count = 0;
	for (const modeling::MeshObjectSnapshot &object : objects) {
		if (!object.mesh_selected) {
			continue;
		}
		for (quader::modeling::Vec3 position : object.authored.positions) {
			sum += to_godot(position);
			++count;
		}
	}
	if (count == 0U) {
		return std::nullopt;
	}
	return sum / static_cast<float>(count);
}

TransformDragBounds QuaderViewportControl::selected_mesh_bounds(
		std::span<const modeling::MeshObjectSnapshot> objects) const {
	TransformDragBounds bounds;
	for (const modeling::MeshObjectSnapshot &object : objects) {
		if (!object.mesh_selected) {
			continue;
		}
		for (quader::modeling::Vec3 position : object.authored.positions) {
			const godot::Vector3 point = to_godot(position);
			if (!bounds.has_bounds) {
				bounds.has_bounds = true;
				bounds.min = point;
				bounds.max = point;
				continue;
			}
			bounds.min.x = std::min(bounds.min.x, point.x);
			bounds.min.y = std::min(bounds.min.y, point.y);
			bounds.min.z = std::min(bounds.min.z, point.z);
			bounds.max.x = std::max(bounds.max.x, point.x);
			bounds.max.y = std::max(bounds.max.y, point.y);
			bounds.max.z = std::max(bounds.max.z, point.z);
		}
	}
	return bounds;
}

render::TransformGizmoInput QuaderViewportControl::transform_gizmo_input(
		std::span<const modeling::MeshObjectSnapshot> objects) const {
	render::TransformGizmoInput input;
	input.tool = transform_tool_;
	input.hover_axis = gizmo_hover_axis_;
	input.active_axis = gizmo_active_axis_;
	input.camera = camera_;
	input.viewport_size = get_size();
	input.has_selection = selection_mode_ == SelectionMode::Mesh;
	if (const std::optional<godot::Vector3> pivot = selected_mesh_pivot(objects)) {
		input.has_pivot = true;
		input.pivot = *pivot;
	}
	return input;
}

bool QuaderViewportControl::begin_transform_drag(godot::Vector2 position) {
	if (selection_mode_ != SelectionMode::Mesh || transform_tool_ == render::TransformGizmoTool::None ||
			camera_ == nullptr) {
		return false;
	}
	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.overlay_objects();
	render::TransformGizmoInput input = transform_gizmo_input(objects);
	render::TransformGizmoPickHit hit = render::pick_transform_gizmo(input, position);
	if (!hit.hit) {
		return false;
	}
	transform_drag_active_ = true;
	gizmo_active_axis_ = hit.axis;
	gizmo_hover_axis_ = hit.axis;
	transform_drag_last_position_ = position;
	transform_drag_start_pivot_ = input.pivot;
	transform_drag_pivot_ = input.pivot;
	transform_drag_unsnapped_move_ = {};
	transform_drag_applied_move_ = {};
	transform_drag_bounds_ = selected_mesh_bounds(objects);
	transform_drag_unsnapped_angle_ = 0.0f;
	transform_drag_applied_angle_ = 0.0f;
	transform_drag_unsnapped_scale_amount_ = 0.0f;
	transform_drag_applied_scale_factor_ = 1.0f;
	clear_hover();
	invalidate_overlays();
	return true;
}

void QuaderViewportControl::update_transform_drag(godot::Vector2 position) {
	if (!transform_drag_active_ || camera_ == nullptr || gizmo_active_axis_ == render::TransformGizmoAxis::None) {
		return;
	}
	const godot::Vector2 delta = position - transform_drag_last_position_;
	if (delta.length_squared() <= 0.0001f) {
		return;
	}

	quader::modeling::OperationReceipt receipt;
	const bool snap_enabled = !keyboard_snap_disabled();
	if (transform_tool_ == render::TransformGizmoTool::Move) {
		godot::Vector3 world_delta;
		if (transform_axis_is_plane(gizmo_active_axis_)) {
			const std::array<godot::Vector3, 2> axes = transform_plane_axes(gizmo_active_axis_);
			const godot::Vector2 screen_pivot = camera_->unproject_position(transform_drag_pivot_);
			const godot::Vector2 screen_a = camera_->unproject_position(transform_drag_pivot_ + axes[0]) - screen_pivot;
			const godot::Vector2 screen_b = camera_->unproject_position(transform_drag_pivot_ + axes[1]) - screen_pivot;
			const float determinant = screen_a.x * screen_b.y - screen_a.y * screen_b.x;
			if (std::abs(determinant) > 0.0001f) {
				const float a = (delta.x * screen_b.y - delta.y * screen_b.x) / determinant;
				const float b = (screen_a.x * delta.y - screen_a.y * delta.x) / determinant;
				world_delta = axes[0] * a + axes[1] * b;
			}
		} else {
			const godot::Vector3 axis = transform_axis_vector(gizmo_active_axis_);
			const godot::Vector2 screen_axis =
					camera_->unproject_position(transform_drag_pivot_ + axis) - camera_->unproject_position(transform_drag_pivot_);
			if (screen_axis.length_squared() > 0.0001f) {
				const float world_units = viewport_world_units_per_pixel_at(camera_, get_size(), transform_drag_pivot_);
				world_delta = axis * (delta.dot(screen_axis.normalized()) * world_units);
			}
		}
		transform_drag_unsnapped_move_ += world_delta;
		const godot::Vector3 target_move = snap_enabled
				? snap_center_drag_delta(gizmo_active_axis_, transform_drag_start_pivot_, transform_drag_unsnapped_move_,
						  visual_settings_.grid_world_size)
				: transform_drag_unsnapped_move_;
		const godot::Vector3 apply_delta = target_move - transform_drag_applied_move_;
		if (apply_delta.length_squared() > kToolEpsilon * kToolEpsilon) {
			receipt = modeling_.translate_selected_meshes(to_modeling(apply_delta));
			if (receipt.success && receipt.changed) {
				transform_drag_applied_move_ = target_move;
				transform_drag_pivot_ = transform_drag_start_pivot_ + transform_drag_applied_move_;
			}
		}
	} else if (transform_tool_ == render::TransformGizmoTool::Rotate) {
		const godot::Vector2 screen_pivot = camera_->unproject_position(transform_drag_pivot_);
		const godot::Vector2 before = transform_drag_last_position_ - screen_pivot;
		const godot::Vector2 after = position - screen_pivot;
		if (before.length_squared() > 0.0001f && after.length_squared() > 0.0001f) {
			const float cross = before.x * after.y - before.y * after.x;
			const float dot = before.dot(after);
			transform_drag_unsnapped_angle_ += -std::atan2(cross, dot);
			const float target_angle = snap_enabled ? snap_to_step(transform_drag_unsnapped_angle_, kRotateSnapRadians)
													: transform_drag_unsnapped_angle_;
			const float apply_angle = target_angle - transform_drag_applied_angle_;
			if (std::abs(apply_angle) > kToolEpsilon) {
				receipt = modeling_.rotate_selected_meshes(axis_radians(gizmo_active_axis_, apply_angle),
						to_modeling(transform_drag_pivot_));
				if (receipt.success && receipt.changed) {
					transform_drag_applied_angle_ = target_angle;
				}
			}
		}
	} else if (transform_tool_ == render::TransformGizmoTool::Scale) {
		float amount = 0.0f;
		if (gizmo_active_axis_ == render::TransformGizmoAxis::All) {
			amount = delta.x - delta.y;
		} else if (transform_axis_is_plane(gizmo_active_axis_)) {
			const std::array<godot::Vector3, 2> axes = transform_plane_axes(gizmo_active_axis_);
			const godot::Vector2 screen_pivot = camera_->unproject_position(transform_drag_pivot_);
			const godot::Vector2 screen_a = camera_->unproject_position(transform_drag_pivot_ + axes[0]) - screen_pivot;
			const godot::Vector2 screen_b = camera_->unproject_position(transform_drag_pivot_ + axes[1]) - screen_pivot;
			godot::Vector2 screen_axis = screen_a.normalized() + screen_b.normalized();
			if (screen_axis.length_squared() > 0.0001f) {
				amount = delta.dot(screen_axis.normalized());
			}
		} else {
			const godot::Vector3 axis = transform_axis_vector(gizmo_active_axis_);
			const godot::Vector2 screen_axis =
					camera_->unproject_position(transform_drag_pivot_ + axis) - camera_->unproject_position(transform_drag_pivot_);
			if (screen_axis.length_squared() > 0.0001f) {
				amount = delta.dot(screen_axis.normalized());
			}
		}
		transform_drag_unsnapped_scale_amount_ += amount;
		float factor = std::max(kMinScaleFactor, 1.0f + transform_drag_unsnapped_scale_amount_ / kScalePixelsPerFactor);
		factor = snapped_scale_factor(gizmo_active_axis_, factor, transform_drag_bounds_, transform_drag_pivot_, snap_enabled,
				visual_settings_.grid_world_size);
		const float relative_factor = factor / std::max(transform_drag_applied_scale_factor_, kMinScaleFactor);
		if (std::abs(relative_factor - 1.0f) > kToolEpsilon) {
			receipt = modeling_.scale_selected_meshes(axis_scale(gizmo_active_axis_, relative_factor),
					to_modeling(transform_drag_pivot_));
			if (receipt.success && receipt.changed) {
				transform_drag_applied_scale_factor_ = factor;
			}
		}
	}

	transform_drag_last_position_ = position;
	if (receipt.success && receipt.changed) {
		refresh_scene_meshes();
	}
	invalidate_overlays();
}

void QuaderViewportControl::end_transform_drag() {
	if (!transform_drag_active_) {
		return;
	}
	transform_drag_active_ = false;
	gizmo_active_axis_ = render::TransformGizmoAxis::None;
	invalidate_overlays();
}

void QuaderViewportControl::update_transform_gizmo_hover(godot::Vector2 position) {
	if (selection_mode_ != SelectionMode::Mesh || transform_tool_ == render::TransformGizmoTool::None ||
			camera_ == nullptr || transform_drag_active_) {
		if (gizmo_hover_axis_ != render::TransformGizmoAxis::None && !transform_drag_active_) {
			gizmo_hover_axis_ = render::TransformGizmoAxis::None;
			invalidate_overlays();
		}
		return;
	}
	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.overlay_objects();
	const render::TransformGizmoPickHit hit = render::pick_transform_gizmo(transform_gizmo_input(objects), position);
	const render::TransformGizmoAxis next_axis = hit.hit ? hit.axis : render::TransformGizmoAxis::None;
	if (next_axis != gizmo_hover_axis_) {
		gizmo_hover_axis_ = next_axis;
		invalidate_overlays();
	}
}

void QuaderViewportControl::handle_keyboard(double delta) {
	godot::Input *input = godot::Input::get_singleton();
	if (input == nullptr) {
		return;
	}

	godot::Vector3 local_direction;
	if (input->is_physical_key_pressed(godot::KEY_W)) {
		local_direction.z += 1.0f;
	}
	if (input->is_physical_key_pressed(godot::KEY_S)) {
		local_direction.z -= 1.0f;
	}
	if (input->is_physical_key_pressed(godot::KEY_D)) {
		local_direction.x += 1.0f;
	}
	if (input->is_physical_key_pressed(godot::KEY_A)) {
		local_direction.x -= 1.0f;
	}
	if (input->is_physical_key_pressed(godot::KEY_E)) {
		local_direction.y += 1.0f;
	}
	if (input->is_physical_key_pressed(godot::KEY_Q)) {
		local_direction.y -= 1.0f;
	}

	const bool fast = input->is_key_pressed(godot::KEY_SHIFT);
	const bool slow = input->is_key_pressed(godot::KEY_CTRL);
	camera_controller_.fly_move(local_direction, delta, fast, slow);
}

void QuaderViewportControl::set_grid_preset(int preset) {
	const int next_preset = std::clamp(preset, 1, 10);
	const bool changed = next_preset != grid_preset_;
	grid_preset_ = next_preset;
	visual_settings_.grid_world_size = grid_world_size_for_preset(grid_preset_);
	render::apply_ground_grid_settings(grid_material_, visual_settings_);
	render::apply_default_quader_material_settings(mesh_material_, visual_settings_);
	request_viewport_redraw();
	if (changed && grid_preset_changed_callback_) {
		grid_preset_changed_callback_(grid_preset_);
	}
}

void QuaderViewportControl::set_grid_preset_changed_callback(std::function<void(int)> callback) {
	grid_preset_changed_callback_ = std::move(callback);
}

void QuaderViewportControl::begin_fly() {
	fly_active_ = true;
	orbiting_ = false;
	panning_ = false;
	grab_focus();
	godot::Input *input = godot::Input::get_singleton();
	if (input != nullptr) {
		input->set_mouse_mode(godot::Input::MOUSE_MODE_CAPTURED);
	}
}

void QuaderViewportControl::end_fly() {
	fly_active_ = false;
	godot::Input *input = godot::Input::get_singleton();
	if (input != nullptr && input->get_mouse_mode() == godot::Input::MOUSE_MODE_CAPTURED) {
		input->set_mouse_mode(godot::Input::MOUSE_MODE_VISIBLE);
	}
}

} // namespace quader_godot::viewport
