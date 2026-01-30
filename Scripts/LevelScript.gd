class_name Level extends Node3D

var health_bar_max_width: int = 0

func _init_gui() -> void:
	health_bar_max_width = %InteriorLife.size.x

func _update_gui_health(percent: float) -> void:
	if percent <= 0.01:
		%InteriorLife.visible = false
	else:
		%InteriorLife.visible = true
		%InteriorLife.size.x = percent * health_bar_max_width

func _ready() -> void:
	_init_gui()
	# TEMP FOR TESTING
	var tween := create_tween()
	tween.set_loops() # infinite
	tween.tween_method(
	func(v: float) -> void:
		_update_gui_health(v),
		0.0,
		1.0,
		3.0).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	tween.tween_method(
	func(v: float) -> void:
		_update_gui_health(v),
		1.0,
		0.0,
		3.0).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	
func _process(delta: float) -> void:
	pass

func _unhandled_input(event: InputEvent) -> void:
	pass
