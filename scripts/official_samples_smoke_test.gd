extends SceneTree

const SAMPLE_HASHES := {
	"travel_icons_pack.riv": "e153076afb9604a1dfda7f721053bf9fba9951b81e9aa54255fa3fa345a569d9",
	"lists_demo.riv": "12cc0f18866432afe84b923680f7aa9d074c175587ceb882d8f057a6aa3e141f",
	"rewards.riv": "63e543092899e68a699ea043b90febc8876454e555fa2990908d24cc1a188cfc",
}

var sample_root: String
var failures: Array[String] = []
var controls: Array[RiveControl] = []

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.is_empty():
		push_error("Pass the local official sample cache directory after --")
		quit(1)
		return
	sample_root = args[0]
	call_deferred("_run")

func _check(condition: bool, message: String) -> bool:
	if not condition:
		failures.append(message)
		push_error(message)
	return condition

func _frames(count := 3) -> void:
	for _i in count:
		await process_frame
		await RenderingServer.frame_post_draw

func _load_sample(name: String, artboard: String, position: Vector2):
	var path := sample_root.path_join(name)
	if not _check(FileAccess.get_sha256(path) == SAMPLE_HASHES[name], "Official sample is missing or differs from the pinned revision: " + name):
		return null
	var control := RiveControl.new()
	controls.append(control)
	control.position = position
	control.size = Vector2(250, 280)
	control.artboard_name = artboard
	var resource := RiveFile.new()
	resource.data = FileAccess.get_file_as_bytes(path)
	control.rive_file = resource
	root.add_child(control)
	if not _check(control.load_file() == OK, "Cannot import official sample: " + name):
		return null
	_check(control.get_view_model_instance() != null, "Official sample has no bound ViewModel: " + name)
	return control

func _test_artboards(control) -> void:
	var vm = control.get_view_model_instance()
	var property = vm.get_artboard_property("icon")
	if not _check(property != null, "Travel pack icon property is missing"):
		return
	_check(property.get_value() == "gas", "Travel pack embedded default is not gas")
	for name in ["map", "car", "gas", "compass", "walk", "food", "GPS", "coffee"]:
		_check(property.get_artboard_list().has(name), "Travel pack is missing icon " + name)
		_check(control.set_property_value("icon", name), "Cannot select travel icon " + name)
		_check(property.get_value() == name, "Travel pack selected the wrong artboard")
		await _frames(1)
	_check(not control.set_property_value("icon", "missing-icon"), "Unknown travel icon was accepted")
	control.set_property_value("icon", "map")
	_check(control.load_file() == OK, "Travel pack reload failed")
	_check(control.get_view_model_instance().get_artboard_property("icon").get_value() == "map", "Travel icon override was lost on reload")

func _test_lists(control) -> void:
	var vm = control.get_view_model_instance()
	var list = vm.get_list_property("menu")
	if not _check(list != null, "Lists demo menu property is missing"):
		return
	var items: Array = list.get_items()
	if not _check(items.size() == 4, "Lists demo must start with four menu items"):
		return
	var expected := ["War of the Stars", "Scufflestar Galactica", "Galaxy Hike", "Dino Planet"]
	for i in items.size():
		_check(items[i].get_string_property("label").get_value() == expected[i], "Lists demo item order/default label differs")
	_check(control.set_property_value("menuOpen", true), "Cannot open official menu")
	_check(control.set_property_value("menu[0].label", "RiveGD official sample"), "Cannot override official list item label")
	_check(control.set_property_value("menu[0].hoverColor", Color.RED), "Cannot override official list item color")
	_check(vm.get_string_property("menu[0].label").get_value() == "RiveGD official sample", "List item path did not resolve to the live item")
	_check(list.append(items[0]), "Cannot append an existing compatible menu item")
	_check(list.get_size() == 5, "Append produced incorrect menu length")
	_check(list.insert(0, items[1]), "Cannot insert an existing compatible menu item")
	_check(list.swap(0, 1), "Cannot swap official menu items")
	_check(list.remove_at(5), "Cannot remove appended menu item")
	_check(list.set_items(items), "Cannot restore official menu items")
	_check(list.get_size() == 4, "Restored menu has incorrect length")
	_check(not list.append(vm), "Official menu accepted a recursive ViewModel cycle")
	await _frames()
	_check(control.load_file() == OK, "Lists demo reload failed")
	vm = control.get_view_model_instance()
	_check(vm.get_list_property("menu").get_size() == 4, "Runtime list mutations polluted persisted scene state")
	_check(vm.get_string_property("menu[0].label").get_value() == "RiveGD official sample", "Persisted menu label was lost on reload")

func _test_enums(control) -> void:
	var vm = control.get_view_model_instance()
	var property = vm.get_enum_property("Item_Selection.Item_Selection")
	if not _check(property != null, "Rewards nested custom enum is missing"):
		return
	_check(property.get_values() == PackedStringArray(["Coin", "Gem"]), "Rewards custom enum has incorrect choices")
	_check(property.get_value_name() == "Gem", "Rewards embedded enum default is not Gem")
	for name in ["Coin", "Gem"]:
		_check(control.set_property_value("Item_Selection.Item_Selection", name), "Cannot select rewards enum by name")
		_check(property.get_value_name() == name, "Rewards enum selected the wrong value")
		await _frames(1)
	_check(control.set_property_value("Coin.Item_Value", 1000), "Cannot set official coin amount")
	_check(control.set_property_value("Gem.Item_Value", 4000), "Cannot set official gem amount")
	_check(control.set_property_value("Energy_Bar.Energy_Bar", 75), "Cannot set official energy amount")
	_check(control.set_property_value("Button.State_1", "RiveGD verified"), "Cannot set official button label")
	_check(not control.set_property_value("Item_Selection.Item_Selection", "missing-token"), "Rewards accepted an unknown enum choice")
	_check(control.load_file() == OK, "Rewards reload failed")
	_check(control.get_view_model_instance().get_enum_property("Item_Selection.Item_Selection").get_value_name() == "Gem", "Rewards enum override was lost on reload")

func _run() -> void:
	await process_frame
	var travel = _load_sample("travel_icons_pack.riv", "Demo", Vector2(10, 10))
	if travel != null:
		await _test_artboards(travel)
	var list_demo = _load_sample("lists_demo.riv", "Main", Vector2(280, 10))
	if list_demo != null:
		await _test_lists(list_demo)
	var rewards = _load_sample("rewards.riv", "Main", Vector2(550, 10))
	if rewards != null:
		await _test_enums(rewards)
	await _frames(4)
	var rendered := root.get_texture().get_image()
	_check(rendered != null and not rendered.is_empty(), "Official sample GPU readback failed")
	if rendered != null and not rendered.is_empty():
		var colored := false
		for y in range(0, rendered.get_height(), 8):
			for x in range(0, rendered.get_width(), 8):
				var pixel := rendered.get_pixel(x, y)
				if maxf(pixel.r, maxf(pixel.g, pixel.b)) - minf(pixel.r, minf(pixel.g, pixel.b)) > 0.15:
					colored = true
					break
			if colored:
				break
		_check(colored, "Official samples did not render colored content")
	for control in controls:
		control.rive_file = null
		control.queue_free()
	controls.clear()
	await _frames()
	if failures.is_empty():
		print("RIVE_OFFICIAL_SAMPLES_SMOKE_PASS: travel icons, menu list, rewards enum, GPU draw and override replay")
	else:
		print("RIVE_OFFICIAL_SAMPLES_SMOKE_FAIL: ", failures)
	quit(0 if failures.is_empty() else 1)
