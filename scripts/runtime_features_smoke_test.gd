extends SceneTree

const AUDIO_HASH := "3f61ed35c7084396947d691f588b5843c7bda4866ba9b63e9bfe6e2b98c58979"
const SCRIPT_HASH := "8fbea33d7677957b5c792975647aab78731272af50fa492129d58266d924b655"

var sample_root: String
var fixture_root: String
var failures: Array[String] = []
var controls: Array[RiveControl] = []
var audio_peak := 0.0

func _initialize() -> void:
	var args := OS.get_cmdline_user_args()
	if args.is_empty():
		push_error("Pass the local official sample cache directory after --")
		quit(1)
		return
	sample_root = args[0]
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

func _load(path: String, expected_hash: String, artboard: String) -> RiveControl:
	if not _check(FileAccess.get_sha256(path) == expected_hash,
			"Fixture is missing or differs from its pinned revision: " + path):
		return null
	var control := RiveControl.new()
	controls.append(control)
	control.size = Vector2(600, 600)
	control.artboard_name = artboard
	control.playing = false
	control.update_mode = RiveControl.UPDATE_MANUAL
	var file := RiveFile.new()
	file.data = FileAccess.get_file_as_bytes(path)
	control.rive_file = file
	root.add_child(control)
	if not _check(control.load_file() == OK,
			"Cannot import fixture: " + path + " (" + file.get_load_error() + ")"):
		return null
	return control

func _peak(pcm: PackedVector2Array) -> float:
	var peak := 0.0
	for sample in pcm:
		if not _check(is_finite(sample.x) and is_finite(sample.y), "Audio mixer returned non-finite PCM"):
			return 0.0
		peak = maxf(peak, maxf(absf(sample.x), absf(sample.y)))
	return peak

func _test_audio() -> void:
	var stream := RiveAudioStream.new()
	var first := stream.instantiate_playback()
	var second := stream.instantiate_playback()
	if not _check(first != null and second != null, "Godot audio stream could not create playback objects"):
		return
	first.start()
	second.start()
	_check(first.is_playing(), "The first Rive audio consumer did not start")
	_check(not second.is_playing(), "Two consumers were allowed to drain the shared Rive mixer")
	_check(_peak(second.mix_audio(1.0, 1024)) == 0.0, "Rejected consumer produced audio")
	_check(second.get_playback_position() == 0.0, "Rejected consumer advanced the shared mixer")
	_check(_peak(first.mix_audio(1.0, 1024)) == 0.0, "Empty Rive mixer must initially be silent")
	var control := _load(sample_root.path_join("lip-sync.riv"), AUDIO_HASH, "Lip_sync_2")
	if control != null:
		var embedded_audio := false
		for asset in control.rive_file.get_asset_metadata():
			if asset.extension == "wav" and asset.embedded:
				embedded_audio = true
		_check(embedded_audio, "Official lip-sync sample has no embedded audio asset")
		var frame_count := maxi(1, int(ceil(AudioServer.get_mix_rate() / 60.0)))
		for _i in 120:
			control.advance(1.0 / 60.0)
			audio_peak = maxf(audio_peak, _peak(first.mix_audio(1.0, frame_count)))
		_check(audio_peak > 0.01, "Authored audio events produced only silent PCM")
		_check(first.get_playback_position() > 1.0, "Audio playback position did not advance")
	first.stop()
	second.start()
	_check(not first.is_playing() and second.is_playing(), "The audio consumer slot was not released by stop()")
	second.stop()

func _test_signed_luau() -> void:
	var control := _load(fixture_root.path_join("script_advance_test.riv"), SCRIPT_HASH, "Artboard")
	if control == null:
		return
	var scripts := 0
	for asset in control.rive_file.get_asset_metadata():
		if asset.extension == "lua":
			scripts += 1
			_check(asset.get("script_verified", false), "Official Luau bytecode did not pass production signature verification")
			_check(not asset.get("script_is_module", true), "Counter fixture unexpectedly contains a utility module")
			_check(int(asset.get("script_generator_ref", 0)) > 0, "Signed Luau generator was not registered")
	_check(scripts == 1, "Counter fixture must contain one signed NodeScript1 asset")
	var vm = control.get_view_model_instance()
	if not _check(vm != null, "Signed Luau fixture has no bound ViewModel"):
		return
	var counter = vm.get_number_property("advanceCount")
	var solo = vm.get_number_property("soloIndex")
	if not _check(counter != null and solo != null, "Signed Luau fixture lacks its authored counter properties"):
		return
	_check(counter.get_value() == 0.0 and solo.get_value() == 0.0, "Signed Luau fixture has unexpected initial state")
	for expected in [1.0, 2.0]:
		control.advance(0.016)
		_check(counter.get_value() == expected, "Signed Luau advance callback did not update its ViewModel")
	solo.set_value(1.0)
	control.advance(0.016)
	_check(counter.get_value() == 3.0, "Alternate scripted solo child did not execute")
	solo.set_value(2.0)
	control.advance(0.016)
	_check(counter.get_value() == 4.0, "Nested scripted artboard did not execute")
	solo.set_value(3.0)
	control.advance(0.016)
	_check(counter.get_value() == 5.0, "Collapsed script transition differs from upstream's one-frame update contract")
	control.advance(0.016)
	_check(counter.get_value() == 5.0, "Collapsed scripts continued to execute")
	solo.set_value(0.0)
	control.advance(0.016)
	_check(counter.get_value() == 5.0, "Uncollapsing skipped upstream's one-frame update contract")
	control.advance(0.016)
	_check(counter.get_value() == 6.0, "Uncollapsed signed script did not resume execution")

func _run() -> void:
	await process_frame
	var capabilities := RivePlayer.get_capabilities()
	print("RIVE_RUNTIME_FEATURES_CAPABILITIES: ", capabilities)
	var audio_ready: bool = capabilities.get("audio_backend", "") == "godot" and RiveAudioStream.is_available()
	var scripting_ready: bool = capabilities.get("scripting", false) and capabilities.get("script_verification", "") == "production_signed"
	_check(audio_ready, "This test requires a build with rive_audio_backend=godot")
	_check(scripting_ready, "This test requires rive_scripting=yes with production-only signature verification")
	if audio_ready:
		_test_audio()
	if scripting_ready:
		_test_signed_luau()
	await _frames(3)
	var rendered := root.get_texture().get_image()
	_check(rendered != null and not rendered.is_empty(), "Runtime feature GPU readback failed")
	for control in controls:
		control.rive_file = null
		control.queue_free()
	controls.clear()
	await _frames()
	if failures.is_empty():
		print("RIVE_RUNTIME_FEATURES_SMOKE_PASS: non-silent PCM peak=", audio_peak,
			", single audio consumer, production-signed Luau callbacks and collapsed/nested script lifecycle")
	else:
		print("RIVE_RUNTIME_FEATURES_SMOKE_FAIL: ", failures)
	quit(0 if failures.is_empty() else 1)
