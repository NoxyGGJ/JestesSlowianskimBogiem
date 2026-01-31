extends Node3D

@export var projectile_scene : PackedScene
@export var useChargedAttak : bool
var projectile

func _process(delta: float) -> void:
	if useChargedAttak:
		charged_attack()
	else:
		normal_attack()

func charged_attack() -> void:
	if Input.is_action_just_pressed("fire"):
		projectile = projectile_scene.instantiate()
		add_child(projectile)
		projectile.position = Vector3.ZERO
		projectile.start_scaling();
	if Input.is_action_just_released("fire") and projectile:
		projectile.reparent(get_tree().root)
		projectile.rotation = global_rotation
		projectile.release();
		
func normal_attack() -> void:
	if Input.is_action_just_pressed("fire"):
		projectile = projectile_scene.instantiate()
		get_tree().root.add_child(projectile)
		projectile.position = global_transform.origin
		projectile.rotation = global_rotation
		projectile.release();
