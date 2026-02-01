class_name GuiScene extends Node2D

var health_bar_max_width: int = 0
var damage_overlay_in_progress = false

func _ready() -> void:
	health_bar_max_width = %InteriorLife.size.x
	

func _process(delta: float) -> void:
	pass

func update_health(percent: float) -> void:
	if percent <= 0.01:
		%InteriorLife.visible = false
	else:
		%InteriorLife.visible = true
		%InteriorLife.size.x = percent * health_bar_max_width

func ping_health() -> void:
	var t = create_tween().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	t.tween_property($GuiCanvasLayer/LifeBar, "scale", Vector2(1.1, 1.1), 0.03)
	t.tween_property($GuiCanvasLayer/LifeBar, "scale", Vector2(1.0, 1.0), 0.3)

# Use index 0...3 or -1 to disable any masks.
func update_mask_overlay(index: int) -> void:
	var sprites_node = %MaskOverlaySprites
	for i in 4:
		sprites_node.get_child(i).visible = (i == index)
	%AstralEffect.visible = index == 3

func take_damage() -> void:
	if damage_overlay_in_progress:
		return
	damage_overlay_in_progress = true
	var damage_overlay_node = $%DamageOverlay
	damage_overlay_node.visible = true
	var tween := get_tree().create_tween().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_OUT)
	tween.tween_property(damage_overlay_node, "self_modulate", Color(1, 1, 1, 0), 0.47)
	await get_tree().create_timer(0.52).timeout
	damage_overlay_node.visible = false
	damage_overlay_node.self_modulate = Color.WHITE
	damage_overlay_in_progress = false
