extends SceneTree

const BACKGROUND := Color(0.88, 0.90, 0.93, 1.0)
const ACCENT := Color(0.16, 0.62, 0.70, 1.0)
const SIZE := Vector2i(588, 384)
const QUAD_COLOR := Color("b874ab")
const QUADRATIC_PATH_NAMES: Array[StringName] = [
	&"direct_quad", &"absolute_quad", &"relative_quad",
	&"reflected_absolute_quad", &"reflected_relative_quad", &"feathered_quad",
]

var failures: Array[String] = []
var canvas: RiveCanvas2D
var draw_count := 0
var paths: Dictionary[StringName, RivePath] = {}
var paints: Dictionary[StringName, RivePaint] = {}
var snapshot_paint: RivePaint
var snapshot_gradient: RiveGradient
var text_font: RiveFont
var text_paint: RivePaint
var live_text: RiveText
var quad_paint: RivePaint
var quad_feather: RivePaint
var quadratic_points_before: Array = []
var quadratic_bounds_before: Array[Rect2] = []

func _initialize() -> void:
	call_deferred("_run")

func _check(condition: bool, message: String) -> void:
	if not condition:
		failures.append(message)
		push_error(message)

func _points(path: RivePath) -> Array[Vector2]:
	var result: Array[Vector2] = []
	var copy := path.morph(func(point: Vector2) -> Vector2:
		result.append(point)
		return point
	)
	_check(copy != null, "Path point probe failed")
	return result

func _same_points(a: Array[Vector2], b: Array[Vector2]) -> bool:
	if a.size() != b.size():
		return false
	for i in range(a.size()):
		if not a[i].is_equal_approx(b[i]):
			return false
	return true

func _geometry_tests() -> void:
	_check(RivePaint.STYLE_STROKE == 0 and RivePaint.STYLE_FILL == 1, "Paint style mapping changed")
	_check(RivePaint.JOIN_MITER == 0 and RivePaint.JOIN_ROUND == 1 and RivePaint.JOIN_BEVEL == 2, "Stroke join mapping changed")
	_check(RivePaint.CAP_BUTT == 0 and RivePaint.CAP_ROUND == 1 and RivePaint.CAP_SQUARE == 2, "Stroke cap mapping changed")
	_check(RivePath.FILL_NON_ZERO == 0 and RivePath.FILL_EVEN_ODD == 1 and RivePath.FILL_CLOCKWISE == 2, "Fill rule mapping changed")
	var round_rect := RivePath.new()
	_check(round_rect.add_round_rect(10, 20, 80, 40, 12), "Valid rounded rectangle rejected")
	_check(round_rect.get_bounds().is_equal_approx(Rect2(10, 20, 80, 40)), "Rounded rectangle bounds are incorrect")
	var before := _points(round_rect)
	var bounds_before := round_rect.get_bounds()
	_check(not round_rect.add_round_rect(NAN, 0, 10, 10, 2), "NaN rectangle accepted")
	_check(not round_rect.add_round_rect(0, INF, 10, 10, 2), "Infinite rectangle accepted")
	_check(not round_rect.add_round_rect(0, 0, 0, 10, 2), "Zero-width rectangle accepted")
	_check(not round_rect.add_round_rect(0, 0, 10, -10, 2), "Negative-size rectangle accepted")
	_check(not round_rect.add_round_rect(0, 0, 10, 10, -1), "Negative corner radius accepted")
	_check(not round_rect.add_round_rect(3.0e38, 0, 3.0e38, 10, 2), "Overflowing rectangle accepted")
	_check(_same_points(before, _points(round_rect)) and round_rect.get_bounds() == bounds_before, "Rejected rectangle partially modified the path")
	var square := RivePath.new()
	_check(square.add_round_rect(-5, -8, 30, 40, 0), "Zero-radius rectangle rejected")
	_check(square.get_bounds().is_equal_approx(Rect2(-5, -8, 30, 40)), "Zero-radius rectangle bounds changed")
	var clamped := RivePath.new()
	var half_side := RivePath.new()
	_check(clamped.add_round_rect(0, 0, 80, 40, 1000), "Oversize radius rejected instead of clamped")
	_check(half_side.add_round_rect(0, 0, 80, 40, 20), "Half-short-side radius rejected")
	_check(_same_points(_points(clamped), _points(half_side)), "Corner radius was not clamped to half the short side")
	var arc := RivePath.new()
	_check(arc.add_arc(50, 50, 20, 0, PI / 2.0), "Quarter arc rejected")
	var arc_points := _points(arc)
	_check(arc_points.front().distance_to(Vector2(70, 50)) < 0.001, "Arc start is incorrect")
	_check(arc_points.back().distance_to(Vector2(50, 70)) < 0.001, "Positive sweep is not clockwise in y-down coordinates")
	before = arc_points
	bounds_before = arc.get_bounds()
	_check(not arc.add_arc(0, 0, -1, 0, 1), "Negative arc radius accepted")
	_check(not arc.add_arc(0, 0, 10, NAN, 1), "NaN start angle accepted")
	_check(not arc.add_arc(0, 0, 10, 0, INF), "Infinite sweep accepted")
	_check(not arc.add_arc(3.0e38, 0, 3.0e38, PI, TAU), "Overflowing arc accepted")
	_check(arc.add_arc(0, 0, 0, 0, TAU), "Zero-radius arc should be a successful no-op")
	_check(arc.add_arc(0, 0, 10, 0, 0), "Zero-sweep arc should be a successful no-op")
	_check(_same_points(before, _points(arc)) and arc.get_bounds() == bounds_before, "Rejected/no-op arc modified the path")
	var reversed := RivePath.new()
	_check(reversed.add_arc(50, 50, 20, 0, -PI / 2.0), "Negative sweep rejected")
	_check(_points(reversed).back().distance_to(Vector2(50, 30)) < 0.001, "Negative sweep is not counter-clockwise")
	var full := RivePath.new()
	var oversized := RivePath.new()
	_check(full.add_arc(50, 50, 20, 0, TAU), "Full circle arc rejected")
	_check(oversized.add_arc(50, 50, 20, 0, TAU * 100.0), "Oversize arc sweep rejected instead of clamped")
	_check(_same_points(_points(full), _points(oversized)), "Arc sweep was not clamped to one turn")
	_check(full.get_bounds().is_equal_approx(Rect2(30, 30, 40, 40)), "Full-circle arc bounds are incorrect")
	var independent := RivePath.new()
	independent.move_to(-200, -200)
	_check(independent.add_arc(50, 50, 20, 0, 1), "Arc appended to existing path rejected")
	var independent_points := _points(independent)
	_check(independent_points[1].distance_to(Vector2(70, 50)) < 0.001, "Arc did not start a fresh contour")
	var direct_quad := RivePath.new()
	direct_quad.move_to(0, 0)
	direct_quad.quad_to(36, -80, 72, 0)
	direct_quad.close()
	var absolute_quad := RivePath.new()
	absolute_quad.parse_svg("M0 0Q36-80 72 0Z")
	var relative_quad := RivePath.new()
	relative_quad.parse_svg("m0 0q36-80 72 0z")
	_check(_same_points(_points(direct_quad), _points(absolute_quad)), "SVG Q did not preserve quadratic controls")
	_check(_same_points(_points(direct_quad), _points(relative_quad)), "SVG q did not preserve relative quadratic controls")
	var reflected_absolute := RivePath.new()
	reflected_absolute.parse_svg("M0 0Q18-48 36 0T72 0L72 36H0Z")
	var reflected_relative := RivePath.new()
	reflected_relative.parse_svg("m0 0q18-48 36 0t36 0v36h-72z")
	var reflected_points := _points(reflected_absolute)
	_check(_same_points(reflected_points, _points(reflected_relative)), "SVG T/t reflection differs between absolute/relative forms")
	_check(reflected_points[3].is_equal_approx(Vector2(54, 48)), "SVG T did not reflect the previous quadratic control point")

func _paint(name: StringName, color: Color, style: int = RivePaint.STYLE_FILL, feather: float = 0.0, thickness: float = 1.0) -> RivePaint:
	var paint := RivePaint.new()
	paint.set_color(color)
	paint.set_style(style)
	paint.set_feather(feather)
	paint.set_thickness(thickness)
	paint.set_join(RivePaint.JOIN_ROUND)
	paint.set_cap(RivePaint.CAP_ROUND)
	paints[name] = paint
	return paint

func _round(name: StringName, rect: Rect2, radius: float) -> RivePath:
	var path := RivePath.new()
	_check(path.add_round_rect(rect.position.x, rect.position.y, rect.size.x, rect.size.y, radius), "GPU rectangle creation failed")
	paths[name] = path
	return path

func _draw(renderer: RiveRendererWrapper) -> void:
	if paths.is_empty():
		_round(&"background", Rect2(0, 0, SIZE.x, SIZE.y), 0)
		var shadow := _round(&"outer_shadow", Rect2(40, 40, 100, 88), 16)
		shadow.set_fill_rule(RivePath.FILL_CLOCKWISE)
		_round(&"raised_face", Rect2(26, 26, 100, 88), 16)
		_round(&"recessed_clip", Rect2(176, 26, 110, 88), 16)
		_round(&"inner_shadow", Rect2(170, 20, 110, 88), 16)
		var arc := RivePath.new()
		_check(arc.add_arc(74, 180, 28, -PI / 2.0, PI * 1.5), "GPU progress arc creation failed")
		paths[&"progress_arc"] = arc
		_round(&"clipping_probe", Rect2(192, 150, 64, 48), 12)
		_paint(&"background", BACKGROUND)
		_paint(&"outer_shadow", Color(0.18, 0.24, 0.35, 0.85), RivePaint.STYLE_FILL, 8)
		_paint(&"raised_face", BACKGROUND)
		_paint(&"inner_shadow", Color(0.20, 0.28, 0.38, 0.85), RivePaint.STYLE_STROKE, 8, 12)
		_paint(&"accent", ACCENT, RivePaint.STYLE_STROKE, 0, 8)
		_paint(&"clipping_probe", Color(0.85, 0.25, 0.30, 1.0))
	renderer.draw_path(paths[&"background"], paints[&"background"])
	renderer.draw_path(paths[&"outer_shadow"], paints[&"outer_shadow"])
	renderer.draw_path(paths[&"raised_face"], paints[&"raised_face"])
	renderer.save()
	renderer.clip_path(paths[&"recessed_clip"])
	renderer.draw_path(paths[&"inner_shadow"], paints[&"inner_shadow"])
	renderer.restore()
	renderer.draw_path(paths[&"progress_arc"], paints[&"accent"])
	renderer.save()
	renderer.clip_path(paths[&"clipping_probe"])
	renderer.draw_path(paths[&"background"], paints[&"clipping_probe"])
	renderer.restore()
	_draw_snapshots(renderer)
	draw_count += 1

func _draw_snapshots(renderer: RiveRendererWrapper) -> void:
	if snapshot_paint == null:
		_round(&"snapshot_red", Rect2(344, 26, 64, 48), 10)
		_round(&"snapshot_blue", Rect2(422, 26, 64, 48), 10)
		_round(&"snapshot_stroke", Rect2(500, 26, 64, 48), 10)
		_round(&"gradient_original", Rect2(344, 108, 64, 48), 10)
		_round(&"gradient_changed", Rect2(422, 108, 64, 48), 10)
		_round(&"gradient_to_solid", Rect2(500, 108, 64, 48), 10)
		var hole := _round(&"even_odd_hole", Rect2(500, 196, 64, 48), 10)
		_check(hole.add_round_rect(514, 208, 36, 24, 6), "Even-odd hole contour failed")
		hole.set_fill_rule(RivePath.FILL_EVEN_ODD)
		var direct := RivePath.new()
		direct.move_to(18, 350)
		direct.quad_to(54, 270, 90, 350)
		direct.close()
		paths[&"direct_quad"] = direct
		var svg_quadratics := {
			&"absolute_quad": "M112 350 Q148 270 184 350Z",
			&"relative_quad": "M206 350 q36 -80 72 0z",
			&"reflected_absolute_quad": "M300 320 Q318 272 336 320 T372 320 L372 356H300Z",
			&"reflected_relative_quad": "M394 320 q18 -48 36 0 t36 0 v36h-72z",
			&"feathered_quad": "M488 350 Q524 270 560 350Z",
		}
		for name: StringName in svg_quadratics:
			var quadratic := RivePath.new()
			quadratic.parse_svg(svg_quadratics[name])
			paths[name] = quadratic
		paths[&"feathered_quad"].set_fill_rule(RivePath.FILL_CLOCKWISE)
		for name in QUADRATIC_PATH_NAMES:
			quadratic_points_before.append(_points(paths[name]))
			quadratic_bounds_before.append(paths[name].get_bounds())
		quad_paint = _paint(&"quad", QUAD_COLOR)
		quad_feather = _paint(&"quad_feather", Color(0.28, 0.36, 0.54, 0.90), RivePaint.STYLE_FILL, 6.0)
		snapshot_paint = RivePaint.new()
		snapshot_gradient = RiveGradient.new()
		snapshot_gradient.set_linear(Vector2(344, 108), Vector2(408, 108))
		text_font = RiveFont.new()
		_check(text_font.load_from_file("res://assets/fonts/InterVariable.ttf"), "Vector text fixture font could not be loaded")
		text_paint = RivePaint.new()
		text_paint.set_color(Color.RED)
		live_text = RiveText.new()
		live_text.append_run("M", text_font, text_paint, 44.0, 56.0, 0.0)
	snapshot_paint.set_gradient(null)
	snapshot_paint.set_style(RivePaint.STYLE_FILL)
	snapshot_paint.set_feather(0)
	snapshot_paint.set_color(Color.RED)
	renderer.draw_path(paths[&"snapshot_red"], snapshot_paint)
	snapshot_paint.set_color(Color.BLUE)
	renderer.draw_path(paths[&"snapshot_blue"], snapshot_paint)
	snapshot_paint.set_color(Color.GREEN)
	snapshot_paint.set_style(RivePaint.STYLE_STROKE)
	snapshot_paint.set_thickness(8)
	snapshot_paint.set_feather(4)
	renderer.draw_path(paths[&"snapshot_stroke"], snapshot_paint)
	snapshot_paint.set_style(RivePaint.STYLE_FILL)
	snapshot_paint.set_feather(0)
	snapshot_paint.set_color(Color.WHITE)
	snapshot_gradient.set_stops(PackedColorArray([Color.RED, Color.RED]), PackedFloat32Array([0.0, 1.0]))
	snapshot_paint.set_gradient(snapshot_gradient)
	renderer.draw_path(paths[&"gradient_original"], snapshot_paint)
	snapshot_gradient.set_stops(PackedColorArray([Color.BLUE, Color.BLUE]), PackedFloat32Array([0.0, 1.0]))
	renderer.draw_path(paths[&"gradient_changed"], snapshot_paint)
	snapshot_paint.set_gradient(null)
	snapshot_paint.set_color(Color.YELLOW)
	renderer.draw_path(paths[&"gradient_to_solid"], snapshot_paint)
	text_paint.set_color(Color.BLUE)
	renderer.save()
	renderer.translate(344, 196)
	live_text.render(renderer, null)
	renderer.restore()
	for name in QUADRATIC_PATH_NAMES:
		if name != &"feathered_quad": renderer.draw_path(paths[name], quad_paint)
	renderer.draw_path(paths[&"feathered_quad"], quad_feather)
	snapshot_paint.set_color(Color.PURPLE)
	renderer.draw_path(paths[&"even_odd_hole"], snapshot_paint)
	text_paint.set_color(Color.RED)
	renderer.save()
	renderer.translate(422, 196)
	live_text.render(renderer, null)
	renderer.restore()

func _color_distance(a: Color, b: Color) -> float:
	return Vector3(a.r, a.g, a.b).distance_to(Vector3(b.r, b.g, b.b))

func _gpu_checks(image: Image) -> void:
	_check(image.get_size() == SIZE, "Vector readback size is incorrect")
	_check(_color_distance(image.get_pixel(76, 66), BACKGROUND) < 0.025, "Raised face fill did not render")
	_check(_color_distance(image.get_pixel(132, 80), BACKGROUND) > 0.04, "Feathered outer shadow did not render")
	_check(_color_distance(image.get_pixel(154, 80), BACKGROUND) < 0.025, "Outer shadow feather extends unexpectedly far")
	_check(_color_distance(image.get_pixel(171, 70), BACKGROUND) < 0.025, "Clipped inner shadow escaped its rounded panel")
	_check(_color_distance(image.get_pixel(228, 70), BACKGROUND) < 0.025, "Clipped inner shadow covered the panel center")
	_check(_color_distance(image.get_pixel(178, 70), BACKGROUND) > 0.04, "Clipped inner shadow did not render inside its panel")
	_check(_color_distance(image.get_pixel(74, 152), ACCENT) < 0.04, "Arc round-cap start did not render")
	_check(_color_distance(image.get_pixel(102, 180), ACCENT) < 0.04, "Arc clockwise progress did not render")
	_check(_color_distance(image.get_pixel(74, 208), ACCENT) < 0.04, "Arc multi-segment progress did not render")
	_check(_color_distance(image.get_pixel(53, 159), BACKGROUND) < 0.025, "Arc sweep filled the missing quadrant")
	_check(image.get_pixel(224, 174).r > 0.80 and image.get_pixel(224, 174).g < 0.30, "Rounded clipping probe lost its inner fill")
	_check(_color_distance(image.get_pixel(192, 150), BACKGROUND) < 0.025, "Rounded clip did not cut off its corner")
	_check(_color_distance(image.get_pixel(188, 174), BACKGROUND) < 0.025, "Rounded clip leaked outside its side")
	_check(_color_distance(image.get_pixel(376, 50), Color.RED) < 0.025, "Reused paint retroactively recolored the first draw")
	_check(_color_distance(image.get_pixel(454, 50), Color.BLUE) < 0.025, "Reused paint retroactively changed the second draw style")
	_check(_color_distance(image.get_pixel(532, 50), BACKGROUND) < 0.025, "Reused paint stroke snapshot filled its center")
	_check(image.get_pixel(501, 50).g > 0.70 and image.get_pixel(501, 50).r < 0.40, "Reused paint feathered stroke did not render")
	_check(_color_distance(image.get_pixel(376, 132), Color.RED) < 0.025, "Gradient edit changed an earlier recorded shader")
	_check(_color_distance(image.get_pixel(454, 132), Color.BLUE) < 0.025, "Same-gradient stops edit did not update its paint snapshot")
	_check(_color_distance(image.get_pixel(532, 132), Color.YELLOW) < 0.025, "Clearing a gradient did not restore solid color")
	_check(_count_text_color(image, Rect2i(344, 196, 64, 60), Color.BLUE) > 100, "RawText run paint did not update after append_run")
	_check(_count_text_color(image, Rect2i(422, 196, 64, 60), Color.RED) > 100, "RawText run paint did not update before its second render")
	_check(_count_text_color(image, Rect2i(344, 196, 64, 60), Color.RED) == 0, "RawText paint update retroactively changed the first text draw")
	_check(_color_distance(image.get_pixel(506, 220), Color.PURPLE) < 0.025, "Even-odd outer contour did not render")
	_check(_color_distance(image.get_pixel(532, 220), BACKGROUND) < 0.025, "Clockwise feather opt-in changed the fill rule of another path")
	for x in [54, 148, 242]:
		_check(_color_distance(image.get_pixel(x, 320), QUAD_COLOR) < 0.025, "Direct/SVG Q/q quadratic arch did not render at x=" + str(x))
		_check(_color_distance(image.get_pixel(x, 296), BACKGROUND) < 0.025, "Quadratic control point was rendered as an endpoint at x=" + str(x))
	_check(_color_distance(image.get_pixel(318, 302), QUAD_COLOR) < 0.025, "SVG T predecessor quadratic arch was lost")
	_check(_color_distance(image.get_pixel(354, 350), QUAD_COLOR) < 0.025, "SVG T reflected downward quadratic did not render")
	_check(_color_distance(image.get_pixel(354, 332), BACKGROUND) < 0.025, "SVG T reflection was replaced by a straight contour")
	_check(_color_distance(image.get_pixel(412, 302), QUAD_COLOR) < 0.025, "SVG t predecessor quadratic arch was lost")
	_check(_color_distance(image.get_pixel(448, 350), QUAD_COLOR) < 0.025, "SVG t reflected downward quadratic did not render")
	_check(_color_distance(image.get_pixel(448, 332), BACKGROUND) < 0.025, "SVG t reflection was replaced by a straight contour")
	_check(_color_distance(image.get_pixel(524, 320), BACKGROUND) > 0.08, "Clockwise feathered quadratic fill did not render")
	_check(_color_distance(image.get_pixel(524, 288), BACKGROUND) < 0.025, "Feathered quadratic fill used its control-point bounds as geometry")
	for index in QUADRATIC_PATH_NAMES.size():
		var path := paths[QUADRATIC_PATH_NAMES[index]]
		_check(_same_points(quadratic_points_before[index], _points(path)), "GPU quadratic conversion mutated source path points")
		_check(quadratic_bounds_before[index] == path.get_bounds(), "GPU quadratic conversion mutated source path bounds")

func _count_text_color(image: Image, rect: Rect2i, color: Color) -> int:
	var count := 0
	for y in range(rect.position.y, rect.end.y):
		for x in range(rect.position.x, rect.end.x):
			if _color_distance(image.get_pixel(x, y), color) < 0.08:
				count += 1
	return count

func _run() -> void:
	_geometry_tests()
	if DisplayServer.get_name() != "headless":
		await process_frame
		canvas = RiveCanvas2D.new()
		canvas.size = SIZE
		root.add_child(canvas)
		var raw := RiveRaw.new()
		raw.draw_rive.connect(_draw)
		canvas.add_child(raw)
		for i in range(10):
			await process_frame
		await RenderingServer.frame_post_draw
		_check(draw_count > 0, "Vector GPU callback did not run")
		var image := canvas.read_image()
		_check(image != null and not image.is_empty(), "Vector GPU readback failed")
		if image != null and not image.is_empty():
			_gpu_checks(image)
			var args := OS.get_cmdline_user_args()
			if not args.is_empty():
				_check(image.save_png(args[0]) == OK, "Vector test PNG could not be saved")
		canvas.queue_free()
		canvas = null
		await process_frame
		await process_frame
	else:
		print("RIVE_VECTOR_GPU_SKIPPED: headless geometry-only run")
	paths.clear()
	paints.clear()
	snapshot_paint = null
	snapshot_gradient = null
	live_text = null
	text_paint = null
	text_font = null
	quad_paint = null
	quad_feather = null
	quadratic_points_before.clear()
	quadratic_bounds_before.clear()
	if failures.is_empty():
		print("RIVE_VECTOR_API_PASS: atomic geometry, clamps, enums, shadows, arc, clip, paint/gradient snapshots, live text, Q/q/T/t GPU paths")
	else:
		print("RIVE_VECTOR_API_FAIL: ", failures)
	quit(0 if failures.is_empty() else 1)
