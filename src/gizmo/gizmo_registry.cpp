#include "gizmo/gizmo_registry.h"

#include "gizmo/move_gizmo.h"
#include "gizmo/rotate_gizmo.h"
#include "gizmo/scale_gizmo.h"

#include <array>

namespace quader_godot::gizmo {
namespace {

constexpr std::string_view kMoveLabel{"Move"};
constexpr std::string_view kRotateLabel{"Rotate"};
constexpr std::string_view kScaleLabel{"Scale"};

const std::array<GizmoDescriptor, 3> &registry_entries() {
	static const std::array<GizmoDescriptor, 3> entries{
			GizmoDescriptor{
					.id = MoveGizmo::kId,
					.label = kMoveLabel,
					.shortcut = godot::KEY_W,
					.tool = &move_gizmo(),
			},
			GizmoDescriptor{
					.id = RotateGizmo::kId,
					.label = kRotateLabel,
					.shortcut = godot::KEY_R,
					.tool = &rotate_gizmo(),
			},
			GizmoDescriptor{
					.id = ScaleGizmo::kId,
					.label = kScaleLabel,
					.shortcut = godot::KEY_S,
					.tool = &scale_gizmo(),
			},
	};
	return entries;
}

} // namespace

std::span<const GizmoDescriptor> registered_gizmos() {
	const std::array<GizmoDescriptor, 3> &entries = registry_entries();
	return {entries.data(), entries.size()};
}

std::optional<const Gizmo *> gizmo_by_id(std::string_view id) {
	for (const GizmoDescriptor &descriptor : registry_entries()) {
		if (descriptor.id == id) {
			return descriptor.tool;
		}
	}
	return std::nullopt;
}

const Gizmo *gizmo_for_shortcut(godot::Key key) {
	if (key == godot::KEY_Q) {
		return nullptr;
	}
	for (const GizmoDescriptor &descriptor : registry_entries()) {
		if (descriptor.shortcut == key) {
			return descriptor.tool;
		}
	}
	return nullptr;
}

} // namespace quader_godot::gizmo
