extends Node
class_name RiveAccessibilityBridge

const PROCESS_PRIORITY_AFTER_RIVE := 110

enum SemanticRole {
	NONE, BUTTON, LINK, CHECKBOX, SWITCH, SLIDER, TEXT_FIELD, TEXT, IMAGE,
	GROUP, LIST, LIST_ITEM, TAB, TAB_LIST, DIALOG, ALERT_DIALOG, RADIO_GROUP, RADIO_BUTTON,
}
enum SemanticTrait {
	EXPANDABLE = 1 << 0, SELECTABLE = 1 << 1, CHECKABLE = 1 << 2,
	TOGGLEABLE = 1 << 3, REQUIRABLE = 1 << 4, ENABLABLE = 1 << 5, FOCUSABLE = 1 << 6,
}
enum SemanticState {
	EXPANDED = 1 << 0, SELECTED = 1 << 1, TOGGLED = 1 << 4,
	REQUIRED = 1 << 5, DISABLED = 1 << 6, FOCUSED = 1 << 7,
	HIDDEN = 1 << 8, READ_ONLY = 1 << 10, MODAL = 1 << 11,
	OBSCURED = 1 << 12, MULTILINE = 1 << 13,
}
enum CheckState { UNCHECKED, CHECKED, MIXED }
const CHECK_STATE_SHIFT := 2
const CHECK_STATE_MASK := 3
const GODOT_ROLES := {
	SemanticRole.NONE: DisplayServer.ROLE_CONTAINER,
	SemanticRole.BUTTON: DisplayServer.ROLE_BUTTON,
	SemanticRole.LINK: DisplayServer.ROLE_LINK,
	SemanticRole.CHECKBOX: DisplayServer.ROLE_CHECK_BOX,
	SemanticRole.SWITCH: DisplayServer.ROLE_CHECK_BUTTON,
	SemanticRole.SLIDER: DisplayServer.ROLE_SLIDER,
	SemanticRole.TEXT_FIELD: DisplayServer.ROLE_TEXT_FIELD,
	SemanticRole.TEXT: DisplayServer.ROLE_STATIC_TEXT,
	SemanticRole.IMAGE: DisplayServer.ROLE_IMAGE,
	SemanticRole.GROUP: DisplayServer.ROLE_CONTAINER,
	SemanticRole.LIST: DisplayServer.ROLE_LIST,
	SemanticRole.LIST_ITEM: DisplayServer.ROLE_LIST_ITEM,
	SemanticRole.TAB: DisplayServer.ROLE_TAB,
	SemanticRole.TAB_LIST: DisplayServer.ROLE_TAB_BAR,
	SemanticRole.DIALOG: DisplayServer.ROLE_DIALOG,
	SemanticRole.ALERT_DIALOG: DisplayServer.ROLE_DIALOG,
	SemanticRole.RADIO_GROUP: DisplayServer.ROLE_CONTAINER,
	SemanticRole.RADIO_BUTTON: DisplayServer.ROLE_RADIO_BUTTON,
}

@export_node_path("RiveControl") var source: NodePath
@export var platform_enabled := true
var _player: RivePlayer
var _nodes: Dictionary = {}
var _elements: Dictionary = {}
var _node_owned_root_element := RID()
var _generation := 0
var _native_generation := -1

func _ready() -> void:
	process_priority = PROCESS_PRIORITY_AFTER_RIVE

func _exit_tree() -> void:
	_clear_elements()
	_nodes.clear()
	_player = null

func get_nodes() -> Dictionary:
	return _nodes.duplicate(true)

func is_platform_connected() -> bool:
	return _node_owned_root_element.is_valid() and DisplayServer.accessibility_has_element(_node_owned_root_element)

func activate(id: int, action: int = RivePlayer.SEMANTIC_TAP) -> bool:
	return _player != null and _nodes.has(id) and _player.fire_semantic_action(id, action)

func _action(_request: Dictionary, id: int, action: int, generation: int) -> void:
	sync()
	if generation == _generation: activate(id, action)

func _focus(_request: Dictionary, id: int, generation: int) -> void:
	sync()
	if generation == _generation and _player != null and _nodes.has(id):
		var control := get_node_or_null(source) as RiveControl
		if control != null: control.grab_focus()
		_player.request_semantic_focus(id)

func _clear_elements() -> void:
	if _elements.is_empty() and not _node_owned_root_element.is_valid(): return
	_generation += 1
	for rid: RID in _elements.values():
		if DisplayServer.accessibility_has_element(rid): DisplayServer.accessibility_free_element(rid)
	_elements.clear()
	_node_owned_root_element = RID()

func _process(_delta: float) -> void:
	sync()

func sync() -> void:
	var control: RiveControl = get_node_or_null(source) as RiveControl if not source.is_empty() else null
	var player: RivePlayer = control.get_player() if control != null else null
	if player != _player:
		_clear_elements()
		_nodes.clear()
		_player = player
		if _player != null: _player.set_semantics_enabled(true)
	if _player == null: return
	var diff := _player.take_semantics_diff()
	var structural := false
	var native_generation: int = diff.get("generation", -1)
	if native_generation != _native_generation:
		_native_generation = native_generation
		_clear_elements()
		_nodes.clear()
		structural = true
	for id: int in diff.get("removed", []):
		_nodes.erase(id)
		structural = true
	for key: String in ["added", "moved", "updated"]:
		for node: Dictionary in diff.get(key, []):
			_nodes[node.id] = node
			if key != "updated": structural = true
	for node: Dictionary in diff.get("bounds_updated", []):
		if _nodes.has(node.id): _nodes[node.id].bounds = node.bounds
	for update: Dictionary in diff.get("children_updated", []):
		for index in update.children.size():
			var id: int = update.children[index]
			if _nodes.has(id):
				_nodes[id].parent_id = update.parent_id
				_nodes[id].sibling_index = index
		structural = true
	if not platform_enabled or not control.is_visible_in_tree():
		_clear_elements()
		return
	var own_root := get_accessibility_element()
	if not own_root.is_valid():
		_clear_elements()
		return
	if structural or not is_platform_connected():
		_clear_elements()
		_node_owned_root_element = own_root
		if not _node_owned_root_element.is_valid(): return
		var pending := _nodes.keys()
		pending.sort_custom(func(a: int, b: int) -> bool:
			return int(_nodes[a].sibling_index) < int(_nodes[b].sibling_index))
		while not pending.is_empty():
			var created_element := false
			for id: int in pending.duplicate():
				var node: Dictionary = _nodes[id]
				if _nodes.has(node.parent_id) and not _elements.has(node.parent_id): continue
				var parent: RID = _elements.get(node.parent_id, _node_owned_root_element)
				var rid := DisplayServer.accessibility_create_sub_element(parent, _role(node))
				if not rid.is_valid(): continue
				_elements[id] = rid
				pending.erase(id)
				created_element = true
			if not created_element: break
	queue_accessibility_update()

func _notification(what: int) -> void:
	# Godot requires property updates here, but forbids RID destruction here.
	if what != NOTIFICATION_ACCESSIBILITY_UPDATE or not is_platform_connected(): return
	var control := get_node_or_null(source) as RiveControl
	if control == null: return
	var matrix := control.get_global_transform_with_canvas() * control.get_artboard_transform()
	var ancestor := get_parent()
	while ancestor != null and not ancestor is CanvasItem: ancestor = ancestor.get_parent()
	if ancestor is CanvasItem: matrix = ancestor.get_global_transform_with_canvas().affine_inverse() * matrix
	DisplayServer.accessibility_update_set_role(_node_owned_root_element, DisplayServer.ROLE_CONTAINER)
	DisplayServer.accessibility_update_set_transform(_node_owned_root_element, matrix)
	for id: int in _elements:
		_update_element(_elements[id], _nodes[id], control)

func _role(node: Dictionary) -> DisplayServer.AccessibilityRole:
	var role: int = node.role
	if role == SemanticRole.TEXT_FIELD and (int(node.state_flags) & SemanticState.MULTILINE) != 0:
		return DisplayServer.ROLE_MULTILINE_TEXT_FIELD
	return GODOT_ROLES.get(role, DisplayServer.ROLE_CONTAINER)

func _update_element(rid: RID, node: Dictionary, control: RiveControl) -> void:
	var state: int = node.state_flags
	var traits: int = node.trait_flags
	var id: int = node.id
	DisplayServer.accessibility_update_add_action(rid, DisplayServer.ACTION_FOCUS, _focus.bind(id, _generation))
	if int(node.role) in [SemanticRole.BUTTON, SemanticRole.LINK, SemanticRole.CHECKBOX,
			SemanticRole.SWITCH, SemanticRole.TAB, SemanticRole.RADIO_BUTTON]:
		DisplayServer.accessibility_update_add_action(rid, DisplayServer.ACTION_CLICK, _action.bind(id, RivePlayer.SEMANTIC_TAP, _generation))
	if int(node.role) == SemanticRole.SLIDER:
		DisplayServer.accessibility_update_add_action(rid, DisplayServer.ACTION_INCREMENT, _action.bind(id, RivePlayer.SEMANTIC_INCREASE, _generation))
		DisplayServer.accessibility_update_add_action(rid, DisplayServer.ACTION_DECREMENT, _action.bind(id, RivePlayer.SEMANTIC_DECREASE, _generation))
	DisplayServer.accessibility_update_set_role(rid, _role(node))
	DisplayServer.accessibility_update_set_name(rid, node.label)
	DisplayServer.accessibility_update_set_description(rid, node.hint)
	DisplayServer.accessibility_update_set_value(rid, "" if (state & SemanticState.OBSCURED) else node.value)
	var rect: Rect2 = node.bounds
	if rect.position.is_finite() and rect.size.is_finite():
		DisplayServer.accessibility_update_set_bounds(rid, rect)
	DisplayServer.accessibility_update_set_flag(rid, DisplayServer.FLAG_HIDDEN, bool(state & SemanticState.HIDDEN))
	DisplayServer.accessibility_update_set_flag(rid, DisplayServer.FLAG_READONLY, bool(state & SemanticState.READ_ONLY))
	DisplayServer.accessibility_update_set_flag(rid, DisplayServer.FLAG_MODAL, bool(state & SemanticState.MODAL))
	DisplayServer.accessibility_update_set_flag(rid, DisplayServer.FLAG_REQUIRED, bool((traits & SemanticTrait.REQUIRABLE) and (state & SemanticState.REQUIRED)))
	DisplayServer.accessibility_update_set_flag(rid, DisplayServer.FLAG_DISABLED, bool((traits & SemanticTrait.ENABLABLE) and (state & SemanticState.DISABLED)))
	if traits & SemanticTrait.CHECKABLE:
		var checked := (state >> CHECK_STATE_SHIFT) & CHECK_STATE_MASK
		DisplayServer.accessibility_update_set_checked(rid, checked == CheckState.CHECKED)
		DisplayServer.accessibility_update_set_state_description(rid, "mixed" if checked >= CheckState.MIXED else "")
	elif traits & SemanticTrait.TOGGLEABLE:
		DisplayServer.accessibility_update_set_checked(rid, bool(state & SemanticState.TOGGLED))
	if traits & SemanticTrait.EXPANDABLE:
		DisplayServer.accessibility_update_set_list_item_expanded(rid, bool(state & SemanticState.EXPANDED))
	if traits & SemanticTrait.SELECTABLE:
		DisplayServer.accessibility_update_set_list_item_selected(rid, bool(state & SemanticState.SELECTED))
	if (traits & SemanticTrait.FOCUSABLE) and (state & SemanticState.FOCUSED):
		DisplayServer.accessibility_update_set_focus(rid)
