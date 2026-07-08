#pragma once

#include "viewport/quader_viewport_selection_mode.h"

#include <quader/modeling/modeling.hpp>

#include <span>
#include <vector>

namespace quader_godot::viewport {

struct ComponentSourceObjectState {
	quader::modeling::ObjectId object;
	bool selected = false;
	bool active = false;
	bool has_selected_vertices = false;
	bool has_selected_edges = false;
	bool has_selected_faces = false;
};

[[nodiscard]] quader::modeling::ObjectId primary_component_source_object(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		quader::modeling::ObjectId candidate);

[[nodiscard]] std::vector<quader::modeling::ObjectId> component_source_wire_objects(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		quader::modeling::ObjectId candidate);

[[nodiscard]] std::vector<quader::modeling::ObjectId> component_vertex_handle_objects(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		quader::modeling::ObjectId candidate);

[[nodiscard]] quader::modeling::ObjectId component_hover_candidate(
		SelectionMode mode, quader::modeling::ObjectId current_candidate,
		quader::modeling::ObjectId surface_object);

[[nodiscard]] std::vector<quader::modeling::ObjectId> component_source_wire_objects_for_hover(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		quader::modeling::ObjectId current_candidate, quader::modeling::ObjectId surface_object);

[[nodiscard]] bool component_source_candidate_draws_unselected_vertices(
		std::span<const ComponentSourceObjectState> objects, SelectionMode mode,
		quader::modeling::ObjectId candidate);

} // namespace quader_godot::viewport
