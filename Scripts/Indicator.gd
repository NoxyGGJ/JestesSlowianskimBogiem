extends Control

func _ready() -> void:
	hide()

func _process(_delta: float) -> void:
	pass

func set_value(value):
	$TextureProgressBar.value = value
	if value > 0:
		show()
	else:
		hide()
