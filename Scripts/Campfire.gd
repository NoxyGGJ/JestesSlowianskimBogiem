extends Node3D
class_name Campfire

@onready var fieldMesh: MeshInstance3D = $Area3D/MeshInstance3D
@onready var loading_indicator: Control = $SubViewport/LoadingIndicator

var capture_progress:float = 0.0
var capturing: bool = false
var captured: bool = false

func _ready() -> void:
	pass 

func _process(delta: float) -> void:
	
	if captured:
		return
		
	if capturing:
		capture_progress += delta * 8.0
		loading_indicator.set_value(capture_progress)

	if capture_progress >= 100:
		captured = true
		capturing = false
		loading_indicator.set_value(0.0)
		fieldMesh.visible = false

func _on_area_3d_body_entered(body: Node3D) -> void:
	if captured:
		return
		
	if body.name == "Player":
		fieldMesh.visible = true
		capturing = true

func _on_area_3d_body_exited(body: Node3D) -> void:
	if captured:
		return
		
	if body.name == "Player":
		fieldMesh.visible = false
		capturing = false
		capture_progress = 0.0
		loading_indicator.set_value(capture_progress)
		
