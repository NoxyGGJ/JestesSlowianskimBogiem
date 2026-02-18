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

func _ready() -> void:
	pass
	
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
