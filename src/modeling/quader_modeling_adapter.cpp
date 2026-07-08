#include "modeling/quader_modeling_adapter.h"

#include <algorithm>
#include <functional>
#include <span>

namespace quader_godot::modeling {
namespace {

std::vector<quader::modeling::EdgeKey> authored_edges(const quader::modeling::AuthoredPolygonPayload &payload) {
	std::vector<quader::modeling::EdgeKey> edges;
	for (const quader::modeling::AuthoredPolygonFacePayload &face : payload.faces) {
		if (face.vertices.size() < 2U) {
			continue;
		}
		for (std::size_t index = 0; index < face.vertices.size(); ++index) {
			const quader::modeling::VertexId a = face.vertices[index];
			const quader::modeling::VertexId b = face.vertices[(index + 1U) % face.vertices.size()];
			quader::modeling::EdgeKey edge = quader::modeling::make_edge_key(a, b);
			if (edge.valid()) {
				edges.push_back(edge);
			}
		}
	}
	std::sort(edges.begin(), edges.end());
	edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
	return edges;
}

} // namespace

QuaderModelingAdapter::QuaderModelingAdapter()
		: api_(quader::modeling::ModelingApi::create(
				  {.error_policy = quader::modeling::ErrorPolicy::StoreLastError})) {
	create_default_scene();
}

std::vector<MeshObjectSnapshot> QuaderModelingAdapter::objects() {
	return snapshots(true);
}

std::vector<MeshObjectSnapshot> QuaderModelingAdapter::overlay_objects() {
	return snapshots(false);
}

std::vector<MeshObjectSnapshot> QuaderModelingAdapter::snapshots(bool include_render_mesh) {
	std::vector<MeshObjectSnapshot> snapshots;
	for (const quader::modeling::MeshSummary &summary : api_.mesh_summaries()) {
		quader::modeling::MeshHandle mesh = api_.mesh(summary.id);
		MeshObjectSnapshot snapshot;
		snapshot.object = summary.id;
		snapshot.name = summary.name;
		snapshot.selected = summary.selected;
		snapshot.mesh_selected = mesh_selection_tracked(summary.id);
		snapshot.active = summary.id == active_object_;
		snapshot.authored = mesh.payloads().authored_polygon();
		if (include_render_mesh) {
			snapshot.mesh = mesh.payloads().compile_mesh();
		}
		snapshot.edges = authored_edges(snapshot.authored);

		const quader::modeling::VertexSelection selected_vertices = mesh.vertices().selected();
		snapshot.selected_vertices.assign(selected_vertices.vertices().begin(), selected_vertices.vertices().end());
		const quader::modeling::EdgeSelection selected_edges = mesh.edges().selected();
		snapshot.selected_edges.assign(selected_edges.edges().begin(), selected_edges.edges().end());
		const quader::modeling::FaceSelection selected_faces = mesh.faces().selected();
		snapshot.selected_faces.assign(selected_faces.faces().begin(), selected_faces.faces().end());
		snapshots.push_back(std::move(snapshot));
	}
	return snapshots;
}

std::vector<quader::modeling::ObjectId> QuaderModelingAdapter::selected_objects() {
	return api_.selection().summary().objects;
}

quader::modeling::ObjectId QuaderModelingAdapter::active_object() const {
	return active_object_;
}

quader::modeling::SelectionSummary QuaderModelingAdapter::selection_summary() {
	return api_.selection().summary();
}

quader::modeling::OperationReceipt QuaderModelingAdapter::clear_selection() {
	clear_mesh_selection_tracking();
	active_object_ = {};
	return api_.selection().clear();
}

quader::modeling::OperationReceipt QuaderModelingAdapter::activate_component_source(quader::modeling::ObjectId object) {
	static_cast<void>(api_.selection().clear());
	clear_mesh_selection_tracking();
	std::vector<quader::modeling::ObjectId> objects{object};
	const quader::modeling::OperationReceipt receipt = api_.meshes().only(objects).add();
	if (receipt.success) {
		active_object_ = object;
	}
	return receipt;
}

quader::modeling::OperationReceipt QuaderModelingAdapter::apply_selection(const SelectionTarget &target,
		quader::modeling::SelectionEdit edit) {
	if (target.kind == quader::modeling::SelectionKind::Object) {
		std::vector<quader::modeling::ObjectId> objects;
		if (target.object.valid()) {
			objects.push_back(target.object);
		}
		if (edit == quader::modeling::SelectionEdit::Replace) {
			static_cast<void>(clear_selection());
			edit = quader::modeling::SelectionEdit::Add;
		}
		const quader::modeling::OperationReceipt receipt = api_.meshes().only(objects).apply(edit);
		if (receipt.success) {
			apply_mesh_selection_tracking(objects, edit);
			if (!objects.empty() && edit == quader::modeling::SelectionEdit::Toggle) {
				active_object_ = mesh_selection_tracked(target.object) ? target.object : quader::modeling::ObjectId{};
				if (!active_object_.valid()) {
					set_active_from_mesh_selection();
				}
			} else if (!objects.empty() && edit != quader::modeling::SelectionEdit::Remove) {
				active_object_ = objects.front();
			} else if (edit == quader::modeling::SelectionEdit::Remove && active_object_ == target.object) {
				set_active_from_mesh_selection();
			}
		}
		return receipt;
	}

	if (edit == quader::modeling::SelectionEdit::Replace) {
		static_cast<void>(clear_selection());
		std::vector<quader::modeling::ObjectId> objects{target.object};
		static_cast<void>(api_.meshes().only(objects).add());
		clear_mesh_selection_tracking();
		active_object_ = target.object;
	} else if (edit == quader::modeling::SelectionEdit::Add) {
		std::vector<quader::modeling::ObjectId> objects{target.object};
		static_cast<void>(api_.meshes().only(objects).add());
		active_object_ = target.object;
	} else if (edit == quader::modeling::SelectionEdit::Toggle) {
		active_object_ = target.object;
	}

	if (target.kind == quader::modeling::SelectionKind::Vertex) {
		std::vector<quader::modeling::VertexId> vertices{target.vertex};
		return api_.mesh(target.object).vertices().only(vertices).apply(edit);
	}
	if (target.kind == quader::modeling::SelectionKind::Edge) {
		std::vector<quader::modeling::EdgeKey> edges{target.edge};
		return api_.mesh(target.object).edges().only(edges).apply(edit);
	}
	std::vector<quader::modeling::FaceId> faces{target.face};
	return api_.mesh(target.object).faces().only(faces).apply(edit);
}

quader::modeling::OperationReceipt QuaderModelingAdapter::flip_selected_mesh_normals() {
	quader::modeling::OperationReceipt receipt;
	for (const quader::modeling::ObjectId object : mesh_selection_) {
		receipt = api_.mesh(object).faces().all().flip_normals();
		if (!receipt.success) {
			return receipt;
		}
	}
	return receipt;
}

quader::modeling::OperationReceipt QuaderModelingAdapter::translate_selected_meshes(quader::modeling::Vec3 delta) {
	return transform_selected_meshes([&](quader::modeling::MeshHandle mesh) { return mesh.transform().translate(delta); });
}

quader::modeling::OperationReceipt QuaderModelingAdapter::rotate_selected_meshes(quader::modeling::Vec3 radians,
		quader::modeling::Vec3 pivot) {
	return transform_selected_meshes(
			[&](quader::modeling::MeshHandle mesh) { return mesh.transform().rotate({.radians = radians, .pivot = pivot}); });
}

quader::modeling::OperationReceipt QuaderModelingAdapter::scale_selected_meshes(quader::modeling::Vec3 scale,
		quader::modeling::Vec3 pivot) {
	return transform_selected_meshes(
			[&](quader::modeling::MeshHandle mesh) { return mesh.transform().scale({.scale = scale, .pivot = pivot}); });
}

void QuaderModelingAdapter::create_default_scene() {
	static_cast<void>(api_.create_box({
			.name = "QuaderCubeA",
	}));
	static_cast<void>(api_.create_box({
			.name = "QuaderCubeB",
			.min = {3.0F, -1.0F, -1.0F},
			.max = {5.0F, 1.0F, 1.0F},
	}));
}

quader::modeling::OperationReceipt QuaderModelingAdapter::transform_selected_meshes(
		const std::function<quader::modeling::OperationReceipt(quader::modeling::MeshHandle)> &operation) {
	quader::modeling::OperationReceipt merged;
	if (mesh_selection_.empty()) {
		merged.success = false;
		merged.error = quader::modeling::make_error(quader::modeling::ErrorCode::InvalidArgument, "No selected mesh.");
		return merged;
	}

	for (const quader::modeling::ObjectId object : mesh_selection_) {
		quader::modeling::MeshHandle mesh = api_.mesh(object);
		quader::modeling::OperationReceipt receipt = operation(mesh);
		if (!receipt.success) {
			return receipt;
		}
		if (receipt.changed) {
			static_cast<void>(mesh.faces().only(std::span<const quader::modeling::FaceId>{}).replace());
		}
		merged.changed = merged.changed || receipt.changed;
		merged.revisions = receipt.revisions;
		merged.dirty.topology = merged.dirty.topology || receipt.dirty.topology;
		merged.dirty.geometry = merged.dirty.geometry || receipt.dirty.geometry;
		merged.dirty.selection = merged.dirty.selection || receipt.dirty.selection;
		merged.dirty.materials = merged.dirty.materials || receipt.dirty.materials;
		merged.dirty.overlays = merged.dirty.overlays || receipt.dirty.overlays;
	}
	return merged;
}

void QuaderModelingAdapter::apply_mesh_selection_tracking(std::span<const quader::modeling::ObjectId> objects,
		quader::modeling::SelectionEdit edit) {
	if (edit == quader::modeling::SelectionEdit::Replace) {
		mesh_selection_.assign(objects.begin(), objects.end());
		return;
	}

	for (const quader::modeling::ObjectId object : objects) {
		const auto found = std::find(mesh_selection_.begin(), mesh_selection_.end(), object);
		if (edit == quader::modeling::SelectionEdit::Add) {
			if (found == mesh_selection_.end()) {
				mesh_selection_.push_back(object);
			}
			continue;
		}
		if (edit == quader::modeling::SelectionEdit::Remove) {
			if (found != mesh_selection_.end()) {
				mesh_selection_.erase(found);
			}
			continue;
		}
		if (found == mesh_selection_.end()) {
			mesh_selection_.push_back(object);
		} else {
			mesh_selection_.erase(found);
		}
	}
}

void QuaderModelingAdapter::clear_mesh_selection_tracking() {
	mesh_selection_.clear();
}

void QuaderModelingAdapter::set_active_from_mesh_selection() {
	active_object_ = mesh_selection_.empty() ? quader::modeling::ObjectId{} : mesh_selection_.back();
}

bool QuaderModelingAdapter::mesh_selection_tracked(quader::modeling::ObjectId object) const {
	return std::find(mesh_selection_.begin(), mesh_selection_.end(), object) != mesh_selection_.end();
}

} // namespace quader_godot::modeling
