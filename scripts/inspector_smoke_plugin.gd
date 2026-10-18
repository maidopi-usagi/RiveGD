@tool
extends EditorPlugin

func _enter_tree() -> void:
	call_deferred("_start_tests")

func _start_tests() -> void:
	while EditorInterface.get_resource_filesystem().is_scanning():
		await get_tree().process_frame
	var suite: Script = load("res://addons/inspector_test/inspector_smoke_test.gd")
	if suite == null or not suite.can_instantiate():
		push_error("Cannot load the Inspector test suite")
		get_tree().quit(1)
		return
	add_child(suite.new())
