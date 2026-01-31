extends Area3D
class_name Projectile

@export var speed : float = 2

func _physics_process(delta: float) -> void:
	position += delta * speed * transform.basis[2]

func _on_body_entered(body: Node3D) -> void:
	var enemy = body is Enemy
	if enemy:
		body.Damage()
		
	queue_free()
