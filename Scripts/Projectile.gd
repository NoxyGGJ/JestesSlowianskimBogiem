extends Area3D
class_name Projectile

@export var speed : float = 2
@export var scaling_time : float = 1.0
@export var do_explode : bool
@export var piercing : bool = false
@export var min_damage: float = 1.0
@export var max_damage: float = 1.0

@export var scaled_mesh: MeshInstance3D
@export var mesh_min_scale: float = 1.0
@export var mesh_max_scale: float = 5.0
@export var area_min_scale: float = 1.0
@export var area_max_scale: float = 8.0


var currentPlayer : CharacterBody3D
var mesh
var particles
var timer
var collision
var is_scaling : bool = true
var scaling_fraction: float = 0.0
var spherefx_scaling: bool = false
var spherefx_time: float = 0.0


func start_scaling() -> void:
	is_scaling = true;
	if particles != null:
		particles.emitting = false

func release() -> void:
	is_scaling = false;
	if particles != null:
		particles.emitting = true

func _ready() -> void:
	mesh = $base
	particles = $particles
	timer = $Timer
	collision = $CollisionShape3D
	if do_explode:
		$explosionArea/explosionCollision.shape = $explosionArea/explosionCollision.shape.duplicate()

func _physics_process(delta: float) -> void:
	if is_scaling:
		scaling_fraction += delta / scaling_time
		if scaling_fraction > 1:
			scaling_fraction = 1.0
		if scaled_mesh:
			var s = lerp(mesh_min_scale, mesh_max_scale, scaling_fraction)
			scaled_mesh.scale = Vector3(s, s, s)
	else:
		position += delta * speed * transform.basis[2]
		
	if spherefx_scaling:
		spherefx_time += delta * 2.0
		if spherefx_time > 1:
			spherefx_time = 1
		var radius = lerp(area_min_scale, area_max_scale, scaling_fraction)
		var s = lerp(radius * 3.9, 0.0, spherefx_time)
		$SphereFX.scale = Vector3(s, s, s)
		
	

func _on_body_entered(body: Node3D) -> void:
	if is_scaling:
		return

	if do_explode:
		explode()
	else:
		if deal_damage(body):
			if piercing:
				return

	if not mesh.is_queued_for_deletion():
		mesh.queue_free()
	
	if particles != null:
		particles.emitting = false
	
		
	timer.start()	
	collision.disabled = true

func explode() -> void:
	$explosionParticles.emitting = true
	$explosionAdditionalParticles.emitting = true
	$smokeParticles.emitting = true

	var radius = lerp(area_min_scale, area_max_scale, scaling_fraction)
	var s = radius * 3.9
	$SphereFX.scale = Vector3(s, s, s)
	$SphereFX.visible = true
	spherefx_scaling = true
	speed = 0
	
	#currentPlayer.startExploadShake();
	#if scaled_area:
	#	scaled_area.shape.radius = 20	#local_scale * scaled_area_base
	#$explosionArea/explosionCollision.shape.radius = 20	#local_scale * scaled_area_base
	
	#await get_tree().process_frame
	#var bodies = $explosionArea.get_overlapping_bodies()
	#for body in bodies:
	#	var enemy = body is Enemy
	#	if enemy:
	#		body.Damage()
	
	var enemies = get_parent().enemy_cache
	for enemy in enemies:
		var delta = enemy.position - position
		if delta.length() <= radius:
			var dlen = delta.length()
			if dlen > 0:
				var falloff = dlen/radius
				var dmg = lerp(max_damage, min_damage, falloff)
				enemy.Damage(dmg)
				enemy.velocity = delta / dlen / (1.0 + falloff*4.0) * 40.0 * (radius/10.0)
				enemy.stunTime = 1.0
	
func deal_damage(body: Node3D) -> bool:
	var enemy = body is Enemy
	if enemy:
		body.Damage(max_damage)
		return true
	return false

func _on_timer_timeout() -> void:
	if not is_queued_for_deletion():
		queue_free()

func _on_max_life_time_timer_timeout() -> void:
	if not is_queued_for_deletion():
		queue_free()
