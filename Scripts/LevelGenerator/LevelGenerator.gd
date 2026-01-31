class_name LevelGenerator extends Node3D


var root: Level
var rng = RandomNumberGenerator.new()
var terrain: Node3D
var player: CharacterBody3D
var spawn_done = false
var rnd_p0: float
var rnd_p1: float
var rnd_index = 0

@export var minX = 0 
@export var maxX = 512
@export var minZ = 0
@export var maxZ = 512


@export var prefab_tree : Resource
@export var prefab_rock : Resource




# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	rng.randomize()
	root = get_parent()
	terrain = root.find_child("HTerrain")	
	player = root.find_child("Player")
	rnd_p0 = rng.randf_range(0, 100)
	rnd_p1 = rng.randf_range(0, 100)

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	if not spawn_done:
		spawn_done = true
		for i in 2000:
			_spawn(prefab_tree, 0)
		for i in 700:
			_spawn(prefab_rock, 4.5)

func _spawn(resource: Resource, rndmod: float) -> void:
	var x = rng.randf_range(minX, maxX)
	var z = rng.randf_range(minZ, maxZ)

	var tmpz = 0.0
	var tmpq = 0.0
	for p in 7:
		tmpz += sin(z/70 + tmpq + rnd_p0 + rnd_index/10000 + rndmod)*25*(1+tmpq*0.4)
		z += cos(x/25 +.5 + tmpq + tmpz/50 + rndmod)*35
		x += sin(tmpz/25 + 4.5 + tmpq + rnd_p1 + rndmod)*70
		if x < minX:
			x = minX
			tmpq += 1
		if x > maxX:
			x = maxX
			tmpq += 1
		if z < minZ:
			z = minZ
			tmpq += 1
		if z > maxZ:
			z = maxZ
			tmpq += 1
	
	var y = terrain.get_data().get_height_at(x,z)
	
	var new_instance = resource.instantiate()

	root.add_child(new_instance)
	root.spawned_trees.append(new_instance)

	new_instance.position = Vector3(x, y, z)
	new_instance.rotation.y = rng.randf() * TAU
	rnd_index += 1
