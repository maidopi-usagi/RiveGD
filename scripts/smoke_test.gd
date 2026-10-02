extends SceneTree

# Run with a real rendering driver, for example:
# godot --path project --rendering-method mobile --rendering-driver vulkan \
#   --render-thread separate --script ../scripts/smoke_test.gd

var canvas: RiveCanvas2D
var control: RiveControl
var path: RivePath
var paint: RivePaint
var image: RiveImage
var draw_count := 0
var failures: Array[String] = []

func _initialize() -> void:
	call_deferred("_run")

func _check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)
		push_error(message)

func _draw(renderer: RiveRendererWrapper) -> void:
	# Decode inside the scene-thread drawing callback to exercise nested dispatch.
	if image == null:
		var pixels := Image.create(2, 2, false, Image.FORMAT_RGBA8)
		pixels.fill(Color.GREEN)
		image = RiveImage.new()
		_check(image.load_from_buffer(pixels.save_png_to_buffer()), "PNG decode failed")
		path = RivePath.new()
		path.add_rect(0, 0, 64, 64)
		paint = RivePaint.new()
		paint.set_color(Color.RED)
	renderer.draw_path(path, paint)
	renderer.draw_image_mesh(image,
		PackedVector2Array([Vector2(64, 0), Vector2(128, 0), Vector2(128, 64), Vector2(64, 64)]),
		PackedVector2Array([Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1)]),
		PackedInt32Array([0, 1, 2, 0, 2, 3]), 1.0, 3)
	draw_count += 1

func _run() -> void:
	await process_frame
	canvas = RiveCanvas2D.new()
	canvas.size = Vector2i(128, 64)
	root.add_child(canvas)
	var raw := RiveRaw.new()
	raw.draw_rive.connect(_draw)
	canvas.add_child(raw)

	control = RiveControl.new()
	control.position = Vector2(0, 80)
	control.size = Vector2(128, 64)
	root.add_child(control)
	var svg := RiveFile.new()
	svg.data = '<svg xmlns="http://www.w3.org/2000/svg" width="128" height="64"><path fill="#0000ff" d="M0 0H128V64H0Z"/></svg>'.to_utf8_buffer()
	control.rive_file = svg
	_check(control.load_file() == OK, "SVG import failed")
	# A changed resource reloads its existing instance without invalidating it.
	svg.data = '<svg xmlns="http://www.w3.org/2000/svg" width="128" height="64"><path fill="#00ffff" d="M0 0H128V64H0Z"/></svg>'.to_utf8_buffer()
	_check(control.load_file() == OK, "SVG changed reload failed")

	for i in range(12):
		await process_frame
	await RenderingServer.frame_post_draw
	_check(draw_count > 0, "RiveRaw did not draw")
	var texture := canvas.get_texture()
	if texture != null:
		var result := texture.get_image()
		_check(result != null and not result.is_empty(), "GPU readback failed")
		if result != null and not result.is_empty():
			var red := result.get_pixel(32, 32)
			var green := result.get_pixel(96, 32)
			_check(red.r > 0.8 and red.g < 0.2, "Recorded path did not render red")
			_check(green.g > 0.8 and green.r < 0.2, "Recorded image mesh did not render green")
	else:
		# Compatibility rendering has no Texture2DRD; read the window viewport.
		var result := root.get_texture().get_image()
		_check(result != null and not result.is_empty(), "Viewport readback failed")
		if result != null and not result.is_empty():
			_check(result.get_pixel(32, 32).r > 0.8, "Compatibility path render failed")
			_check(result.get_pixel(96, 32).g > 0.8, "Compatibility mesh render failed")

	var args := OS.get_cmdline_user_args()
	var fixture := args[0] if not args.is_empty() else ProjectSettings.globalize_path("res://").path_join("../third-party/rive-runtime/tests/unit_tests/assets/animation_reset_cases.riv")
	var binary := RiveFile.new()
	binary.data = FileAccess.get_file_as_bytes(fixture)
	_check(not binary.data.is_empty(), "Binary Rive fixture is missing: " + fixture)
	control.rive_file = binary
	_check(control.load_file() == OK, "Binary Rive import failed")
	_check(not control.get_artboard_list().is_empty(), "Binary artboards are missing")
	for i in range(4):
		await process_frame
	control.rive_file = null
	canvas.queue_free()
	control.queue_free()
	image = null
	path = null
	paint = null
	await process_frame
	await process_frame
	if failures.is_empty():
		print("RIVE_SMOKE_PASS: paths, image mesh, SVG reload, binary import, teardown")
	else:
		print("RIVE_SMOKE_FAIL: ", failures)
	quit(0 if failures.is_empty() else 1)
