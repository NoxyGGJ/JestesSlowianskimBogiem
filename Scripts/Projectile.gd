extends Area3D
class_name Projectile

@export var speed : float = 2
var mesh
var particles
var timer

func _ready() -> void:
	mesh = $base
	particles = $particles
	timer = $Timer

func _physics_process(delta: float) -> void:
	position += delta * speed * transform.basis[2]

func _on_body_entered(body: Node3D) -> void:
	var enemy = body is Enemy
	if enemy:
		body.Damage()
	timer.start()	

	if mesh.is_queued_for_deletion():
		mesh.queue_free()
		
	particles.emitting = false

func _on_timer_timeout() -> void:
	queue_free()
