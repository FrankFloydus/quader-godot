#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "editor/quader_editor_plugin_host.h"
#include "editor/quader_editor_window.h"
#include "gizmo/gizmo.h"
#include "render/quader_godot_selection_overlay.h"
#include "viewport/quader_viewport_control.h"

using namespace godot;

void initialize_gdextension_types(ModuleInitializationLevel p_level)
{
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(quader_godot::editor::QuaderEditorPluginHost);
	GDREGISTER_CLASS(quader_godot::editor::QuaderEditorWindow);
	GDREGISTER_CLASS(quader_godot::viewport::QuaderViewportControl);
}

void uninitialize_gdextension_types(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	quader_godot::render::clear_overlay_material_cache();
	quader_godot::gizmo::clear_gizmo_material_cache();
}

extern "C"
{
	// Initialization
	GDExtensionBool GDE_EXPORT quader_godot_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization)
	{
		GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
		init_obj.register_initializer(initialize_gdextension_types);
		init_obj.register_terminator(uninitialize_gdextension_types);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

		return init_obj.init();
	}
}
