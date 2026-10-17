extends SceneTree
const Follower = preload("res://addons/godot-rive/helpers/rive_component_follower.gd")
const Loader = preload("res://addons/godot-rive/helpers/rive_asset_loader.gd")
const Accessibility = preload("res://addons/godot-rive/helpers/rive_accessibility_bridge.gd")
var failures: Array[String] = []
var fixture_root: String
var owned: Array[Node] = []
var resolver_calls := 0

func _initialize() -> void:
	fixture_root = ProjectSettings.globalize_path("res://../third-party/rive-runtime/tests/unit_tests/assets")
	call_deferred("_run")

func _check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)
		push_error(message)

func _frames(count := 3) -> void:
	for _i in count:
		await process_frame
		await RenderingServer.frame_post_draw

func _replace_dictionary_resource(owner: Object, property: StringName) -> WeakRef:
	var font := FontFile.new()
	var previous: WeakRef = weakref(font)
	owner.set(property, {"asset": font})
	_check(owner.get(property).get("asset") == font, "Dictionary copy changed Resource identity: " + str(property))
	owner.set(property, {})
	return previous

func _test_dictionary_ownership() -> void:
	var file := RiveFile.new()
	var preset := RiveViewModelPreset.new()
	var control := RiveControl.new()
	var previous: Array[WeakRef] = [
		_replace_dictionary_resource(file, &"asset_overrides"),
		_replace_dictionary_resource(preset, &"values"),
		_replace_dictionary_resource(control, &"property_values"),
	]
	control.free()
	for resource in previous:
		_check(resource.get_ref() == null, "Dictionary replacement retained its old Resource")
	print("RUNTIME_HELPERS_DICTIONARY_OWNERSHIP_PASS")

func _file(path: String) -> RiveFile:
	var file := RiveFile.new()
	file.data = FileAccess.get_file_as_bytes(fixture_root.path_join(path))
	return file

func _control(path: String) -> RiveControl:
	var control := RiveControl.new()
	control.rive_file = _file(path)
	control.size = Vector2(400, 400)
	control.update_mode = RiveControl.UPDATE_MANUAL
	root.add_child(control)
	owned.append(control)
	_check(control.load_file() == OK, "Cannot load " + path)
	return control

func _asset_bytes(asset: Dictionary) -> Variant:
	resolver_calls += 1
	var path := fixture_root.path_join("out_of_band").path_join(asset.unique_name + "." + asset.extension)
	_check(FileAccess.file_exists(path), "Missing upstream asset " + path)
	return FileAccess.get_file_as_bytes(path)

func _test_follower() -> void:
	var control := _control("two_bone_ik.riv")
	control.position = Vector2(30, 45)
	control.rotation = 0.15
	var follower := Follower.new()
	root.add_child(follower)
	owned.append(follower)
	follower.source = follower.get_path_to(control)
	for item: Dictionary in control.get_player().get_components():
		if item.kind == "bone":
			follower.component_name = item.name
			follower.follow_bone_tip = true
			_check(follower.update_follow(), "Follower did not acquire bone")
			var expected: Vector2 = control.get_global_transform_with_canvas() * control.get_artboard_transform() * item.root_tip_position
			_check(follower.global_position.is_equal_approx(expected), "Follower lost Control fit or canvas transform")
			break
	_check(follower.tracking, "Bone fixture had no followed bone")
	follower.component_name = "__missing__"
	_check(not follower.update_follow(), "Missing component left stale tracking")
	print("RUNTIME_HELPERS_FOLLOWER_PASS")

func _test_assets() -> void:
	var file := _file("out_of_band/walle.riv")
	var original := file.asset_overrides
	var rejected := file.resolve_assets(func(_asset: Dictionary) -> Variant: return 42)
	_check(rejected.error != OK and file.asset_overrides == original, "Invalid resolver partially committed overrides")
	var result := file.resolve_assets(_asset_bytes)
	_check(result.error == OK and result.resolved > 0 and resolver_calls > 0, "Main-thread resolver did not populate external images")
	var calls := resolver_calls
	_check(file.resolve_assets(_asset_bytes).error == OK and calls == resolver_calls, "Resolver called again for already overridden assets")
	var loader := Loader.new()
	root.add_child(loader)
	owned.append(loader)
	loader.rive_file = _file("out_of_band/walle.riv")
	loader.resource_resolver = _asset_bytes
	_check(await loader.resolve() == OK, "Async resolver did not commit valid assets")
	_check(not loader.rive_file.asset_overrides.is_empty(), "Async loader returned no overrides")
	loader.rive_file = _file("out_of_band/walle.riv")
	loader.resource_resolver = func(_asset: Dictionary) -> Variant: return PackedByteArray()
	_check(await loader.resolve() != OK and loader.rive_file.asset_overrides.is_empty(), "Failed decode replaced working source overrides")
	loader.resource_resolver = Callable()
	loader.allowed_https_hosts = ["rivegd-test.invalid"]
	loader.url_resolver = func(_asset: Dictionary) -> String:
		loader.call_deferred("cancel")
		return "https://rivegd-test.invalid/asset.png"
	_check(await loader.resolve() == ERR_SKIP, "Cancellation stranded the suspended resolver")
	_check(loader.rive_file.asset_overrides.is_empty(), "Cancelled download committed partial data")
	loader.url_resolver = func(_asset: Dictionary) -> String: return "https://not-allowed.invalid/image.png"
	_check(await loader.resolve() == ERR_UNAUTHORIZED, "Unapproved network host accepted")
	print("RUNTIME_HELPERS_ASSET_PASS")

func _test_accessibility() -> void:
	var control := _control("semantic/simpsons.riv")
	var bridge := Accessibility.new()
	root.add_child(bridge)
	owned.append(bridge)
	bridge.source = bridge.get_path_to(control)
	bridge.platform_enabled = false
	bridge.sync()
	for _i in 8: control.advance(0.016)
	bridge.sync()
	var nodes := bridge.get_nodes()
	_check(nodes.size() >= 3, "Semantic mirror did not discover labelled tabs")
	var chosen := -1
	for id: int in nodes:
		if nodes[id].role == 12 and (int(nodes[id].state_flags) & 2) == 0:
			chosen = id
			break
	_check(chosen > 0 and bridge.activate(chosen), "Native semantic tap was not dispatched")
	for _i in 8: control.advance(0.016)
	bridge.sync()
	nodes = bridge.get_nodes()
	_check(nodes.has(chosen) and (int(nodes[chosen].state_flags) & 2) != 0, "Semantic action did not select the authored tab")
	var second := -1
	for id: int in nodes:
		if nodes[id].role == 12 and (int(nodes[id].state_flags) & 2) == 0:
			second = id
			break
	if second > 0:
		bridge.call("_action", {}, second, RivePlayer.SEMANTIC_TAP, bridge.get("_generation"))
		for _i in 8: control.advance(0.016)
		bridge.sync()
		_check((int(bridge.get_nodes()[second].state_flags) & 2) != 0, "Platform action callback signature/routing failed")
	_check(not bridge.activate(-1), "Bridge dispatched a stale/invalid semantic ID")
	bridge.platform_enabled = true
	bridge.sync()
	await _frames(4)
	print("RUNTIME_HELPERS_ACCESSIBILITY_PASS: platform=", bridge.is_platform_connected())

func _run() -> void:
	create_timer(30).timeout.connect(func() -> void:
		push_error("Runtime helper watchdog timed out")
		quit(2))
	await _frames()
	_test_dictionary_ownership()
	_test_follower()
	await _test_assets()
	await _test_accessibility()
	for node in owned: node.queue_free()
	owned.clear()
	await _frames(4)
	if failures.is_empty(): print("RUNTIME_HELPERS_SMOKE_PASS")
	quit(0 if failures.is_empty() else 1)
