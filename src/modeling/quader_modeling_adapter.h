#pragma once

#include <quader/modeling/modeling.hpp>

#include <functional>
#include <span>
#include <string>
#include <vector>

namespace quader_godot::modeling {

using MeshPayload = quader::modeling::MeshPayload;

struct MeshObjectSnapshot {
	quader::modeling::ObjectId object;
	std::string name;
	bool selected = false;
	bool mesh_selected = false;
	bool active = false;
	quader::modeling::MeshPayload mesh;
	quader::modeling::AuthoredPolygonPayload authored;
	std::vector<quader::modeling::EdgeKey> edges;
	std::vector<quader::modeling::VertexId> selected_vertices;
	std::vector<quader::modeling::EdgeKey> selected_edges;
	std::vector<quader::modeling::FaceId> selected_faces;
};

struct SelectionTarget {
	quader::modeling::SelectionKind kind = quader::modeling::SelectionKind::Object;
	quader::modeling::ObjectId object;
	quader::modeling::VertexId vertex;
	quader::modeling::EdgeKey edge;
	quader::modeling::FaceId face;
};

class QuaderModelingAdapter {
public:
	QuaderModelingAdapter();

	[[nodiscard]] std::vector<MeshObjectSnapshot> objects();
	[[nodiscard]] std::vector<MeshObjectSnapshot> overlay_objects();
	[[nodiscard]] std::vector<quader::modeling::ObjectId> selected_objects();
	[[nodiscard]] quader::modeling::ObjectId active_object() const;
	[[nodiscard]] quader::modeling::SelectionSummary selection_summary();
	quader::modeling::OperationReceipt clear_selection();
	quader::modeling::OperationReceipt activate_component_source(quader::modeling::ObjectId object);
	quader::modeling::OperationReceipt create_box_from_bounds(quader::modeling::Vec3 min,
			quader::modeling::Vec3 max, std::string name = "Box");
	quader::modeling::OperationReceipt create_box_from_corners(std::span<const quader::modeling::Vec3> corners,
			std::string name = "Box");
	quader::modeling::OperationReceipt apply_selection(const SelectionTarget &target,
			quader::modeling::SelectionEdit edit);
	quader::modeling::OperationReceipt flip_selected_mesh_normals();
	quader::modeling::OperationReceipt translate_selected_meshes(quader::modeling::Vec3 delta);
	quader::modeling::OperationReceipt rotate_selected_meshes(quader::modeling::Vec3 radians,
			quader::modeling::Vec3 pivot);
	quader::modeling::OperationReceipt scale_selected_meshes(quader::modeling::Vec3 scale,
			quader::modeling::Vec3 pivot);

private:
	[[nodiscard]] std::vector<MeshObjectSnapshot> snapshots(bool include_render_mesh);
	[[nodiscard]] quader::modeling::OperationReceipt transform_selected_meshes(
			const std::function<quader::modeling::OperationReceipt(quader::modeling::MeshHandle)> &operation);
	void apply_mesh_selection_tracking(std::span<const quader::modeling::ObjectId> objects,
			quader::modeling::SelectionEdit edit);
	void clear_mesh_selection_tracking();
	void set_active_from_mesh_selection();
	[[nodiscard]] bool mesh_selection_tracked(quader::modeling::ObjectId object) const;

	quader::modeling::ModelingApi api_;
	std::vector<quader::modeling::ObjectId> mesh_selection_;
	quader::modeling::ObjectId active_object_;
};

} // namespace quader_godot::modeling
