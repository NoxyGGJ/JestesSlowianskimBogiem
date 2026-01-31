class_name LevelGenerator extends Node3D

class LGTileData:
	var blocked = false
	var reserved = false
	var marked = false


var root: Level
var rng = RandomNumberGenerator.new()
var terrain: Node3D
var player: CharacterBody3D
var spawn_done = false
var rnd_p0: float
var rnd_p1: float
var rnd_index = 0

# Tile types:
#	0 - free
#	1 - blocked
#	2 - avoid blocking
#
var tiles = []
var tile_num = 11
var tile_size = 48
var tile_offset = (512 - tile_num*tile_size)/2
var tile_center = tile_offset + tile_size/2

@export var minX = 0 
@export var maxX = 512
@export var minZ = 0
@export var maxZ = 512


@export var prefab_tree : Resource
@export var prefab_rock : Resource
@export var prefab_gate : Resource
@export var prefab_mountain : Resource




# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	rng.randomize()
	root = get_parent()
	terrain = root.find_child("HTerrain")	
	player = root.find_child("Player")
	rnd_p0 = rng.randf_range(0, 100)
	rnd_p1 = rng.randf_range(0, 100)
	tiles = []
	for i in tile_num*tile_num:
		tiles.append(LGTileData.new())
		
	var midtile = _gettile(int(tile_num/2), int(tile_num/2))
	midtile.reserved = true
		
	#tiles[1].blocked = true
	#tiles[tile_num].blocked = true
	#tiles[tile_num+1].blocked = true
	#if check_tile_connectivity():
	#	print("Connectivity OK")
	#else:
	#	print("Connectivity BAD")
	


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	if not spawn_done:
		spawn_done = true

		# generate up to 100 mountains (bad try counts as 1/10th of a mountain)
		var mtries = 20 * 10
		while mtries > 0:
			if _generate_mountain():
				mtries -= 10
			else:
				mtries -= 1

		for i in 2000:
			_spawn(prefab_tree, 0)
		for i in 700:
			_spawn(prefab_rock, 4.5)


func _gettile(x:int, y:int) -> LGTileData:
	if x<0 or x>=tile_num or y<0 or y>=tile_num:
		return null
	return tiles[x + y*tile_num]

func _gettile_by_coord(x:float, z:float) -> LGTileData:
	x = floor((x - tile_offset)/tile_size)
	z = floor((z - tile_offset)/tile_size)
	return _gettile(int(x), int(z))
	
func reserve_location(x:float, z:float):
	var tile = _gettile_by_coord(x, z)
	tile.reserved = true
	
func reserve_location_r(x:float, z:float, r:float):
	reserve_location(x-r, z-r)
	reserve_location(x-r, z+r)
	reserve_location(x+r, z-r)
	reserve_location(x+r, z+r)

func check_tile_connectivity():
	var start_index = 0

	for i in tile_num*tile_num:
		tiles[i].marked = false
		if not tiles[i].blocked:
			start_index = i

	var tasks = [start_index]
	var tidx = 0
	while tidx < tasks.size():
		var index = tasks[tidx]
		tidx += 1

		var t = tiles[index]
		if t.blocked or t.marked:
			continue
		t.marked = true
		var x = index % tile_num
		var y = index / tile_num
		if x > 0:
			tasks.append(index - 1)
		if x < tile_num-1:
			tasks.append(index + 1)
		if y > 0:
			tasks.append(index - tile_num)
		if y > tile_num-1:
			tasks.append(index + tile_num)

	for i in tile_num*tile_num:
		if not tiles[i].blocked and not tiles[i].marked:
			return false
			
	return true

func _generate_mountain() -> bool:
	var x = rng.randi_range(0, tile_num-1)
	var y = rng.randi_range(0, tile_num-1)
	var t = tiles[x + y*tile_num]
	
	if t.blocked or t.reserved:
		return false
	
	t.blocked = true
	if not check_tile_connectivity():
		t.blocked = false
		return false
		
	var mx = x * tile_size + tile_center
	var mz = y * tile_size + tile_center
	var my = terrain.get_data().get_height_at(mx,mz)
	var new_instance = prefab_mountain.instantiate()
	root.add_child(new_instance)
	new_instance.position = Vector3(mx, my, mz)
	new_instance.rotation.y = rng.randf() * TAU

	return true
	

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
	
	if _gettile_by_coord(x, z).blocked:
		return
	
	var y = terrain.get_data().get_height_at(x,z)
	
	var new_instance = resource.instantiate()

	root.add_child(new_instance)
	root.spawned_trees.append(new_instance)

	new_instance.position = Vector3(x, y, z)
	new_instance.rotation.y = rng.randf() * TAU
	rnd_index += 1
