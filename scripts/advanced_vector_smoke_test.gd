extends SceneTree
var failures: Array[String] = []
var canvas: RiveCanvas2D
var mesh := RiveMesh.new()
var image := RiveImage.new()
var path := RivePath.new()
var paint := RivePaint.new()
var white_paint := RivePaint.new()
var inside_path := RivePath.new()
var outside_path := RivePath.new()
var clip_path := RivePath.new()
var clip_fill := RivePath.new()
var inside_paint := RivePaint.new()
var outside_paint := RivePaint.new()
var draw_count := 0

func _initialize() -> void:
	call_deferred("_run")

func _check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)
		push_error(message)

func _frames(count := 4) -> void:
	for _i in count:
		await process_frame
		await RenderingServer.frame_post_draw

func _draw(renderer: RiveRendererWrapper) -> void:
	draw_count += 1
	renderer.save()
	renderer.modulate_opacity(0.5)
	renderer.draw_path(path, paint)
	renderer.restore()
	renderer.save()
	renderer.translate(220, 0)
	renderer.modulate_color(Color.RED)
	renderer.modulate_color(Color.BLUE, true)
	renderer.draw_path(path, white_paint)
	renderer.restore()
	renderer.draw_path(inside_path, inside_paint)
	renderer.draw_path(outside_path, outside_paint)
	renderer.save()
	renderer.clip_stroke(clip_path, 10, RivePaint.JOIN_MITER, RivePaint.CAP_BUTT, RivePaint.STROKE_CENTER)
	renderer.draw_path(clip_fill, paint)
	renderer.restore()
	_check(renderer.draw_image_mesh_instanced(image, mesh, [Transform2D(0, Vector2(70, 0)), Transform2D(0, Vector2(130, 0))], PackedFloat32Array([1.0, 0.5]), RiveRendererWrapper.make_image_sampler(1)), "Native instanced draw rejected")
	_check(not renderer.draw_image_mesh_instanced(image, mesh, [Transform2D.IDENTITY], PackedFloat32Array([INF])), "Invalid instance opacity accepted")

func _run() -> void:
	create_timer(30).timeout.connect(func() -> void: quit(2))
	var vertices := PackedVector2Array([Vector2(10, 10), Vector2(50, 10), Vector2(50, 50), Vector2(10, 50)])
	var uvs := PackedVector2Array([Vector2.ZERO, Vector2.RIGHT, Vector2.ONE, Vector2.DOWN])
	_check(mesh.set_geometry(vertices, uvs, PackedInt32Array([0, 1, 2, 0, 2, 3])), "Valid reusable geometry rejected")
	_check(not mesh.set_geometry(vertices, uvs, PackedInt32Array([0, 1, 9])), "Invalid mesh index accepted")
	_check(mesh.get_vertex_count() == 4 and mesh.get_index_count() == 6, "Rejected geometry changed cached mesh")
	_check(RiveRendererWrapper.make_image_sampler(1, 2, 2) == 17 and RiveRendererWrapper.make_image_sampler(3) == -1, "Image sampler mapping differs from native")
	var pixels := Image.create(2, 2, false, Image.FORMAT_RGBA8)
	pixels.fill(Color(1, 0, 0, 1))
	_check(image.load_from_buffer(pixels.save_png_to_buffer()), "Cannot create instance image")
	path.add_rect(10, 10, 40, 40)
	paint.set_color(Color(0, 1, 0, 1))
	white_paint.set_color(Color.WHITE)
	inside_path.add_rect(10, 90, 40, 40)
	outside_path.add_rect(80, 90, 40, 40)
	clip_path.add_rect(150, 90, 40, 40)
	clip_fill.add_rect(140, 80, 60, 60)
	inside_paint.set_style(RivePaint.STYLE_STROKE)
	inside_paint.set_thickness(10)
	inside_paint.set_stroke_position(RivePaint.STROKE_INSIDE)
	inside_paint.set_color(Color.RED)
	outside_paint.set_style(RivePaint.STYLE_STROKE)
	outside_paint.set_thickness(10)
	outside_paint.set_stroke_position(RivePaint.STROKE_OUTSIDE)
	outside_paint.set_color(Color.BLUE)
	canvas = RiveCanvas2D.new()
	canvas.size = Vector2i(300, 160)
	root.add_child(canvas)
	var raw := RiveRaw.new()
	raw.bounds = Rect2(0, 0, 300, 160)
	raw.draw_rive.connect(_draw)
	canvas.add_child(raw)
	await _frames(6)
	var result := canvas.read_image()
	_check(result != null and draw_count > 0, "No advanced vector GPU output")
	if result != null:
		_check(absf(result.get_pixel(30, 30).a - 0.5) < 0.1, "Opacity scope was not replayed")
		_check(result.get_pixel(100, 30).r > 0.9 and result.get_pixel(100, 30).a > 0.9, "First mesh instance absent or scoped opacity leaked")
		var second := result.get_pixel(160, 30)
		_check(second.r >= second.a * 0.9 and second.g < 0.1 and absf(second.a - 0.5) < 0.1, "Second mesh instance opacity differs: " + str(second))
		_check(result.get_pixel(250, 30).b > 0.9 and result.get_pixel(250, 30).g < 0.1, "Color replacement scope was not replayed")
		_check(result.get_pixel(5, 110).a < 0.1 and result.get_pixel(15, 110).r > 0.9, "Inside stroke position is incorrect")
		_check(result.get_pixel(75, 110).b > 0.9 and result.get_pixel(85, 110).a < 0.1, "Outside stroke position is incorrect")
		_check(result.get_pixel(150, 110).g > 0.9 and result.get_pixel(170, 110).a < 0.1, "Stroke clipping did not preserve its hollow center")
	vertices[2] = Vector2(45, 48)
	_check(mesh.set_geometry(vertices, uvs, PackedInt32Array([0, 1, 2, 0, 2, 3])), "Mesh replacement rejected")
	raw.queue_redraw()
	await _frames()
	canvas.queue_free()
	await _frames()
	if failures.is_empty(): print("ADVANCED_VECTOR_SMOKE_PASS")
	quit(0 if failures.is_empty() else 1)
