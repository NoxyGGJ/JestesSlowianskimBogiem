extends Area3D
class_name Projectile

@export var speed : float = 2
@export var max_scale : Vector3
@export var speed_scaling : float
var mesh
var particles
var timer
var is_scaling : bool = true;
var time : float


func start_scaling() -> void:
	is_scaling = true;

func release() -> void:
	is_scaling = false;

func _ready() -> void:
	mesh = $base
	particles = $particles
	timer = $Timer

func _physics_process(delta: float) -> void:
	if is_scaling:
		time += delta * speed_scaling
		scale = scale.lerp(max_scale, time)
		
	else:
		position += delta * speed * transform.basis[2]

func _on_body_entered(body: Node3D) -> void:
	if is_scaling:
		return
	
	var enemy = body is Enemy
	if enemy:
		body.Damage()
	timer.start()	

	if mesh.is_queued_for_deletion():
		mesh.queue_free()
	
	if particles != null:
		particles.emitting = false

func _on_timer_timeout() -> void:
	queue_free()
