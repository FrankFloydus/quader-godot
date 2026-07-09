#pragma once

#include "selection/selection_mode.h"

#include <quader/modeling/modeling.hpp>

#include <span>
#include <vector>

namespace quader::editor::selection {

using quader::modeling::ObjectId;

struct ComponentSourceObjectState {
	ObjectId object;
	bool selected = false;
	bool active = false;
	bool has_selected_vertices = false;
	bool has_selected_edges = false;
	bool has_selected_faces = false;
};

[[nodiscard]] ObjectId primary_component_source_object(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		ObjectId candidate);

[[nodiscard]] std::vector<ObjectId> component_source_wire_objects(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		ObjectId candidate);

[[nodiscard]] std::vector<ObjectId> component_vertex_handle_objects(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		ObjectId candidate);

[[nodiscard]] ObjectId component_hover_candidate(
		SelectionMode mode, ObjectId current_candidate,
		ObjectId surface_object);

[[nodiscard]] std::vector<ObjectId> component_source_wire_objects_for_hover(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		ObjectId current_candidate, ObjectId surface_object);

[[nodiscard]] bool component_source_candidate_draws_unselected_vertices(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		ObjectId candidate);

} // namespace quader::editor::selection
