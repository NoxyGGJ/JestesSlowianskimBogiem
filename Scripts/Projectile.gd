extends Area3D
class_name Projectile

@export var speed : float = 2
@export var max_scale : Vector3
@export var speed_scaling : float
@export var do_explode : bool
var currentPlayer : CharacterBody3D
var mesh
var particles
var timer
var collision
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
	collision = $CollisionShape3D

func _physics_process(delta: float) -> void:
	if is_scaling:
		time += delta * speed_scaling
		scale = scale.lerp(max_scale, time)
	else:
		position += delta * speed * transform.basis[2]

func _on_body_entered(body: Node3D) -> void:
	if is_scaling:
		return

	if not mesh.is_queued_for_deletion():
		mesh.queue_free()
	
	if particles != null:
		particles.emitting = false
	
	if do_explode:
		explode()
	else:
		deal_damage(body)
		
	timer.start()	
	collision.disabled = true

func explode() -> void:
	$explosionParticles.emitting = true
	$explosionAdditionalParticles.emitting = true
	$smokeParticles.emitting = true
	#currentPlayer.startExploadShake();
	
	var bodies = $explosionArea.get_overlapping_bodies()
	for body in bodies:
		var enemy = body is Enemy
		if enemy:
			body.Damage()
	
func deal_damage(body: Node3D) -> void:
	var enemy = body is Enemy
	if enemy:
		body.Damage()

func _on_timer_timeout() -> void:
	if not is_queued_for_deletion():
		queue_free()

func _on_max_life_time_timer_timeout() -> void:
	if not is_queued_for_deletion():
		queue_free()
