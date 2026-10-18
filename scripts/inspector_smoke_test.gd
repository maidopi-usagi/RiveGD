@tool
extends Node

var failures: Array[String] = []
var controls: Array[RiveControl] = []
var inspectors: Array[Control] = []
var fixture_root: String
var test_window: Window
var test_viewport: SubViewport
var host: Control

func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 1:
		push_error("Run this suite with scripts/run_inspector_smoke_test.py")
		get_tree().quit(2)
		return
	fixture_root = args[0]
	test_window = Window.new()
	test_window.title = "Rive Inspector regression"
	test_window.size = Vector2i(600, 720)
	add_child(test_window)
	test_viewport = SubViewport.new()
	test_viewport.size = test_window.size
	test_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	test_window.add_child(test_viewport)
	host = Control.new()
	host.theme = EditorInterface.get_editor_theme()
	test_viewport.add_child(host)
	host.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var preview := TextureRect.new()
	preview.texture = test_viewport.get_texture()
	preview.mouse_filter = Control.MOUSE_FILTER_IGNORE
	preview.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	test_window.add_child(preview)
	preview.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	test_viewport.notify_mouse_entered()
	test_window.popup_centered()
	call_deferred("_run")

func _check(condition: bool, message: String) -> bool:
	if not condition:
		failures.append(message)
		push_error(message)
	return condition

func _frames(count := 2) -> void:
	for _i in count:
		await get_tree().process_frame

func _wait_for_editor_idle() -> void:
	var idle_frames := 0
	while idle_frames < 8:
		await get_tree().process_frame
		idle_frames = 0 if EditorInterface.get_resource_filesystem().is_scanning() else idle_frames + 1

func _confirm_editor_exit() -> void:
	for _frame in 120:
		await get_tree().process_frame
		var pending: Array[Node] = [get_tree().root]
		while not pending.is_empty():
			var node: Node = pending.pop_back()
			if node is ConfirmationDialog and node.visible:
				node.confirmed.emit.call_deferred()
				break
			pending.append_array(node.get_children(true))

func _close_editor() -> void:
	var editor: Node = EditorInterface.get_base_control()
	while editor != null and editor.get_class() != "EditorNode":
		editor = editor.get_parent()
	if not _check(editor != null, "Cannot find the editor lifecycle owner"):
		get_tree().quit(1)
		return
	editor.notification.call_deferred(NOTIFICATION_WM_CLOSE_REQUEST)
	_confirm_editor_exit.call_deferred()

func _load_fixture(file_name: String, property_name: String):
	var control = RiveControl.new()
	controls.append(control)
	control.size = Vector2(128, 128)
	control.update_mode = RiveControl.UPDATE_MANUAL
	host.add_child(control)
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
	for previous in inspectors:
		previous.hide()
	host.add_child(inspector)
	inspector.position = Vector2(12, 12)
	inspector.size.x = 570
	inspector.show()
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

func _list_tree(inspector, control, path: String) -> Tree:
	_find_row(inspector, control, path)
	for child in inspector.get_children():
		if child is Tree and child.get_meta("rive_list_path", "") == path:
			return child
	_check(false, "Missing list structure editor: " + path)
	return null

func _list_colors(control) -> Array[Color]:
	var colors: Array[Color] = []
	var list = control.get_view_model_instance().get_list_property("Test List")
	for index in list.get_size():
		colors.append(list.get_item(index).get_property_value("Color"))
	return colors

func _mouse_button(position: Vector2, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.position = position
	event.global_position = position
	event.button_index = MOUSE_BUTTON_LEFT
	event.button_mask = MOUSE_BUTTON_MASK_LEFT if pressed else 0
	event.pressed = pressed
	test_viewport.push_input(event, true)

func _mouse_motion(position: Vector2, relative: Vector2) -> void:
	var event := InputEventMouseMotion.new()
	event.position = position
	event.global_position = position
	event.relative = relative
	event.button_mask = MOUSE_BUTTON_MASK_LEFT
	test_viewport.push_input(event, true)

func _test_list_editor() -> void:
	var failures_before := failures.size()
	var control = _load_fixture("data_bind_test_cmdq.riv", "Test List")
	if control == null:
		return
	var inspector = _new_inspector(control)
	var original := _list_colors(control)
	var add = _editor_for(inspector, control, "Test List", "MenuButton")
	if add == null or original.is_empty():
		return
	var model: String = control.get_view_model_instance().get_list_property("Test List").get_item(0).get_view_model_name()
	var popup: PopupMenu = add.get_popup()
	var choice := -1
	for index in popup.item_count:
		if popup.get_item_text(index) == model + " / <Blank>":
			choice = popup.get_item_id(index)
	if not _check(choice >= 0, "List Add menu omitted a blank instance of its item model"):
		return
	popup.emit_signal("id_pressed", choice)
	await _frames(3)
	_check(_list_colors(control).size() == original.size() + 1, "Add did not append a new list item")
	var history = _history(control)
	history.undo()
	await _frames(3)
	_check(_list_colors(control) == original, "Add Undo did not restore the authored list")
	history.redo()
	await _frames(3)
	_check(_list_colors(control).size() == original.size() + 1, "Add Redo did not restore the new item")
	var color = _editor_for(inspector, control, "Test List[0].Color", "ColorPickerButton")
	if color == null:
		return
	color.emit_signal("color_changed", Color.MAGENTA)
	await _frames(3)
	var tree := _list_tree(inspector, control, "Test List")
	if tree == null:
		return
	await _frames(3)
	var before := _list_colors(control)
	var first: TreeItem = tree.get_root().get_first_child()
	var second: TreeItem = first.get_next()
	var source_rect := tree.get_item_area_rect(first)
	var target_rect := tree.get_item_area_rect(second)
	var source := tree.global_position + Vector2(18, source_rect.position.y + source_rect.size.y * 0.5)
	var target := tree.global_position + Vector2(18, target_rect.position.y + target_rect.size.y * 0.75)
	_mouse_button(source, true)
	_mouse_motion(source + Vector2(14, 0), Vector2(14, 0))
	await _frames(2)
	_mouse_motion(target, target - source - Vector2(14, 0))
	_check(test_viewport.gui_is_dragging(), "Actual mouse motion did not start a list drag")
	var payload: Variant = test_viewport.gui_get_drag_data()
	_check(payload is Dictionary and payload.get("type", "") == "rive_view_model_list_item", "List drag used the wrong payload")
	_check(inspector.call("_can_drop_list_data", target - inspector.position - tree.position, payload, tree), "List rejected a valid in-list reorder")
	_check(not inspector.call("_can_drop_list_data", Vector2.ZERO, {"type": "files"}, tree), "List accepted an unrelated drag")
	if payload is Dictionary:
		var foreign: Dictionary = payload.duplicate()
		foreign["inspector"] = 0
		_check(not inspector.call("_can_drop_list_data", target - tree.global_position, foreign, tree), "List accepted another Inspector's drag")
		var stale: Dictionary = payload.duplicate()
		stale["revision"] = -1
		_check(not inspector.call("_can_drop_list_data", target - tree.global_position, stale, tree), "List accepted a stale drag")
	_mouse_button(target, false)
	await _frames(4)
	_check(test_viewport.gui_is_drag_successful(), "Viewport did not deliver the list drop")
	var reordered := before.duplicate()
	reordered[0] = before[1]
	reordered[1] = before[0]
	_check(_list_colors(control) == reordered, "Drag reorder did not move the item and its edited values")
	_check(not control.has_property_override("Test List[0].Color"), "Reorder retained a stale index-specific override")
	history.undo()
	await _frames(3)
	_check(_list_colors(control) == before, "Reorder Undo did not restore item state and order")
	history.redo()
	await _frames(3)
	_check(_list_colors(control) == reordered, "Reorder Redo did not restore item state and order")
	tree = _list_tree(inspector, control, "Test List")
	var first_item: TreeItem = tree.get_root().get_first_child()
	_check(first_item.get_button_count(0) == 4, "List row omitted duplicate/remove/move buttons")
	tree.emit_signal("button_clicked", first_item, 0, 0, MOUSE_BUTTON_LEFT)
	await _frames(3)
	var duplicated := _list_colors(control)
	_check(duplicated.size() == reordered.size() + 1 and duplicated[0] == duplicated[1], "Duplicate did not copy the selected item")
	tree = _list_tree(inspector, control, "Test List")
	tree.emit_signal("button_clicked", tree.get_root().get_first_child(), 0, 1, MOUSE_BUTTON_LEFT)
	await _frames(3)
	_check(_list_colors(control) == reordered, "Remove deleted the wrong item")
	tree = _list_tree(inspector, control, "Test List")
	tree.emit_signal("button_clicked", tree.get_root().get_first_child(), 0, 3, MOUSE_BUTTON_LEFT)
	await _frames(3)
	_check(_list_colors(control) == before, "Move-down button did not preserve item values")
	tree = _list_tree(inspector, control, "Test List")
	tree.emit_signal("button_clicked", tree.get_root().get_first_child().get_next(), 0, 2, MOUSE_BUTTON_LEFT)
	await _frames(3)
	_check(_list_colors(control) == reordered, "Move-up button did not restore the order")
	_check(control.load_file() == OK and _list_colors(control) == reordered, "List structure changed after reload")
	var packed := PackedScene.new()
	var path := "user://rivegd-inspector-list-%d.tscn" % Time.get_ticks_usec()
	_check(packed.pack(control) == OK and ResourceSaver.save(packed, path) == OK, "Cannot save list editor scene")
	var saved := ResourceLoader.load(path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE) as PackedScene
	if _check(saved != null, "Cannot reload list editor scene"):
		var restored := saved.instantiate() as RiveControl
		controls.append(restored)
		host.add_child(restored)
		_check(_list_colors(restored) == reordered, "Saved scene lost the edited list structure or item values")
	_check(DirAccess.remove_absolute(ProjectSettings.globalize_path(path)) == OK, "Cannot remove the test-owned scene")
	if failures.size() == failures_before:
		print("RIVE_INSPECTOR_LIST_EDITOR_PASS: add, real mouse drag, duplicate/remove, Undo/Redo, reload and serialization")

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
	await get_tree().process_frame
	if not _check(Engine.is_editor_hint() and ClassDB.class_exists("RiveViewModelInspector"), "This smoke test requires --editor"):
		get_tree().quit(1)
		return
	var control = _load_fixture("data_bind_test_cmdq.riv", "Test Enum")
	if control != null:
		var inspector = _new_inspector(control)
		if inspector != null:
			await _test_enum_image_list(control, inspector)
	await _test_font()
	await _test_artboard()
	await _test_list_editor()
	for inspector in inspectors:
		inspector.set_rive_control(null)
		inspector.queue_free()
	inspectors.clear()
	for instance in controls:
		var manager := EditorInterface.get_editor_undo_redo()
		manager.clear_history(manager.get_object_history_id(instance), false)
		instance.rive_file = null
		instance.queue_free()
	controls.clear()
	test_window.queue_free()
	await _frames()
	await _wait_for_editor_idle()
	if failures.is_empty():
		print("RIVE_INSPECTOR_SMOKE_PASS: enum, image, list children, font, artboard, Undo and Redo")
	else:
		print("RIVE_INSPECTOR_SMOKE_FAIL: ", failures.size(), " assertions")
	_close_editor.call_deferred()
