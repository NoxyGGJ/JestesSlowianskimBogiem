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

@export var speed: float = 2.0
@export var wrap_to_player: float = 30.0
var distance: float = 0.0
var attacking: bool = false
var dead:bool = false
const START_LIFE := 5
@export var life: float = START_LIFE
var restartHit:bool = false
var cooldown: float = 0.0
var attackCooldown: float = 1.0
var stunTime: float = 0.0

var rng = RandomNumberGenerator.new()

func _ready() -> void:
	rng.randomize()
	currentPlayer = get_parent().find_child("Player")
	healthBar = find_child("LifeSprite")
	
	if type == EnemyType.BOSS:
		life = 50
		updateLife()
		spawner = $AnimatedSprite3D/Spawner

func _fix_y():
	position.y = get_parent().find_child("HTerrain").get_data().get_height_at(position.x, position.z) + 1.0

func _physics_process(delta: float) -> void:
	var difficulty: DifficultyLevel = get_parent().getDifficulty()
	var current_speed = speed * difficulty.SpeedMultiplier
	if type == EnemyType.MARZANA:
		current_speed = difficulty.MarzannaSpeed
		if GlobalObject.CurrentMask == 1:
			current_speed *= 0.35
	
	if stunTime > 0:
		stunTime -= delta
	else:
		if currentPlayer:
			var currPos:Vector2 = Vector2(global_position.x, global_position.z)
			var playerPos:Vector2 = Vector2(currentPlayer.global_position.x,
											currentPlayer.global_position.z)
			
			var direction = global_position.direction_to(currentPlayer.global_position)
			var new_velocity = direction * current_speed
			velocity = Vector3(new_velocity.x, velocity.y, new_velocity.z)
			distance = currPos.distance_to(playerPos)

		if type != EnemyType.SKELETON and type != EnemyType.BOSS and GlobalObject.CurrentMask == 2:
			var skeletons = get_parent().enemy_cache	#find_children("*", "Enemy", false, false)
			if !skeletons.is_empty():
				var random_index = rng.randi() % skeletons.size()
				var randomSkeleton = skeletons[random_index]
				if randomSkeleton.type == EnemyType.SKELETON:
					var dir = -global_position.direction_to(randomSkeleton.global_position)
					velocity.x = (dir * current_speed).x
					velocity.z = (dir * current_speed).z
				
	if dead:
		animationSprite.animation = "Death"
		return

	var wrap = wrap_to_player
	var dx = position.x - currentPlayer.position.x
	var dz = position.z - currentPlayer.position.z
	if dx < -wrap:
		position.x += wrap*2
		_fix_y()
	if dx > wrap:
		position.x -= wrap*2
		_fix_y()
	if dz < -wrap:
		position.z += wrap*2
		_fix_y()
	if dz > wrap:
		position.z -= wrap*2
		_fix_y()
		
	if type == EnemyType.SKELETON and GlobalObject.CurrentMask == 3:
		animationSprite.animation = "Dance"
		return
		
	cooldown -= delta * 3.0
		
	if type == EnemyType.SNOWMAN and GlobalObject.CurrentMask == 1 and difficulty.SnowmanFrostDamageFraction < 0.9:
		$LifeSprite.modulate = Color.AQUA
	else:
		$LifeSprite.modulate = Color.RED

	if type == EnemyType.SNOWMAN and GlobalObject.CurrentMask == 0 and cooldown <= 0.0:
		Damage()
		
	if type == EnemyType.MARZANA and cooldown <= 0.0:
		if GlobalObject.CurrentMask == 1:
			#Damage()
			$AnimatedSprite3D.modulate = Color(0.2,0.6,5.0)
		else:
			$AnimatedSprite3D.modulate = Color.WHITE
		
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
	
func Damage(amount = 1.0) -> void:

	if type == EnemyType.SNOWMAN and GlobalObject.CurrentMask == 1:
		var difficulty: DifficultyLevel = get_parent().getDifficulty()
		amount *= difficulty.SnowmanFrostDamageFraction

	life -= amount
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
