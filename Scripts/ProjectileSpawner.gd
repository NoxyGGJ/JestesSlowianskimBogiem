extends Node3D

@export var projectile_scene : PackedScene
@export var useChargedAttak : bool
@export var supportedMaskIndex: int = -1
@export var FireRate: float
var projectile
var can_shoot : bool = true
var is_release : bool

func _ready() -> void:
	$FireRateTimer.wait_time = FireRate

func _process(delta: float) -> void:
	if GlobalObject.CurrentMask == supportedMaskIndex:
		if useChargedAttak:
			charged_attack()
		else:
			normal_attack()

func charged_attack() -> void:
	if Input.is_action_just_pressed("fire") and can_shoot:
		projectile = projectile_scene.instantiate()
		projectile.currentPlayer = $"../../../.."
		add_child(projectile)
		is_release = false
	if Input.is_action_just_pressed("fire") and not is_release and projectile:
		projectile.position = Vector3.ZERO
		projectile.start_scaling();
	if Input.is_action_just_released("fire") and projectile:
		projectile.reparent(get_tree().root)
		projectile.rotation = global_rotation
		projectile.release();
		can_shoot = false
		is_release = true
		
func normal_attack() -> void:
	if Input.is_action_pressed("fire") and can_shoot:
		projectile = projectile_scene.instantiate()
		projectile.currentPlayer = $"../../../.."
		get_tree().root.add_child(projectile)
		projectile.position = global_transform.origin
		projectile.rotation = global_rotation
		projectile.release();
		can_shoot = false

func _on_fire_rate_timer_timeout() -> void:
	can_shoot = true
