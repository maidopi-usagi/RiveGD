extends Node
class_name RiveTextSelection

enum SelectionPhase { DOWN, UP, MOVE, CANCEL }

@export_node_path("RiveControl") var source: NodePath
var _captured: RivePlayer
var _pointer := -1

func _exit_tree() -> void:
	_cancel()

func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_WINDOW_FOCUS_OUT: _cancel()

func _cancel() -> void:
	if _captured != null: _captured.selection_pointer(0, Vector2.ZERO, SelectionPhase.CANCEL, _pointer)
	_captured = null
	_pointer = -1

func _input(event: InputEvent) -> void:
	var control := get_node_or_null(source) as RiveControl if not source.is_empty() else null
	if control == null or not control.is_visible_in_tree():
		_cancel()
		return
	var player := control.get_player()
	if _captured != null and _captured != player: _cancel()
	if player == null or not player.get_text_input_state().is_empty(): return
	var id := 0
	var phase := -1
	var position := Vector2.ZERO
	var extend := false
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		phase = SelectionPhase.DOWN if event.pressed else SelectionPhase.UP
		position = event.position
		extend = event.shift_pressed
	elif event is InputEventMouseMotion:
		phase = SelectionPhase.MOVE
		position = event.position
	elif event is InputEventScreenTouch:
		id = event.index + 1
		phase = SelectionPhase.CANCEL if event.canceled else (SelectionPhase.DOWN if event.pressed else SelectionPhase.UP)
		position = event.position
	elif event is InputEventScreenDrag:
		id = event.index + 1
		phase = SelectionPhase.MOVE
		position = event.position
	if phase < 0: return
	var canvas_transform := control.get_global_transform_with_canvas()
	var fit := control.get_artboard_transform()
	if is_zero_approx(canvas_transform.determinant()) or is_zero_approx(fit.determinant()): return
	var local_position := canvas_transform.affine_inverse() * position
	var native_position := fit.affine_inverse() * local_position
	if phase == SelectionPhase.DOWN:
		if _pointer >= 0 or not Rect2(Vector2.ZERO, control.size).has_point(local_position): return
		var hit := player.hit_test_selectable_text(native_position)
		if hit.is_empty() or not player.selection_pointer(hit.id, native_position, SelectionPhase.DOWN, id, extend): return
		_captured = player
		_pointer = id
		control.grab_focus()
		player.clear_focus()
		get_viewport().set_input_as_handled()
	elif _captured != null and _pointer == id:
		_captured.selection_pointer(0, native_position, phase, id)
		if phase == SelectionPhase.UP or phase == SelectionPhase.CANCEL:
			_captured = null
			_pointer = -1
		get_viewport().set_input_as_handled()
