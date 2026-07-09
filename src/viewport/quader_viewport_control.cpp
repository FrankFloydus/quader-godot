#include "viewport/quader_viewport_control.h"

#include "gizmo/gizmo_registry.h"
#include "render/quader_godot_render_utils.h"
#include "render/quader_godot_selection_overlay.h"
#include "selection/component_source_policy.h"

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
#include <godot_cpp/variant/string.hpp>

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
using quader::modeling::OperationReceipt;
using quader::modeling::SelectionEdit;
using quader::modeling::SelectionKind;
using quader::modeling::Vec3;
using quader::modeling::VertexId;
using quader::modeling::make_edge_key;
using quader::editor::selection::ComponentSourceObjectState;
using quader::editor::selection::component_hover_candidate;
using quader::editor::selection::component_vertex_handle_objects;
using quader::editor::selection::component_source_wire_objects;

struct Ray {
	godot::Vector3 origin;
	godot::Vector3 direction;
};

struct PickHit {
	bool hit = false;
	float distance = std::numeric_limits<float>::max();
	float depth = std::numeric_limits<float>::max();
	godot::Vector3 position;
	godot::Vector3 normal{0.0f, 1.0f, 0.0f};
	modeling::SelectionTarget target;
};

constexpr float kPickOcclusionTolerance = 0.01f;
constexpr float kOverlayLineDepthBiasPixels = 1.0f;
constexpr float kOverlayPointDepthBiasPixels = 1.5f;
constexpr float kToolEpsilon = 0.000001f;
constexpr float kScreenEpsilon = 0.0001f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kBoxPreviewDashPixels = 8.0f;
constexpr float kBoxPreviewDashGapPixels = 5.0f;
constexpr float kBoxPreviewLineWidthPixels = 2.0f;
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
constexpr int kBoxPreviewRenderPriority = godot::Material::RENDER_PRIORITY_MAX - 3;

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

bool transform_gizmo_key(godot::Key key) {
	switch (key) {
	case godot::KEY_Q:
	case godot::KEY_W:
	case godot::KEY_R:
	case godot::KEY_S:
		return true;
	default:
		return false;
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

godot::Color box_preview_line_color() {
	return godot::Color(godot::String("#ffeb29d1"));
}

float safe_box_grid_size(float grid_size) {
	return std::isfinite(grid_size) && grid_size > kToolEpsilon ? grid_size : 1.0f;
}

std::pair<float, float> directional_snap_pair(float start, float end, float grid_size) {
	const float grid = safe_box_grid_size(grid_size);
	if (end >= start) {
		const float start_snap = std::floor(start / grid) * grid;
		float end_snap = std::ceil(end / grid) * grid;
		if (end_snap - start_snap <= kToolEpsilon) {
			end_snap = start_snap + grid;
		}
		return {start_snap, end_snap};
	}

	const float start_snap = std::ceil(start / grid) * grid;
	float end_snap = std::floor(end / grid) * grid;
	if (start_snap - end_snap <= kToolEpsilon) {
		end_snap = start_snap - grid;
	}
	return {start_snap, end_snap};
}

godot::Vector3 normalized_or(godot::Vector3 value, godot::Vector3 fallback) {
	if (value.length_squared() <= kToolEpsilon * kToolEpsilon) {
		return fallback;
	}
	return value.normalized();
}

godot::Vector3 project_point_to_plane(godot::Vector3 point, godot::Vector3 origin, godot::Vector3 normal) {
	const godot::Vector3 safe_normal = normalized_or(normal, {0.0f, 1.0f, 0.0f});
	return point - safe_normal * ((point - origin).dot(safe_normal));
}

BoxConstructionPlane make_box_construction_plane(godot::Vector3 origin, godot::Vector3 normal) {
	BoxConstructionPlane plane;
	plane.origin = origin;
	plane.normal = normalized_or(normal, {0.0f, 1.0f, 0.0f});
	plane.snap_origin = project_point_to_plane({}, plane.origin, plane.normal);

	godot::Vector3 preferred{1.0f, 0.0f, 0.0f};
	if (std::abs(plane.normal.dot(preferred)) > 0.9f) {
		preferred = {0.0f, 0.0f, 1.0f};
	}
	godot::Vector3 candidate_u = preferred - plane.normal * preferred.dot(plane.normal);
	if (candidate_u.length_squared() <= kToolEpsilon * kToolEpsilon) {
		const float ax = std::abs(plane.normal.x);
		const float ay = std::abs(plane.normal.y);
		const float az = std::abs(plane.normal.z);
		godot::Vector3 reference{1.0f, 0.0f, 0.0f};
		if (ay <= ax && ay <= az) {
			reference = {0.0f, 1.0f, 0.0f};
		} else if (az <= ax && az <= ay) {
			reference = {0.0f, 0.0f, 1.0f};
		}
		candidate_u = reference - plane.normal * reference.dot(plane.normal);
	}
	plane.axis_u = normalized_or(candidate_u, {1.0f, 0.0f, 0.0f});
	plane.axis_v = normalized_or(plane.normal.cross(plane.axis_u), {0.0f, 0.0f, -1.0f});
	return plane;
}

godot::Vector3 box_plane_point(const BoxConstructionPlane &plane, float u, float v) {
	return plane.snap_origin + plane.axis_u * u + plane.axis_v * v;
}

bool intersect_ray_plane(const Ray &ray, const BoxConstructionPlane &plane, godot::Vector3 &point) {
	const float denominator = ray.direction.dot(plane.normal);
	if (std::abs(denominator) <= kToolEpsilon) {
		return false;
	}
	const float distance = (plane.origin - ray.origin).dot(plane.normal) / denominator;
	if (!std::isfinite(distance) || distance < 0.0f) {
		return false;
	}
	point = ray.origin + ray.direction * distance;
	return true;
}

BoxToolFootprint make_box_tool_footprint(const BoxConstructionPlane &plane, godot::Vector3 raw_start,
		godot::Vector3 raw_end, float grid_size) {
	if (plane.normal.length_squared() <= kToolEpsilon * kToolEpsilon ||
			plane.axis_u.length_squared() <= kToolEpsilon * kToolEpsilon ||
			plane.axis_v.length_squared() <= kToolEpsilon * kToolEpsilon) {
		return {};
	}
	const godot::Vector3 raw_start_delta = raw_start - plane.snap_origin;
	const godot::Vector3 raw_end_delta = raw_end - plane.snap_origin;
	const std::pair<float, float> u =
			directional_snap_pair(raw_start_delta.dot(plane.axis_u), raw_end_delta.dot(plane.axis_u), grid_size);
	const std::pair<float, float> v =
			directional_snap_pair(raw_start_delta.dot(plane.axis_v), raw_end_delta.dot(plane.axis_v), grid_size);
	const float height = safe_box_grid_size(grid_size);
	BoxToolFootprint footprint;
	footprint.valid = true;
	footprint.corners[0] = box_plane_point(plane, u.first, v.first);
	footprint.corners[1] = box_plane_point(plane, u.first, v.second);
	footprint.corners[2] = box_plane_point(plane, u.second, v.second);
	footprint.corners[3] = box_plane_point(plane, u.second, v.first);
	const godot::Vector3 height_offset = plane.normal * height;
	for (std::size_t index = 0; index < 4U; ++index) {
		footprint.corners[index + 4U] = footprint.corners[index] + height_offset;
	}
	return footprint;
}

bool camera_can_see_point(const godot::Camera3D *camera, godot::Vector3 point) {
	const godot::Vector3 forward = -camera->get_global_transform().basis.get_column(2).normalized();
	const float depth = (point - camera->get_global_transform().origin).dot(forward);
	return depth > std::max(static_cast<float>(camera->get_near()), 0.0001f);
}

void append_dashed_box_preview_edge(const godot::Camera3D *camera, godot::Vector3 start, godot::Vector3 end,
		std::vector<render::OverlaySegment> &segments) {
	if (camera == nullptr || !camera_can_see_point(camera, start) || !camera_can_see_point(camera, end)) {
		return;
	}
	const godot::Vector2 screen_start = camera->unproject_position(start);
	const godot::Vector2 screen_end = camera->unproject_position(end);
	const godot::Vector2 screen_delta = screen_end - screen_start;
	const float edge_length = screen_delta.length();
	if (edge_length <= 0.001f) {
		return;
	}
	const godot::Vector3 world_delta = end - start;
	for (float offset = 0.0f; offset < edge_length; offset += kBoxPreviewDashPixels + kBoxPreviewDashGapPixels) {
		const float segment_end = std::min(offset + kBoxPreviewDashPixels, edge_length);
		const float start_t = offset / edge_length;
		const float end_t = segment_end / edge_length;
		segments.push_back({
				start + world_delta * start_t,
				start + world_delta * end_t,
		});
	}
}

void append_box_preview_segments(const godot::Camera3D *camera, const BoxToolFootprint &footprint,
		std::vector<render::OverlaySegment> &segments) {
	if (!footprint.valid) {
		return;
	}
	for (std::size_t index = 0; index < 4U; ++index) {
		append_dashed_box_preview_edge(camera, footprint.corners[index], footprint.corners[(index + 1U) % 4U], segments);
	}
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

Vec3 to_modeling(godot::Vector3 value) {
	return {value.x, value.y, value.z};
}

std::vector<ComponentSourceObjectState> component_source_states(
		const std::vector<modeling::MeshObjectSnapshot> &objects) {
	std::vector<ComponentSourceObjectState> states;
	states.reserve(objects.size());
	for (const modeling::MeshObjectSnapshot &object : objects) {
		states.push_back({
				.object = object.object,
				.selected = object.selected,
				.active = object.active,
				.has_selected_vertices = !object.selected_vertices.empty(),
				.has_selected_edges = !object.selected_edges.empty(),
				.has_selected_faces = !object.selected_faces.empty(),
		});
	}
	return states;
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
	const EdgeKey normalized = make_edge_key(edge.a, edge.b);
	if (!normalized.valid() || face.vertices.size() < 2U) {
		return false;
	}
	for (std::size_t index = 0; index < face.vertices.size(); ++index) {
		const VertexId a = face.vertices[index];
		const VertexId b = face.vertices[(index + 1U) % face.vertices.size()];
		if (make_edge_key(a, b) == normalized) {
			return true;
		}
	}
	return false;
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
					best.position = ray.origin + ray.direction * depth;
					const std::optional<godot::Vector3> normal = face_normal(object.authored, face);
					best.normal = normal.value_or(normalized_or((*b - *origin).cross(*c - *origin),
							{0.0f, 1.0f, 0.0f}));
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
				best.position = ray.origin + ray.direction * depth;
				const std::optional<godot::Vector3> normal = face_normal(object.authored, face);
				best.normal = normal.value_or(normalized_or((*b - *origin).cross(*c - *origin),
						{0.0f, 1.0f, 0.0f}));
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

bool component_selected(const modeling::MeshObjectSnapshot &object, SelectionKind kind, VertexId vertex, EdgeKey edge) {
	if (kind == SelectionKind::Vertex) {
		return contains_id<VertexId>(object.selected_vertices, vertex);
	}
	if (kind == SelectionKind::Edge) {
		return contains_id<EdgeKey>(object.selected_edges, edge);
	}
	return false;
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
		std::span<const ObjectId> source_objects, bool prefer_selected_target) {
	PickHit best_selected;
	best_selected.target.kind = SelectionKind::Vertex;
	PickHit best_unselected;
	best_unselected.target.kind = SelectionKind::Vertex;
	for (const modeling::MeshObjectSnapshot &object : objects) {
		const bool source_wire_object =
				object.selected || std::find(source_objects.begin(), source_objects.end(), object.object) != source_objects.end();
		if (!source_wire_object) {
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
			const bool selected = contains_id<VertexId>(object.selected_vertices, object.authored.vertices[index]);
			PickHit &best = selected ? best_selected : best_unselected;
			if (distance <= radius && (distance < best.distance || (distance == best.distance && depth < best.depth))) {
				best.hit = true;
				best.distance = distance;
				best.depth = depth;
				best.target.object = object.object;
				best.target.vertex = object.authored.vertices[index];
			}
		}
	}
	if (!best_selected.hit) {
		return best_unselected;
	}
	if (!best_unselected.hit) {
		return best_selected;
	}
	const float selected_tolerance = prefer_selected_target ? radius : std::max(kToolEpsilon, radius * 0.25f);
	return best_selected.distance <= best_unselected.distance + selected_tolerance ? best_selected : best_unselected;
}

PickHit pick_edge_target(const std::vector<modeling::MeshObjectSnapshot> &objects, const Ray &ray, float radius,
		std::span<const ObjectId> source_objects, bool prefer_selected_target) {
	PickHit best_selected;
	best_selected.target.kind = SelectionKind::Edge;
	PickHit best_unselected;
	best_unselected.target.kind = SelectionKind::Edge;
	for (const modeling::MeshObjectSnapshot &object : objects) {
		const bool source_wire_object =
				object.selected || std::find(source_objects.begin(), source_objects.end(), object.object) != source_objects.end();
		if (!source_wire_object) {
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
			const bool selected = contains_id<EdgeKey>(object.selected_edges, edge);
			PickHit &best = selected ? best_selected : best_unselected;
			if (distance <= radius && (distance < best.distance || (distance == best.distance && depth < best.depth))) {
				best.hit = true;
				best.distance = distance;
				best.depth = depth;
				best.target.object = object.object;
				best.target.edge = edge;
			}
		}
	}
	if (!best_selected.hit) {
		return best_unselected;
	}
	if (!best_unselected.hit) {
		return best_selected;
	}
	const float selected_tolerance = prefer_selected_target ? radius : std::max(kToolEpsilon, radius * 0.25f);
	return best_selected.distance <= best_unselected.distance + selected_tolerance ? best_selected : best_unselected;
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

godot::MeshInstance3D *make_overlay_instance(const char *name, godot::Node3D *parent) {
	godot::MeshInstance3D *instance = memnew(godot::MeshInstance3D);
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
	end_transform_drag();
	box_drag_active_ = false;
	camera_bridge_.release_mouse_capture();
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
				if (box_tool_active_) {
					static_cast<void>(begin_box_drag(mouse_button->get_position()));
					accept_event();
					return;
				}
				if (begin_transform_drag(mouse_button->get_position())) {
					accept_event();
					return;
				}
				const bool remove = mouse_button->is_ctrl_pressed() || mouse_button->is_meta_pressed();
				const SelectionEdit edit = edit_from_modifiers(mouse_button->is_shift_pressed(), remove);
				static_cast<void>(select_at(mouse_button->get_position(), edit));
				update_transform_gizmo_hover(mouse_button->get_position());
				if (!gizmo::has_gizmo_handle(hovered_gizmo_handle_)) {
					update_hover(mouse_button->get_position(), remove);
				}
			} else {
				if (box_drag_active_) {
					commit_box_drag(mouse_button->get_position());
					accept_event();
					return;
				}
				end_transform_drag();
			}
			accept_event();
			return;
		}
		const CameraInputResult camera_button_result =
				camera_bridge_.handle_mouse_button(mouse_button, keyboard_shift_pressed());
		if (camera_button_result.consumed) {
			if (mouse_button->is_pressed() &&
					(button == godot::MOUSE_BUTTON_MIDDLE || button == godot::MOUSE_BUTTON_RIGHT)) {
				grab_focus();
			}
			if (camera_button_result.changed) {
				invalidate_overlays();
			}
			accept_event();
			return;
		}
	}

	godot::Ref<godot::InputEventMouseMotion> mouse_motion = event;
	if (mouse_motion.is_valid()) {
		if (box_drag_active_) {
			update_box_drag(mouse_motion->get_position());
			accept_event();
			return;
		}
		if (active_gizmo_drag_ != nullptr) {
			update_transform_drag(mouse_motion->get_position());
			accept_event();
			return;
		}
		const CameraInputResult camera_motion_result =
				camera_bridge_.handle_mouse_motion(mouse_motion, std::max(get_size().y, 1.0f), keyboard_shift_pressed());
		if (camera_motion_result.consumed) {
			if (camera_motion_result.changed) {
				invalidate_overlays();
			}
			accept_event();
			return;
		}
		if (box_tool_active_) {
			update_box_hover(mouse_motion->get_position());
			accept_event();
			return;
		}
		update_transform_gizmo_hover(mouse_motion->get_position());
		if (gizmo::has_gizmo_handle(hovered_gizmo_handle_)) {
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
		if (key->get_keycode() == godot::KEY_ESCAPE && camera_bridge_.handle_escape()) {
			accept_event();
			return;
		}
		if (camera_bridge_.core().is_flying()) {
			return;
		}
		if (key->get_keycode() == godot::KEY_ESCAPE && box_tool_active_) {
			cancel_box_tool();
			accept_event();
			return;
		}
		if (key->get_keycode() == godot::KEY_B) {
			activate_box_tool();
			accept_event();
			return;
		}
		if (transform_gizmo_key(key->get_keycode())) {
			cancel_box_tool();
			set_active_gizmo(gizmo::gizmo_for_shortcut(key->get_keycode()));
			accept_event();
			return;
		}
		if (const std::optional<SelectionMode> mode = selection_mode_for_key(key->get_keycode())) {
			cancel_box_tool();
			selection_mode_ = *mode;
			clear_hover();
			hovered_gizmo_handle_ = gizmo::GizmoHandle::None;
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
			const OperationReceipt receipt = modeling_.flip_selected_mesh_normals();
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
	if (camera_bridge_.update(delta)) {
		invalidate_overlays();
	}
	refresh_overlays_if_dirty();
}

const ViewportVisualSettings &QuaderViewportControl::visual_settings() const {
	return visual_settings_;
}

int QuaderViewportControl::grid_preset() const {
	return grid_preset_;
}

void QuaderViewportControl::set_visual_settings(const ViewportVisualSettings &settings) {
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
	box_preview_wire_overlay_ = make_overlay_instance("BoxPreviewWireOverlay", overlay_root_);
	transform_gizmo_triangle_overlay_ = make_overlay_instance("TransformGizmoTriangleOverlay", overlay_root_);
	transform_gizmo_line_overlay_ = make_overlay_instance("TransformGizmoLineOverlay", overlay_root_);

	camera_bridge_.build(scene_root_);
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
	invalidate_overlays();
}

void QuaderViewportControl::refresh_scene_meshes() {
	if (scene_root_ == nullptr || mesh_material_.is_null()) {
		return;
	}

	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.objects();
	bool changed = false;
	for (const modeling::MeshObjectSnapshot &object : objects) {
		std::vector<SceneMeshNode>::iterator found =
				std::find_if(scene_meshes_.begin(), scene_meshes_.end(),
						[&](const SceneMeshNode &node) { return same_object(node.object, object.object); });
		if (found == scene_meshes_.end()) {
			godot::MeshInstance3D *instance = memnew(godot::MeshInstance3D);
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
	const godot::Camera3D *camera = camera_bridge_.camera();
	if (!built_ || camera == nullptr || selection_face_overlay_ == nullptr) {
		return;
	}

	const godot::Vector2 viewport_size = get_size();
	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.overlay_objects();
	std::vector<render::OverlayTriangle> selection_faces;
	std::vector<render::OverlayTriangle> hover_faces;
	std::vector<render::OverlaySegment> source_wire;
	std::vector<render::OverlaySegment> selection_wire;
	std::vector<render::OverlaySegment> hover_wire;
	std::vector<render::OverlaySegment> box_preview_wire;
	std::vector<godot::Vector3> vertex_points;
	std::vector<godot::Vector3> selected_vertex_points;
	std::vector<godot::Vector3> hover_vertex_points;
	bool hover_target_selected = false;
	const bool component_mode = selection_mode_ != SelectionMode::Mesh;
	const std::vector<ComponentSourceObjectState> source_states = component_source_states(objects);
	const std::vector<ObjectId> source_objects =
			component_mode ? component_source_wire_objects(source_states, selection_mode_, component_source_candidate_)
						   : std::vector<ObjectId>{};
	const std::vector<ObjectId> vertex_handle_objects =
			component_mode ? component_vertex_handle_objects(source_states, selection_mode_, component_source_candidate_)
						   : std::vector<ObjectId>{};
	bool source_component_draw_on_top = false;

	for (const modeling::MeshObjectSnapshot &object : objects) {
		if (selection_mode_ == SelectionMode::Mesh && object.mesh_selected) {
			append_all_face_triangles(object.authored, selection_faces);
			append_all_edge_segments(object, selection_wire);
		}

		const bool draws_source_wire =
				std::find(source_objects.begin(), source_objects.end(), object.object) != source_objects.end();
		if (component_mode && draws_source_wire) {
			append_all_edge_segments(object, source_wire);
			source_component_draw_on_top = source_component_draw_on_top || authored_faces_are_inside_out(object.authored);
		}

		const bool draws_vertex_handles =
				std::find(vertex_handle_objects.begin(), vertex_handle_objects.end(), object.object) !=
				vertex_handle_objects.end();
		if (component_mode && draws_vertex_handles) {
			std::vector<godot::Vector3> object_vertices = all_vertex_points(object.authored);
			vertex_points.insert(vertex_points.end(), object_vertices.begin(), object_vertices.end());
		}

		if (component_mode) {
			if (selection_mode_ == SelectionMode::Vertex) {
				for (VertexId vertex : object.selected_vertices) {
					if (const std::optional<godot::Vector3> position = vertex_position(object.authored, vertex)) {
						selected_vertex_points.push_back(*position);
					}
				}
			}
			if (selection_mode_ == SelectionMode::Edge) {
				for (EdgeKey edge : object.selected_edges) {
					append_edge_segment(object, edge, selection_wire);
				}
			}
			if (selection_mode_ == SelectionMode::Face) {
				for (FaceId face_id : object.selected_faces) {
					if (const AuthoredPolygonFacePayload *face = find_face(object.authored, face_id)) {
						append_face_triangles(object.authored, *face, selection_faces);
						append_face_segments(object.authored, *face, selection_wire);
					}
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

	if (box_preview_visible_) {
		append_box_preview_segments(camera, box_preview_, box_preview_wire);
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
			render::make_overlay_line_mesh(source_wire, camera, viewport_size, visual_settings_.source_wire_line_size,
					source_line_depth_bias),
			render::make_overlay_line_material(visual_settings_.source_wire_color, source_wire_draw_on_top,
					kSourceWireRenderPriority, source_clip_depth_bias));
	set_overlay_mesh(selection_wire_overlay_,
			render::make_overlay_line_mesh(selection_wire, camera, viewport_size,
					selection_mode_ == SelectionMode::Edge ? visual_settings_.selection_edge_line_size
														   : visual_settings_.selection_face_wire_line_size,
					selected_line_depth_bias),
			render::make_overlay_line_material(visual_settings_.selection_wire_color, draw_on_top,
					kSelectedWireRenderPriority, selected_clip_depth_bias));
	set_overlay_mesh(hover_wire_overlay_,
			render::make_overlay_line_mesh(hover_wire, camera, viewport_size, visual_settings_.hover_wire_line_size,
					hover_line_depth_bias),
			render::make_overlay_line_material(hover_wire_color, draw_on_top, kHoverRenderPriority,
					hover_clip_depth_bias));

	set_overlay_mesh(vertex_overlay_,
			render::make_overlay_point_mesh(vertex_points, camera, viewport_size, visual_settings_.vertex_size,
					vertex_draw_on_top ? 0.0f : source_point_depth_bias),
			render::make_overlay_point_material(visual_settings_.vertex_color, vertex_draw_on_top,
					kVertexBaseRenderPriority,
					source_clip_depth_bias));
	set_overlay_mesh(selected_vertex_outline_overlay_,
			render::make_overlay_point_mesh(selected_vertex_points, camera, viewport_size,
					visual_settings_.vertex_size + visual_settings_.selected_vertex_growth +
							visual_settings_.vertex_outline_size,
					vertex_draw_on_top ? 0.0f : selected_point_depth_bias),
			render::make_overlay_point_material(visual_settings_.vertex_outline_color, vertex_draw_on_top,
					kSelectedVertexOutlineRenderPriority, selected_clip_depth_bias));
	set_overlay_mesh(selected_vertex_overlay_,
			render::make_overlay_point_mesh(selected_vertex_points, camera, viewport_size,
					visual_settings_.vertex_size + visual_settings_.selected_vertex_growth,
					vertex_draw_on_top ? 0.0f : selected_point_depth_bias),
			render::make_overlay_point_material(visual_settings_.selected_vertex_color, vertex_draw_on_top,
					kVertexSelectedRenderPriority, selected_clip_depth_bias));
	set_overlay_mesh(hover_vertex_outline_overlay_,
			render::make_overlay_point_mesh(hover_vertex_points, camera, viewport_size,
					visual_settings_.vertex_size + visual_settings_.hover_vertex_growth +
							visual_settings_.vertex_outline_size,
					vertex_draw_on_top ? 0.0f : hover_point_depth_bias),
			render::make_overlay_point_material(visual_settings_.vertex_outline_color, vertex_draw_on_top,
					kHoverVertexOutlineRenderPriority, hover_clip_depth_bias));
	set_overlay_mesh(hover_vertex_overlay_,
			render::make_overlay_point_mesh(hover_vertex_points, camera, viewport_size,
					visual_settings_.vertex_size + visual_settings_.hover_vertex_growth,
					vertex_draw_on_top ? 0.0f : hover_point_depth_bias),
			render::make_overlay_point_material(hover_vertex_color, vertex_draw_on_top,
					kVertexHoverRenderPriority, hover_clip_depth_bias));
	set_overlay_mesh(box_preview_wire_overlay_,
			render::make_overlay_line_mesh(box_preview_wire, camera, viewport_size, kBoxPreviewLineWidthPixels),
			render::make_overlay_line_material(box_preview_line_color(), true, kBoxPreviewRenderPriority));

	const gizmo::GizmoMeshes gizmo_meshes = active_gizmo_ != nullptr
			? active_gizmo_->draw(transform_gizmo_input(objects))
			: gizmo::make_empty_gizmo_meshes();
	set_overlay_mesh(transform_gizmo_triangle_overlay_, gizmo_meshes.triangles,
			gizmo::make_gizmo_triangle_material());
	set_overlay_mesh(transform_gizmo_line_overlay_, gizmo_meshes.lines, gizmo::make_gizmo_line_material());
}

void QuaderViewportControl::refresh_overlays_if_dirty() {
	if (!overlays_dirty_) {
		return;
	}
	if (!built_ || camera_bridge_.camera() == nullptr || selection_face_overlay_ == nullptr) {
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
	const godot::Camera3D *camera = camera_bridge_.camera();
	if (camera == nullptr) {
		clear_hover();
		return;
	}
	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.overlay_objects();
	const Ray ray = make_ray(camera, position);
	const PickHit surface_hit = pick_face_target(objects, ray, SelectionKind::Face);
	const std::vector<ComponentSourceObjectState> source_states = component_source_states(objects);
	const ObjectId hover_candidate = component_hover_candidate(selection_mode_, component_source_candidate_,
			surface_hit.hit ? surface_hit.target.object : ObjectId{});
	const std::vector<ObjectId> source_objects =
			selection_mode_ == SelectionMode::Mesh
					? std::vector<ObjectId>{}
					: component_source_wire_objects(source_states, selection_mode_, hover_candidate);
	PickHit hit;
	if (selection_mode_ == SelectionMode::Mesh) {
		hit = pick_face_target(objects, ray, SelectionKind::Object);
	} else if (selection_mode_ == SelectionMode::Vertex) {
		hit = pick_vertex_target(objects, ray, visual_settings_.pick_vertex_radius, source_objects, remove_preview);
	} else if (selection_mode_ == SelectionMode::Edge) {
		hit = pick_edge_target(objects, ray, visual_settings_.pick_edge_radius, source_objects, remove_preview);
	} else {
		hit = surface_hit;
	}
	if (selection_mode_ != SelectionMode::Mesh && hover_candidate != component_source_candidate_) {
		component_source_candidate_ = hover_candidate;
	}
	if (!hit.hit) {
		if (selection_mode_ != SelectionMode::Mesh && component_source_candidate_.valid()) {
			clear_hover();
			invalidate_overlays();
			return;
		}
		clear_hover();
		return;
	}
	if (hit.target.object.valid()) {
		component_source_candidate_ = hit.target.object;
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
	const godot::Camera3D *camera = camera_bridge_.camera();
	if (camera == nullptr) {
		return false;
	}
	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.overlay_objects();
	const Ray ray = make_ray(camera, position);
	const PickHit surface_hit = pick_face_target(objects, ray, SelectionKind::Face);
	const std::vector<ComponentSourceObjectState> source_states = component_source_states(objects);
	const ObjectId hover_candidate = component_hover_candidate(selection_mode_, component_source_candidate_,
			surface_hit.hit ? surface_hit.target.object : ObjectId{});
	const std::vector<ObjectId> source_objects =
			selection_mode_ == SelectionMode::Mesh
					? std::vector<ObjectId>{}
					: component_source_wire_objects(source_states, selection_mode_, hover_candidate);
	PickHit hit;
	if (selection_mode_ == SelectionMode::Mesh) {
		hit = pick_face_target(objects, ray, SelectionKind::Object);
	} else if (selection_mode_ == SelectionMode::Vertex) {
		hit = pick_vertex_target(objects, ray, visual_settings_.pick_vertex_radius, source_objects,
				edit == SelectionEdit::Remove);
	} else if (selection_mode_ == SelectionMode::Edge) {
		hit = pick_edge_target(objects, ray, visual_settings_.pick_edge_radius, source_objects,
				edit == SelectionEdit::Remove);
	} else {
		hit = surface_hit;
	}
	if (selection_mode_ != SelectionMode::Mesh && hover_candidate != component_source_candidate_) {
		component_source_candidate_ = hover_candidate;
	}

	if (!hit.hit) {
		if ((selection_mode_ == SelectionMode::Vertex || selection_mode_ == SelectionMode::Edge) && surface_hit.hit &&
				edit == SelectionEdit::Replace) {
			static_cast<void>(modeling_.activate_component_source(surface_hit.target.object));
			component_source_candidate_ = surface_hit.target.object;
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
	if (hit.target.object.valid()) {
		component_source_candidate_ = hit.target.object;
	}
	invalidate_overlays();
	return true;
}

void QuaderViewportControl::set_active_gizmo(const gizmo::Gizmo *gizmo) {
	if (active_gizmo_ == gizmo) {
		return;
	}
	active_gizmo_ = gizmo;
	hovered_gizmo_handle_ = gizmo::GizmoHandle::None;
	active_gizmo_drag_.reset();
	invalidate_overlays();
}

gizmo::GizmoMutationResult QuaderViewportControl::apply_gizmo_mutation(const gizmo::GizmoMutation &mutation) {
	OperationReceipt receipt;
	switch (mutation.kind) {
	case gizmo::GizmoMutationKind::TranslateSelection:
		receipt = modeling_.translate_selected_meshes(to_modeling(mutation.value));
		break;
	case gizmo::GizmoMutationKind::RotateSelection:
		receipt = modeling_.rotate_selected_meshes(to_modeling(mutation.value), to_modeling(mutation.pivot));
		break;
	case gizmo::GizmoMutationKind::ScaleSelection:
		receipt = modeling_.scale_selected_meshes(to_modeling(mutation.value), to_modeling(mutation.pivot));
		break;
	}
	return {
			.success = receipt.success,
			.changed = receipt.success && receipt.changed,
	};
}

void QuaderViewportControl::activate_box_tool() {
	box_tool_active_ = true;
	box_drag_active_ = false;
	box_preview_visible_ = false;
	box_plane_ = make_box_construction_plane({}, {0.0f, 1.0f, 0.0f});
	box_preview_ = {};
	clear_hover();
	set_active_gizmo(nullptr);
	selection_mode_ = SelectionMode::Mesh;
	grab_focus();
	invalidate_overlays();
}

void QuaderViewportControl::cancel_box_tool() {
	if (!box_tool_active_ && !box_drag_active_ && !box_preview_visible_) {
		return;
	}
	box_tool_active_ = false;
	box_drag_active_ = false;
	box_preview_visible_ = false;
	box_preview_ = {};
	invalidate_overlays();
}

std::optional<godot::Vector3> QuaderViewportControl::box_construction_point(godot::Vector2 position, bool seed_plane) {
	const godot::Camera3D *camera = camera_bridge_.camera();
	if (camera == nullptr) {
		return std::nullopt;
	}
	const Ray ray = make_ray(camera, position);
	if (seed_plane) {
		const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.overlay_objects();
		const PickHit hit = pick_face_target(objects, ray, SelectionKind::Face);
		box_plane_ = hit.hit ? make_box_construction_plane(hit.position, hit.normal)
							 : make_box_construction_plane({}, {0.0f, 1.0f, 0.0f});
	}
	godot::Vector3 point;
	if (!intersect_ray_plane(ray, box_plane_, point)) {
		return std::nullopt;
	}
	return point;
}

bool QuaderViewportControl::update_box_preview(godot::Vector3 raw_start, godot::Vector3 raw_end) {
	box_raw_start_ = raw_start;
	box_raw_end_ = raw_end;
	box_preview_ = make_box_tool_footprint(box_plane_, box_raw_start_, box_raw_end_, visual_settings_.grid_world_size);
	box_preview_visible_ = box_preview_.valid;
	invalidate_overlays();
	return box_preview_visible_;
}

bool QuaderViewportControl::begin_box_drag(godot::Vector2 position) {
	const std::optional<godot::Vector3> point = box_construction_point(position, true);
	if (!point.has_value()) {
		box_drag_active_ = false;
		box_preview_visible_ = false;
		invalidate_overlays();
		return false;
	}
	box_drag_active_ = true;
	clear_hover();
	return update_box_preview(*point, *point);
}

void QuaderViewportControl::update_box_drag(godot::Vector2 position) {
	if (!box_drag_active_) {
		return;
	}
	const std::optional<godot::Vector3> point = box_construction_point(position, false);
	if (!point.has_value()) {
		return;
	}
	static_cast<void>(update_box_preview(box_raw_start_, *point));
}

void QuaderViewportControl::update_box_hover(godot::Vector2 position) {
	if (!box_tool_active_ || box_drag_active_) {
		return;
	}
	const std::optional<godot::Vector3> point = box_construction_point(position, true);
	if (!point.has_value()) {
		if (box_preview_visible_) {
			box_preview_visible_ = false;
			invalidate_overlays();
		}
		return;
	}
	clear_hover();
	static_cast<void>(update_box_preview(*point, *point));
}

void QuaderViewportControl::commit_box_drag(godot::Vector2 position) {
	if (!box_drag_active_) {
		return;
	}
	update_box_drag(position);
	box_drag_active_ = false;
	if (!box_preview_.valid) {
		box_preview_visible_ = false;
		invalidate_overlays();
		return;
	}

	std::array<Vec3, 8> corners{};
	for (std::size_t index = 0; index < corners.size(); ++index) {
		corners[index] = to_modeling(box_preview_.corners[index]);
	}
	const OperationReceipt receipt =
			modeling_.create_box_from_corners(corners, "Box");
	box_tool_active_ = false;
	box_preview_visible_ = false;
	box_preview_ = {};
	if (receipt.success && receipt.changed) {
		refresh_scene_meshes();
	}
	clear_hover();
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
		for (Vec3 position : object.authored.positions) {
			sum += to_godot(position);
			++count;
		}
	}
	if (count == 0U) {
		return std::nullopt;
	}
	return sum / static_cast<float>(count);
}

gizmo::GizmoSelectionBounds QuaderViewportControl::selected_mesh_bounds(
		std::span<const modeling::MeshObjectSnapshot> objects) const {
	gizmo::GizmoSelectionBounds bounds;
	for (const modeling::MeshObjectSnapshot &object : objects) {
		if (!object.mesh_selected) {
			continue;
		}
		for (Vec3 position : object.authored.positions) {
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

gizmo::GizmoInput QuaderViewportControl::transform_gizmo_input(
		std::span<const modeling::MeshObjectSnapshot> objects) const {
	gizmo::GizmoInput input;
	input.hovered_handle = hovered_gizmo_handle_;
	input.active_handle = active_gizmo_drag_ != nullptr ? active_gizmo_drag_->handle() : gizmo::GizmoHandle::None;
	input.camera = camera_bridge_.camera();
	input.viewport_size = get_size();
	input.has_selection = selection_mode_ == SelectionMode::Mesh;
	if (const std::optional<godot::Vector3> pivot = selected_mesh_pivot(objects)) {
		input.has_pivot = true;
		input.pivot = *pivot;
	}
	return input;
}

bool QuaderViewportControl::begin_transform_drag(godot::Vector2 position) {
	if (selection_mode_ != SelectionMode::Mesh || active_gizmo_ == nullptr || camera_bridge_.camera() == nullptr) {
		return false;
	}
	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.overlay_objects();
	gizmo::GizmoInput input = transform_gizmo_input(objects);
	gizmo::GizmoPickHit hit = active_gizmo_->pick(input, position);
	if (!hit.hit) {
		return false;
	}
	gizmo::GizmoDragStart start;
	start.hit = hit;
	start.screen_position = position;
	start.pivot = input.pivot;
	start.selection_bounds = selected_mesh_bounds(objects);
	active_gizmo_drag_ = active_gizmo_->begin_drag(start);
	if (active_gizmo_drag_ == nullptr) {
		return false;
	}
	hovered_gizmo_handle_ = hit.handle;
	clear_hover();
	invalidate_overlays();
	return true;
}

void QuaderViewportControl::update_transform_drag(godot::Vector2 position) {
	const godot::Camera3D *camera = camera_bridge_.camera();
	if (active_gizmo_drag_ == nullptr || camera == nullptr) {
		return;
	}
	gizmo::GizmoDragContext context;
	context.position = position;
	context.camera = camera;
	context.viewport_size = get_size();
	context.grid_size = visual_settings_.grid_world_size;
	context.snap_enabled = !keyboard_snap_disabled();
	bool scene_changed = false;
	context.apply_mutation = [this, &scene_changed](const gizmo::GizmoMutation &mutation) {
		const gizmo::GizmoMutationResult result = apply_gizmo_mutation(mutation);
		scene_changed = scene_changed || result.changed;
		return result;
	};
	const gizmo::GizmoDragOperation operation = active_gizmo_drag_->update_drag(context);
	if (!operation.dragged) {
		return;
	}
	if (scene_changed || operation.changed) {
		refresh_scene_meshes();
	}
	invalidate_overlays();
}

void QuaderViewportControl::end_transform_drag() {
	if (active_gizmo_drag_ == nullptr) {
		return;
	}
	active_gizmo_drag_.reset();
	invalidate_overlays();
}

void QuaderViewportControl::update_transform_gizmo_hover(godot::Vector2 position) {
	if (selection_mode_ != SelectionMode::Mesh || active_gizmo_ == nullptr || camera_bridge_.camera() == nullptr ||
			active_gizmo_drag_ != nullptr) {
		if (gizmo::has_gizmo_handle(hovered_gizmo_handle_) && active_gizmo_drag_ == nullptr) {
			hovered_gizmo_handle_ = gizmo::GizmoHandle::None;
			invalidate_overlays();
		}
		return;
	}
	const std::vector<modeling::MeshObjectSnapshot> objects = modeling_.overlay_objects();
	const gizmo::GizmoPickHit hit = active_gizmo_->pick(transform_gizmo_input(objects), position);
	const gizmo::GizmoHandle next_handle = hit.hit ? hit.handle : gizmo::GizmoHandle::None;
	if (next_handle != hovered_gizmo_handle_) {
		hovered_gizmo_handle_ = next_handle;
		invalidate_overlays();
	}
}

void QuaderViewportControl::set_grid_preset(int preset) {
	const int next_preset = std::clamp(preset, 1, 10);
	const bool changed = next_preset != grid_preset_;
	grid_preset_ = next_preset;
	visual_settings_.grid_world_size = grid_world_size_for_preset(grid_preset_);
	render::apply_ground_grid_settings(grid_material_, visual_settings_);
	render::apply_default_quader_material_settings(mesh_material_, visual_settings_);
	if (box_preview_visible_) {
		box_preview_ = make_box_tool_footprint(box_plane_, box_raw_start_, box_raw_end_, visual_settings_.grid_world_size);
	}
	request_viewport_redraw();
	invalidate_overlays();
	if (changed && grid_preset_changed_callback_) {
		grid_preset_changed_callback_(grid_preset_);
	}
}

void QuaderViewportControl::set_grid_preset_changed_callback(std::function<void(int)> callback) {
	grid_preset_changed_callback_ = std::move(callback);
}

} // namespace quader_godot::viewport
