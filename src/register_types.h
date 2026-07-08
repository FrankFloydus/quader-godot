#ifndef QUADER_GODOT_REGISTER_TYPES_H
#define QUADER_GODOT_REGISTER_TYPES_H

#include <godot_cpp/godot.hpp>

void initialize_gdextension_types(godot::ModuleInitializationLevel p_level);
void uninitialize_gdextension_types(godot::ModuleInitializationLevel p_level);

#endif // QUADER_GODOT_REGISTER_TYPES_H
