extends ColorRect
class_name Cell

@export var place: Label
@export var playerName: Label 
@export var time: Label 

func setPlace(currentPlace:String) -> void:
	place.text = currentPlace
	
func setName(currName:String) -> void:
	playerName.text = currName
	
func setTime(currTime:String) -> void:
	time.text = currTime
