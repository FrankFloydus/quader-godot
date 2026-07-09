#pragma once

#include "gizmo/gizmo.h"

#include <godot_cpp/classes/global_constants.hpp>

#include <optional>
#include <span>
#include <string_view>

namespace quader_godot::gizmo {

struct GizmoDescriptor {
	std::string_view id;
	std::string_view label;
	godot::Key shortcut = godot::KEY_NONE;
	const Gizmo *tool = nullptr;
};

[[nodiscard]] std::span<const GizmoDescriptor> registered_gizmos();
[[nodiscard]] std::optional<const Gizmo *> gizmo_by_id(std::string_view id);
[[nodiscard]] const Gizmo *gizmo_for_shortcut(godot::Key key);

} // namespace quader_godot::gizmo
