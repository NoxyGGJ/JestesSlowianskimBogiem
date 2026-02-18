class_name GlobalScript extends Node

enum GameDifficulty
{
	UNKNOWN,
	EASY,
	NORMAL,
	HARD,
	INSANE
}

var gameDifficulty: GameDifficulty = GameDifficulty.NORMAL

var CurrentMask: int = 0
var invincible_cheat := false

var firstTotemFinished: bool = false
var secondTotemFinished: bool = false
var thirdTotemFinished: bool = false
var foruthTotemFinished: bool = false

var LastBestTime: String = ""

var exeLocatinPath:String = OS.get_executable_path().get_base_dir() + "/Data/"
var fileName:String = "data.tres"

func _ready() -> void:
	DirAccess.make_dir_absolute(exeLocatinPath)
	
func _process(_delta: float) -> void:
	pass
	
func reset() -> void:
	CurrentMask = 0
	invincible_cheat = false
	firstTotemFinished = false
	secondTotemFinished = false
	thirdTotemFinished = false
	foruthTotemFinished = false

func get_totems_status() -> Array[bool]:
	return [firstTotemFinished, secondTotemFinished, thirdTotemFinished, foruthTotemFinished]

func get_finished_totem_count() -> int:
	var res := 0
	res += 1 if firstTotemFinished else 0
	res += 1 if secondTotemFinished else 0
	res += 1 if thirdTotemFinished else 0
	res += 1 if foruthTotemFinished else 0
	return res
	
func GetLeaderboardData() -> LeaderboardData:
	var data:LeaderboardData
	var outputPath = exeLocatinPath + fileName
	var result = ResourceLoader.load(outputPath)
	
	if result:
		data = result.duplicate(true)
		print("Loaded leaderboards")
	else:
		data = LeaderboardData.new()		
		var error = ResourceSaver.save(data, outputPath)
	
		if error != Error.OK: 
			printerr("Could not save initial leaderboard! " + outputPath + " - " + str(error))
		else:
			print("Created initial leaderboards")
			
	return data
	
func SaveLeaderboardData(data: LeaderboardData) -> void:
	var outputPath = exeLocatinPath + fileName
	var error = ResourceSaver.save(data, outputPath)
	
	if error != Error.OK: 
		printerr("Could not save leaderboard! " + outputPath + " - " + str(error))
	else:
		print("Laderboards updated")
