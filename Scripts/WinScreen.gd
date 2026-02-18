class_name WinScreen extends Node

@onready var time_label: Label = $Panel/TimeLabel
@onready var text_edit: TextEdit = $Panel2/TextEdit

func _ready() -> void:
	
	Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	
	if time_label:
		time_label.text = GlobalObject.LastBestTime
		
	if text_edit:
		text_edit.grab_focus()

	

func _on_submit_pressed() -> void:
	
	var leaderboards:LeaderboardData = GlobalObject.GetLeaderboardData()
	var newUser:PlayerData = PlayerData.new()
	
	newUser.PlayerName = text_edit.text
	newUser.PlayerTime = GlobalObject.LastBestTime
	
	match GlobalObject.gameDifficulty:
		GlobalScript.GameDifficulty.EASY:
			leaderboards.easyList.push_back(newUser)
		GlobalScript.GameDifficulty.NORMAL:
			leaderboards.normalList.push_back(newUser)
		GlobalScript.GameDifficulty.HARD:
			leaderboards.hardList.push_back(newUser)
	
	GlobalObject.SaveLeaderboardData(leaderboards)
	
	get_tree().change_scene_to_file("res://Scenes/Leaderboard.tscn")
