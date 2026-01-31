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
	
	if firstTotemFinished and\
	secondTotemFinished and\
	thirdTotemFinished and\
	foruthTotemFinished:
		spawnBoss()
		
func spawnBoss() -> void:
	pass
		
