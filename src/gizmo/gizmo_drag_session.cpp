#include "gizmo/gizmo_drag_session.h"

namespace quader_godot::gizmo {

GizmoDragSession::GizmoDragSession(GizmoHandle handle, godot::Vector2 screen_position, godot::Vector3 pivot)
		: handle_(handle),
		  last_position_(screen_position),
		  start_pivot_(pivot),
		  pivot_(pivot) {}

GizmoHandle GizmoDragSession::handle() const {
	return handle_;
}

godot::Vector2 GizmoDragSession::last_position() const {
	return last_position_;
}

void GizmoDragSession::set_last_position(godot::Vector2 position) {
	last_position_ = position;
}

godot::Vector3 GizmoDragSession::start_pivot() const {
	return start_pivot_;
}

godot::Vector3 GizmoDragSession::pivot() const {
	return pivot_;
}

void GizmoDragSession::set_pivot(godot::Vector3 pivot) {
	pivot_ = pivot;
}

bool has_gizmo_handle(GizmoHandle handle) {
	return handle != GizmoHandle::None;
}

} // namespace quader_godot::gizmo
