@tool
extends Node2D
class_name RiveComponentFollower

const PROCESS_PRIORITY_AFTER_RIVE := 100

@export_node_path("RiveControl", "RiveFileInstance") var source: NodePath
@export var component_name := ""
@export var nested_path := ""
@export var offset := Transform2D.IDENTITY
@export var follow_bone_tip := false
var tracking := false

func _ready() -> void:
	process_priority = PROCESS_PRIORITY_AFTER_RIVE

func _process(_delta: float) -> void:
	update_follow()

func update_follow() -> bool:
	tracking = false
	var target := get_node_or_null(source) if not source.is_empty() else null
	if target == null or not target is CanvasItem or not target.has_method("get_player"):
		return false
	var player: RivePlayer = target.call("get_player")
	if player == null:
		return false
	var snapshot := player.get_component(component_name, nested_path)
	if not snapshot.get("root_transform") is Transform2D:
		return false
	var transform_value: Transform2D = snapshot.root_transform
	if follow_bone_tip and snapshot.get("root_tip_position") is Vector2:
		transform_value.origin = snapshot.root_tip_position
	var fit := Transform2D.IDENTITY
	if target.has_method("get_artboard_transform"):
		fit = target.call("get_artboard_transform")
	global_transform = get_canvas_transform().affine_inverse() * target.get_global_transform_with_canvas() * fit * transform_value * offset
	tracking = true
	return true

func _get_configuration_warnings() -> PackedStringArray:
	if source.is_empty() or component_name.is_empty():
		return ["Assign a Rive source and component name."]
	return []
