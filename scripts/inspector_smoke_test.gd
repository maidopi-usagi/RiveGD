extends SceneTree

var failures: Array[String] = []
var controls: Array[RiveControl] = []
var inspectors: Array[Control] = []
var fixture_root: String

func _initialize() -> void:
	fixture_root = ProjectSettings.globalize_path("res://").path_join(
		"../third-party/rive-runtime/tests/unit_tests/assets")
	call_deferred("_run")

func _check(condition: bool, message: String) -> bool:
	if not condition:
		failures.append(message)
		push_error(message)
	return condition

func _frames(count := 2) -> void:
	for _i in count:
		await process_frame

func _load_fixture(file_name: String, property_name: String):
	var control = RiveControl.new()
	controls.append(control)
	control.size = Vector2(128, 128)
	root.add_child(control)
	var file := RiveFile.new()
	file.data = FileAccess.get_file_as_bytes(fixture_root.path_join(file_name))
	if not _check(not file.data.is_empty(), "Missing fixture: " + file_name):
		return null
	control.rive_file = file
	if not _check(control.load_file() == OK, "Cannot load fixture: " + file_name):
		return null
	var vm = control.get_view_model_instance()
	if vm != null and vm.has_property(property_name):
		return control
	for artboard in control.get_artboard_list():
		control.artboard_name = artboard
		vm = control.get_view_model_instance()
		if vm != null and vm.has_property(property_name):
			return control
	_check(false, "Fixture has no bound property '%s': %s" % [property_name, file_name])
	return null

func _new_inspector(control):
	var inspector = ClassDB.instantiate("RiveViewModelInspector")
	if not _check(inspector != null and inspector.has_method("set_rive_control"),
			"Built extension does not expose the updated native Inspector"):
		if inspector != null:
			inspector.free()
		return null
	inspectors.append(inspector)
	root.add_child(inspector)
	inspector.set_rive_control(control)
	return inspector

func _find_row(inspector, control, path: String):
	inspector.set_rive_control(control)
	for _page in 102:
		for row in inspector.get_children():
			if not row is HBoxContainer:
				continue
			for child in row.get_children():
				if child is Label and child.tooltip_text == path:
					return row
		var has_next := false
		for row in inspector.get_children():
			for child in row.get_children():
				if child is Button and child.text == ">" and not child.disabled:
					has_next = true
		if not has_next:
			break
		inspector.call("_on_next_page")
	return null

func _editor_for(inspector, control, path: String, editor_class: String):
	var row = _find_row(inspector, control, path)
	if not _check(row != null, "Inspector omitted property: " + path):
		return null
	for child in row.get_children():
		if child.is_class(editor_class):
			return child
	_check(false, "Property '%s' has no %s editor" % [path, editor_class])
	return null

func _history(control):
	var manager = EditorInterface.get_editor_undo_redo()
	return manager.get_history_undo_redo(manager.get_object_history_id(control))

func _test_enum_image_list(control, inspector) -> void:
	var default_enum: int = control.get_view_model_instance().get_enum_property("Test Enum").get_value()
	var enum_editor = _editor_for(inspector, control, "Test Enum", "OptionButton")
	if enum_editor != null and _check(enum_editor.item_count >= 2, "Custom enum choices are missing"):
		var chosen: int = enum_editor.get_item_id(1)
		enum_editor.select(1)
		enum_editor.emit_signal("item_selected", 1)
		_check(control.get_property_values().get("Test Enum") == chosen, "Enum UI persisted the wrong index")
		_check(control.get_view_model_instance().get_enum_property("Test Enum").get_value() == chosen, "Enum UI did not apply its value")
		var history = _history(control)
		if _check(history != null and history.has_undo(), "Enum UI did not create an Undo action"):
			history.undo()
			await _frames()
			_check(control.get_view_model_instance().get_enum_property("Test Enum").get_value() == default_enum, "Enum Undo did not restore native default")
			history.redo()
			await _frames()
			_check(control.get_view_model_instance().get_enum_property("Test Enum").get_value() == chosen, "Enum Redo did not replay selected value")

	var image_editor = _editor_for(inspector, control, "Test Image", "EditorResourcePicker")
	if image_editor != null:
		_check(image_editor.get_base_type() == "Texture2D", "Image picker must accept Texture2D")
		var pixels := Image.create(2, 2, false, Image.FORMAT_RGBA8)
		pixels.fill(Color.GREEN)
		var texture := ImageTexture.create_from_image(pixels)
		image_editor.set_edited_resource(texture)
		image_editor.emit_signal("resource_changed", texture)
		_check(control.get_property_values().get("Test Image") == texture, "Image UI override was not persisted")
		_check(control.get_view_model_instance().get_image_property("Test Image").get_value() == texture, "Image picker did not apply Texture2D")
		var history = _history(control)
		if _check(history != null and history.has_undo(), "Image picker did not create an Undo action"):
			history.undo()
			await _frames()
			_check(not control.get_property_values().has("Test Image"), "Image Undo did not remove override")
			history.redo()
			await _frames()
			_check(control.get_view_model_instance().get_image_property("Test Image").get_value() == texture, "Image Redo did not restore texture")

	var list = control.get_view_model_instance().get_list_property("Test List")
	if not _check(list != null, "List wrapper missing for Inspector test"):
		return
	if _check(list.get_size() > 0, "Native fixture list must contain a child"):
		var nested_color = _editor_for(inspector, control, "Test List[0].Color", "ColorPickerButton")
		if nested_color != null:
			nested_color.emit_signal("color_changed", Color.MAGENTA)
			_check(control.get_property_values().get("Test List.0.Color") == Color.MAGENTA and control.has_property_override("Test List[0].Color"), "List child edit did not persist its canonical path")
			_check(control.get_view_model_instance().get_color_property("Test List[0].Color").get_value().is_equal_approx(Color.MAGENTA), "List child edit changed the wrong property")
		var list_row = _find_row(inspector, control, "Test List")
		if list_row != null:
			for child in list_row.get_children():
				if child is Button and child.text == "Clear":
					child.emit_signal("pressed")
					break
		_check(control.get_view_model_instance().get_list_property("Test List").get_size() == 0, "Inspector Clear did not empty the list")
		_check(not control.has_property_override("Test List[0].Color"), "List Clear retained a stale child override")
		await _frames()

func _test_font() -> void:
	var control = _load_fixture("data_bind_font_test.riv", "fontProperty")
	if control == null:
		return
	var inspector = _new_inspector(control)
	if inspector == null:
		return
	var picker = _editor_for(inspector, control, "fontProperty", "EditorResourcePicker")
	if picker != null:
		_check(picker.get_base_type() == "FontFile", "Font picker must accept FontFile")
		var font := FontFile.new()
		font.data = FileAccess.get_file_as_bytes(fixture_root.path_join("kablammo.ttf"))
		picker.set_edited_resource(font)
		picker.emit_signal("resource_changed", font)
		_check(control.get_property_values().get("fontProperty") == font, "Font picker override was not persisted")
		_check(control.get_view_model_instance().get_font_property("fontProperty").get_value() == font, "Font picker did not apply the FontFile")
		var history = _history(control)
		if _check(history != null and history.has_undo(), "Font picker did not create an Undo action"):
			history.undo()
			await _frames()
			_check(not control.get_property_values().has("fontProperty"), "Font Undo did not remove override")
			history.redo()
			await _frames()
			_check(control.get_view_model_instance().get_font_property("fontProperty").get_value() == font, "Font Redo did not restore FontFile")
		picker = _editor_for(inspector, control, "fontProperty", "EditorResourcePicker")
		if picker != null:
			var before: Dictionary = control.get_property_values().duplicate(true)
			var undo_version: int = history.get_version() if history != null else -1
			var empty_font := FontFile.new()
			picker.set_edited_resource(empty_font)
			picker.emit_signal("resource_changed", empty_font)
			_check(control.get_property_values() == before, "Empty FontFile replaced a valid persisted override")
			_check(control.get_view_model_instance().get_font_property("fontProperty").get_value() == font, "Empty FontFile changed the native property")
			_check(history == null or history.get_version() == undo_version, "Rejected FontFile created an Undo action")
			await _frames()
			picker = _editor_for(inspector, control, "fontProperty", "EditorResourcePicker")
			if picker != null:
				_check(picker.get_edited_resource() == font, "Rejected font selection was not restored in the UI")

func _test_artboard() -> void:
	var control = _load_fixture("data_binding_artboards_test.riv", "ab")
	if control == null:
		return
	var inspector = _new_inspector(control)
	if inspector == null:
		return
	var picker = _editor_for(inspector, control, "ab", "OptionButton")
	if picker != null:
		var default_name: String = control.get_view_model_instance().get_artboard_property("ab").get_value()
		_check(picker.get_item_text(picker.selected) == default_name, "Artboard picker did not resolve its native default ID")
		var chosen := -1
		for index in picker.item_count:
			if picker.get_item_text(index) == "ch1":
				chosen = index
		if _check(chosen >= 0, "Artboard picker is missing same-file choices"):
			picker.select(chosen)
			picker.emit_signal("item_selected", chosen)
			_check(control.get_property_values().get("ab") == "ch1", "Artboard picker did not persist its name")
			_check(control.get_view_model_instance().get_artboard_property("ab").get_value() == "ch1", "Artboard picker did not apply chosen asset")

func _run() -> void:
	await process_frame
	if not _check(Engine.is_editor_hint() and ClassDB.class_exists("RiveViewModelInspector"), "This smoke test requires --editor"):
		quit(1)
		return
	var control = _load_fixture("data_bind_test_cmdq.riv", "Test Enum")
	if control != null:
		var inspector = _new_inspector(control)
		if inspector != null:
			await _test_enum_image_list(control, inspector)
	await _test_font()
	await _test_artboard()
	for inspector in inspectors:
		inspector.set_rive_control(null)
		inspector.queue_free()
	inspectors.clear()
	for instance in controls:
		instance.rive_file = null
		instance.queue_free()
	controls.clear()
	await _frames()
	if failures.is_empty():
		print("RIVE_INSPECTOR_SMOKE_PASS: enum, image, list children, font, artboard, Undo and Redo")
		quit(0)
	else:
		print("RIVE_INSPECTOR_SMOKE_FAIL: ", failures.size(), " assertions")
		quit(1)
