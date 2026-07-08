@tool
extends EditorPlugin

const _QUADER_EXTENSION := preload("res://bin/quader_godot.gdextension")
const _HOST_CLASS := "QuaderEditorPluginHost"

var _host: Node

func _enter_tree() -> void:
	if not ClassDB.class_exists(_HOST_CLASS):
		push_error("Quader GDExtension class is not registered: %s" % _HOST_CLASS)
		return

	_host = ClassDB.instantiate(_HOST_CLASS) as Node
	if _host == null:
		push_error("Could not instantiate Quader GDExtension class: %s" % _HOST_CLASS)
		return

	add_child(_host)
	_host.initialize(self)

func _exit_tree() -> void:
	if _host != null:
		_host.shutdown()
		_host.queue_free()
		_host = null
