extends RefCounted
## Source: project/icon.svg, Godot Engine Logo (c) 2017 Andrea Calabró.
## Licensed CC BY 4.0: https://creativecommons.org/licenses/by/4.0/
## Upstream attribution: https://github.com/godotengine/godot/blob/master/misc/logo/LICENSE.txt
## Modified here: soft blue shading, dual shadows, animated eyes and gaze.

const CANONICAL_HEAD_WIDTH := 1000.0

const BODY_DATA := "m105 673 152 14q12 1 15 14l4 67 132 10 8-61q2-11 15-15h162q13 4 15 15l8 61 132-10 4-67q3-13 15-14l152-14V427q30-39 56-81-35-59-83-108-43 20-82 47-40-37-88-64 7-51 8-102-59-28-123-42-26 43-46 89-49-7-98 0-20-46-46-89-64 14-123 42 1 51 8 102-48 27-88 64-39-27-82-47-48 49-83 108 26 42 56 81zm0 33v39c0 276 813 276 814 0v-39l-134 12-5 69q-2 10-14 13l-162 11q-12 0-16-11l-10-65H446l-10 65q-4 11-16 11l-162-11q-12-3-14-13l-5-69z"
const CHIN_DATA := "M105 673v33q407 354 814 0v-33z"
const NOSE_DATA := "M483 600c0 34 58 34 58 0v-86c0-34-58-34-58 0z"
const HEAD_CENTER := Vector2(512, 512)
const LEFT_EYE := Vector2(299, 526)
const RIGHT_EYE := Vector2(725, 526)

var _ready := false
var _body: RivePath
var _chin: RivePath
var _nose: RivePath
var _eye: RivePath
var _pupil: RivePath
var _body_paint: RivePaint
var _white_paint: RivePaint
var _pupil_paint: RivePaint
var _light_shadow: RivePaint
var _dark_shadow: RivePaint
var _body_gradient: RiveGradient

func _ensure() -> void:
	if _ready:
		return
	_body = RivePath.new()
	_body.parse_svg(BODY_DATA)
	_chin = RivePath.new()
	_chin.parse_svg(CHIN_DATA)
	_nose = RivePath.new()
	_nose.parse_svg(NOSE_DATA)
	_eye = RivePath.new()
	_eye.add_circle(0, 0, 90)
	_pupil = RivePath.new()
	_pupil.add_circle(0, 0, 60)
	_body_gradient = RiveGradient.new()
	_body_gradient.set_linear(Vector2(512, 77), Vector2(512, 960))
	_body_gradient.set_stops(
		PackedColorArray([Color("63a0ca"), Color("478cbf"), Color("397cad")]),
		PackedFloat32Array([0.0, 0.55, 1.0])
	)
	_body_paint = _paint(Color("478cbf"))
	_body_paint.set_gradient(_body_gradient)
	_white_paint = _paint(Color("fafcff"))
	_pupil_paint = _paint(Color("414042"))
	_light_shadow = _paint(Color(1.0, 1.0, 1.0, 0.75))
	_light_shadow.set_style(RivePaint.STYLE_STROKE)
	_light_shadow.set_thickness(18.0)
	_light_shadow.set_feather(24.0)
	_dark_shadow = _paint(Color(0.49, 0.55, 0.67, 0.48))
	_dark_shadow.set_style(RivePaint.STYLE_STROKE)
	_dark_shadow.set_thickness(20.0)
	_dark_shadow.set_feather(24.0)
	_ready = true

func _paint(color: Color) -> RivePaint:
	var paint := RivePaint.new()
	paint.set_style(RivePaint.STYLE_FILL)
	paint.set_color(color)
	paint.set_join(RivePaint.JOIN_ROUND)
	paint.set_cap(RivePaint.CAP_ROUND)
	return paint

func draw(renderer: RiveRendererWrapper, center: Vector2, display_width: float, rotation_radians: float = 0.0, blink_open_ratio: float = 1.0, look_direction: Vector2 = Vector2.ZERO) -> void:
	if renderer == null or not center.is_finite() or not look_direction.is_finite():
		return
	if display_width <= 0.0 or not is_finite(display_width) or not is_finite(rotation_radians) or not is_finite(blink_open_ratio):
		return
	_ensure()
	var openness := lerpf(0.045, 1.0, clampf(blink_open_ratio, 0.0, 1.0))
	var gaze := look_direction.limit_length(1.0) * 15.0
	renderer.save()
	renderer.translate(center.x, center.y)
	renderer.rotate(rotation_radians)
	renderer.scale(display_width / CANONICAL_HEAD_WIDTH, display_width / CANONICAL_HEAD_WIDTH)
	renderer.translate(-HEAD_CENTER.x, -HEAD_CENTER.y)
	renderer.save()
	renderer.translate(-9, -9)
	renderer.draw_path(_body, _light_shadow)
	renderer.restore()
	renderer.save()
	renderer.translate(12, 15)
	renderer.draw_path(_body, _dark_shadow)
	renderer.restore()
	renderer.draw_path(_chin, _white_paint)
	renderer.draw_path(_body, _body_paint)
	renderer.draw_path(_nose, _white_paint)
	_draw_eye(renderer, LEFT_EYE, Vector2(8, 6), openness, gaze)
	_draw_eye(renderer, RIGHT_EYE, Vector2(-8, 6), openness, gaze)
	renderer.restore()

func _draw_eye(renderer: RiveRendererWrapper, center: Vector2, pupil_offset: Vector2, openness: float, gaze: Vector2) -> void:
	renderer.save()
	renderer.translate(center.x, center.y)
	renderer.scale(1.0, openness)
	renderer.draw_path(_eye, _white_paint)
	renderer.translate(pupil_offset.x + gaze.x, pupil_offset.y + gaze.y)
	renderer.draw_path(_pupil, _pupil_paint)
	renderer.restore()
