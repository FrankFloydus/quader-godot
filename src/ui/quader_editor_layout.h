#pragma once

namespace godot {
class Control;
} // namespace godot

namespace quader_godot::ui {

using godot::Control;

[[nodiscard]] Control *make_quader_editor_body(Control *viewport);

} // namespace quader_godot::ui
