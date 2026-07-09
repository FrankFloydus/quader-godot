#pragma once

#include <godot_cpp/variant/vector3.hpp>

namespace quader_godot::gizmo {

enum class GizmoMutationKind {
	TranslateSelection,
	RotateSelection,
	ScaleSelection,
};

struct GizmoMutation {
	GizmoMutationKind kind = GizmoMutationKind::TranslateSelection;
	godot::Vector3 value;
	godot::Vector3 pivot;
};

struct GizmoMutationResult {
	bool success = false;
	bool changed = false;
};

} // namespace quader_godot::gizmo
