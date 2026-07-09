#pragma once

#include "gizmo/gizmo_handle.h"
#include "gizmo/gizmo_mutation.h"

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <functional>

namespace quader_godot::gizmo {

struct GizmoSelectionBounds {
	bool has_bounds = false;
	godot::Vector3 min;
	godot::Vector3 max;
};

struct GizmoPickHit {
	bool hit = false;
	GizmoHandle handle = GizmoHandle::None;
	godot::Vector2 screen_anchor;
	float distance = 0.0f;
};

struct GizmoDragStart {
	GizmoPickHit hit;
	godot::Vector2 screen_position;
	godot::Vector3 pivot;
	GizmoSelectionBounds selection_bounds;
};

struct GizmoDragContext {
	godot::Vector2 position;
	const godot::Camera3D *camera = nullptr;
	godot::Vector2 viewport_size;
	float grid_size = 1.0f;
	bool snap_enabled = true;
	std::function<GizmoMutationResult(const GizmoMutation &)> apply_mutation;
};

struct GizmoDragOperation {
	bool dragged = false;
	bool changed = false;
};

class GizmoDragSession {
public:
	GizmoDragSession(GizmoHandle handle, godot::Vector2 screen_position, godot::Vector3 pivot);
	virtual ~GizmoDragSession() = default;

	[[nodiscard]] GizmoHandle handle() const;
	[[nodiscard]] virtual GizmoDragOperation update_drag(const GizmoDragContext &context) = 0;

protected:
	[[nodiscard]] godot::Vector2 last_position() const;
	void set_last_position(godot::Vector2 position);
	[[nodiscard]] godot::Vector3 start_pivot() const;
	[[nodiscard]] godot::Vector3 pivot() const;
	void set_pivot(godot::Vector3 pivot);

private:
	GizmoHandle handle_ = GizmoHandle::None;
	godot::Vector2 last_position_;
	godot::Vector3 start_pivot_;
	godot::Vector3 pivot_;
};

} // namespace quader_godot::gizmo
