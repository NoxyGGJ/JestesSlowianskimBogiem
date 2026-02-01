extends Node3D

@export var minX = 0 
@export var maxX = 500 
@export var minZ = 0
@export var maxZ = 500
@export var playerRandom = 15

@export var objects = [
	preload("res://Scenes/Enemy.tscn"),
	preload("res://Scenes/EnemySnow.tscn"),
	preload("res://Scenes/EnemyMarzana.tscn")
]

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
		
		#var random_index = rng.randi() % objects.size()
		var random_index := -1
		match GlobalObject.CurrentMask:
			0:
				random_index = 2 # Marzanna
			1:
				random_index = 1 # Balwan
			2:
				random_index = 0 # Szkielet
		if random_index != -1:
			var object_scene = objects[random_index]
			var instance2 = object_scene.instantiate()
			root.add_child(instance2)
			instance2.position = Vector3(x, y, z)

	spawn_timer.wait_time = rng.randf_range(difficulty.MinSpawnTime, difficulty.MaxSpawnTime)
	spawn_timer.start()
