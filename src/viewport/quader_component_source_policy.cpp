#include "viewport/quader_component_source_policy.h"

#include <algorithm>

namespace quader_godot::viewport {
namespace {

bool has_mode_components(const ComponentSourceObjectState &object, SelectionMode mode) {
	if (mode == SelectionMode::Vertex) {
		return object.has_selected_vertices;
	}
	if (mode == SelectionMode::Edge) {
		return object.has_selected_edges;
	}
	if (mode == SelectionMode::Face) {
		return object.has_selected_faces;
	}
	return false;
}

bool has_any_components(const ComponentSourceObjectState &object) {
	return object.has_selected_vertices || object.has_selected_edges || object.has_selected_faces;
}

bool has_any_selected_component_object(std::span<const ComponentSourceObjectState> objects) {
	return std::any_of(objects.begin(), objects.end(), [](const ComponentSourceObjectState &object) {
		return object.selected && object.object.valid() && has_any_components(object);
	});
}

bool has_sticky_source_wire(const ComponentSourceObjectState &object, SelectionMode mode) {
	static_cast<void>(mode);
	return object.selected && object.object.valid() && has_any_components(object);
}

const ComponentSourceObjectState *find_object(std::span<const ComponentSourceObjectState> objects,
		quader::modeling::ObjectId id) {
	const auto found = std::find_if(objects.begin(), objects.end(),
			[id](const ComponentSourceObjectState &object) { return object.object == id; });
	return found == objects.end() ? nullptr : &*found;
}

bool contains_object(std::span<const quader::modeling::ObjectId> objects, quader::modeling::ObjectId id) {
	return std::find(objects.begin(), objects.end(), id) != objects.end();
}

void append_unique(std::vector<quader::modeling::ObjectId> &objects, quader::modeling::ObjectId id) {
	if (id.valid() && !contains_object(objects, id)) {
		objects.push_back(id);
	}
}

} // namespace

quader::modeling::ObjectId primary_component_source_object(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		quader::modeling::ObjectId candidate) {
	if (mode == SelectionMode::Mesh) {
		return {};
	}
	for (const ComponentSourceObjectState &object : objects) {
		if (object.active && has_sticky_source_wire(object, mode)) {
			return object.object;
		}
	}
	for (const ComponentSourceObjectState &object : objects) {
		if (has_sticky_source_wire(object, mode)) {
			return object.object;
		}
	}
	if (candidate.valid()) {
		return candidate;
	}
	for (const ComponentSourceObjectState &object : objects) {
		if (object.active && object.selected && object.object.valid()) {
			return object.object;
		}
	}
	for (const ComponentSourceObjectState &object : objects) {
		if (object.selected && object.object.valid()) {
			return object.object;
		}
	}
	return {};
}

std::vector<quader::modeling::ObjectId> component_source_wire_objects(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		quader::modeling::ObjectId candidate) {
	std::vector<quader::modeling::ObjectId> result;
	if (mode == SelectionMode::Mesh) {
		return result;
	}
	for (const ComponentSourceObjectState &object : objects) {
		if (has_sticky_source_wire(object, mode)) {
			append_unique(result, object.object);
		}
	}
	const ComponentSourceObjectState *candidate_object = candidate.valid() ? find_object(objects, candidate) : nullptr;
	if (candidate_object != nullptr && !has_sticky_source_wire(*candidate_object, mode) &&
			!contains_object(result, candidate)) {
		append_unique(result, candidate);
	}
	if (result.empty()) {
		append_unique(result, primary_component_source_object(objects, mode, {}));
	}
	return result;
}

std::vector<quader::modeling::ObjectId> component_vertex_handle_objects(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		quader::modeling::ObjectId candidate) {
	std::vector<quader::modeling::ObjectId> result;
	if (mode != SelectionMode::Vertex) {
		return result;
	}
	const bool has_component_selection = has_any_selected_component_object(objects);
	for (const ComponentSourceObjectState &object : objects) {
		if (object.selected && object.object.valid() && object.has_selected_vertices) {
			append_unique(result, object.object);
		}
	}
	const ComponentSourceObjectState *candidate_object = candidate.valid() ? find_object(objects, candidate) : nullptr;
	if (candidate_object != nullptr && !candidate_object->has_selected_vertices &&
			!contains_object(result, candidate)) {
		append_unique(result, candidate);
	}
	if (result.empty() && !has_component_selection) {
		append_unique(result, primary_component_source_object(objects, mode, {}));
	}
	return result;
}

quader::modeling::ObjectId component_hover_candidate(
		SelectionMode mode, quader::modeling::ObjectId current_candidate,
		quader::modeling::ObjectId surface_object) {
	if (mode == SelectionMode::Mesh) {
		return {};
	}
	return surface_object.valid() ? surface_object : current_candidate;
}

std::vector<quader::modeling::ObjectId> component_source_wire_objects_for_hover(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		quader::modeling::ObjectId current_candidate, quader::modeling::ObjectId surface_object) {
	return component_source_wire_objects(objects, mode, component_hover_candidate(mode, current_candidate, surface_object));
}

bool component_source_candidate_draws_unselected_vertices(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		quader::modeling::ObjectId candidate) {
	const ComponentSourceObjectState *candidate_object = candidate.valid() ? find_object(objects, candidate) : nullptr;
	return mode == SelectionMode::Vertex && candidate_object != nullptr &&
			!candidate_object->has_selected_vertices;
}

} // namespace quader_godot::viewport
