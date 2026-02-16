class_name GameOverScreen extends Node2D

@onready var time_label: Label = $Panel/TimeLabel

var input_enabled := false

func _ready() -> void:
	if time_label:
		time_label.text = GlobalObject.LastBestTime

func _unhandled_input(event: InputEvent) -> void:
	if input_enabled and event is InputEventKey and event.pressed and not event.echo:
		get_tree().change_scene_to_file("res://Scenes/Level.tscn")
		return

func _on_input_enable_timer_timeout() -> void:
	input_enabled = true
