extends Node3D

@export var minX = 0 
@export var maxX = 500 
@export var minZ = 0
@export var maxZ = 500
@export var playerRandom = 15

@export var e_skeleton = preload("res://Scenes/Enemy.tscn")
@export var e_snowman = preload("res://Scenes/EnemySnow.tscn")
@export var e_marzanna = preload("res://Scenes/EnemyMarzana.tscn")

@onready var spawn_timer: Timer = $SpawnTimer

var root: Level
var rng = RandomNumberGenerator.new()
var terrain: Node3D
var player: CharacterBody3D

func _ready() -> void:
	rng.randomize()
	spawn_timer.wait_time = 1.0 #5.0
	spawn_timer.start()
	root = get_parent()
	terrain = root.find_child("HTerrain")	
	player = root.find_child("Player")
		
func _on_spawn_timer_timeout() -> void:
	var difficulty = root.getDifficulty()

	if get_parent().enemy_cache.size() < difficulty.MaxEnemies:
		var random_value = rng.randf()
		
		var x = rng.randf_range(minX, maxX)
		var z = rng.randf_range(minZ, maxZ)
		
		#if player and rng.randi() % 2:
		#	x = player.position.x + rng.randf_range(-playerRandom, playerRandom)
		#	z = player.position.z + rng.randf_range(-playerRandom, playerRandom)
		
		var ang = rng.randf_range(0, TAU)
		var dist = playerRandom
		x = player.position.x + sin(ang)*dist
		z = player.position.z + cos(ang)*dist
			
		var y = terrain.get_data().get_height_at(x,z) + 1.0

		var object_scene = null
		match GlobalObject.CurrentMask:
			0:	# Summer
				if rng.randi() % 100 < difficulty.EnemyMixFactor:
					object_scene = e_marzanna
				else:
					object_scene = e_skeleton
				
			1:	# Winter
				if rng.randi() % 100 < difficulty.EnemyMixFactor:
					object_scene = e_snowman
				elif rng.randi() % 100 < 80:
					object_scene = e_skeleton
				else:
					object_scene = e_marzanna
				
			2:	# Night
				if rng.randi() % 100 < difficulty.EnemyMixFactor:
					object_scene = e_skeleton
				elif rng.randi() % 100 < 50:
					object_scene = e_marzanna
				else:
					object_scene = e_snowman
				
			3:	# Psy
				if rng.randi() % 100 < difficulty.EnemyMixFactor:
					if rng.randi() % 100 < 50:
						object_scene = e_marzanna
					else:
						object_scene = e_snowman
				else:
					object_scene = e_skeleton
						
		if object_scene:
			var instance2 = object_scene.instantiate()
			root.add_child(instance2)
			instance2.position = Vector3(x, y, z)

	spawn_timer.wait_time = rng.randf_range(difficulty.MinSpawnTime, difficulty.MaxSpawnTime)
	spawn_timer.start()
