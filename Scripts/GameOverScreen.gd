class_name GameOverScreen extends Node2D

var input_enabled := false

func _unhandled_input(event: InputEvent) -> void:
	if input_enabled and event is InputEventKey and event.pressed and not event.echo:
		get_tree().change_scene_to_file("res://Level.tscn")
		return

func _on_input_enable_timer_timeout() -> void:
	input_enabled = true
