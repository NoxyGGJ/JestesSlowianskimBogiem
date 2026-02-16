extends Control

@onready var level = $"../../"

func _on_resume_pressed() -> void:
	level.pause()

func _on_quit_pressed() -> void:
	get_tree().change_scene_to_file("res:///Scenes/main_menu.tscn")
