extends SceneTree

var failures: Array[String] = []
var owned: Array[Node] = []
var fixture_root: String

func _initialize() -> void:
	fixture_root = ProjectSettings.globalize_path("res://../third-party/rive-runtime/tests/unit_tests/assets")
	call_deferred("_run")

func _check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)
		push_error(message)

func _control(asset: String, board := "") -> RiveControl:
	var file := RiveFile.new()
	file.data = FileAccess.get_file_as_bytes(fixture_root.path_join(asset))
	var control := RiveControl.new()
	control.size = Vector2(400, 400)
	control.artboard_name = board
	control.update_mode = RiveControl.UPDATE_MANUAL
	control.rive_file = file
	owned.append(control)
	root.add_child(control)
	_check(control.load_file() == OK, "Cannot load fixture: " + asset)
	return control

func _frames(count := 3) -> void:
	for _frame in count:
		await process_frame
		await RenderingServer.frame_post_draw

func _test_nested_and_volume() -> void:
	var control := _control("runtime_nested_inputs.riv", "MainArtboard")
	_check(control.set_input_value("CircleOuterState", true, "CircleOuter"), "Nested bool rejected")
	_check(control.get_input_value("CircleOuterState", "CircleOuter") == true, "Nested bool readback")
	_check(control.set_input_value("CircleOuterNumber", 42.5, "CircleOuter"), "Nested number rejected")
	_check(is_equal_approx(float(control.get_input_value("CircleOuterNumber", "CircleOuter")), 42.5), "Nested number readback")
	_check(not control.set_input_value("CircleOuterState", 1, "CircleOuter"), "Wrong nested bool type accepted")
	_check(not control.set_input_value("CircleOuterNumber", INF, "CircleOuter"), "Nonfinite nested input accepted")
	_check(control.set_input_value("CircleInnerState", true, "CircleOuter/CircleInner"), "Deep nested bool rejected")
	var player := control.get_player()
	_check(player.fire_input("CircleOuterTrigger", "CircleOuter"), "Nested trigger rejected")
	_check(not player.fire_input("CircleOuterState", "CircleOuter"), "Nontrigger fired through unsafe cast")
	_check(control.get_input_list("CircleOuter").size() >= 3, "Nested inputs not enumerated")
	_check(control.get_input_value("CircleOuterState", "missing") == null, "Invalid path aliased a root input")
	control.volume = 0.25
	_check(is_equal_approx(player.volume, 0.25), "Control volume did not reach Player")
	var second := _control("runtime_nested_inputs.riv", "MainArtboard")
	_check(is_equal_approx(second.get_player().volume, 1.0), "Artboard volume leaked to another instance")
	player.volume = NAN
	_check(is_equal_approx(player.volume, 0.25), "Invalid volume poisoned native state")
	print("RUNTIME_API_NESTED_VOLUME_PASS")

func _test_playback() -> void:
	var control := _control("timeline_event_test.riv")
	var player := control.get_player()
	var names := player.get_animation_list()
	_check(not names.is_empty(), "Missing animation fixture")
	if names.is_empty(): return
	player.play_animation(names[0])
	_check(player.set_animation_loop(0), "Cannot override loop")
	_check(player.set_animation_direction(-1), "Cannot reverse animation")
	control.seek(minf(0.5, control.get_duration() * 0.5))
	var before := control.get_time()
	control.advance(0.1)
	print("RUNTIME_API_REVERSE_PROBE: name=", names[0], " duration=", control.get_duration(), " before=", before, " after=", control.get_time(), " direction=", player.get_animation_direction())
	_check(control.get_time() < before, "Reverse advance did not reduce timeline time")
	_check(not player.set_animation_loop(5), "Invalid loop accepted")
	_check(not player.set_animation_direction(0), "Invalid direction accepted")
	_check(player.get_animation_direction() == -1, "Rejected direction changed state")
	player.play_animation(names[0])
	_check(is_equal_approx(player.get_time(), player.get_duration()), "A new reverse animation did not start at its end")
	player.set_animation_direction(1)
	player.set_animation_loop(1)
	control.seek(maxf(0, control.get_duration() - 0.05))
	control.advance(0.1)
	_check(control.get_time() < control.get_duration(), "Runtime loop override did not wrap")
	_check(player.set_animation_layers([{"name": names[0], "time": 0.7, "weight": 0.5, "speed": 0.5}]), "Cannot install a native animation layer")
	control.advance(0.1)
	var layers := player.get_animation_layers()
	_check(layers.size() == 1 and is_equal_approx(layers[0].time, 0.75), "Layer did not advance at its independent speed")
	_check(not player.set_animation_layers([{"name": "__missing__"}]), "Missing animation layer accepted")
	_check(player.get_animation_layers().size() == 1, "Failed layer validation destroyed the working mix")
	player.reset()
	_check(is_equal_approx(player.get_animation_layers()[0].time, 0.0), "Reset left overlay time running")
	_check(player.cross_fade_animation(names[0], 0.25), "Cannot cross-fade linear animations")
	control.advance(0.125)
	control.advance(0.125)
	_check(player.get_animation_layers().is_empty(), "Cross-fade retained stale overlay layers")
	_check(not player.cross_fade_animation("__missing__", 0.5), "Cross-fade accepted missing animation")
	print("RUNTIME_API_PLAYBACK_PASS")

func _test_components() -> void:
	var control := _control("two_bone_ik.riv")
	var player := control.get_player()
	var bones := 0
	for item: Dictionary in player.get_components():
		if item.get("kind") == "bone":
			bones += 1
			_check(item.get("world_transform") is Transform2D, "Bone has no transform snapshot")
			_check(item.get("tip_position") is Vector2, "Bone has no tip position")
	_check(bones >= 2, "Bone fixture was not discoverable")
	_check(player.get_component("__missing__").is_empty(), "Missing component returned unrelated data")
	var stick_control := _control("joystick_flag_test.riv")
	var stick_player := stick_control.get_player()
	var sticks := 0
	for item: Dictionary in stick_player.get_components():
		if item.get("kind") == "joystick":
			sticks += 1
			_check(stick_player.set_joystick(item.name, Vector2(0.3, -0.4)), "Joystick setter rejected")
			_check(Vector2(stick_player.get_component(item.name).value).is_equal_approx(Vector2(0.3, -0.4)), "Joystick value snapshot differs")
	_check(sticks > 0, "Joystick fixture had no discoverable joystick")
	_check(not stick_player.set_joystick("__missing__", Vector2.ZERO), "Missing joystick accepted")
	_check(not stick_player.set_joystick("bad", Vector2(INF, 0)), "Nonfinite joystick accepted")
	print("RUNTIME_API_COMPONENTS_PASS")

func _test_gamepad() -> void:
	var control := _control("gamepad_test.riv")
	var player := control.get_player()
	var button := InputEventJoypadButton.new()
	button.device = 7
	button.button_index = JOY_BUTTON_LEFT_SHOULDER
	button.pressed = true
	_check(player.gamepad_input(button), "Native gamepad rejected valid Godot event")
	var state := player.get_gamepad_state(7)
	_check(not state.is_empty() and state.buttons[4] == 1.0, "Godot shoulder was not remapped to Rive standard")
	var motion := InputEventJoypadMotion.new()
	motion.device = 7
	motion.axis = JOY_AXIS_LEFT_X
	motion.axis_value = 0.625
	_check(player.gamepad_input(motion), "Native axis event rejected")
	_check(is_equal_approx(player.get_gamepad_state(7).axes[0], 0.625), "Axis snapshot differs")
	motion.axis = JOY_AXIS_TRIGGER_LEFT
	motion.axis_value = 0.75
	_check(player.gamepad_input(motion), "Trigger axis rejected")
	_check(is_equal_approx(player.get_gamepad_state(7).buttons[6], 0.75), "Godot trigger did not reach Rive analog button")
	_check(player.set_gamepad_connected(7, false), "Native disconnect rejected")
	_check(player.get_gamepad_state(7).is_empty(), "Disconnected state survived")
	_check(not player.set_gamepad_connected(-1, true), "Negative device accepted")
	_check(not player.send_scroll(Vector2.ZERO, Vector2.ZERO, 9), "Invalid scroll phase accepted")
	_check(not player.send_pointer(Vector2(INF, 0), 0), "Invalid pointer accepted")
	_check(not player.fire_semantic_action(-1, 0), "Invalid semantic ID accepted")
	control.advance(0)
	print("RUNTIME_API_GAMEPAD_PASS")

func _test_font_and_text() -> void:
	var font := RiveFont.new()
	_check(font.load_from_file("res://assets/fonts/InterVariable.ttf"), "Cannot load font")
	var axes := font.get_axes()
	_check(not axes.is_empty(), "Variable font axes missing")
	var configured := font.with_options({"wght": 650.0}, {"liga": false})
	_check(configured != null and configured.is_loaded(), "Cannot configure font options")
	_check(font.with_options({"bad": 1.0}) == null, "Invalid font axis tag accepted")
	_check(font.with_options({"wght": INF}) == null, "Nonfinite font axis accepted")
	_check(RiveFont.set_fallback_fonts([font]), "Font fallback setup failed")
	var arabic := RiveFont.new()
	_check(arabic.load_from_file(fixture_root.path_join("IBMPlexSansArabic-Regular.ttf")), "Cannot load upstream fallback font")
	_check(RiveFont.set_fallback_fonts([arabic]), "Cannot install Arabic fallback")
	var shaped := font.shape_text("مرحبا", 24.0)
	_check(not shaped.is_empty(), "Fallback produced no shaped glyphs")
	for glyph: Dictionary in shaped:
		_check(glyph.glyph_id != 0, "Missing Arabic glyph was not resolved by the fallback chain")
	var text := RiveText.new()
	var paint := RivePaint.new()
	paint.set_color(Color.WHITE)
	text.append_run("alpha beta gamma delta epsilon", font, paint, 24.0, -1.0, 0.0)
	text.set_max_width(130)
	text.set_sizing(1)
	text.set_direction(0)
	text.set_logical_align(1)
	text.set_word_break(2)
	_check(text.get_lines().size() >= 2, "Wrapped native line layout was not exposed")
	_check(text.get_direction() == 0 and text.get_logical_align() == 1, "Text options did not round-trip")
	RiveFont.clear_fallback_fonts()
	print("RUNTIME_API_FONT_TEXT_PASS")

func _test_blob_and_selection() -> void:
	var control := _control("data_bind_blob_test.riv")
	var model := control.get_view_model_instance()
	_check(model != null, "Blob fixture has no ViewModel")
	if model != null:
		var blob := model.get_blob_property("xml")
		_check(blob != null, "Blob typed wrapper missing")
		if blob != null:
			var bytes := "<test>héllo 世界</test>".to_utf8_buffer()
			_check(blob.set_value(bytes), "Cannot assign blob bytes")
			_check(blob.get_value() == bytes and model.get_property_value("xml") == bytes, "Blob byte readback differs")
			_check(not model.set_property_value("xml", "not bytes"), "Blob accepted a String")
			blob.clear()
			_check(blob.get_value().is_empty(), "Blob clear left bytes behind")
	var text_control := _control("new_text.riv")
	var player := text_control.get_player()
	var enabled := 0
	for item: Dictionary in player.get_components():
		if player.set_text_selectable(item.name, true): enabled += 1
	var targets := player.get_selectable_texts()
	_check(enabled > 0 and not targets.is_empty(), "Text selection discovery did not register regular text")
	if not targets.is_empty():
		var target: Dictionary = targets[0]
		var length := mini(3, String(target.text).length())
		_check(length > 0 and player.set_text_selection(target.id, 0, target.id, length), "Regular text range rejected")
		_check(player.get_selected_text() == String(target.text).substr(0, length), "Regular text copy differs")
		player.clear_text_selection()
		_check(player.get_selected_text().is_empty(), "Selection clear retained copied text")
	_check(not player.set_text_selection(-1, 0, -1, 1), "Invalid selection token accepted")
	_check(player.get_text_input_state().is_empty(), "Unfocused non-input reported an active editor")
	print("RUNTIME_API_BLOB_SELECTION_PASS")

func _key(player: RivePlayer, code: Key, ctrl := false, shift := false) -> void:
	var event := InputEventKey.new()
	event.keycode = code
	event.pressed = true
	event.ctrl_pressed = ctrl and not OS.has_feature("macos")
	event.meta_pressed = ctrl and OS.has_feature("macos")
	event.shift_pressed = shift
	player.key_input(event)

func _test_list_index() -> void:
	var file := RiveFile.new()
	file.data = FileAccess.get_file_as_bytes(fixture_root.path_join("list_index_script_access.riv"))
	var found := 0
	for name: String in file.get_view_model_names():
		var model := file.instantiate_view_model(name)
		if model == null: continue
		for property: Dictionary in model.get_properties():
			if property.type != "list_index": continue
			var index := model.get_list_index_property(property.name)
			_check(index != null and property.read_only, "Missing readonly ListIndex wrapper")
			if index != null:
				_check(model.get_property_value(property.name) == index.get_value(), "ListIndex generic/typed getters differ")
				_check(not model.set_property_value(property.name, 123), "Read-only ListIndex accepted a setter")
				found += 1
	_check(found > 0, "Upstream ListIndex properties were not discoverable")
	print("RUNTIME_API_LIST_INDEX_PASS")

func _test_ime() -> void:
	var control := _control("text_input.riv", "Text Input - Multiline")
	var player := control.get_player()
	for _i in 12:
		if not player.get_text_input_state().is_empty(): break
		player.focus_next()
	var initial := player.get_text_input_state()
	_check(not initial.is_empty(), "No focused native TextInput in IME fixture")
	if initial.is_empty(): return
	_key(player, KEY_A, true)
	_check(player.text_input("before"), "Cannot initialize IME input")
	_key(player, KEY_A, true)
	_check(player.update_text_composition("ni", Vector2i(1, 1)), "Native IME preedit rejected")
	var state := player.get_text_input_state()
	_check(state.text == "before" and state.display_text == "ni" and state.composing, "Preedit changed committed text or did not replace the selected range")
	_check(player.update_text_composition("你好🙂", Vector2i(2, 1)), "Unicode preedit selection rejected")
	state = player.get_text_input_state()
	_check(state.text == "before" and state.display_text == "你好🙂", "Unicode composition duplicated earlier previews")
	_check(not player.update_text_composition("x", Vector2i(-1, 0)), "Invalid composition range accepted")
	_check(state.get("caret_rect") is Rect2, "Native input did not expose a candidate caret")
	player.cancel_text_composition()
	state = player.get_text_input_state()
	_check(not state.composing and state.display_text == "before" and state.selection_from == 0 and state.selection_to == 6, "Composition cancellation lost source text or selection")
	_key(player, KEY_Z, true)
	_check(player.get_text_input_state().text == initial.text, "Cancelled preedit polluted the native undo journal")
	_key(player, KEY_Z, true, true)
	_check(player.get_text_input_state().text == "before", "Native redo lost its original entry")
	_key(player, KEY_A, true)
	player.update_text_composition("candidate", Vector2i(0, 9))
	player.text_input("你好🙂")
	state = player.get_text_input_state()
	_check(not state.composing and state.text == "你好🙂", "Composition commit inserted preview twice")
	_key(player, KEY_Z, true)
	_check(player.get_text_input_state().text == "before", "IME commit was not one undoable replacement")
	player.update_text_composition("cancel on blur", Vector2i(0, 0))
	player.clear_focus()
	_check(player.get_text_input_state().is_empty(), "Blur retained an active composition target")
	print("RUNTIME_API_IME_PASS")

func _run() -> void:
	create_timer(30).timeout.connect(func() -> void:
		push_error("Runtime API watchdog timed out")
		quit(2))
	await _frames()
	_test_nested_and_volume()
	_test_playback()
	_test_components()
	_test_gamepad()
	_test_font_and_text()
	_test_blob_and_selection()
	_test_list_index()
	_test_ime()
	await _frames()
	for node in owned: node.queue_free()
	owned.clear()
	await _frames(4)
	if failures.is_empty(): print("RUNTIME_API_SMOKE_PASS")
	else: push_error("RUNTIME_API_SMOKE_FAIL: " + str(failures))
	quit(0 if failures.is_empty() else 1)
