#pragma once

#include <quader/modeling/modeling.hpp>

#include <functional>
#include <span>
#include <string>
#include <vector>

namespace quader_godot::modeling {

using quader::modeling::AuthoredPolygonPayload;
using quader::modeling::EdgeKey;
using quader::modeling::FaceId;
using quader::modeling::MeshHandle;
using quader::modeling::MeshPayload;
using quader::modeling::ModelingApi;
using quader::modeling::ObjectId;
using quader::modeling::OperationReceipt;
using quader::modeling::SelectionEdit;
using quader::modeling::SelectionKind;
using quader::modeling::SelectionSummary;
using quader::modeling::Vec3;
using quader::modeling::VertexId;

struct MeshObjectSnapshot {
	ObjectId object;
	std::string name;
	bool selected = false;
	bool mesh_selected = false;
	bool active = false;
	MeshPayload mesh;
	AuthoredPolygonPayload authored;
	std::vector<EdgeKey> edges;
	std::vector<VertexId> selected_vertices;
	std::vector<EdgeKey> selected_edges;
	std::vector<FaceId> selected_faces;
};

struct SelectionTarget {
	SelectionKind kind = SelectionKind::Object;
	ObjectId object;
	VertexId vertex;
	EdgeKey edge;
	FaceId face;
};

class QuaderModelingAdapter {
public:
	QuaderModelingAdapter();

	[[nodiscard]] std::vector<MeshObjectSnapshot> objects();
	[[nodiscard]] std::vector<MeshObjectSnapshot> overlay_objects();
	[[nodiscard]] std::vector<ObjectId> selected_objects();
	[[nodiscard]] ObjectId active_object() const;
	[[nodiscard]] SelectionSummary selection_summary();
	OperationReceipt clear_selection();
	OperationReceipt activate_component_source(ObjectId object);
	OperationReceipt create_box_from_bounds(Vec3 min,
			Vec3 max, std::string name = "Box");
	OperationReceipt create_box_from_corners(std::span<const Vec3> corners,
			std::string name = "Box");
	OperationReceipt apply_selection(const SelectionTarget &target,
			SelectionEdit edit);
	OperationReceipt flip_selected_mesh_normals();
	OperationReceipt translate_selected_meshes(Vec3 delta);
	OperationReceipt rotate_selected_meshes(Vec3 radians,
			Vec3 pivot);
	OperationReceipt scale_selected_meshes(Vec3 scale,
			Vec3 pivot);
	OperationReceipt translate_selected_components(Vec3 delta);
	OperationReceipt rotate_selected_components(Vec3 radians,
			Vec3 pivot);
	OperationReceipt scale_selected_components(Vec3 scale,
			Vec3 pivot);

private:
	[[nodiscard]] std::vector<MeshObjectSnapshot> snapshots(bool include_render_mesh);
	[[nodiscard]] OperationReceipt transform_selected_meshes(
			const std::function<OperationReceipt(MeshHandle)> &operation);
	void apply_mesh_selection_tracking(std::span<const ObjectId> objects,
			SelectionEdit edit);
	void clear_mesh_selection_tracking();
	void set_active_from_mesh_selection();
	[[nodiscard]] bool mesh_selection_tracked(ObjectId object) const;

	ModelingApi api_;
	std::vector<ObjectId> mesh_selection_;
	ObjectId active_object_;
};

} // namespace quader_godot::modeling
