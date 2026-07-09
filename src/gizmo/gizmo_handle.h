#pragma once

namespace quader_godot::gizmo {

enum class GizmoHandle {
	None,
	X,
	Y,
	Z,
	XY,
	XZ,
	YZ,
	All,
};

[[nodiscard]] bool has_gizmo_handle(GizmoHandle handle);

} // namespace quader_godot::gizmo
