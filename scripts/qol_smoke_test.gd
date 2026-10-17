extends SceneTree

const SAMPLE_HASHES := {
	"lists_demo.riv": "12cc0f18866432afe84b923680f7aa9d074c175587ceb882d8f057a6aa3e141f",
	"travel_icons_pack.riv": "e153076afb9604a1dfda7f721053bf9fba9951b81e9aa54255fa3fa345a569d9",
}

var fixture_root: String
var official_root: String
var failures: Array[String] = []
var controls: Array[RiveControl] = []
var temporary_paths: Array[String] = []
var leak_probes: Array = []
var trace_leaks := false

func _initialize() -> void:
	fixture_root = ProjectSettings.globalize_path("res://").path_join("../third-party/rive-runtime/tests/unit_tests/assets")
	var args := OS.get_cmdline_user_args()
	if not args.is_empty():
		official_root = args[0]
	trace_leaks = args.has("--trace-leaks")
	call_deferred("_run")

func _watch(object, label: String) -> void:
	if trace_leaks and object != null:
		leak_probes.append({"ref": weakref(object), "label": label, "id": object.get_instance_id()})

func _report_live_probes() -> void:
	for probe in leak_probes:
		var object = probe["ref"].get_ref()
		if object != null:
			print("RIVE_QOL_LIVE_PROBE: ", probe["label"], " / ", object.get_class(), " / ", probe["id"])
	leak_probes.clear()

func _check(condition: bool, message: String) -> bool:
	if not condition:
		failures.append(message)
		push_error(message)
	return condition

func _frames(count := 2) -> void:
	for _i in count:
		await process_frame
		await RenderingServer.frame_post_draw

func _file(name: String, official := false):
	var path := (official_root if official else fixture_root).path_join(name)
	if official and not _check(FileAccess.get_sha256(path) == SAMPLE_HASHES[name], "Missing or mismatched pinned official sample: " + name):
		return null
	var bytes := FileAccess.get_file_as_bytes(path)
	if not _check(not bytes.is_empty(), "Missing fixture: " + path):
		return null
	var file := RiveFile.new()
	_watch(file, "file:" + name)
	file.data = bytes
	return file

func _control(file, artboard := ""):
	var control := RiveControl.new()
	_watch(control, "control:" + artboard)
	control.size = Vector2(200, 200)
	control.artboard_name = artboard
	control.rive_file = file
	controls.append(control)
	root.add_child(control)
	if not _check(control.load_file() == OK, "Control cannot load " + artboard):
		return null
	control.set_process(false)
	return control

func _test_factory(file, control) -> void:
	var bound = control.get_view_model_instance()
	if not _check(bound != null, "Default fixture ViewModel is missing"):
		return
	var model: String = bound.get_view_model_name()
	_check(file.get_view_model_names().has(model), "File did not enumerate the bound ViewModel")
	var names: PackedStringArray = file.get_view_model_instance_names(model)
	_check(not names.is_empty() and names.has(bound.get_name()), "Authored instance names are missing")
	var first = file.instantiate_view_model(model)
	var second = file.instantiate_view_model_for_artboard()
	_watch(first, "factory:first")
	_watch(second, "factory:second")
	if not _check(first != null and second != null, "Default instance factories failed"):
		return
	first.set_property_value("Test Num", 777)
	_check(is_equal_approx(second.get_property_value("Test Num"), 123), "Factory instances share mutable state")
	_check(is_equal_approx(bound.get_property_value("Test Num"), 123), "Factory mutation polluted the bound instance")
	_check(file.instantiate_view_model(model, names[0]) != null, "Named instance factory failed")
	_check(file.instantiate_view_model(model, "", 0) != null, "Indexed instance factory failed")
	_check(file.instantiate_view_model(model, "", -1, true) != null, "Blank instance factory failed")
	_check(file.instantiate_view_model("__missing__") == null, "Unknown definition was accepted")
	_check(file.instantiate_view_model(model, "__missing__") == null, "Unknown instance name was accepted")
	_check(file.instantiate_view_model(model, "", 999999) == null, "Invalid instance index was accepted")
	_check(file.instantiate_view_model(model, names[0], 0) == null, "Conflicting instance selectors were accepted")
	_check(file.instantiate_view_model(model, names[0], -1, true) == null, "Named blank selector was accepted")
	_check(not file.get_view_model_schema(model).is_empty(), "ViewModel schema is empty")
	var enum_info: Dictionary = bound.get_property_info("Test Enum")
	_check(enum_info.get("type") == "enum" and enum_info.get("values", []).size() >= 2, "Enum schema metadata is incomplete")
	_check(file.get_artboard_names().size() > 0 and file.get_intrinsic_size().x > 0, "Artboard metadata is incomplete")
	_check(file.get_load_error().is_empty() and file.get_load_error_code() == OK, "Valid file retained an import error")
	_check(control.get_player().bind_view_model(second), "Player could not bind a factory-created compatible instance")

func _test_paths_and_observation(control) -> void:
	var vm = control.get_view_model_instance()
	_watch(vm, "paths:vm")
	_check(RiveViewModelInstance.canonicalize_property_path("Test List[0]/Color") == "Test List.0.Color", "Slash/bracket canonicalization differs")
	_check(RiveViewModelInstance.canonicalize_property_path("Test List[-1].Color").is_empty(), "Negative canonical index was accepted")
	var number = vm.get_number_property("Test Num")
	_watch(number, "paths:number")
	var count := [0]
	number.value_changed.connect(func(_value): count[0] += 1)
	vm.flush_changes()
	number.set_value(21)
	_check(count[0] == 1, "A scalar setter must emit one value_changed signal")
	vm.flush_changes()
	vm.flush_changes()
	_check(count[0] == 1, "Native flush duplicated a setter notification")
	number.set_value(21)
	_check(count[0] == 1, "No-op setter emitted value_changed")
	var aggregate := [0]
	vm.property_changed.connect(func(_path, _value): aggregate[0] += 1)
	number.set_value(22)
	_check(vm.flush_changes(), "VM flush did not retain the setter's dirty revision")
	_check(aggregate[0] == 1, "Scalar change produced duplicate aggregate notifications")
	_check(not vm.flush_changes(), "Clean ViewModel still reports pending changes")
	_check(control.set_property_value("Test Num", 80), "Cannot create a persistent override")
	vm.get_number_property("Test Num").set_value(90)
	_check(is_equal_approx(control.get_property_value("Test Num"), 90), "Control getter returns an old override instead of runtime state")
	_check(control.has_property_override("Test Num") and int(control.get_property_override("Test Num")) == 80, "Runtime mutation changed the scene override")
	_check(control.set_property_value("Test List[0]/Color", Color.GREEN), "Canonical list override failed")
	_check(control.has_property_override("Test List.0.Color"), "Equivalent path aliases did not share an override key")
	var list = vm.get_list_property("Test List")
	var item = list.get_item(0)
	_watch(list, "paths:list")
	_watch(item, "paths:item")
	var item_changes := [0]
	item.get_color_property("Color").value_changed.connect(func(_value): item_changes[0] += 1)
	item.set_property_value("Color", Color.BLUE)
	_check(vm.flush_changes() and item_changes[0] == 1, "List child observation was not flushed recursively")
	var nested = vm.get_view_model_property("Test Nested")
	_watch(nested, "paths:nested")
	_check(nested != null, "Explicit nested VM accessor failed")
	var replacement = control.rive_file.instantiate_view_model(nested.get_view_model_name())
	_watch(replacement, "paths:replacement")
	replacement.set_property_value("Nested Number", 444)
	_check(vm.replace_view_model("Test Nested", replacement), "Compatible nested VM replacement failed")
	_check(is_equal_approx(vm.get_property_value("Test Nested/Nested Number"), 444), "Nested replacement left stale property caches")
	_check(not vm.replace_view_model("Test Nested", vm), "Incompatible/cyclic nested replacement was accepted")
	var trigger_name := ""
	for info in vm.get_properties():
		if info["type"] == "trigger":
			trigger_name = info["name"]
			break
	if _check(not trigger_name.is_empty(), "Fixture has no trigger property"):
		var trigger = vm.get_trigger_property(trigger_name)
		_watch(trigger, "paths:trigger")
		var fired := [0]
		var aggregate_fired := [0]
		trigger.triggered.connect(func(): fired[0] += 1)
		vm.trigger_fired.connect(func(_path): aggregate_fired[0] += 1)
		trigger.fire()
		trigger.fire()
		vm.flush_changes()
		_check(fired[0] == 2 and aggregate_fired[0] == 2, "Trigger burst was lost or duplicated")
		control.get_player().advance(0.016)
		vm.flush_changes()
		_check(fired[0] == 2 and aggregate_fired[0] == 2, "Native trigger reset emitted a phantom event")
		trigger.fire()
		vm.flush_changes()
		_check(fired[0] == 3 and aggregate_fired[0] == 3, "Trigger after advance was not counted once")

func _make_preset(vm):
	var preset := RiveViewModelPreset.new()
	_watch(preset, "preset:root")
	preset.view_model_name = vm.get_view_model_name()
	var nested := RiveViewModelPreset.new()
	_watch(nested, "preset:nested")
	nested.values = {"Nested Number": 345}
	var item := RiveViewModelPreset.new()
	_watch(item, "preset:item")
	item.values = {"Color": Color.MAGENTA}
	preset.values = {"Test Num": 246, "Test Nested": nested, "Test List": [item]}
	return preset

func _make_cleared_preset_probes() -> Array:
	var parent := RiveViewModelPreset.new()
	var child := RiveViewModelPreset.new()
	var recursive := RiveViewModelPreset.new()
	var asset_file := RiveFile.new()
	var asset_font := FontFile.new()
	var override_font := FontFile.new()
	var detached_control := RiveControl.new()
	var probes := [weakref(child), weakref(recursive), weakref(asset_font), weakref(override_font)]
	parent.values = {"child": child}
	parent.values = {}
	recursive.values = {"self": recursive}
	recursive.values = {}
	asset_file.asset_overrides = {"font": asset_font}
	asset_file.asset_overrides = {}
	detached_control.set_property_values({"font": override_font})
	detached_control.set_property_values({})
	detached_control.free()
	return probes

func _test_preset(file) -> void:
	var default_vm = file.instantiate_view_model_for_artboard()
	_watch(default_vm, "preset:default_vm")
	var preset = _make_preset(default_vm)
	var created = preset.instantiate(file)
	_watch(created, "preset:created")
	if not _check(created != null, "Recursive ViewModel preset failed"):
		return
	_check(created.get_property_value("Test Num") == 246 and created.get_property_value("Test Nested.Nested Number") == 345, "Preset scalar/nested values were not applied")
	_check(created.get_list_property("Test List").get_size() == 1 and created.get_property_value("Test List[0].Color").is_equal_approx(Color.MAGENTA), "Preset list was not instantiated")
	var independent = preset.instantiate(file)
	_watch(independent, "preset:independent")
	created.set_property_value("Test Num", 999)
	_check(independent != null and independent.get_property_value("Test Num") == 246, "Preset instances share state")
	var path := "user://rivegd_qol_%d.tres" % Time.get_ticks_usec()
	temporary_paths.append(path)
	if _check(ResourceSaver.save(preset, path) == OK, "Cannot save ViewModel preset"):
		var reloaded = ResourceLoader.load(path, "", ResourceLoader.CACHE_MODE_IGNORE)
		_watch(reloaded, "preset:reloaded")
		var restored = reloaded.instantiate(file) if reloaded != null else null
		_watch(restored, "preset:restored")
		_check(restored != null and restored.get_list_property("Test List").get_size() == 1, "Saved preset list did not survive ResourceSaver reload")
		_check(restored != null and restored.get_property_value("Test Nested.Nested Number") == 345, "Saved nested preset did not survive reload")
	var invalid := RiveViewModelPreset.new()
	_watch(invalid, "preset:invalid")
	invalid.view_model_name = default_vm.get_view_model_name()
	invalid.values = {"Test Num": "bad"}
	_check(invalid.instantiate(file) == null, "Invalid preset value was accepted")
	_check(default_vm.get_property_value("Test Num") == 123, "Failed preset polluted authored defaults")
	var recursive := RiveViewModelPreset.new()
	_watch(recursive, "preset:recursive")
	recursive.view_model_name = default_vm.get_view_model_name()
	recursive.values = {"Test Nested": recursive}
	_check(recursive.instantiate(file) == null, "Preset reference cycle was accepted")
	recursive.values = {}
	var control = _control(file)
	control.view_model_preset = preset
	_check(control.get_view_model_instance().get_list_property("Test List").get_size() == 1, "Control did not bind the preset")
	_check(control.load_file() == OK and control.get_property_value("Test Num") == 246, "Control preset values did not replay after reload")
	var notifications := [0]
	control.view_model_changed.connect(func(): notifications[0] += 1)
	var child: RiveViewModelPreset = preset.values["Test Nested"]
	child.values = {"Nested Number": 456}
	_check(control.get_property_value("Test Nested.Nested Number") == 456, "Nested child preset edit did not reload Control")
	_check(notifications[0] == 1, "Child preset change caused duplicate VM replacement notifications")
	var list_item: RiveViewModelPreset = preset.values["Test List"][0]
	list_item.values = {"Color": Color.CYAN}
	_check(control.get_property_value("Test List[0].Color").is_equal_approx(Color.CYAN), "List item preset edit did not reload Control")
	_check(notifications[0] == 2, "List preset change caused duplicate VM replacement notifications")
	var replacement := RiveViewModelPreset.new()
	_watch(replacement, "preset:replacement")
	replacement.values = {"Nested Number": 567}
	var values: Dictionary = preset.values
	values["Test Nested"] = replacement
	preset.values = values
	var current_vm = control.get_view_model_instance()
	child.values = {"Nested Number": 678}
	_check(control.get_view_model_instance() == current_vm and control.get_property_value("Test Nested.Nested Number") == 567, "Removed child preset retained its hot-reload subscription")

func _test_official_list_and_cross_file() -> void:
	var list_file = _file("lists_demo.riv", true)
	var travel_file = _file("travel_icons_pack.riv", true)
	if list_file == null or travel_file == null:
		return
	var list_control = _control(list_file, "Main")
	var travel_control = _control(travel_file, "Demo")
	if list_control == null or travel_control == null:
		return
	var item = list_file.instantiate_view_model("listItem", "", -1, true)
	_watch(item, "official:new_list_item")
	if _check(item != null, "Official Todo listItem blank factory failed"):
		item.set_property_value("label", "New Godot todo")
		var list = list_control.get_view_model_instance().get_list_property("menu")
		var before: int = list.get_size()
		_check(list.append(item), "Cannot append a newly created official list item")
		_check(list.get_size() == before + 1 and list.get_item(before).get_property_value("label") == "New Godot todo", "New official list item has incorrect state")
		var preset := RiveViewModelPreset.new()
		_watch(preset, "official:list_preset")
		preset.view_model_name = list_control.get_view_model_instance().get_view_model_name()
		var child := RiveViewModelPreset.new()
		_watch(child, "official:list_child_preset")
		child.view_model_name = "listItem"
		child.blank = true
		child.values = {"label": "Saved todo"}
		preset.values = {"menu": [child]}
		var saved = preset.instantiate(list_file, "Main")
		_watch(saved, "official:instantiated_list")
		_check(saved != null and saved.get_property_value("menu[0].label") == "Saved todo", "Official list preset could not create a new item from an empty/default list")
	var external = _file("data_binding_artboards_test.riv")
	if external == null:
		return
	var names: PackedStringArray = external.get_artboard_names()
	var selected := "ch1" if names.has("ch1") else names[0]
	var reference := RiveArtboardReference.new()
	_watch(reference, "official:artboard_reference")
	reference.file = external
	reference.artboard_name = selected
	var icon = travel_control.get_view_model_instance().get_artboard_property("icon")
	_watch(icon, "official:artboard_property")
	_check(icon.set_reference(reference), "Cross-file BindableArtboard assignment failed")
	_check(icon.get_value() == selected and icon.get_reference() == reference, "Cross-file artboard did not retain its source reference")
	var referenced_vm = external.instantiate_view_model_for_artboard(selected)
	_watch(referenced_vm, "official:referenced_vm")
	if referenced_vm != null:
		_check(icon.bind_view_model(referenced_vm), "Cross-file artboard could not bind a VM from its source file")
		_check(icon.get_view_model() == referenced_vm, "Bound artboard VM identity was lost")
		reference.file = travel_file
		_check(icon.get_reference() == null and icon.get_view_model() == referenced_vm, "Mutating an ArtboardReference misreported the bound native source")
		_check(icon.bind_view_model(referenced_vm), "Artboard lost its assigned source after the descriptor was edited")
		var unrelated_vm = travel_file.instantiate_view_model_for_artboard()
		if unrelated_vm != null:
			_check(not icon.bind_view_model(unrelated_vm), "Edited descriptor allowed a VM from the wrong native source file")
		reference.file = external
	_check(icon.set_value("map") and icon.get_reference() == null and icon.get_view_model() == null, "Local artboard reassignment retained stale cross-file state")

func _test_assets() -> void:
	var file = _file("out_of_band/walle.riv")
	if file == null:
		return
	var initial: Array = file.get_asset_metadata()
	if not _check(initial.size() >= 2, "External asset metadata was not enumerated"):
		return
	var overrides := {}
	for info in initial:
		_check(info["unresolved_external"], "Out-of-band fixture was incorrectly marked embedded")
		var name := "walle-370.png" if str(info["name"]).to_lower().contains("walle") else "eve-317.png"
		overrides[info["id"]] = fixture_root.path_join("out_of_band/" + name)
	file.asset_overrides = overrides
	_check(file.load_rive_file() == OK, "External asset mapping did not decode")
	for info in file.get_asset_metadata():
		_check(info["overridden"] and not info["unresolved_external"] and info["resolution"] == "override", "Mapped asset metadata has an incorrect resolution")
	_check(file.get_external_dependencies().size() == 2, "Asset dependency paths were not exposed")
	var previous: Dictionary = file.asset_overrides
	file.asset_overrides = {RefCounted.new(): PackedByteArray([0])}
	_check(file.asset_overrides == previous, "Malicious asset map key changed the resource")
	var first: Dictionary = initial[0]
	file.asset_overrides = {first["id"]: PackedByteArray([0, 1, 2])}
	_check(file.load_rive_file() != OK and not file.get_load_error().is_empty(), "Invalid mapped image bytes were silently accepted")
	file.asset_overrides = overrides
	_check(file.load_rive_file() == OK and file.get_load_error_code() == OK, "Corrected asset map did not clear the previous import error")
	var embedded = _file("data_bind_font_test.riv")
	var font := FontFile.new()
	_watch(font, "assets:font")
	font.data = FileAccess.get_file_as_bytes(fixture_root.path_join("nabla.ttf"))
	for info in embedded.get_asset_metadata():
		if info["extension"] in ["ttf", "otf"]:
			embedded.asset_overrides = {info["id"]: font}
			_check(embedded.load_rive_file() == OK and embedded.get_asset_metadata()[0]["overridden"], "FontFile asset override could not decode")
			break

func _observe_native_changes(vm, native_events: Array, depth := 0) -> void:
	_watch(vm, "native:observed_vm:" + str(depth))
	if depth > 16:
		return
	for info in vm.get_properties():
		var path: String = info["name"]
		if info["type"] == "trigger":
			vm.get_trigger_property(path).triggered.connect(func(): native_events[0] += 1)
		elif info["type"] == "view_model":
			var child = vm.get_view_model_property(path)
			if child != null:
				_observe_native_changes(child, native_events, depth + 1)
		elif info["type"] == "string":
			vm.get_string_property(path).value_changed.connect(func(_value): native_events[0] += 1)
		elif info["type"] == "number":
			vm.get_number_property(path).value_changed.connect(func(_value): native_events[0] += 1)

func _advance_native_transitions(player, label: String, iterations := 3) -> void:
	print(label, ": warmup / 0.016")
	player.advance(0.016)
	for i in iterations:
		print(label, ": ", i, " / 0.1")
		player.advance(0.1)
		print(label, ": ", i, " / 1.0")
		player.advance(1.0)

func _test_native_trigger() -> void:
	var file = _file("state_transition_fire_trigger.riv")
	if file == null:
		return
	var baseline := RivePlayer.new()
	_watch(baseline, "native:baseline_player")
	if not _check(baseline.load_from_bytes(file.data), "Cannot load native transition baseline"):
		return
	_check(baseline.configure_playback("", ""), "Cannot reconfigure native transition baseline")
	_advance_native_transitions(baseline, "RIVE_QOL_NATIVE_BASELINE")
	for cycle in 2:
		_check(baseline.configure_playback("", ""), "Repeated AUTO configuration failed")
		baseline.reset()
		_advance_native_transitions(baseline, "RIVE_QOL_NATIVE_RECONFIGURE_" + str(cycle), 2)
	_check(baseline.load_from_bytes(file.data), "Standalone Player could not reload after repeated scene replacement")
	_check(baseline.configure_playback("", ""), "AUTO configuration failed after reload")
	_advance_native_transitions(baseline, "RIVE_QOL_NATIVE_RELOAD", 2)
	var observed := RivePlayer.new()
	_watch(observed, "native:observed_player")
	if not _check(observed.load_from_bytes(file.data), "Cannot load observed native transition Player"):
		return
	var observed_events := [0]
	_observe_native_changes(observed.get_rive_view_model_instance(), observed_events)
	_advance_native_transitions(observed, "RIVE_QOL_NATIVE_PLAYER_OBSERVED")
	_check(observed_events[0] > 0, "Standalone Player native transitions did not notify Godot observers")
	var events_before_reload: int = observed_events[0]
	_check(observed.load_from_bytes(file.data), "Observed standalone Player reload failed")
	_observe_native_changes(observed.get_rive_view_model_instance(), observed_events)
	_check(observed.configure_playback("", ""), "Observed Player AUTO reconfiguration failed after reload")
	observed.reset()
	_advance_native_transitions(observed, "RIVE_QOL_NATIVE_PLAYER_OBSERVED_RELOAD", 2)
	_check(observed_events[0] > events_before_reload, "Reloaded Player stopped reporting Rive-authored native changes")
	var control = _control(file, "main")
	if control == null:
		return
	var vm = control.get_view_model_instance()
	var native_events := [0]
	print("RIVE_QOL_NATIVE_OBSERVERS: ", vm.get_properties())
	_observe_native_changes(vm, native_events)
	_advance_native_transitions(control.get_player(), "RIVE_QOL_NATIVE_ADVANCE")
	_check(native_events[0] > 0, "Rive-authored state transitions did not notify Godot property observers")

func _run() -> void:
	await process_frame
	var file = _file("data_bind_test_cmdq.riv")
	if file != null:
		var control = _control(file)
		if control != null:
			_test_factory(file, control)
			_test_paths_and_observation(control)
		_test_preset(file)
	for probe in _make_cleared_preset_probes():
		_check(probe.get_ref() == null, "Replacing a resource Dictionary leaked a removed child, self-reference, font asset, or Control override")
	if _check(not official_root.is_empty(), "Pass the official sample cache path after --"):
		_test_official_list_and_cross_file()
	_test_assets()
	_test_native_trigger()
	await _frames(3)
	for control in controls:
		control.rive_file = null
		control.queue_free()
	controls.clear()
	await _frames()
	for path in temporary_paths:
		if FileAccess.file_exists(path):
			_check(DirAccess.remove_absolute(ProjectSettings.globalize_path(path)) == OK, "Cannot remove this test's temporary preset: " + path)
	if trace_leaks:
		_report_live_probes()
	if failures.is_empty():
		print("RIVE_QOL_SMOKE_PASS: factories, presets, native observers, triggers, canonical paths, cross-file artboards, asset maps")
	else:
		print("RIVE_QOL_SMOKE_FAIL: ", failures)
	quit(0 if failures.is_empty() else 1)
