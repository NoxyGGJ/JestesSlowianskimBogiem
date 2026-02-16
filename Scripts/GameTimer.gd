extends Panel

class_name GameTimer

var time: float = 0.0
var minutes: int = 0
var seconds: int = 0
var msec: int = 0

@onready var minutesLabel: Label = $Minutes
@onready var secondsLabel: Label = $Seconds
@onready var msecLabel: Label = $MiliSeconds


func _process(delta: float) -> void:
	time += delta
	msec = fmod(time, 1) * 100
	seconds = fmod(time, 60)
	minutes = fmod(time, 3600)  / 60
	minutesLabel.text = "%02d:" % minutes
	secondsLabel.text = "%02d." % seconds
	msecLabel.text = "%03d" % msec


func stop() -> void:
	set_process(false)
	
func start() -> void:
	set_process(true)

func getTimeFormated() -> String:
	return "%02d:%02d.%03d" % [minutes, seconds, msec]
