#pragma once

#include "gizmo/gizmo.h"

namespace quader_godot::gizmo {

class ScaleGizmo final : public Gizmo {
public:
	static constexpr std::string_view kId{"scale"};

	[[nodiscard]] std::string_view id() const override;
	[[nodiscard]] GizmoPickHit pick(const GizmoInput &input, godot::Vector2 screen_position) const override;
	[[nodiscard]] GizmoMeshes draw(const GizmoInput &input) const override;
	[[nodiscard]] std::unique_ptr<GizmoDragSession> begin_drag(const GizmoDragStart &start) const override;
};

[[nodiscard]] const ScaleGizmo &scale_gizmo();

} // namespace quader_godot::gizmo
