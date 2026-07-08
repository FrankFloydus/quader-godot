#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace godot {
class Button;
class EditorPlugin;
class Texture2D;
} // namespace godot

namespace quader_godot::editor {

class QuaderEditorWindow;

class QuaderEditorPluginHost : public godot::Node {
	GDCLASS(QuaderEditorPluginHost, godot::Node)

public:
	QuaderEditorPluginHost() = default;
	~QuaderEditorPluginHost() override = default;

	void initialize(godot::EditorPlugin *plugin);
	void shutdown();
	void open_editor_window();

protected:
	static void _bind_methods();

private:
	void create_toolbar_button();
	void place_toolbar_button_before_run_bar();
	void ensure_window();
	[[nodiscard]] godot::Ref<godot::Texture2D> load_toolbar_icon() const;

	godot::EditorPlugin *plugin_ = nullptr;
	godot::Button *toolbar_button_ = nullptr;
	QuaderEditorWindow *window_ = nullptr;
};

} // namespace quader_godot::editor
