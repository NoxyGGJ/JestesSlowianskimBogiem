extends Area3D
class_name BossProjectile

@export var speed : float = 2
@export var max_scale : Vector3
@export var speed_scaling : float

var mesh
var timer
var collision
var time : float

func _ready() -> void:
	mesh = $base
	timer = $Timer
	collision = $CollisionShape3D

func _physics_process(delta: float) -> void:
		position += delta * speed * transform.basis[2]

func _on_body_entered(body: Node3D) -> void:
	var enemy = body is Enemy
	if enemy:
		return
	
	if not mesh.is_queued_for_deletion():
		mesh.queue_free()

	deal_damage(body)
		
	timer.start()	
	collision.disabled = true
	
func deal_damage(body: Node3D) -> void:
	if body.name == "Player":
		body.hit()

func _on_timer_timeout() -> void:
	if not is_queued_for_deletion():
		queue_free()

func _on_max_life_time_timer_timeout() -> void:
	if not is_queued_for_deletion():
		queue_free()
