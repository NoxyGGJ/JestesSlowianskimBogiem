class_name GuiScene extends Node2D

var health_bar_max_width: int = 0

func _ready() -> void:
	health_bar_max_width = %InteriorLife.size.x
	# TEMP FOR TESTING
	var tween := create_tween()
	tween.set_loops() # infinite
	tween.tween_method(
	func(v: float) -> void:
		update_health(v),
		0.0,
		1.0,
		3.0).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	tween.tween_method(
	func(v: float) -> void:
		update_health(v),
		1.0,
		0.0,
		3.0).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)

func _process(delta: float) -> void:
	pass

func update_health(percent: float) -> void:
	if percent <= 0.01:
		%InteriorLife.visible = false
	else:
		%InteriorLife.visible = true
		%InteriorLife.size.x = percent * health_bar_max_width

# Use index 0...3 or -1 to disable any masks.
func update_mask_overlay(index: int) -> void:
	var sprites_node = %MaskOverlaySprites
	for i in 4:
		sprites_node.get_child(i).visible = (i == index)
