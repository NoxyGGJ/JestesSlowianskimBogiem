extends Control

@export var difficulty: VFlowContainer
@export var main: VFlowContainer

func _on_play_button_pressed() -> void:
	main.hide()
	difficulty.show()

func _on_button_exit_pressed() -> void:
	get_tree().quit()

func _on_easy_pressed() -> void:
	GlobalObject.gameDifficulty = GlobalScript.GameDifficulty.EASY
	startGame()

func _on_normal_pressed() -> void:
	GlobalObject.gameDifficulty = GlobalScript.GameDifficulty.NORMAL
	startGame()

func _on_hard_pressed() -> void:
	GlobalObject.gameDifficulty = GlobalScript.GameDifficulty.HARD
	startGame()
	
func startGame() -> void:
	get_tree().change_scene_to_file("res:///Scenes/Level.tscn")
