extends SceneTree
const RESOURCE := "res://rivegd_export_test_fixture.tres"
const PAYLOAD := "res://rivegd_export_test_payload.bin"
const PRESET := "res://export_presets.cfg"
const MARKER := "RiveGD export dependency regression"

func _initialize() -> void:
	call_deferred("_run")

func _write_payload(version: String) -> void:
	var file := FileAccess.open(PAYLOAD, FileAccess.WRITE)
	file.store_string(MARKER + ":" + version)

func _run() -> void:
	var args := OS.get_cmdline_user_args()
	if args.is_empty(): quit(2); return
	if args[0] == "setup":
		for path: String in [RESOURCE, PAYLOAD, PRESET]:
			if FileAccess.file_exists(path):
				push_error("Refusing to replace existing export test path: " + path)
				quit(2)
				return
		_write_payload("one")
		var file := RiveFile.new()
		file.asset_overrides = {12: PAYLOAD}
		if ResourceSaver.save(file, RESOURCE) != OK: quit(2); return
		var preset := ConfigFile.new()
		preset.set_value("preset.0", "name", MARKER)
		preset.set_value("preset.0", "platform", "Windows Desktop")
		preset.set_value("preset.0", "runnable", false)
		preset.set_value("preset.0", "export_filter", "resources")
		preset.set_value("preset.0", "export_files", PackedStringArray([RESOURCE]))
		preset.set_value("preset.0", "include_filter", "")
		preset.set_value("preset.0", "exclude_filter", "")
		preset.set_value("preset.0.options", "binary_format/architecture", "x86_64")
		if preset.save(PRESET) != OK: quit(2); return
		print("EXPORT_DEPENDENCIES_SETUP_PASS")
	elif args[0] == "update":
		if FileAccess.get_file_as_string(PAYLOAD) != MARKER + ":one": quit(2); return
		_write_payload("two")
		print("EXPORT_DEPENDENCIES_UPDATE_PASS")
	elif args[0] == "verify" and args.size() == 3:
		if FileAccess.file_exists(ProjectSettings.globalize_path(PAYLOAD)):
			push_error("Run cleanup before verify to exclude filesystem fallback")
			quit(2)
			return
		if not ProjectSettings.load_resource_pack(args[1], true): quit(2); return
		if FileAccess.get_file_as_string(PAYLOAD) != MARKER + ":" + args[2]:
			push_error("Exported payload missing or stale")
			quit(1)
			return
		print("EXPORT_DEPENDENCIES_VERIFY_PASS: ", args[2])
	elif args[0] == "cleanup":
		var preset := ConfigFile.new()
		if preset.load(PRESET) != OK or preset.get_value("preset.0", "name", "") != MARKER:
			push_error("Refusing cleanup: test preset marker missing")
			quit(2)
			return
		for path: String in [RESOURCE, PAYLOAD, PRESET]:
			DirAccess.remove_absolute(path)
		print("EXPORT_DEPENDENCIES_CLEANUP_PASS")
	else: quit(2); return
	quit(0)
