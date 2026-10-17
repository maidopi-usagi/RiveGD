extends SceneTree

const DEMO_SCENE := "res://neumorphism_demo.tscn"
const WATCHDOG_MSEC := 45000
const SAMPLE_COLUMNS := 80
const SAMPLE_ROWS := 52
const INITIAL_ROBOT_OFFSET_FROM_PLAY := Vector2(0.0, -232.0)
const ROBOT_CROWN_LOCAL_BOUNDS := Rect2(-35.0, -83.0, 70.0, 31.0)

var output_directory: String
var failures: Array[String] = []
var started_msec := 0
var finished := false
var demo: Node
var screenshot_paths: Array[String] = []

func _initialize() -> void:
	started_msec = Time.get_ticks_msec()
	var arguments := OS.get_cmdline_user_args()
	if arguments.is_empty() or not arguments[0].is_absolute_path():
		push_error("Pass an absolute temporary screenshot directory after --")
		quit(1)
		return
	output_directory = arguments[0]
	if DirAccess.make_dir_recursive_absolute(output_directory) != OK:
		push_error("Cannot create screenshot directory: " + output_directory)
		quit(1)
		return
	if DisplayServer.get_name() == "headless":
		push_error("Neumorphism pixel tests require a real GPU viewport, not --headless")
		quit(1)
		return
	call_deferred("_run")

func _process(_delta: float) -> bool:
	if not finished and Time.get_ticks_msec() - started_msec > WATCHDOG_MSEC:
		push_error("NEUMORPHISM_SMOKE_TIMEOUT: exceeded 45 seconds")
		finished = true
		if is_instance_valid(demo):
			demo.queue_free()
		quit(2)
	return false

func _check(condition: bool, message: String) -> bool:
	if not condition:
		failures.append(message)
		push_error(message)
	return condition

func _frames(count := 2) -> void:
	for _index in mini(count, 180):
		await process_frame
		await RenderingServer.frame_post_draw

func _state() -> Dictionary:
	var result: Variant = demo.call("get_demo_state")
	if not _check(result is Dictionary, "get_demo_state() did not return a Dictionary"):
		return {}
	return result

func _action(action: String, value: Variant = null) -> void:
	var accepted: Variant = demo.call("set_demo_action", action, value)
	_check(accepted == true, "Demo rejected its documented action hook: " + action)

func _sample_color(image: Image, column: int, row: int) -> Color:
	var x := clampi(int((column + 0.5) * image.get_width() / SAMPLE_COLUMNS), 0, image.get_width() - 1)
	var y := clampi(int((row + 0.5) * image.get_height() / SAMPLE_ROWS), 0, image.get_height() - 1)
	return image.get_pixel(x, y)

func _non_flat(image: Image) -> bool:
	var colors := {}
	var darkest := 1.0
	var brightest := 0.0
	for row in SAMPLE_ROWS:
		for column in SAMPLE_COLUMNS:
			var color := _sample_color(image, column, row)
			var key := Vector3i(int(color.r * 63.0), int(color.g * 63.0), int(color.b * 63.0))
			colors[key] = true
			var luminance := color.get_luminance()
			darkest = minf(darkest, luminance)
			brightest = maxf(brightest, luminance)
	print("NEUMORPHISM_GPU_CONTENT: sampled_colors=", colors.size(), ", luminance_span=", brightest - darkest)
	return colors.size() >= 16 and brightest - darkest > 0.12

func _changed_samples(before: Image, after: Image) -> int:
	if before.get_size() != after.get_size():
		return -1
	var changed := 0
	for row in SAMPLE_ROWS:
		for column in SAMPLE_COLUMNS:
			var first := _sample_color(before, column, row)
			var second := _sample_color(after, column, row)
			var distance := absf(first.r - second.r) + absf(first.g - second.g) + absf(first.b - second.b)
			if distance > 0.025:
				changed += 1
	return changed

func _capture(label: String) -> Image:
	await _frames(3)
	var image := root.get_texture().get_image()
	if not _check(image != null and not image.is_empty(), "GPU readback is empty: " + label):
		return null
	var path := output_directory.path_join("neumorphism_" + label + ".png")
	if _check(image.save_png(path) == OK, "Cannot save GPU screenshot: " + path):
		screenshot_paths.append(path)
		print("NEUMORPHISM_SCREENSHOT: ", path)
	return image

func _check_robot_crown(image: Image, initial_state: Dictionary) -> void:
	var scale := float(initial_state.get("viewport_scale", 0.0))
	if not _check(scale > 0.0, "Robot crown probe has no valid viewport scale"):
		return
	_check(is_equal_approx(float(initial_state.get("rendered_preview_scale", -1.0)), 1.0),
		"Robot crown regression expects the authored initial unit preview scale")
	var play_rectangle := _hit_rect("play")
	if not play_rectangle.has_area():
		return
	var center := play_rectangle.get_center() + INITIAL_ROBOT_OFFSET_FROM_PLAY * scale
	var region := Rect2(center + ROBOT_CROWN_LOCAL_BOUNDS.position * scale, ROBOT_CROWN_LOCAL_BOUNDS.size * scale)
	if not _check(Rect2(Vector2.ZERO, Vector2(image.get_size())).encloses(region),
			"Robot crown pixel probe falls outside the captured viewport"):
		return
	var blue_pixels := 0
	for y in range(floori(region.position.y), ceili(region.end.y)):
		for x in range(floori(region.position.x), ceili(region.end.x)):
			var color := image.get_pixel(x, y)
			if color.b > color.r + 0.08 and color.g > color.r + 0.10:
				blue_pixels += 1
	print("NEUMORPHISM_ROBOT_CROWN: blue_pixels=", blue_pixels, ", region=", region)
	_check(blue_pixels > 20, "Godot robot crown teeth are missing from the real GPU silhouette: blue_pixels=" + str(blue_pixels))

func _mouse_motion(position: Vector2, dragging := false) -> void:
	var event := InputEventMouseMotion.new()
	event.position = position
	event.global_position = position
	event.button_mask = MOUSE_BUTTON_MASK_LEFT if dragging else 0
	Input.parse_input_event(event)

func _mouse_button(position: Vector2, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.position = position
	event.global_position = position
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = pressed
	event.button_mask = MOUSE_BUTTON_MASK_LEFT if pressed else 0
	Input.parse_input_event(event)

func _hit_rect(action: String) -> Rect2:
	var result: Variant = demo.call("get_demo_hit_rect", action)
	if not _check(result is Rect2, "Missing viewport hit rectangle for " + action):
		return Rect2()
	var rectangle: Rect2 = result
	_check(rectangle.has_area() and Rect2(Vector2.ZERO, Vector2(root.size)).encloses(rectangle),
		"Hit rectangle is empty or outside the viewport: " + action + " " + str(rectangle))
	return rectangle

func _click(action: String) -> void:
	var rectangle := _hit_rect(action)
	if not rectangle.has_area():
		return
	var position := rectangle.get_center()
	_mouse_motion(position)
	_mouse_button(position, true)
	await _frames()
	_mouse_button(position, false)
	await _frames()

func _test_actions() -> void:
	_action("play", true)
	_action("motion", true)
	_action("node2d")
	_action("idle")
	_action("advance", 1.0)
	var before := _state()
	_check(str(before.get("selected_node", "")) == "Node2D", "Node hook did not select Node2D")
	_check(str(before.get("animation_name", "")) == "Idle", "Animation hook did not select Idle")
	var first: Image = await _capture("playing_node2d_before")
	_action("advance", 1.25)
	var after := _state()
	_check(is_equal_approx(float(after.get("elapsed", -1.0)), float(before.get("elapsed", -1.0)) + 1.25),
		"Hook advance did not advance the playing clock by its supplied delta")
	_check(is_equal_approx(float(after.get("wave_time", -1.0)), float(before.get("wave_time", -1.0)) + 1.25),
		"Playing motion did not advance the waveform clock")
	var second: Image = await _capture("playing_node2d_after")
	if first != null and second != null:
		var changed := _changed_samples(first, second)
		_check(changed >= 8, "Playing animation produced too little pixel change: " + str(changed))
		print("NEUMORPHISM_HOOK_ANIMATION: changed_samples=", changed)
	_action("play", false)
	var paused_elapsed := float(_state().get("elapsed", -1.0))
	var paused_wave := float(_state().get("wave_time", -1.0))
	_action("advance", 0.75)
	_check(not bool(_state().get("playing", true)), "Play/pause hook did not pause")
	_check(is_equal_approx(float(_state().get("elapsed", -2.0)), paused_elapsed), "Paused hook advanced the playback clock")
	_check(is_equal_approx(float(_state().get("wave_time", -2.0)), paused_wave), "Paused hook advanced the waveform clock")
	_action("control")
	_action("scale", -0.2)
	_check(is_equal_approx(float(_state().get("preview_scale", -1.0)), 0.6), "Scale hook did not clamp its lower bound")
	_action("scale", 1.2)
	_check(is_equal_approx(float(_state().get("preview_scale", -1.0)), 1.4), "Scale hook did not clamp its upper bound")
	_action("scale", 0.23)
	_action("spin")
	_action("motion", false)
	_action("advance", 1.0)
	var changed_state := _state()
	_check(str(changed_state.get("selected_node", "")) == "Control", "Node hook did not select Control")
	_check(is_equal_approx(float(changed_state.get("preview_scale", -1.0)), 0.784), "Scale hook did not apply the real preview scale")
	_check(absf(float(changed_state.get("rendered_preview_scale", -1.0)) - 0.784) < 0.003,
		"The actual robot draw scale did not converge to the requested scale")
	_check(str(changed_state.get("animation_name", "")) == "Spin", "Animation hook did not select Spin")
	_check(not bool(changed_state.get("motion_enabled", true)), "Motion switch hook did not disable motion")
	_action("play", true)
	var disabled_wave := float(_state().get("wave_time", -1.0))
	_action("advance", 0.25)
	_check(is_equal_approx(float(_state().get("wave_time", -2.0)), disabled_wave),
		"Disabling motion did not freeze the robot animation clock while playing")
	_action("next")
	_check(str(_state().get("animation_name", "")) == "Idle", "Next hook did not wrap the animation selection")
	_action("previous")
	_check(str(_state().get("animation_name", "")) == "Spin", "Previous hook did not wrap the animation selection")
	_action("reset")
	var reset_state := _state()
	_check(not bool(reset_state.get("playing", true)) and is_zero_approx(float(reset_state.get("elapsed", -1.0))),
		"Reset hook did not stop and reset the playback clock")
	await _capture("paused_control")
	print("NEUMORPHISM_ACTION_HOOKS: ", reset_state)

func _test_animation_choices() -> void:
	_action("node2d")
	_action("scale", 0.64)
	_action("motion", true)
	_action("play", true)
	var images: Array[Image] = []
	for animation: String in ["idle", "hop", "spin"]:
		_action(animation)
		_action("advance", 0.37)
		_check(str(_state().get("animation_name", "")) == animation.capitalize(),
			"Animation selector disagrees with the displayed animation: " + animation)
		var image: Image = await _capture("robot_" + animation)
		if image != null:
			_check(_non_flat(image), "Robot animation capture is flat: " + animation)
		images.append(image)
	for pair: Vector2i in [Vector2i(0, 1), Vector2i(1, 2), Vector2i(0, 2)]:
		if images[pair.x] != null and images[pair.y] != null:
			var changed := _changed_samples(images[pair.x], images[pair.y])
			_check(changed >= 8, "Idle/Hop/Spin produced indistinguishable GPU frames: " + str(pair) + " samples=" + str(changed))
			print("NEUMORPHISM_ROBOT_ANIMATIONS: pair=", pair, ", changed_samples=", changed)

func _test_mouse_input() -> void:
	_action("play", false)
	await _click("play")
	_check(bool(_state().get("playing", false)), "Injected mouse click did not activate the real play handler")
	_action("node2d")
	await _click("control")
	_check(str(_state().get("selected_node", "")) == "Control", "Injected mouse click did not switch Node2D to Control")
	await _click("hop")
	_check(str(_state().get("animation_name", "")) == "Hop", "Injected mouse click did not activate the real Hop handler")
	var rectangle := _hit_rect("scale")
	if rectangle.has_area():
		var start := Vector2(rectangle.position.x + rectangle.size.x * 0.25, rectangle.get_center().y)
		var end := Vector2(rectangle.position.x + rectangle.size.x * 0.75, rectangle.get_center().y)
		_mouse_motion(start)
		_mouse_button(start, true)
		await _frames()
		var low := float(_state().get("preview_scale", -1.0))
		_mouse_motion(end, true)
		await _frames()
		_mouse_button(end, false)
		await _frames()
		var high := float(_state().get("preview_scale", -1.0))
		_check(low >= 0.6 and high <= 1.4 and high - low > 0.24,
			"Injected real scale drag did not resize the preview: " + str(low) + " -> " + str(high))
	_action("advance", 0.7)
	var mouse_state := _state()
	_check(absf(float(mouse_state.get("rendered_preview_scale", -1.0)) - float(mouse_state.get("preview_scale", -2.0))) < 0.003,
		"Mouse scale drag changed its value but not the actual robot draw scale")
	print("NEUMORPHISM_REAL_MOUSE_INPUT: ", _state())
	await _capture("mouse_interaction")

func _key_press(code: Key) -> void:
	for pressed: bool in [true, false]:
		var event := InputEventKey.new()
		event.keycode = code
		event.pressed = pressed
		Input.parse_input_event(event)
		await _frames()

func _touch(position: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = 7
	event.position = position
	event.pressed = pressed
	Input.parse_input_event(event)

func _test_keyboard_and_touch() -> void:
	var playing := bool(_state().get("playing", false))
	await _key_press(KEY_SPACE)
	_check(bool(_state().get("playing", false)) != playing, "Real Space key did not toggle robot playback")
	await _click("scale")
	var keyboard_before := float(_state().get("preview_scale", -1.0))
	await _key_press(KEY_RIGHT)
	_check(float(_state().get("preview_scale", -1.0)) > keyboard_before + 0.01,
		"Real Right key did not increase the focused preview scale")
	await _key_press(KEY_LEFT)
	_check(is_equal_approx(float(_state().get("preview_scale", -1.0)), keyboard_before),
		"Real Left key did not restore the focused preview scale")
	var spin_rect := _hit_rect("spin")
	if spin_rect.has_area():
		_touch(spin_rect.get_center(), true)
		await _frames()
		_touch(spin_rect.get_center(), false)
		await _frames()
		_check(str(_state().get("animation_name", "")) == "Spin", "Real touch did not select Spin")
	var scale_rect := _hit_rect("scale")
	if scale_rect.has_area():
		var start := Vector2(scale_rect.position.x + scale_rect.size.x * 0.3, scale_rect.get_center().y)
		var end := Vector2(scale_rect.position.x + scale_rect.size.x * 0.7, scale_rect.get_center().y)
		_touch(start, true)
		await _frames()
		var before := float(_state().get("preview_scale", -1.0))
		var drag := InputEventScreenDrag.new()
		drag.index = 7
		drag.position = end
		drag.relative = end - start
		Input.parse_input_event(drag)
		await _frames()
		_touch(end, false)
		await _frames()
		_check(float(_state().get("preview_scale", -1.0)) > before + 0.2,
			"Real touch drag did not increase the robot preview scale")
	_action("advance", 0.7)
	print("NEUMORPHISM_REAL_KEYBOARD_TOUCH: ", _state())
	await _capture("keyboard_touch")

func _run() -> void:
	await process_frame
	var packed := load(DEMO_SCENE) as PackedScene
	if not _check(packed != null, "Cannot load procedural demo scene: " + DEMO_SCENE):
		_finish()
		return
	demo = packed.instantiate()
	if not _check(demo != null, "Cannot instantiate the procedural demo scene"):
		_finish()
		return
	root.add_child(demo)
	for method in ["get_demo_state", "set_demo_action", "get_demo_hit_rect"]:
		if not _check(demo.has_method(method), "Demo is missing test interface: " + method):
			await _cleanup()
			_finish()
			return
	demo.set_process(false)
	await _frames(5)
	var initial := _state()
	_check(bool(initial.get("resources_ready", false)), "Rive vector/font resources were not created")
	_check(int(initial.get("draw_count", 0)) > 0, "Procedural Rive draw callback did not run")
	var first: Image = await _capture("initial")
	if first != null:
		_check(_non_flat(first), "GPU output contains only a flat or nearly empty background")
		_check_robot_crown(first, initial)
	await _test_actions()
	await _test_animation_choices()
	await _test_mouse_input()
	await _test_keyboard_and_touch()
	var previous_scale := float(_state().get("viewport_scale", 0.0))
	root.size = Vector2i(960, 720)
	var resized: Image = await _capture("resized_960x720")
	_check(root.size == Vector2i(960, 720), "Window did not resize to the requested viewport")
	if resized != null:
		_check(resized.get_size() == Vector2i(960, 720), "GPU readback retained the old viewport size")
		_check(_non_flat(resized), "Resized viewport lost the procedural Rive contents")
	var resized_scale := float(_state().get("viewport_scale", 0.0))
	_check(resized_scale > 0.0 and not is_equal_approx(resized_scale, previous_scale), "Demo did not adapt its layout scale after resize")
	_action("node2d")
	await _click("control")
	_check(str(_state().get("selected_node", "")) == "Control", "Resized hit mapping did not switch Node2D to Control")
	await _cleanup()
	_finish()

func _cleanup() -> void:
	if not is_instance_valid(demo):
		return
	var watched: Array[Dictionary] = []
	for node in [demo, demo.get_node_or_null("Canvas"), demo.get_node_or_null("Canvas/Raw")]:
		if node != null:
			watched.append({"reference": weakref(node), "label": str(node.get_path())})
	demo.queue_free()
	demo = null
	await _frames(4)
	for probe in watched:
		_check(probe["reference"].get_ref() == null, "Demo node survived queue_free(): " + str(probe["label"]))

func _finish() -> void:
	finished = true
	if failures.is_empty():
		print("NEUMORPHISM_SMOKE_PASS: real GPU robot vectors, Idle/Hop/Spin readback, node/scale action hooks, actual mouse/keyboard/touch input, resize, scene teardown; screenshots=", screenshot_paths)
	else:
		print("NEUMORPHISM_SMOKE_FAIL: ", failures)
	quit(0 if failures.is_empty() else 1)
