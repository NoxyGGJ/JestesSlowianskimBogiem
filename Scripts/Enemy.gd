extends CharacterBody3D
class_name Enemy

enum EnemyType 
{
	NONE,
	SKELETON,
	SNOWMAN,
	MARZANA,
	BOSS
}

@export var type: EnemyType = EnemyType.NONE
@export var bossBullet = preload("res://Projectiles/BossProjectile.tscn")

@onready var animationSprite: AnimatedSprite3D = $AnimatedSprite3D
@onready var collisionShape: CollisionShape3D = $CollisionShape3D

const deadDestroyTimeout := 20.0

var currentPlayer : CharacterBody3D
var healthBar: Sprite3D
var spawner: Node3D

var speed: float = 2.0
var distance: float = 0.0
var attacking: bool = false
var dead:bool = false
const START_LIFE := 5
var life: int = START_LIFE
var restartHit:bool = false
var cooldown: float = 0.0
var attackCooldown: float = 1.0

var rng = RandomNumberGenerator.new()

func _ready() -> void:
	rng.randomize()
	currentPlayer = get_parent().find_child("Player")
	healthBar = find_child("LifeSprite")
	
	if type == EnemyType.BOSS:
		life = 50
		updateLife()
		spawner = $AnimatedSprite3D/Spawner
	
func _physics_process(delta: float) -> void:
	if currentPlayer:
		var currPos:Vector2 = Vector2(global_position.x, global_position.z)
		var playerPos:Vector2 = Vector2(currentPlayer.global_position.x,
										currentPlayer.global_position.z)
		
		var direction = global_position.direction_to(currentPlayer.global_position)
		velocity = direction * speed
		distance = currPos.distance_to(playerPos)

	if dead:
		animationSprite.animation = "Death"
		return
		
	if type != EnemyType.SKELETON and type != EnemyType.BOSS and GlobalObject.CurrentMask == 2:
		var skeletons = get_parent().find_children("*", "Enemy", false, false)
		if !skeletons.is_empty():
			var random_index = rng.randi() % skeletons.size()
			var randomSkeleton = skeletons[random_index]
			var dir = -global_position.direction_to(randomSkeleton.global_position)
			velocity = dir * speed
		
	if type == EnemyType.SKELETON and GlobalObject.CurrentMask == 3:
		animationSprite.animation = "Dance"
		return
		
	cooldown -= delta * 3.0
		
	if type == EnemyType.SNOWMAN and GlobalObject.CurrentMask == 0 and cooldown <= 0.0:
		Damage()
		
	if type == EnemyType.MARZANA and GlobalObject.CurrentMask == 1 and cooldown <= 0.0:
		Damage()
		
	if life <= 0:
		life = 0
		Die()
	
	if not is_on_floor():
		velocity += get_gravity() * delta
			
	if type == EnemyType.BOSS:
		attackCooldown -= delta * 0.5
		
	if attackCooldown < 0:
		attackCooldown = 1.0
		bossAttack()
			
	if distance < 1.5:
		animationSprite.animation = "Attack"
		attacking = true
		
	elif distance < 40:	
		move_and_slide()	
		animationSprite.animation = "Walk"
		attacking = false
	else:
		animationSprite.animation = "Idle"
		attacking = false
		
	if restartHit:
		return
		
	if attacking and currentPlayer and !animationSprite.animation_looped.is_connected(_on_finished):
		animationSprite.animation_looped.connect(_on_finished)
		restartHit = true
		await get_tree().create_timer(0.3).timeout
		_on_hit()
	elif animationSprite.animation_looped.is_connected(_on_finished):
		animationSprite.animation_looped.disconnect(_on_finished)
		restartHit = false

func bossAttack() -> void:	
	var bullet = bossBullet.instantiate()
	if spawner and currentPlayer:
		spawner.add_child(bullet)
		bullet.position = spawner.position
		bullet.transform.basis = Basis.looking_at(global_position - currentPlayer.global_position, Vector3.UP)
		bullet.scale = Vector3(4.0, 4.0, 4.0)
		bullet.reparent(get_tree().root)
		

func _on_hit() -> void:
	if attacking:
		currentPlayer.hit()
	
func _on_finished() -> void:
	restartHit = false
		
func isAttacking() -> bool:
	return attacking
	
func Die() -> void:
	dead = true
	collisionShape.disabled = true
	
	if healthBar:
		healthBar.hide()
	
	await get_tree().create_timer(deadDestroyTimeout).timeout
	queue_free()
	
func Damage() -> void:
	life -= 1
	cooldown = 1.0
	updateLife()
	var t = create_tween().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	t.tween_property($AnimatedSprite3D, "modulate", Color(1.0, 0.5, 0.5, 1.0), 0.1)
	t.tween_property($AnimatedSprite3D, "modulate", Color.WHITE, 0.1)
	
func updateLife() -> void:
	$LifeSprite.scale.x = max(float(life) / START_LIFE, 0)
	
# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass
