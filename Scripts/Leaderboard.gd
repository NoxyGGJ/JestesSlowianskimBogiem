extends Control

@onready var v_box_container: VBoxContainer = $ScrollContainer/VBoxContainer

var leaderboard_data:LeaderboardData
var defaultCell = preload("res://Scenes/Cell.tscn")


func _ready() -> void:
	leaderboard_data = GlobalObject.GetLeaderboardData()
	reloadWithDifficulty(GlobalObject.gameDifficulty)

func reloadWithDifficulty(difficulty:GlobalObject.GameDifficulty) -> void:
	for child in v_box_container.get_children():
		child.queue_free()
		
	match difficulty:
		GlobalScript.GameDifficulty.EASY:
			layoutData(leaderboard_data.easyList)
		GlobalScript.GameDifficulty.NORMAL:
			layoutData(leaderboard_data.normalList)
		GlobalScript.GameDifficulty.HARD:
			layoutData(leaderboard_data.hardList)
	
func layoutData(list:Array[PlayerData]) -> void:
	var dataToDisplay = list.duplicate()
	dataToDisplay.sort_custom(func(a, b): return a.PlayerTime.naturalnocasecmp_to(b.PlayerTime) < 0)
	
	for i in dataToDisplay.size():
		var cell = defaultCell.instantiate()
		var data = dataToDisplay[i]
		cell.setPlace(str(i+1))
		cell.setName(data.PlayerName)
		cell.setTime(data.PlayerTime)
		
		v_box_container.add_child(cell)

func _on_exit_pressed() -> void:
	get_tree().change_scene_to_file("res:///Scenes/main_menu.tscn")

func _on_easy_button_pressed() -> void:
	reloadWithDifficulty(GlobalScript.GameDifficulty.EASY)

func _on_normal_button_pressed() -> void:
	reloadWithDifficulty(GlobalScript.GameDifficulty.NORMAL)

func _on_hard_button_pressed() -> void:
	reloadWithDifficulty(GlobalScript.GameDifficulty.HARD)
