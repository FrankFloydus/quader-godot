#include "modeling/quader_modeling_adapter.h"

#include <algorithm>
#include <functional>
#include <span>
#include <utility>

namespace quader_godot::modeling {

using quader::modeling::AuthoredPolygonFacePayload;
using quader::modeling::EdgeSelection;
using quader::modeling::ErrorCode;
using quader::modeling::ErrorPolicy;
using quader::modeling::FaceSelection;
using quader::modeling::MeshSummary;
using quader::modeling::PolygonDocument;
using quader::modeling::Result;
using quader::modeling::VertexSelection;
using quader::modeling::make_edge_key;
using quader::modeling::make_error;

namespace {

std::vector<EdgeKey> authored_edges(const AuthoredPolygonPayload &payload) {
	std::vector<EdgeKey> edges;
	for (const AuthoredPolygonFacePayload &face : payload.faces) {
		if (face.vertices.size() < 2U) {
			continue;
		}
		for (std::size_t index = 0; index < face.vertices.size(); ++index) {
			const VertexId a = face.vertices[index];
			const VertexId b = face.vertices[(index + 1U) % face.vertices.size()];
			EdgeKey edge = make_edge_key(a, b);
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
		: api_(ModelingApi::create(
				  {.error_policy = ErrorPolicy::StoreLastError})) {}

std::vector<MeshObjectSnapshot> QuaderModelingAdapter::objects() {
	return snapshots(true);
}

std::vector<MeshObjectSnapshot> QuaderModelingAdapter::overlay_objects() {
	return snapshots(false);
}

std::vector<MeshObjectSnapshot> QuaderModelingAdapter::snapshots(bool include_render_mesh) {
	std::vector<MeshObjectSnapshot> snapshots;
	for (const MeshSummary &summary : api_.mesh_summaries()) {
		MeshHandle mesh = api_.mesh(summary.id);
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

		const VertexSelection selected_vertices = mesh.vertices().selected();
		snapshot.selected_vertices.assign(selected_vertices.vertices().begin(), selected_vertices.vertices().end());
		const EdgeSelection selected_edges = mesh.edges().selected();
		snapshot.selected_edges.assign(selected_edges.edges().begin(), selected_edges.edges().end());
		const FaceSelection selected_faces = mesh.faces().selected();
		snapshot.selected_faces.assign(selected_faces.faces().begin(), selected_faces.faces().end());
		snapshots.push_back(std::move(snapshot));
	}
	return snapshots;
}

std::vector<ObjectId> QuaderModelingAdapter::selected_objects() {
	return api_.selection().summary().objects;
}

ObjectId QuaderModelingAdapter::active_object() const {
	return active_object_;
}

SelectionSummary QuaderModelingAdapter::selection_summary() {
	return api_.selection().summary();
}

OperationReceipt QuaderModelingAdapter::clear_selection() {
	clear_mesh_selection_tracking();
	return api_.selection().clear();
}

OperationReceipt QuaderModelingAdapter::activate_component_source(ObjectId object) {
	static_cast<void>(api_.selection().clear());
	clear_mesh_selection_tracking();
	std::vector<ObjectId> objects{object};
	const OperationReceipt receipt = api_.meshes().only(objects).add();
	if (receipt.success) {
		active_object_ = object;
	}
	return receipt;
}

OperationReceipt QuaderModelingAdapter::create_box_from_bounds(Vec3 min,
		Vec3 max, std::string name) {
	Result<MeshHandle> created = api_.checked().create_box({
			.name = std::move(name),
			.min = min,
			.max = max,
	});
	if (!created.ok()) {
		OperationReceipt receipt;
		receipt.success = false;
		receipt.error = created.error();
		return receipt;
	}

	const ObjectId object = created.value().id();
	std::vector<ObjectId> objects{object};
	OperationReceipt receipt = api_.meshes().only(objects).replace();
	if (receipt.success) {
		mesh_selection_ = objects;
		active_object_ = object;
		receipt.changed = true;
		receipt.dirty.topology = true;
		receipt.dirty.geometry = true;
		receipt.dirty.selection = true;
		receipt.dirty.overlays = true;
	}
	return receipt;
}

OperationReceipt QuaderModelingAdapter::create_box_from_corners(
		std::span<const Vec3> corners, std::string name) {
	Result<PolygonDocument> document =
			PolygonDocument::make_box_from_corners(corners);
	if (!document.ok()) {
		OperationReceipt receipt;
		receipt.success = false;
		receipt.error = document.error();
		return receipt;
	}

	Result<MeshHandle> created =
			api_.checked().add_document(std::move(document).value(), std::move(name));
	if (!created.ok()) {
		OperationReceipt receipt;
		receipt.success = false;
		receipt.error = created.error();
		return receipt;
	}

	const ObjectId object = created.value().id();
	std::vector<ObjectId> objects{object};
	OperationReceipt receipt = api_.meshes().only(objects).replace();
	if (receipt.success) {
		mesh_selection_ = objects;
		active_object_ = object;
		receipt.changed = true;
		receipt.dirty.topology = true;
		receipt.dirty.geometry = true;
		receipt.dirty.selection = true;
		receipt.dirty.overlays = true;
	}
	return receipt;
}

OperationReceipt QuaderModelingAdapter::apply_selection(const SelectionTarget &target,
		SelectionEdit edit) {
	if (target.kind == SelectionKind::Object) {
		std::vector<ObjectId> objects;
		if (target.object.valid()) {
			objects.push_back(target.object);
		}
		if (edit == SelectionEdit::Replace) {
			static_cast<void>(clear_selection());
			edit = SelectionEdit::Add;
		}
		const OperationReceipt receipt = api_.meshes().only(objects).apply(edit);
		if (receipt.success) {
			apply_mesh_selection_tracking(objects, edit);
		}
		return receipt;
	}

	const std::vector<ObjectId> selected_before = api_.selection().summary().objects;
	const bool was_edit_target = active_object_ == target.object ||
			std::find(selected_before.begin(), selected_before.end(), target.object) != selected_before.end();

	if (edit == SelectionEdit::Replace) {
		static_cast<void>(clear_selection());
		std::vector<ObjectId> objects{target.object};
		static_cast<void>(api_.meshes().only(objects).add());
		mesh_selection_.clear();
		active_object_ = target.object;
	} else if (edit == SelectionEdit::Add) {
		std::vector<ObjectId> objects{target.object};
		static_cast<void>(api_.meshes().only(objects).add());
		active_object_ = target.object;
	} else if (edit == SelectionEdit::Toggle) {
		active_object_ = target.object;
	}

	OperationReceipt receipt;
	if (target.kind == SelectionKind::Vertex) {
		std::vector<VertexId> vertices{target.vertex};
		receipt = api_.mesh(target.object).vertices().only(vertices).apply(edit);
	} else if (target.kind == SelectionKind::Edge) {
		std::vector<EdgeKey> edges{target.edge};
		receipt = api_.mesh(target.object).edges().only(edges).apply(edit);
	} else {
		std::vector<FaceId> faces{target.face};
		receipt = api_.mesh(target.object).faces().only(faces).apply(edit);
	}

	if (receipt.success && edit == SelectionEdit::Remove && was_edit_target) {
		bool selection_empty = true;
		MeshHandle mesh = api_.mesh(target.object);
		if (target.kind == SelectionKind::Vertex) {
			selection_empty = mesh.vertices().selected().empty();
		} else if (target.kind == SelectionKind::Edge) {
			selection_empty = mesh.edges().selected().empty();
		} else if (target.kind == SelectionKind::Face) {
			selection_empty = mesh.faces().selected().empty();
		}
		if (selection_empty) {
			std::vector<ObjectId> objects{target.object};
			const OperationReceipt keep_target = api_.meshes().only(objects).add();
			if (keep_target.success) {
				active_object_ = target.object;
				receipt.changed = receipt.changed || keep_target.changed;
				receipt.dirty.selection = true;
				receipt.dirty.overlays = true;
			}
		}
	}

	return receipt;
}

OperationReceipt QuaderModelingAdapter::flip_selected_mesh_normals() {
	OperationReceipt receipt;
	for (const ObjectId object : mesh_selection_) {
		receipt = api_.mesh(object).faces().all().flip_normals();
		if (!receipt.success) {
			return receipt;
		}
	}
	return receipt;
}

OperationReceipt QuaderModelingAdapter::translate_selected_meshes(Vec3 delta) {
	return transform_selected_meshes([&](MeshHandle mesh) { return mesh.transform().translate(delta); });
}

OperationReceipt QuaderModelingAdapter::rotate_selected_meshes(Vec3 radians,
		Vec3 pivot) {
	return transform_selected_meshes(
			[&](MeshHandle mesh) { return mesh.transform().rotate({.radians = radians, .pivot = pivot}); });
}

OperationReceipt QuaderModelingAdapter::scale_selected_meshes(Vec3 scale,
		Vec3 pivot) {
	return transform_selected_meshes(
			[&](MeshHandle mesh) { return mesh.transform().scale({.scale = scale, .pivot = pivot}); });
}

OperationReceipt QuaderModelingAdapter::translate_selected_components(Vec3 delta) {
	return api_.operations().translate_selection(delta);
}

OperationReceipt QuaderModelingAdapter::rotate_selected_components(Vec3 radians,
		Vec3 pivot) {
	return api_.operations().rotate_selection({.radians = radians, .pivot = pivot});
}

OperationReceipt QuaderModelingAdapter::scale_selected_components(Vec3 scale,
		Vec3 pivot) {
	return api_.operations().scale_selection({.scale = scale, .pivot = pivot});
}

OperationReceipt QuaderModelingAdapter::transform_selected_meshes(
		const std::function<OperationReceipt(MeshHandle)> &operation) {
	OperationReceipt merged;
	if (mesh_selection_.empty()) {
		merged.success = false;
		merged.error = make_error(ErrorCode::InvalidArgument, "No selected mesh.");
		return merged;
	}

	for (const ObjectId object : mesh_selection_) {
		MeshHandle mesh = api_.mesh(object);
		OperationReceipt receipt = operation(mesh);
		if (!receipt.success) {
			return receipt;
		}
		if (receipt.changed) {
			static_cast<void>(mesh.faces().only(std::span<const FaceId>{}).replace());
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

void QuaderModelingAdapter::apply_mesh_selection_tracking(
		std::span<const ObjectId> objects, SelectionEdit edit) {
	if (edit == SelectionEdit::Replace) {
		mesh_selection_.assign(objects.begin(), objects.end());
		active_object_ = mesh_selection_.empty() ? ObjectId{} : mesh_selection_.front();
		return;
	}

	if (edit == SelectionEdit::Add) {
		for (ObjectId object : objects) {
			if (!mesh_selection_tracked(object)) {
				mesh_selection_.push_back(object);
			}
		}
		if (!objects.empty()) {
			active_object_ = objects.front();
		}
		return;
	}

	if (edit == SelectionEdit::Remove) {
		for (ObjectId object : objects) {
			mesh_selection_.erase(std::remove(mesh_selection_.begin(), mesh_selection_.end(), object),
					mesh_selection_.end());
			if (active_object_ == object) {
				set_active_from_mesh_selection();
			}
		}
		return;
	}

	if (edit == SelectionEdit::Toggle) {
		for (ObjectId object : objects) {
			if (mesh_selection_tracked(object)) {
				mesh_selection_.erase(std::remove(mesh_selection_.begin(), mesh_selection_.end(), object),
						mesh_selection_.end());
				if (active_object_ == object) {
					set_active_from_mesh_selection();
				}
			} else {
				mesh_selection_.push_back(object);
				active_object_ = object;
			}
		}
	}
}

void QuaderModelingAdapter::clear_mesh_selection_tracking() {
	mesh_selection_.clear();
	active_object_ = {};
}

void QuaderModelingAdapter::set_active_from_mesh_selection() {
	active_object_ = mesh_selection_.empty() ? ObjectId{} : mesh_selection_.front();
}

bool QuaderModelingAdapter::mesh_selection_tracked(ObjectId object) const {
	return std::find(mesh_selection_.begin(), mesh_selection_.end(), object) != mesh_selection_.end();
}

} // namespace quader_godot::modeling
