extends CharacterBody3D
class_name Enemy

@onready var animationSprite: AnimatedSprite3D = $AnimatedSprite3D

var currentPlayer : CharacterBody3D
var speed: float = 2.0
var distance: float = 0.0
var attacking: bool = false
var dead:bool = false
var life: int = 5

func _ready() -> void:
	currentPlayer = get_parent().find_child("Player")
	
func _physics_process(delta: float) -> void:
	if currentPlayer:
		var direction = global_position.direction_to(currentPlayer.global_position)
		velocity = direction * speed
		distance = global_position.distance_to(currentPlayer.global_position)

	if dead:
		animationSprite.animation = "Death"
		return
		
	if life <= 0:
		life = 0
		Die()
		
	if distance < 1.5:
		animationSprite.animation = "Attack"
		attacking = true
		
	elif distance < 10:	
		move_and_slide()	
		animationSprite.animation = "Walk"
		attacking = false
	else:
		animationSprite.animation = "Idle"
		attacking = false
		
func isAttacking() -> bool:
	return attacking
	
func Die() -> void:
	dead = true
	
func Damage() -> void:
	life -= 1
	
# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass
