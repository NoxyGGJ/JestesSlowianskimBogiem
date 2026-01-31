extends Area3D
class_name Enemy

@export var speed : float = 2

func _physics_process(delta: float) -> void:
	position += delta * speed * transform.basis[2]

func _on_body_entered(body: Node3D) -> void:
	print(body.name)
	var enemy = body as Enemy
	if enemy:
		enemy.Damage()
	queue_free()
