extends Node3D
class_name Campfire

enum CapturePoint 
{
	NONE,
	FIRST,
	SECOND,
	THIRD,
	FOURTH
}

@onready var fieldMesh: MeshInstance3D = $Area3D/MeshInstance3D
@onready var loading_indicator: Control = $SubViewport/LoadingIndicator

var capture_progress:float = 0.0
var capturing: bool = false
var captured: bool = false

@export var currentPoint:CapturePoint = CapturePoint.NONE

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
		$"Swiatelko do nieba!".visible = false
		capturing = false
		loading_indicator.set_value(0.0)
		fieldMesh.visible = false
		
		match currentPoint:
			CapturePoint.FIRST:
				GlobalObject.firstTotemFinished = true
			CapturePoint.SECOND:
				GlobalObject.secondTotemFinished = true
			CapturePoint.THIRD:
				GlobalObject.thirdTotemFinished = true
			CapturePoint.FOURTH:
				GlobalObject.foruthTotemFinished = true

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
		
