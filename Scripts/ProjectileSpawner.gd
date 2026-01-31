extends Node3D

@export var projectile_scene: PackedScene

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	if Input.is_action_just_pressed("fire"):
		var projectile = projectile_scene.instantiate()
		get_tree().root.add_child(projectile)
		projectile.position = global_transform.origin
		projectile.rotation = global_rotation
