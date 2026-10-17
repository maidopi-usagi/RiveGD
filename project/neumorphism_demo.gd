extends Node

const DESIGN_SIZE := Vector2(1280, 840)
const BACKGROUND := Color("e6e8f0")
const INK := Color("44475f")
const MUTED := Color("858a9f")
const ACCENT := Color("478cbf")
const TEAL := Color("78a6b8")
const LOOP_SECONDS := 4.0
const GODOT_HEAD := preload("res://neumorphism_godot_head.gd")
const HIT_RECTS := {
	"play": Rect2(385, 625, 90, 90),
	"previous": Rect2(270, 641, 58, 58),
	"next": Rect2(532, 641, 58, 58),
	"node2d": Rect2(856, 258, 155, 52),
	"control": Rect2(1011, 258, 155, 52),
	"scale": Rect2(842, 375, 338, 48),
	"idle": Rect2(856, 443, 98, 44),
	"hop": Rect2(963, 443, 98, 44),
	"spin": Rect2(1070, 443, 96, 44),
	"motion": Rect2(1085, 530, 82, 40),
}
const ACTION_ALIASES := {"focus": "node2d", "ambient": "control", "volume": "scale", "rain": "idle", "waves": "hop", "air": "spin"}
const KEYBOARD_ORDER := ["play", "node2d", "control", "scale", "idle", "hop", "spin", "motion"]

@export var font_resource: FontFile
@onready var _canvas: RiveCanvas2D = $Canvas
@onready var _raw: RiveRaw = $Canvas/Raw

var _playing := true
var _mode := 0
var _volume := 0.5
var _sound := 0
var _motion_enabled := true
var _elapsed := 0.0
var _ui_time := 0.0
var _wave_time := 0.0
var _draw_count := 0
var _hover := ""
var _pressed := ""
var _dragging := false
var _touch_id := -1
var _focus := ""
var _keyboard_focus := false
var _layout_scale := 1.0
var _layout_origin := Vector2.ZERO
var _viewport_size := DESIGN_SIZE
var _visual_volume := 0.5
var _mode_position := 0.0
var _mode_velocity := 0.0
var _toggle_position := 1.0
var _toggle_velocity := 0.0
var _press_mix := 0.0
var _sound_mix := [1.0, 0.0, 0.0]
var _head: RefCounted = GODOT_HEAD.new()

var _resources_ready := false
var _resources_failed := false
var _font: RiveFont
var _paths: Dictionary = {}
var _shape_specs: Dictionary = {}
var _shadow_paths: Dictionary = {}
var _paints: Dictionary = {}
var _texts: Dictionary = {}
var _progress: RivePath
var _timeline: RivePath
var _background: RivePath
var _last_background_size := Vector2.ZERO


func _ready() -> void:
	get_viewport().size_changed.connect(_fit_layout)
	_fit_layout()
	_raw.set_update_mode(RiveNode.UPDATE_ALWAYS)
	get_window().title = "Godot — neumorphism_demo.tscn"


func _fit_layout() -> void:
	_viewport_size = get_viewport().get_visible_rect().size
	if _viewport_size.x <= 0.0 or _viewport_size.y <= 0.0:
		return
	_layout_scale = minf(_viewport_size.x / DESIGN_SIZE.x, _viewport_size.y / DESIGN_SIZE.y)
	_layout_origin = (_viewport_size - DESIGN_SIZE * _layout_scale) * 0.5
	_canvas.size = Vector2i(_viewport_size.ceil())
	_raw.bounds = Rect2(Vector2.ZERO, _viewport_size)


func _process(delta: float) -> void:
	var dt := maxf(delta, 0.0)
	_ui_time += dt
	if _playing:
		_elapsed += dt
		if _motion_enabled:
			_wave_time += dt
	var ease := 1.0 - exp(-dt * 12.0)
	_visual_volume = lerpf(_visual_volume, _volume, ease)
	_press_mix = lerpf(_press_mix, 1.0 if _pressed == "play" else 0.0, 1.0 - exp(-dt * 22.0))
	for i in range(3):
		_sound_mix[i] = lerpf(_sound_mix[i], 1.0 if _sound == i else 0.0, ease)
	var remaining := minf(dt, 1.0)
	while remaining > 0.0:
		var step := minf(remaining, 1.0 / 120.0)
		_mode_velocity += (float(_mode) - _mode_position) * 260.0 * step
		_mode_velocity *= exp(-step * 22.0)
		_mode_position += _mode_velocity * step
		_toggle_velocity += ((1.0 if _motion_enabled else 0.0) - _toggle_position) * 300.0 * step
		_toggle_velocity *= exp(-step * 19.0)
		_toggle_position += _toggle_velocity * step
		remaining -= step
	if is_instance_valid(_raw):
		_raw.request_rive_update()


func get_demo_state() -> Dictionary:
	return {
		"playing": _playing, "mode": _mode, "volume": _volume, "sound": _sound,
		"motion_enabled": _motion_enabled, "elapsed": _elapsed,
		"animation_time": _ui_time, "wave_time": _wave_time,
		"selected_node": "Node2D" if _mode == 0 else "Control",
		"preview_scale": 0.6 + 0.8 * _volume,
		"rendered_preview_scale": 0.6 + 0.8 * _visual_volume,
		"animation_name": ["Idle", "Hop", "Spin"][_sound],
		"loop_position": fposmod(_wave_time, LOOP_SECONDS) / LOOP_SECONDS,
		"draw_count": _draw_count, "resources_ready": _resources_ready,
		"viewport_scale": _layout_scale, "active_action": _pressed,
		"smoothed_volume": _visual_volume, "smoothed_mode": _mode_position,
		"toggle_position": _toggle_position, "cached_text_count": _texts.size(),
	}


func get_demo_hit_rect(action: String) -> Rect2:
	action = ACTION_ALIASES.get(action, action)
	if not HIT_RECTS.has(action):
		return Rect2()
	var rect: Rect2 = HIT_RECTS[action]
	return Rect2(_layout_origin + (rect.position + Vector2(0, _entry_offset_y())) * _layout_scale, rect.size * _layout_scale)


func set_demo_action(action: String, value: Variant = null) -> bool:
	action = ACTION_ALIASES.get(action, action)
	match action:
		"play":
			_playing = not _playing if value == null else bool(value)
		"mode":
			_mode = 1 if (value == 1 or str(value).to_lower() in ["control", "ambient"]) else 0
		"node2d": _mode = 0
		"control": _mode = 1
		"scale": _volume = clampf(float(value), 0.0, 1.0)
		"sound":
			if value is String:
				_sound = maxi(0, ["idle", "hop", "spin"].find(ACTION_ALIASES.get(value.to_lower(), value.to_lower())))
			else:
				_sound = clampi(int(value), 0, 2)
		"idle": _sound = 0
		"hop": _sound = 1
		"spin": _sound = 2
		"motion": _motion_enabled = not _motion_enabled if value == null else bool(value)
		"previous": _sound = posmod(_sound - 1, 3)
		"next": _sound = posmod(_sound + 1, 3)
		"reset":
			_elapsed = 0.0
			_wave_time = 0.0
			_playing = false
		"advance": _process(maxf(0.0, float(value)))
		_: return false
	return true


func _logical(position: Vector2) -> Vector2:
	return (position - _layout_origin) / maxf(_layout_scale, 0.001) - Vector2(0, _entry_offset_y())


func _entry_offset_y() -> float:
	return exp(-_ui_time * 4.0) * 22.0


func _hit(position: Vector2) -> String:
	var point := _logical(position)
	for action: String in HIT_RECTS:
		if (HIT_RECTS[action] as Rect2).has_point(point):
			return action
	return ""


func _drag_volume(position: Vector2) -> void:
	_volume = clampf((_logical(position).x - 864.0) / 294.0, 0.0, 1.0)


func _pointer_down(position: Vector2) -> void:
	_keyboard_focus = false
	_pressed = _hit(position)
	_focus = _pressed
	_dragging = _pressed == "scale"
	if _dragging:
		_drag_volume(position)


func _pointer_up(position: Vector2) -> void:
	if _dragging:
		_drag_volume(position)
	elif _pressed != "" and _hit(position) == _pressed:
		set_demo_action(_pressed)
	_pressed = ""
	_dragging = false


func _input(event: InputEvent) -> void:
	if event is InputEventMouse and event.device == InputEvent.DEVICE_ID_EMULATION:
		return
	if event is InputEventMouseMotion:
		_hover = _hit(event.position)
		if _dragging and _touch_id < 0:
			_drag_volume(event.position)
		Input.set_default_cursor_shape(Input.CURSOR_POINTING_HAND if _hover != "" else Input.CURSOR_ARROW)
	elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if event.pressed:
			_pointer_down(event.position)
		else:
			_pointer_up(event.position)
		if _focus != "":
			get_viewport().set_input_as_handled()
	elif event is InputEventScreenTouch:
		if event.pressed and _touch_id < 0:
			_touch_id = event.index
			_pointer_down(event.position)
		elif not event.pressed and event.index == _touch_id:
			_pointer_up(event.position)
			_touch_id = -1
	elif event is InputEventScreenDrag and event.index == _touch_id and _dragging:
		_drag_volume(event.position)
	elif event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_SPACE:
				set_demo_action("play")
				get_viewport().set_input_as_handled()
			KEY_TAB:
				var index := KEYBOARD_ORDER.find(_focus)
				if index < 0 and event.shift_pressed:
					index = 0
				_focus = KEYBOARD_ORDER[posmod(index + (-1 if event.shift_pressed else 1), KEYBOARD_ORDER.size())]
				_keyboard_focus = true
				get_viewport().set_input_as_handled()
			KEY_ENTER, KEY_KP_ENTER:
				if _focus != "" and _focus != "scale":
					set_demo_action(_focus)
			KEY_LEFT, KEY_RIGHT:
				if _focus == "scale":
					set_demo_action("scale", _volume + (-0.05 if event.keycode == KEY_LEFT else 0.05))
			KEY_N: set_demo_action("node2d")
			KEY_C: set_demo_action("control")
			KEY_ESCAPE:
				_pressed = ""
				_dragging = false
				_touch_id = -1


func _paint(color: Color, stroke := 0.0, feather := 0.0) -> RivePaint:
	var paint := RivePaint.new()
	paint.set_style(RivePaint.STYLE_STROKE if stroke > 0.0 else RivePaint.STYLE_FILL)
	paint.set_color(color)
	paint.set_thickness(stroke)
	paint.set_join(RivePaint.JOIN_ROUND)
	paint.set_cap(RivePaint.CAP_ROUND)
	paint.set_feather(feather)
	return paint


func _round(key: String, width: float, height: float, radius: float) -> RivePath:
	if not _paths.has(key):
		var path := RivePath.new()
		path.add_round_rect(0.0, 0.0, width, height, radius)
		_paths[key] = path
		_shape_specs[path.get_instance_id()] = {"bounds": Rect2(0, 0, width, height), "radius": radius, "circle": false}
	return _paths[key]


func _circle(key: String, radius: float) -> RivePath:
	if not _paths.has(key):
		var path := RivePath.new()
		path.add_circle(0.0, 0.0, radius)
		_paths[key] = path
		_shape_specs[path.get_instance_id()] = {"bounds": Rect2(-radius, -radius, radius * 2, radius * 2), "radius": radius, "circle": true}
	return _paths[key]


func _ensure_resources() -> void:
	if _resources_ready or _resources_failed:
		return
	_resources_failed = true
	_font = RiveFont.new()
	if font_resource == null or not _font.load_from_buffer(font_resource.get_data()):
		push_error("Godot vector demo requires its bundled Inter FontFile resource.")
		return
	_paints = {
		"bg": _paint(BACKGROUND), "ink": _paint(INK), "muted": _paint(MUTED),
		"accent": _paint(ACCENT), "teal": _paint(TEAL), "white": _paint(Color("f9faff")),
		"shadow": _paint(Color(0.65, 0.68, 0.77, 0.44), 0.0, 22.0),
		"light": _paint(Color(1.0, 1.0, 1.0, 0.56), 0.0, 22.0),
		"small_shadow": _paint(Color(0.64, 0.67, 0.76, 0.48), 0.0, 12.0),
		"small_light": _paint(Color(1.0, 1.0, 1.0, 0.62), 0.0, 12.0),
		"inner_shadow": _paint(Color(0.60, 0.64, 0.74, 0.65), 10.0, 11.0),
		"inner_light": _paint(Color(1.0, 1.0, 1.0, 0.90), 10.0, 11.0),
		"hairline": _paint(Color(1.0, 1.0, 1.0, 0.48), 1.1),
		"icon": _paint(INK, 2.5), "accent_icon": _paint(ACCENT, 2.5),
		"track": _paint(Color("d7dae6"), 7.0),
		"progress": _paint(TEAL, 7.0), "volume_fill": _paint(ACCENT),
		"grid": _paint(Color(0.49, 0.53, 0.66, 0.10), 1.0),
		"selection": _paint(Color(0.28, 0.55, 0.75, 0.58), 1.2),
		"axis_x": _paint(Color("ce7f84"), 1.6),
		"axis_y": _paint(Color("7ea891"), 1.6),
		"timeline_ticks": _paint(Color(0.52, 0.56, 0.66, 0.52), 1.0),
		"timeline_marker": _paint(ACCENT),
		"focus_outline": _paint(Color(0.51, 0.49, 0.77, 0.7), 2.0),
	}
	var gradient := RiveGradient.new()
	gradient.set_linear(Vector2(-160, -160), Vector2(350, 600))
	gradient.set_stops(PackedColorArray([Color("eef0f6"), Color("e2e5ee")]), PackedFloat32Array([0.0, 1.0]))
	var surface := _paint(BACKGROUND)
	surface.set_gradient(gradient)
	_paints["surface"] = surface
	_progress = RivePath.new()
	_timeline = RivePath.new()
	_background = RivePath.new()
	_build_icons()
	for digit in "0123456789:":
		_cache_text(digit, 13.0)
	for text in ["Godot", "neumorphism_demo.tscn", "Preview", "RobotHead", "Inspector",
		"Node2D", "Control", "Scale", "%", "Idle", "Hop", "Spin",
		"Animation", "Timeline", "Playing", "Paused", "0s", "2s", "4s", "X", "Y"]:
		_cache_text(text, _text_size(text))
	_resources_ready = true
	_resources_failed = false


func _text_size(text: String) -> float:
	match text:
		"Godot": return 31.0
		"neumorphism_demo.tscn": return 15.0
		"RobotHead", "Inspector": return 24.0
		"Node2D", "Control", "Animation": return 15.0
		"Idle", "Hop", "Spin", "Scale": return 13.0
		_: return 11.0


func _cache_text(text: String, size: float) -> Dictionary:
	var key := text + "@" + str(size)
	if not _texts.has(key):
		var glyphs := _font.shape_text(text, size)
		var width := 0.0
		if not glyphs.is_empty():
			var last: Dictionary = glyphs[-1]
			width = float(last["x"]) + float(last["advance"])
		_texts[key] = {"glyphs": glyphs, "width": width}
	return _texts[key]


func _text(renderer: RiveRendererWrapper, text: String, x: float, baseline: float,
		paint: String = "ink", centered := false, size := 0.0) -> void:
	var shaped := _cache_text(text, _text_size(text) if size == 0.0 else size)
	renderer.save()
	renderer.translate(x - (float(shaped["width"]) * 0.5 if centered else 0.0), baseline)
	for entry: Dictionary in shaped["glyphs"]:
		renderer.draw_path(entry["path"], _paints[paint])
	renderer.restore()


func _digits(renderer: RiveRendererWrapper, text: String, x: float, baseline: float,
		size: float, paint := "ink", centered := false) -> void:
	var spacing := size * 0.615
	var width := spacing * (text.length() - 1) + size * 0.60
	var start := x - (width * 0.5 if centered else 0.0)
	for i in text.length():
		_text(renderer, text[i], start + i * spacing, baseline, paint, false, size)


func _build_icons() -> void:
	var play := RivePath.new()
	play.move_to(-6, -11)
	play.line_to(11, 0)
	play.line_to(-6, 11)
	play.close()
	_paths["play_icon"] = play
	var pause := RivePath.new()
	pause.add_round_rect(-8, -11, 5, 22, 2)
	pause.add_round_rect(3, -11, 5, 22, 2)
	_paths["pause_icon"] = pause
	var arrow := RivePath.new()
	arrow.move_to(-4, -6)
	arrow.line_to(3, 0)
	arrow.line_to(-4, 6)
	_paths["arrow_icon"] = arrow
	var node_icon := RivePath.new()
	node_icon.add_circle(0, 0, 8)
	node_icon.move_to(-12, 0)
	node_icon.line_to(12, 0)
	node_icon.move_to(0, -12)
	node_icon.line_to(0, 12)
	_paths["node_icon"] = node_icon
	var control_icon := RivePath.new()
	control_icon.add_round_rect(-9, -9, 18, 18, 2)
	_paths["control_icon"] = control_icon
	var grid := RivePath.new()
	for line in range(-120, 121, 40):
		grid.move_to(line, -120)
		grid.line_to(line, 120)
		grid.move_to(-120, line)
		grid.line_to(120, line)
	_paths["preview_grid"] = grid
	var axis_x := RivePath.new()
	axis_x.move_to(-24, 0)
	axis_x.line_to(70, 0)
	axis_x.move_to(63, -4)
	axis_x.line_to(70, 0)
	axis_x.line_to(63, 4)
	_paths["axis_x"] = axis_x
	var axis_y := RivePath.new()
	axis_y.move_to(0, -16)
	axis_y.line_to(0, 50)
	axis_y.move_to(-4, 43)
	axis_y.line_to(0, 50)
	axis_y.line_to(4, 43)
	_paths["axis_y"] = axis_y
	var keyframe := RivePath.new()
	keyframe.move_to(0, -4)
	keyframe.line_to(4, 0)
	keyframe.line_to(0, 4)
	keyframe.line_to(-4, 0)
	keyframe.close()
	_paths["keyframe"] = keyframe
	var playhead := RivePath.new()
	playhead.move_to(-5, 0)
	playhead.line_to(5, 0)
	playhead.line_to(0, 6)
	playhead.close()
	_paths["playhead"] = playhead


func _translated_path(renderer: RiveRendererWrapper, path: RivePath,
		paint: String, x: float, y: float) -> void:
	renderer.save()
	renderer.translate(x, y)
	renderer.draw_path(path, _paints[paint])
	renderer.restore()


func _surface(renderer: RiveRendererWrapper, path: RivePath,
		x: float, y: float, small := false, inset := false, amount := 1.0) -> void:
	var offset := (5.0 if small else 10.0) * amount
	renderer.save()
	renderer.translate(x, y)
	if not inset:
		var shadow_path := _outer_shadow_path(path, small)
		_translated_path(renderer, shadow_path, "small_shadow" if small else "shadow", offset, offset)
		_translated_path(renderer, shadow_path, "small_light" if small else "light", -offset, -offset)
	renderer.draw_path(path, _paints["surface"] if not inset else _paints["bg"])
	if inset:
		renderer.save()
		renderer.clip_path(path)
		_translated_path(renderer, path, "inner_shadow", 4.0, 4.0)
		_translated_path(renderer, path, "inner_light", -4.0, -4.0)
		renderer.restore()
	else:
		renderer.draw_path(path, _paints["hairline"])
	renderer.restore()


func _outer_shadow_path(path: RivePath, small: bool) -> RivePath:
	var cache_key := str(path.get_instance_id()) + ("_small" if small else "_large")
	if not _shadow_paths.has(cache_key):
		var spec: Dictionary = _shape_specs[path.get_instance_id()]
		var bounds: Rect2 = spec["bounds"]
		var radius: float = spec["radius"]
		var spread := 2.0 if small else 5.0
		var expanded := RivePath.new()
		expanded.set_fill_rule(RivePath.FILL_CLOCKWISE)
		if bool(spec["circle"]):
			expanded.add_circle(0.0, 0.0, radius + spread)
		else:
			expanded.add_round_rect(bounds.position.x - spread, bounds.position.y - spread,
				bounds.size.x + spread * 2.0, bounds.size.y + spread * 2.0, radius + spread)
		_shadow_paths[cache_key] = expanded
	return _shadow_paths[cache_key]


func _on_draw_rive(renderer: RiveRendererWrapper) -> void:
	_ensure_resources()
	if not _resources_ready:
		return
	_draw_count += 1
	if _last_background_size != _viewport_size:
		_background.reset()
		_background.add_rect(0, 0, _viewport_size.x, _viewport_size.y)
		_last_background_size = _viewport_size
	renderer.draw_path(_background, _paints["bg"])
	renderer.save()
	renderer.translate(_layout_origin.x, _layout_origin.y)
	renderer.scale(_layout_scale, _layout_scale)
	renderer.translate(0, _entry_offset_y())
	_draw_header(renderer)
	_draw_preview(renderer)
	_draw_inspector(renderer)
	if _keyboard_focus and _focus != "":
		var rect: Rect2 = HIT_RECTS[_focus]
		var outline := _round("focus_" + _focus, rect.size.x + 8.0, rect.size.y + 8.0, 16.0)
		_translated_path(renderer, outline, "focus_outline", rect.position.x - 4.0, rect.position.y - 4.0)
	renderer.restore()


func _draw_header(renderer: RiveRendererWrapper) -> void:
	_surface(renderer, _round("logo", 44, 44, 14), 82, 65, true)
	_head.call("draw", renderer, Vector2(104, 87), 30.0, 0.0, 1.0, Vector2.ZERO)
	_text(renderer, "Godot", 146, 96)
	_text(renderer, "neumorphism_demo.tscn", 265, 95, "muted")


func _draw_preview(renderer: RiveRendererWrapper) -> void:
	_surface(renderer, _round("main_card", 700, 590, 38), 80, 160)
	_text(renderer, "Preview", 122, 207, "muted")
	_text(renderer, "RobotHead", 122, 245)
	_translated_path(renderer, _paths["node_icon"] if _mode == 0 else _paths["control_icon"], "accent_icon", 726, 218)
	var center := Vector2(430, 438)
	renderer.save()
	renderer.translate(center.x, center.y)
	_surface(renderer, _circle("preview_outer", 160), 0, 0)
	_surface(renderer, _circle("preview_inset", 143), 0, 0, true, true)
	renderer.save()
	renderer.clip_path(_circle("preview_clip", 133))
	renderer.draw_path(_paths["preview_grid"], _paints["grid"])
	renderer.restore()
	renderer.draw_path(_circle("loop_track", 143), _paints["track"])
	_progress.reset()
	var sweep := TAU * maxf(0.004, fposmod(_wave_time, LOOP_SECONDS) / LOOP_SECONDS)
	_progress.add_arc(0.0, 0.0, 143.0, -PI * 0.5, sweep)
	renderer.draw_path(_progress, _paints["progress"])
	_draw_robot_pose(renderer)
	renderer.restore()
	_text(renderer, "Playing" if _playing and _motion_enabled else "Paused", 430, 610, "muted", true)
	_draw_transport(renderer, "previous", Vector2(299, 670), -1.0)
	_draw_transport(renderer, "next", Vector2(561, 670), 1.0)
	var button_y := 670.0 + _press_mix * 2.0
	_surface(renderer, _circle("play_button", 44), 430, button_y, true, _pressed == "play", 1.0 - _press_mix * 0.6)
	var icon: RivePath = _paths["pause_icon"] if _playing else _paths["play_icon"]
	renderer.save()
	renderer.translate(430, button_y)
	var scale_factor := 0.93 + 0.07 * (1.0 - _press_mix)
	renderer.scale(scale_factor, scale_factor)
	renderer.draw_path(icon, _paints["accent"])
	renderer.restore()


func _draw_robot_pose(renderer: RiveRendererWrapper) -> void:
	var phase := fposmod(_wave_time, LOOP_SECONDS) / LOOP_SECONDS * TAU
	var scale_factor := 0.6 + 0.8 * _visual_volume
	var offset := Vector2.ZERO
	var rotation := 0.0
	match _sound:
		0:
			offset.y = sin(phase) * 5.0
			rotation = sin(phase) * 0.045
		1:
			offset.y = -absf(sin(phase)) * 28.0
			rotation = sin(phase * 2.0) * 0.07
		2:
			rotation = phase
	var blink := 1.0 - pow(maxf(0.0, sin(phase)), 32.0)
	var look := Vector2(sin(phase), cos(phase)) * 0.45
	_head.call("draw", renderer, offset, 175.0 * scale_factor, rotation, blink, look)
	if _mode == 0:
		_translated_path(renderer, _paths["axis_x"], "axis_x", offset.x, offset.y + 89)
		_translated_path(renderer, _paths["axis_y"], "axis_y", offset.x, offset.y + 89)
		_text(renderer, "X", offset.x + 76, offset.y + 93, "muted")
		_text(renderer, "Y", offset.x - 4, offset.y + 153, "muted")
	else:
		var extent := 94.0 * scale_factor
		var bounds_path := _round("control_bounds", 188, 166, 3)
		renderer.save()
		renderer.translate(offset.x - extent, offset.y - extent * 0.88)
		renderer.scale(scale_factor, scale_factor)
		renderer.draw_path(bounds_path, _paints["selection"])
		renderer.restore()
		for point: Vector2 in [Vector2(-extent, -extent * 0.88), Vector2(extent, -extent * 0.88),
			Vector2(-extent, extent * 0.88), Vector2(extent, extent * 0.88)]:
			_translated_path(renderer, _round("anchor", 6, 6, 1), "accent", offset.x + point.x - 3, offset.y + point.y - 3)


func _draw_transport(renderer: RiveRendererWrapper, action: String, center: Vector2, direction: float) -> void:
	_surface(renderer, _circle("transport", 28), center.x, center.y, true, _pressed == action,
		1.25 if _hover == action else 1.0)
	renderer.save()
	renderer.translate(center.x, center.y)
	renderer.scale(direction, 1.0)
	renderer.draw_path(_paths["arrow_icon"], _paints["icon"])
	renderer.restore()


func _draw_inspector(renderer: RiveRendererWrapper) -> void:
	_surface(renderer, _round("settings", 380, 590, 38), 820, 160)
	_text(renderer, "Inspector", 856, 210)
	_text(renderer, "Node2D" if _mode == 0 else "Control", 856, 236, "muted", false, 11.0)
	_surface(renderer, _round("mode_track", 310, 52, 17), 856, 258, true, true)
	var mode_x := 861.0 + 155.0 * _mode_position
	_surface(renderer, _round("mode_selected", 145, 42, 13), mode_x, 263, true)
	_text(renderer, "Node2D", 933, 291, "accent" if _mode == 0 else "muted", true)
	_text(renderer, "Control", 1089, 291, "accent" if _mode == 1 else "muted", true)
	_text(renderer, "Scale", 856, 356, "muted")
	_digits(renderer, str(roundi((0.6 + 0.8 * _volume) * 100.0)), 1118, 356, 13.0, "ink")
	_text(renderer, "%", 1157, 356, "muted")
	_surface(renderer, _round("volume_track", 310, 16, 8), 856, 391, true, true)
	var fill_width := maxf(1.0, 294.0 * _visual_volume)
	renderer.save()
	renderer.translate(864, 395)
	renderer.scale(fill_width / 294.0, 1.0)
	renderer.draw_path(_round("volume_fill", 294, 8, 4), _paints["volume_fill"])
	renderer.restore()
	var knob_x := 864.0 + 294.0 * _visual_volume
	_surface(renderer, _circle("volume_knob", 16), knob_x, 399, true, _dragging)
	_translated_path(renderer, _circle("knob_mark", 3), "accent", knob_x, 399)
	for i in range(3):
		var x := 856.0 + i * 107.0
		var action: String = ["idle", "hop", "spin"][i]
		var selected: float = _sound_mix[i]
		_surface(renderer, _round("sound_chip", 96, 44, 15), x, 443, true,
			_pressed == action or selected > 0.5, 0.6 + selected * 0.4)
		_text(renderer, ["Idle", "Hop", "Spin"][i], x + 48, 471,
			"accent" if _sound == i else "muted", true)
	_text(renderer, "Animation", 856, 554)
	_surface(renderer, _round("toggle", 82, 40, 20), 1085, 530, true, true)
	var toggle_x := 1105.0 + 42.0 * _toggle_position
	_surface(renderer, _circle("toggle_knob", 16), toggle_x, 550, true)
	_translated_path(renderer, _circle("toggle_dot", 4), "teal" if _motion_enabled else "muted", toggle_x, 550)
	_text(renderer, "Timeline", 856, 621, "muted")
	_surface(renderer, _round("wave_well", 310, 70, 20), 856, 638, true, true)
	_draw_timeline(renderer)


func _draw_timeline(renderer: RiveRendererWrapper) -> void:
	_timeline.reset()
	for i in range(17):
		var x := 877.0 + i * 16.8
		_timeline.move_to(x, 650)
		_timeline.line_to(x, 655.0 + (5.0 if i % 4 == 0 else 0.0))
	_timeline.move_to(877, 678)
	_timeline.line_to(1146, 678)
	renderer.draw_path(_timeline, _paints["timeline_ticks"])
	for i in range(5):
		_translated_path(renderer, _paths["keyframe"], "timeline_marker", 877.0 + i * 67.2, 678)
	_text(renderer, "0s", 877, 697, "muted", true)
	_text(renderer, "2s", 1011, 697, "muted", true)
	_text(renderer, "4s", 1146, 697, "muted", true)
	var playhead_x := 877.0 + 269.0 * fposmod(_wave_time, LOOP_SECONDS) / LOOP_SECONDS
	_translated_path(renderer, _paths["playhead"], "timeline_marker", playhead_x, 651)
	var line := _round("playhead_line", 1.5, 29, 0.7)
	_translated_path(renderer, line, "accent", playhead_x - 0.75, 657)


func _exit_tree() -> void:
	Input.set_default_cursor_shape(Input.CURSOR_ARROW)
