extends TextureRect

@export var duration = 5.0

var end_scale = Vector2(1.5, 1.5) 
var end_position = Vector2(-200, -200)
var tween:Tween
var alphaTween:Tween
var displaySize:Vector2 
var images:Array[Texture] = []

func _ready() -> void:
	
	displaySize = get_viewport().get_visible_rect().size
	
	images.push_back(load("res://Resources/Background/A.png"))
	images.push_back(load("res://Resources/Background/B.png"))
	images.push_back(load("res://Resources/Background/C.png"))
	images.push_back(load("res://Resources/Background/D.png"))
	randomizeValues()
	runTween()
	
func runTween() -> void:
	
	tween = create_tween().set_trans(Tween.TRANS_LINEAR).set_ease(Tween.EASE_IN_OUT)
	tween.tween_property(self, "modulate", Color.WHITE, 1.0)
	tween.parallel()
	tween.tween_property(self, "scale", end_scale, duration)
	tween.set_parallel(true)
	tween.tween_property(self, "position", end_position, duration)
	await tween.finished
	tween.kill()
	
	resetTween()

func resetTween() -> void:	
	alphaTween = create_tween().set_trans(Tween.TRANS_LINEAR).set_ease(Tween.EASE_IN_OUT)
	alphaTween.tween_property(self, "modulate", Color.TRANSPARENT, 1.0)
	alphaTween.tween_callback(loadNextImage)
	await alphaTween.finished
	alphaTween.kill()

	runTween()
	
func randomizeValues() -> void:
	var scaleRandStart = randf_range(0.7, 1.0)
	var scaleRandEnd = randf_range(1.2, 1.6)
	
	scale = Vector2(scaleRandStart, scaleRandStart)
	end_scale = Vector2(scaleRandEnd, scaleRandEnd)
	
	var offsetSX = (size.x * scale.x) - displaySize.x
	var offsetSY = (size.y * scale.y) - displaySize.y 
	
	var offsetEX = (size.x * end_scale.x) - displaySize.x 
	var offsetEY = (size.y * end_scale.y) - displaySize.y 
	
	position = Vector2(randf_range(-offsetSX, 0.0), randf_range(-offsetSY, 0.0))
	end_position = Vector2(randf_range(-offsetEX, 0.0), randf_range(-offsetEY, 0.0))
		
func loadNextImage() -> void:
	scale = Vector2.ONE
	position = Vector2.ZERO
	texture = images.pick_random()
	randomizeValues()
