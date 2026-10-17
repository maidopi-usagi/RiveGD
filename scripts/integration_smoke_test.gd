extends SceneTree

var failures: Array[String] = []
var owned: Array[Node] = []
var fixture_root: String
var events: Array[Dictionary] = []
var trigger_count := 0

class TriggerTarget extends Node:
	var hits := 0
	func on_rive_trigger() -> void:
		hits += 1

class ScriptBindingSource extends Node:
	var model
	var binding_to_free: Node
	func get_view_model_instance():
		if is_instance_valid(binding_to_free):
			binding_to_free.free()
			binding_to_free = null
		return model

class ScriptBindingTarget extends Node:
	var binding_to_free: Node
	var value := 123.0:
		get:
			if is_instance_valid(binding_to_free):
				binding_to_free.free()
				binding_to_free = null
			return value
	signal value_changed(value)

func _initialize() -> void:
	fixture_root = ProjectSettings.globalize_path("res://").path_join("../third-party/rive-runtime/tests/unit_tests/assets")
	call_deferred("_run")

func _check(value: bool, message: String) -> bool:
	if not value:
		failures.append(message)
		push_error(message)
	return value

func _frames(count := 3) -> void:
	for _i in count:
		await process_frame
		await RenderingServer.frame_post_draw

func _file(name: String):
	var file := RiveFile.new()
	file.data = FileAccess.get_file_as_bytes(fixture_root.path_join(name))
	if not _check(not file.data.is_empty(), "Missing fixture: " + name):
		return null
	return file

func _control(file, artboard := ""):
	var control := RiveControl.new()
	control.size = Vector2(160, 120)
	control.artboard_name = artboard
	control.rive_file = file
	owned.append(control)
	root.add_child(control)
	_check(control.load_file() == OK, "Cannot load Control")
	return control

func _on_event(name: String, payload: Dictionary) -> void:
	events.append({"name": name, "payload": payload})

func _on_trigger() -> void:
	trigger_count += 1

func _test_config_and_playback() -> void:
	var file = _file("timeline_event_test.riv")
	if file == null:
		return
	var names: PackedStringArray = file.get_animation_names()
	if not _check(not names.is_empty(), "Timeline metadata missing"):
		return
	var control := RiveControl.new()
	control.size = Vector2(128, 128)
	control.animation_name = names[0]
	control.rive_file = file
	control.update_mode = RiveControl.UPDATE_MANUAL
	control.playing = false
	owned.append(control)
	root.add_child(control)
	_check(control.get_player().get_animation_name() == names[0], "Pre-tree animation selection was lost")
	control.advance(0.25)
	_check(control.get_time() > 0.0, "Manual advance must work while paused")
	_check(control.seek(0.1), "Linear animation seek failed")
	_check(is_equal_approx(control.get_time(), 0.1), "Seek did not set timeline time")
	await _frames()
	_check(is_equal_approx(control.get_time(), 0.1), "MANUAL/paused playback advanced automatically")
	var scene := PackedScene.new()
	_check(scene.pack(control) == OK, "Cannot pack playback configuration")
	var restored = scene.instantiate()
	owned.append(restored)
	root.add_child(restored)
	_check(restored.animation_name == names[0] and restored.get_player().get_animation_name() == names[0], "Scene did not restore selected animation")
	_check(not restored.playing and restored.update_mode == RiveControl.UPDATE_MANUAL, "Scene did not restore playback options")
	control.stop()
	_check(not control.playing, "Stop did not pause playback")
	control.play()
	_check(control.playing, "Play did not resume playback")

func _test_layout() -> void:
	var file = _file("data_bind_test_cmdq.riv")
	if file == null:
		return
	var control = _control(file)
	control.fit = RiveControl.FIT_LAYOUT
	control.layout_scale = 2.0
	control.size = Vector2(240, 160)
	await _frames()
	_check(control.get_layout_size().is_equal_approx(Vector2(120, 80)), "Layout did not receive Control size / layout scale")
	var authored: Vector2 = control.get_intrinsic_size()
	control.expand_mode = RiveControl.EXPAND_KEEP_SIZE
	_check(control.get_combined_minimum_size().is_equal_approx(authored), "Intrinsic minimum size changed with live layout")
	control.expand_mode = RiveControl.EXPAND_IGNORE_SIZE
	control.fit = RiveControl.FIT_CONTAIN
	_check(control.get_layout_size().is_equal_approx(authored), "Leaving layout did not restore authored dimensions")

func _test_bindings() -> void:
	var file = _file("data_bind_test_cmdq.riv")
	if file == null:
		return
	var control = _control(file)
	var vm = control.get_view_model_instance()
	if vm == null or not vm.has_property("Test Num"):
		for name in control.get_artboard_list():
			control.artboard_name = name
			vm = control.get_view_model_instance()
			if vm != null and vm.has_property("Test Num"):
				break
	if not _check(vm != null and vm.has_property("Test Num"), "Binding fixture VM missing"):
		return
	control.name = "BindingSource"
	var slider := HSlider.new()
	slider.name = "BindingTarget"
	slider.max_value = 1000
	owned.append(slider)
	root.add_child(slider)
	var binding := RiveBinding.new()
	binding.rive_node = NodePath("../BindingSource")
	binding.target_node = NodePath("../BindingTarget")
	binding.property_path = "Test Num"
	binding.target_property = &"value"
	binding.direction = RiveBinding.TWO_WAY
	owned.append(binding)
	root.add_child(binding)
	_check(binding.get_binding_error().is_empty(), "Range binding error: " + binding.get_binding_error())
	vm.get_number_property("Test Num").value = 246.0
	await _frames()
	_check(is_equal_approx(slider.value, 246.0), "VM -> Range synchronization failed")
	slider.value = 357.0
	_check(is_equal_approx(vm.get_number_property("Test Num").value, 357.0), "Range -> VM synchronization failed")
	_check(not control.has_property_override("Test Num"), "Runtime binding polluted scene overrides")
	control.load_file()
	await _frames()
	vm = control.get_view_model_instance()
	vm.get_number_property("Test Num").value = 468.0
	await _frames()
	_check(is_equal_approx(slider.value, 468.0), "Binding retained the old VM after reload")
	root.remove_child(binding)
	root.add_child(binding)
	await _frames()
	vm.get_number_property("Test Num").value = 569.0
	await _frames()
	_check(is_equal_approx(slider.value, 569.0), "Binding did not reconnect after re-entering tree")
	var old_vm = vm
	control.rive_file = null
	old_vm.get_number_property("Test Num").value = 680.0
	await _frames()
	_check(is_equal_approx(slider.value, 569.0), "Cleared source left an old ViewModel connected")
	control.rive_file = file
	control.load_file()
	await _frames()
	vm = control.get_view_model_instance()
	var replacement = file.instantiate_view_model_for_artboard(control.artboard_name)
	_check(control.bind_view_model(replacement), "Explicit VM binding failed")
	vm.get_number_property("Test Num").value = 681.0
	replacement.get_number_property("Test Num").value = 791.0
	await _frames()
	_check(is_equal_approx(slider.value, 791.0), "Binding did not follow explicit VM replacement")
	vm = replacement
	var receiver := TriggerTarget.new()
	receiver.name = "TriggerTarget"
	owned.append(receiver)
	root.add_child(receiver)
	var trigger_binding := RiveBinding.new()
	trigger_binding.rive_node = NodePath("../BindingSource")
	trigger_binding.target_node = NodePath("../TriggerTarget")
	trigger_binding.property_path = "Test Trigger"
	trigger_binding.target_property = &"on_rive_trigger"
	trigger_binding.direction = RiveBinding.TRIGGER_TO_METHOD
	owned.append(trigger_binding)
	root.add_child(trigger_binding)
	_check(trigger_binding.get_binding_error().is_empty(), "Trigger-to-method binding failed")
	var trigger = vm.get_trigger_property("Test Trigger")
	if _check(trigger != null, "Trigger wrapper missing"):
		trigger.triggered.connect(_on_trigger)
		trigger_count = 0
		trigger.fire()
		trigger.fire()
		control.advance(0.01)
		trigger.fire()
		control.advance(0.01)
		_check(trigger_count == 3, "Trigger was duplicated, lost, or fired on reset")
		_check(receiver.hits == 3, "Trigger-to-method binding duplicated or lost burst events")

func _test_binding_getter_lifetime() -> void:
	var file = _file("data_bind_test_cmdq.riv")
	if file == null:
		return
	var source := ScriptBindingSource.new()
	source.name = "ScriptBindingSource"
	source.model = file.instantiate_view_model_for_artboard()
	var target := ScriptBindingTarget.new()
	target.name = "ScriptBindingTarget"
	owned.append(source)
	owned.append(target)
	root.add_child(source)
	root.add_child(target)
	for kill_source in [true, false]:
		var binding := RiveBinding.new()
		binding.rive_node = NodePath("../ScriptBindingSource")
		binding.target_node = NodePath("../ScriptBindingTarget")
		binding.property_path = "Test Num"
		binding.target_property = &"value"
		binding.target_signal = &"value_changed"
		binding.direction = RiveBinding.GODOT_TO_RIVE
		root.add_child(binding)
		_check(binding.get_binding_error().is_empty(), "Script getter fixture could not bind")
		if kill_source:
			source.binding_to_free = binding
			_check(not binding.rebind(), "Rebind did not invalidate a freed binding")
		else:
			target.binding_to_free = binding
			binding.sync_to_rive()
		_check(not is_instance_valid(binding), "Script getter did not free its test binding")

func _test_animation_player() -> void:
	var file = _file("data_bind_test_cmdq.riv")
	if file == null:
		return
	var control = _control(file)
	control.name = "AnimatedRive"
	control.update_mode = RiveControl.UPDATE_MANUAL
	control.playing = false
	var path := &"rive/Test Num"
	var found := false
	for info in control.get_property_list():
		if info["name"] == path:
			found = true
			_check(info["type"] == TYPE_FLOAT, "Dynamic numeric property was not typed for AnimationPlayer")
			_check((int(info["usage"]) & PROPERTY_USAGE_STORAGE) == 0, "Dynamic Rive properties duplicate serialized overrides")
	if not _check(found, "AnimationPlayer target property missing from typed property list"):
		return
	_check(control.property_can_revert(path) and is_equal_approx(control.property_get_revert(path), 123.0), "Dynamic property revert does not expose authored defaults")
	var animation := Animation.new()
	animation.length = 2.0
	var track := animation.add_track(Animation.TYPE_VALUE)
	animation.track_set_path(track, NodePath(".:rive/Test Num"))
	animation.track_insert_key(track, 0.0, 100.0)
	animation.track_insert_key(track, 1.0, 200.0)
	animation.track_insert_key(track, 2.0, 123.0)
	var library := AnimationLibrary.new()
	_check(library.add_animation(&"property_track", animation) == OK, "Cannot add Rive property animation")
	var player := AnimationPlayer.new()
	player.name = "AnimationPlayer"
	player.callback_mode_process = AnimationMixer.ANIMATION_CALLBACK_MODE_PROCESS_MANUAL
	control.add_child(player)
	player.owner = control
	_check(player.add_animation_library(&"", library) == OK, "Cannot add Rive property animation library")
	player.play(&"property_track")
	player.advance(0.5)
	_check(is_equal_approx(control.get(path), 150.0) and is_equal_approx(control.get_property_value("Test Num"), 150.0), "AnimationPlayer did not animate the dynamic Rive numeric property")
	_check(control.has_property_override("Test Num") and is_equal_approx(control.get_property_override("Test Num"), 150.0), "Animated property did not use the unified override store")
	var packed := PackedScene.new()
	if not _check(packed.pack(control) == OK, "Cannot pack animated RiveControl"):
		return
	var state := packed.get_state()
	for node_index in state.get_node_count():
		for property_index in state.get_node_property_count(node_index):
			_check(not String(state.get_node_property_name(node_index, property_index)).begins_with("rive/"), "PackedScene serialized a dynamic Rive property twice")
	var restored = packed.instantiate()
	owned.append(restored)
	root.add_child(restored)
	_check(is_equal_approx(restored.get_property_value("Test Num"), 150.0), "PackedScene did not restore animated Rive overrides")
	var restored_player: AnimationPlayer = restored.get_node("AnimationPlayer")
	restored_player.play(&"property_track")
	restored_player.seek(0.25, true)
	_check(is_equal_approx(restored.get_property_value("Test Num"), 125.0), "Restored AnimationPlayer lost the dynamic Rive track target")
	var live_vm = restored.get_view_model_instance()
	restored_player.seek(2.0, true)
	_check(is_equal_approx(restored.get_property_value("Test Num"), 123.0) and not restored.has_property_override("Test Num"), "Authored-default animation key did not remove the override")
	_check(restored.get_view_model_instance() == live_vm, "Scalar default key unnecessarily rebuilt the live Rive instance")
	await _frames()

func _test_events_and_canvas() -> void:
	var canvas := RiveCanvas2D.new()
	canvas.size = Vector2i(256, 256)
	canvas.cull_enabled = false
	owned.append(canvas)
	root.add_child(canvas)
	var node := RiveFileInstance.new()
	node.update_mode = RiveNode.UPDATE_MANUAL
	node.rive_file = _file("timeline_event_test.riv")
	canvas.add_child(node)
	node.rive_event.connect(_on_event)
	events.clear()
	node.advance(0.4)
	node.advance(0.2)
	_check(events.size() == 1 and events[0].name == "Half", "Timeline event delivery failed")
	if events.size() == 1:
		_check(is_equal_approx(events[0].payload.delay_seconds, 0.1), "Event delay was lost")
	node.rive_file = _file("event_on_listener.riv")
	events.clear()
	node.pointer_down_with_id(Vector2(343, 116), 1)
	node.pointer_up_with_id(Vector2(343, 116), 1)
	node.advance(0.0)
	_check(events.size() == 2, "Pointer events were lost by the following advance")
	node.advance(0.0)
	_check(events.size() == 2, "Pointer events were reported twice")
	await _frames()
	var texture: Texture2D = canvas.get_texture()
	_check(texture != null and texture.get_rid().is_valid(), "Canvas output texture RID missing")
	_check(texture.get_size() == Vector2(256, 256), "Output texture size mismatch")
	canvas.size = Vector2i(192, 128)
	await _frames()
	_check(texture.get_size() == Vector2(192, 128) and texture.get_rid().is_valid(), "Retained output texture did not follow resize")
	var pixels = canvas.read_image()
	_check(pixels != null and not pixels.is_empty(), "Cross-backend Canvas readback failed")
	var timeline = _file("timeline_event_test.riv")
	var animations: PackedStringArray = timeline.get_animation_names()
	if not animations.is_empty():
		node.rive_file = timeline
		node.animation_name = animations[0]
		node.update_mode = RiveNode.UPDATE_ALWAYS
		node.process_mode = Node.PROCESS_MODE_DISABLED
		node.reset()
		await _frames()
		_check(is_zero_approx(node.get_time()), "Canvas advanced a disabled child")
		node.process_mode = Node.PROCESS_MODE_INHERIT
		await _frames()
		_check(node.get_time() > 0.0, "Canvas did not resume inherited processing")
		var multi := RiveMultiInstance.new()
		multi.animation_name = animations[0]
		multi.rive_file = timeline
		multi.transforms = [Transform2D.IDENTITY, Transform2D(0.0, Vector2(32, 32))]
		canvas.add_child(multi)
		_check(multi.get_player().get_animation_name() == animations[0], "MultiInstance preferred default SM over explicit animation")
		_check(multi.get_transforms().is_typed(), "Multi transforms were not typed")

func _run() -> void:
	await process_frame
	await _test_config_and_playback()
	await _test_layout()
	await _test_bindings()
	_test_binding_getter_lifetime()
	await _test_events_and_canvas()
	await _test_animation_player()
	var capabilities: Dictionary = RivePlayer.get_capabilities()
	_check(capabilities.has("scripting") and capabilities.has("audio_backend"), "Capability diagnostics missing")
	for node in owned:
		if is_instance_valid(node):
			node.queue_free()
	owned.clear()
	await _frames()
	if failures.is_empty():
		print("RIVE_INTEGRATION_SMOKE_PASS: scenes, playback, layout, bindings, events, Canvas lifecycle and textures")
	else:
		print("RIVE_INTEGRATION_SMOKE_FAIL: ", failures)
	quit(0 if failures.is_empty() else 1)
