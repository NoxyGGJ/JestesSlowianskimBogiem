class_name Level extends Node3D

func _ready() -> void:
	%Gui.update_mask_overlay(GlobalObject.CurrentMask)
	pass
	
func _process(delta: float) -> void:
	var player = $Player
	if player and player.playerHealth <= 0.0:
		await get_tree().create_timer(0.75).timeout
		get_tree().change_scene_to_file("res://GameOverScreen.tscn")
		return

func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("PrevMask"):
		set_mask(3 if GlobalObject.CurrentMask == 0 else GlobalObject.CurrentMask - 1)
	elif event.is_action_pressed("NextMask"):
		set_mask(0 if GlobalObject.CurrentMask == 3 else GlobalObject.CurrentMask + 1)

func set_mask(MaskIndex: int) -> void:
	GlobalObject.CurrentMask = MaskIndex
	%Gui.update_mask_overlay(MaskIndex)
