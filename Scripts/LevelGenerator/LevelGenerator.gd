class_name LevelGenerator extends Node3D


var root: Level
var rng = RandomNumberGenerator.new()
var terrain: Node3D
var player: CharacterBody3D
var spawn_done = false

@export var minX = 0 
@export var maxX = 512
@export var minZ = 0
@export var maxZ = 512


@export var prefab_tree : Resource




# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	rng.randomize()
	root = get_parent()
	terrain = root.find_child("HTerrain")	
	player = root.find_child("Player")

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	if not spawn_done:
		spawn_done = true
		for i in 2000:
			_spawn()


func _spawn() -> void:
	var x = rng.randf_range(minX, maxX)
	var z = rng.randf_range(minZ, maxZ)
	var y = terrain.get_data().get_height_at(x,z)
	
	var object_scene = prefab_tree
	var new_instance = object_scene.instantiate()

	root.add_child(new_instance)
	root.spawned_trees.append(new_instance)

	new_instance.position = Vector3(x, y, z)
	new_instance.rotation.y = rng.randf() * TAU;
	
