class_name GlobalScript extends Node

var CurrentMask: int = 0
var invincible_cheat := false

var firstTotemFinished: bool = false
var secondTotemFinished: bool = false
var thirdTotemFinished: bool = false
var foruthTotemFinished: bool = false

func _ready() -> void:
	pass
	
func _process(delta: float) -> void:
	pass

func get_totems_status() -> Array[bool]:
	return [firstTotemFinished, secondTotemFinished, thirdTotemFinished, foruthTotemFinished]

func get_finished_totem_count() -> int:
	var res := 0
	res += 1 if firstTotemFinished else 0
	res += 1 if secondTotemFinished else 0
	res += 1 if thirdTotemFinished else 0
	res += 1 if foruthTotemFinished else 0
	return res
