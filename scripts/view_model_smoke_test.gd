extends SceneTree

var failures: Array[String] = []
var controls: Array[RiveControl] = []
var fixture_root: String
var property_change_count := 0

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
		await RenderingServer.frame_post_draw

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

func _on_property_values_changed() -> void:
	property_change_count += 1

func _test_scalars(control) -> void:
	var vm = control.get_view_model_instance()
	var number = vm.get_number_property("Test Num")
	if not _check(number != null, "Number wrapper missing"):
		return
	var default_number: float = number.get_value()
	control.property_values_changed.connect(_on_property_values_changed)
	_check(control.set_property_value("Test Num", 137), "Integer must set a number property")
	_check(is_equal_approx(number.get_value(), 137.0), "Integer was dispatched as an enum")
	var stored: Dictionary = control.get_property_values().duplicate(true)
	_check(not control.set_property_value("Test Num", "invalid"), "Invalid number value accepted")
	_check(control.get_property_values() == stored, "Rejected value changed persisted overrides")
	var named_values: Dictionary = stored.duplicate(true)
	named_values.erase("Test Num")
	named_values[&"Test Num"] = 138
	control.set_property_values(named_values)
	_check(is_equal_approx(control.get_view_model_instance().get_number_property("Test Num").get_value(), 138.0), "StringName override key was not replayed")
	_check(control.get_property_values().has("Test Num"), "StringName override key was not normalized for lookup")
	_check(control.set_property_value("Test Num", 137), "Cannot restore normalized number override")
	var malicious_keys: Dictionary = stored.duplicate(true)
	malicious_keys[RefCounted.new()] = 456
	control.set_property_values(malicious_keys)
	_check(control.get_property_values() == stored, "Bulk overrides accepted a non-string object key")
	_check(is_equal_approx(number.get_value(), 137.0), "Rejected override key changed native state")
	_check(not vm.has_property("Test List[-1].Nested Number"), "Negative list index accepted")
	_check(not vm.has_property("Test List[999999].Nested Number"), "Out-of-range list index accepted")
	_check(not vm.has_property("Test Nested..Nested Number"), "Malformed path accepted")
	_check(control.set_property_value("Test Nested.Nested Number", 321), "Nested number assignment failed")
	_check(control.load_file() == OK, "Scalar override reload failed")
	vm = control.get_view_model_instance()
	_check(is_equal_approx(vm.get_number_property("Test Num").get_value(), 137.0), "Number override lost after reload")
	_check(is_equal_approx(vm.get_number_property("Test Nested.Nested Number").get_value(), 321.0), "Nested override lost after reload")
	control.set_property_values({})
	vm = control.get_view_model_instance()
	_check(is_equal_approx(vm.get_number_property("Test Num").get_value(), default_number), "Removed override did not restore the native default")
	_check(property_change_count >= 3, "Override changes did not emit property_values_changed")
	var enum_property = vm.get_enum_property("Test Enum")
	if _check(enum_property != null, "Custom enum wrapper missing"):
		var choices: PackedStringArray = enum_property.get_values()
		if _check(choices.size() >= 2, "Custom enum choices missing"):
			_check(control.set_property_value("Test Enum", choices[1]), "Enum name assignment failed")
			_check(enum_property.get_value() == 1, "Enum name resolved to wrong index")
			_check(control.set_property_value("Test Enum", 0), "Enum index assignment failed")
			_check(not control.set_property_value("Test Enum", -1), "Negative enum index accepted")
			_check(not control.set_property_value("Test Enum", choices.size()), "Out-of-range enum index accepted")
			_check(not control.set_property_value("Test Enum", "__missing__"), "Unknown enum name accepted")
			_check(enum_property.get_value() == 0, "Invalid enum assignment changed its value")
			control.load_file()
			_check(control.get_view_model_instance().get_enum_property("Test Enum").get_value() == 0, "Enum override lost after reload")
	_check(control.set_property_value("Test Trigger", true), "Trigger assignment failed")
	_check(not control.get_property_values().has("Test Trigger"), "Trigger was persisted as state")
	var replay_values: Dictionary = control.get_property_values().duplicate(true)
	replay_values["Test Trigger"] = true
	control.set_property_values(replay_values)
	_check(not control.get_property_values().has("Test Trigger"), "Bulk overrides retained a trigger")

func _test_images(control) -> void:
	var pixels := Image.create(2, 2, false, Image.FORMAT_RGBA8)
	pixels.fill(Color.GREEN)
	var texture := ImageTexture.create_from_image(pixels)
	_check(control.set_property_value("Test Image", texture), "Image texture assignment failed")
	var image_property = control.get_view_model_instance().get_image_property("Test Image")
	if not _check(image_property != null, "Image wrapper missing"):
		return
	_check(image_property.get_value() == texture, "Image wrapper lost its assigned texture")
	_check(not control.set_property_value("Test Image", 123), "Invalid image value accepted")
	_check(control.load_file() == OK, "Image override reload failed")
	image_property = control.get_view_model_instance().get_image_property("Test Image")
	_check(image_property.get_value() == texture, "Image override lost after reload")
	_check(control.set_property_value("Test Image", null), "Null image clearing failed")
	_check(image_property.get_value() == null, "Image was not cleared")
	await _frames()

func _test_lists(control) -> void:
	var vm = control.get_view_model_instance()
	var list = vm.get_list_property("Test List")
	if not _check(list != null, "List wrapper missing"):
		return
	var original_items: Array = list.get_items()
	var original_size: int = list.get_size()
	var first = vm.get("Test Nested")
	var other = RiveControl.new()
	controls.append(other)
	other.rive_file = control.rive_file
	other.artboard_name = control.artboard_name
	other.size = Vector2(128, 128)
	root.add_child(other)
	var second = other.get_view_model_instance().get("Test Nested")
	if not _check(first != null and second != null, "Nested instances for list test missing"):
		return
	var stored: Dictionary = control.get_property_values().duplicate(true)
	_check(not control.set_property_value("Test List", [first, second]), "Control persisted nonempty runtime VM handles")
	_check(control.get_property_values() == stored and list.get_size() == original_size, "Rejected persistent list changed stored or native state")
	var invalid_bulk: Dictionary = stored.duplicate(true)
	invalid_bulk["Test Num"] = 999
	invalid_bulk["Test List"] = [first]
	var original_number: float = vm.get_number_property("Test Num").get_value()
	control.set_property_values(invalid_bulk)
	_check(control.get_property_values() == stored, "Bulk overrides accepted nonempty runtime VM handles")
	_check(is_equal_approx(vm.get_number_property("Test Num").get_value(), original_number), "Rejected bulk override partially mutated native state")
	var detached: Dictionary = control.get_property_values()
	detached["Test Num"] = 999
	_check(control.get_property_values() == stored, "Override getter exposed the internal mutable dictionary")
	_check(first.set_property_value("Nested Number", 11), "First list child assignment failed")
	_check(second.set_property_value("Nested Number", 22), "Second list child assignment failed")
	_check(list.set_items([first, second]), "List replacement failed")
	_check(list.get_size() == 2, "List replacement has incorrect size")
	var retained = vm.get_number_property("Test List[0].Nested Number")
	if _check(retained != null, "Bracket list path did not resolve"):
		_check(is_equal_approx(retained.get_value(), 11.0), "Bracket list path resolved wrong child")
		var dotted = vm.get_number_property("Test List.0.Nested Number")
		_check(dotted != null and is_equal_approx(dotted.get_value(), 11.0), "Dotted list path did not resolve")
		_check(list.swap(0, 1), "List swap failed")
		var swapped = vm.get_number_property("Test List[0].Nested Number")
		_check(swapped != null and is_equal_approx(swapped.get_value(), 22.0), "List path cache was stale after swap")
		_check(list.remove_at(1), "List remove failed")
		_check(is_equal_approx(retained.get_value(), 11.0), "Retained child wrapper became invalid after remove")
	_check(not list.remove_at(-1), "Negative list removal accepted")
	_check(not list.insert(9999, first), "Out-of-range list insertion accepted")
	_check(not list.set_items([vm]), "List accepted a reference cycle")
	_check(list.set_items(original_items), "Restoring original list failed")
	_check(control.set_property_value("Test List", []), "Persistent list clearing failed")
	_check(control.load_file() == OK, "Empty list override reload failed")
	_check(control.get_view_model_instance().get_list_property("Test List").get_size() == 0, "Empty list override lost after reload")
	var values: Dictionary = control.get_property_values().duplicate(true)
	values.erase("Test List")
	control.set_property_values(values)
	_check(control.get_view_model_instance().get_list_property("Test List").get_size() == original_size, "Undo list clear did not restore the native list")
	await _frames()

func _test_fonts() -> void:
	var control = _load_fixture("data_bind_font_test.riv", "fontProperty")
	if control == null:
		return
	var font := FontFile.new()
	font.data = FileAccess.get_file_as_bytes(fixture_root.path_join("kablammo.ttf"))
	var alternate := FontFile.new()
	alternate.data = FileAccess.get_file_as_bytes(fixture_root.path_join("nabla.ttf"))
	_check(not font.data.is_empty() and not alternate.data.is_empty(), "Font fixtures missing")
	_check(control.set_property_value("fontProperty", font), "FontFile assignment failed")
	var property = control.get_view_model_instance().get_font_property("fontProperty")
	if not _check(property != null, "Font wrapper missing"):
		return
	_check(property.get_value() == font, "Font wrapper lost FontFile")
	_check(control.set_property_value("fontProperty", alternate), "Alternate font assignment failed")
	_check(property.get_value() == alternate, "Alternate font was not retained")
	_check(not control.set_property_value("fontProperty", "invalid"), "Invalid font value accepted")
	var runtime_font := RiveFont.new()
	_check(runtime_font.load_from_buffer(font.data), "Runtime font decoding failed")
	_check(property.set_value(runtime_font), "VM wrapper did not accept runtime RiveFont")
	var stored: Dictionary = control.get_property_values().duplicate(true)
	_check(not control.set_property_value("fontProperty", runtime_font), "Control persisted a runtime-only RiveFont handle")
	_check(control.get_property_values() == stored, "Rejected runtime font changed persisted override")
	var invalid_bulk: Dictionary = stored.duplicate(true)
	invalid_bulk["fontProperty"] = runtime_font
	control.set_property_values(invalid_bulk)
	_check(control.get_property_values() == stored, "Bulk overrides accepted runtime-only RiveFont")
	_check(control.load_file() == OK, "Font override reload failed")
	property = control.get_view_model_instance().get_font_property("fontProperty")
	_check(property.get_value() == alternate, "Font override lost after reload")
	_check(control.set_property_value("fontProperty", null), "Null font clearing failed")
	_check(property.get_value() == null, "Font was not cleared")
	await _frames()

func _test_artboards() -> void:
	var control = _load_fixture("data_binding_artboards_test.riv", "ab")
	if control == null:
		return
	var property = control.get_view_model_instance().get_artboard_property("ab")
	if not _check(property != null, "Artboard wrapper missing"):
		return
	var default_name: String = property.get_value()
	var choices: PackedStringArray = property.get_artboard_list()
	_check(choices.has("ch1") and choices.has("ch2"), "Artboard choices missing")
	_check(control.set_property_value("ab", "ch1"), "Artboard name assignment failed")
	_check(property.get_value() == "ch1", "Artboard selection was not applied")
	_check(not control.set_property_value("ab", "__missing__"), "Unknown artboard accepted")
	_check(property.get_value() == "ch1", "Invalid artboard changed selection")
	_check(control.set_property_value("ab", ""), "Artboard clearing failed")
	_check(property.get_value().is_empty(), "Artboard was not cleared")
	_check(control.set_property_value("ab", "ch2"), "Artboard reassignment failed")
	_check(control.load_file() == OK, "Artboard override reload failed")
	_check(control.get_view_model_instance().get_artboard_property("ab").get_value() == "ch2", "Artboard override lost after reload")
	control.set_property_values({})
	_check(control.get_view_model_instance().get_artboard_property("ab").get_value() == default_name, "Undo artboard override did not restore default")
	await _frames()

func _roundtrip_scene(control, label: String):
	var packed := PackedScene.new()
	if not _check(packed.pack(control) == OK, label + ": PackedScene.pack failed"):
		return null
	var path := "user://rivegd-view-model-%s-%d.scn" % [label, Time.get_ticks_usec()]
	if not _check(not FileAccess.file_exists(path), "Temporary scene path already exists"):
		return null
	var saved := ResourceSaver.save(packed, path, ResourceSaver.FLAG_COMPRESS)
	if not _check(saved == OK, label + ": ResourceSaver failed"):
		if FileAccess.file_exists(path):
			_check(DirAccess.remove_absolute(ProjectSettings.globalize_path(path)) == OK,
				label + ": Cannot remove the partial temporary scene")
		return null
	var loaded = ResourceLoader.load(path, "PackedScene", ResourceLoader.CACHE_MODE_IGNORE)
	_check(DirAccess.remove_absolute(ProjectSettings.globalize_path(path)) == OK,
		label + ": Cannot remove the temporary scene")
	if not _check(loaded is PackedScene, label + ": ResourceLoader failed"):
		return null
	var instance = loaded.instantiate()
	if not _check(instance is RiveControl, label + ": Instantiated node is not RiveControl"):
		if instance != null:
			instance.free()
		return null
	controls.append(instance)
	root.add_child(instance)
	if not _check(instance.load_file() == OK, label + ": Saved Rive resource failed to load"):
		return null
	return instance

func _test_scene_serialization() -> void:
	var control = _load_fixture("data_bind_test_cmdq.riv", "Test Num")
	if control == null:
		return
	var pixels := Image.create(2, 2, false, Image.FORMAT_RGBA8)
	pixels.fill(Color.GREEN)
	var texture := ImageTexture.create_from_image(pixels)
	var item_color := Color(0.2, 0.4, 0.6, 1.0)
	control.set_property_values({
		"Test Num": 987,
		"Test Enum": "Value 2",
		"Test Image": texture,
		"Test Nested.Nested Number": 654,
		"Test List[0].Color": item_color,
	})
	var restored = _roundtrip_scene(control, "image-scalars")
	if restored != null:
		var vm = restored.get_view_model_instance()
		_check(is_equal_approx(vm.get_number_property("Test Num").get_value(), 987.0), "Scene lost number override")
		_check(vm.get_enum_property("Test Enum").get_value() == 1, "Scene lost enum name override")
		_check(is_equal_approx(vm.get_number_property("Test Nested.Nested Number").get_value(), 654.0), "Scene lost nested scalar override")
		var color = vm.get_color_property("Test List[0].Color")
		if _check(color != null, "Serialized list item path did not resolve"):
			_check(color.get_value().is_equal_approx(item_color), "Scene lost list item color override")
		var image = vm.get_image_property("Test Image").get_value()
		if _check(image is Texture2D, "Scene did not restore image resource"):
			var decoded: Image = image.get_image()
			_check(decoded != null and decoded.get_pixel(0, 0).is_equal_approx(Color.GREEN), "Scene image pixel data changed")
		_check(restored.get_property_values()["Test Image"] == image, "Scene image override was not replayed into native VM")
	_check(control.set_property_value("Test Image", null), "Scene setup could not clear image")
	_check(control.set_property_value("Test List", []), "Scene setup could not clear list")
	var empty = _roundtrip_scene(control, "null-image-empty-list")
	if empty != null:
		var vm = empty.get_view_model_instance()
		_check(vm.get_image_property("Test Image").get_value() == null, "Scene lost null image override")
		_check(vm.get_list_property("Test List").get_size() == 0, "Scene lost empty list override")
		_check(empty.get_property_values().has("Test Image"), "Scene dropped explicit null image override")
	var font_control = _load_fixture("data_bind_font_test.riv", "fontProperty")
	if font_control != null:
		var font := FontFile.new()
		font.data = FileAccess.get_file_as_bytes(fixture_root.path_join("kablammo.ttf"))
		_check(font_control.set_property_value("fontProperty", font), "Scene setup could not assign FontFile")
		var font_scene = _roundtrip_scene(font_control, "font-resource")
		if font_scene != null:
			var loaded_font = font_scene.get_view_model_instance().get_font_property("fontProperty").get_value()
			if _check(loaded_font is FontFile, "Scene did not restore FontFile resource"):
				_check(loaded_font.data == font.data, "Scene changed FontFile byte data")
			_check(font_scene.get_property_values()["fontProperty"] == loaded_font, "Scene FontFile override was not replayed into native VM")
		_check(font_control.set_property_value("fontProperty", null), "Scene setup could not clear font")
		var null_font_scene = _roundtrip_scene(font_control, "null-font")
		if null_font_scene != null:
			_check(null_font_scene.get_view_model_instance().get_font_property("fontProperty").get_value() == null, "Scene lost null font override")
			_check(null_font_scene.get_property_values().has("fontProperty"), "Scene dropped explicit null font override")
	var artboard_control = _load_fixture("data_binding_artboards_test.riv", "ab")
	if artboard_control != null:
		_check(artboard_control.set_property_value("ab", "ch1"), "Scene setup could not assign artboard")
		var artboard_scene = _roundtrip_scene(artboard_control, "artboard-name")
		if artboard_scene != null:
			_check(artboard_scene.get_view_model_instance().get_artboard_property("ab").get_value() == "ch1", "Scene lost artboard name override")
	await _frames()

func _run() -> void:
	await process_frame
	var probe := RiveControl.new()
	var has_api: bool = probe.has_method("set_property_value")
	probe.free()
	if not _check(has_api, "Built extension does not contain the issue #4 property API"):
		quit(1)
		return
	var control = _load_fixture("data_bind_test_cmdq.riv", "Test Num")
	if control != null:
		_test_scalars(control)
		await _test_images(control)
		await _test_lists(control)
	await _test_fonts()
	await _test_artboards()
	await _test_scene_serialization()
	for instance in controls:
		instance.rive_file = null
		instance.queue_free()
	controls.clear()
	await _frames()
	if failures.is_empty():
		print("RIVE_VIEW_MODEL_SMOKE_PASS: enum, image, list, artboard, font, override replay and scene serialization")
		quit(0)
	else:
		print("RIVE_VIEW_MODEL_SMOKE_FAIL: ", failures.size(), " assertions")
		quit(1)
