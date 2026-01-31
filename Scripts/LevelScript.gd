class_name Level extends Node3D

@onready var Environments: Array[Environment] = [
	preload("res://Environments/SummerEnv.tres"),
	preload("res://Environments/WinterEnv.tres"),
	preload("res://Environments/NightEnv.tres"),
	preload("res://Environments/AstralEnv.tres")
]
const udp_enabled := true
var udp: PacketPeerUDP

func _ready() -> void:
	%Gui.update_mask_overlay(GlobalObject.CurrentMask)
	if udp_enabled:
		udp = PacketPeerUDP.new()
		udp.bind(9571, "0.0.0.0") # listen on all interfaces

func _process_udp_packets() -> void:
	while udp.get_available_packet_count() > 0:
		var packet := udp.get_packet()
		var sender_ip := udp.get_packet_ip()
		var sender_port := udp.get_packet_port()
		var packet_string := packet.get_string_from_utf8()
		print("Received packet %s:%d -> %s" % [sender_ip, sender_port, packet_string])
		if packet_string.length() != 5:
			return
		if not packet_string.begins_with("MASK"):
			return
		var digit := packet_string[4].to_int() - 48  # ASCII '0' = 48
		if digit < 0 or digit > 3:
			return
		set_mask(digit)

func _process(delta: float) -> void:
	var player = $Player
	if player and player.playerHealth <= 0.0:
		await get_tree().create_timer(0.75).timeout
		get_tree().change_scene_to_file("res://GameOverScreen.tscn")
		return
	if udp_enabled:
		_process_udp_packets()

func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("PrevMask"):
		set_mask(3 if GlobalObject.CurrentMask == 0 else GlobalObject.CurrentMask - 1)
	elif event.is_action_pressed("NextMask"):
		set_mask(0 if GlobalObject.CurrentMask == 3 else GlobalObject.CurrentMask + 1)
	elif event.is_action_pressed("Mask0"):
		set_mask(0)
	elif event.is_action_pressed("Mask1"):
		set_mask(1)
	elif event.is_action_pressed("Mask2"):
		set_mask(2)
	elif event.is_action_pressed("Mask3"):
		set_mask(3)
	
func set_mask(MaskIndex: int) -> void:
	GlobalObject.CurrentMask = MaskIndex
	%Gui.update_mask_overlay(MaskIndex)
	$WorldEnvironment.environment = Environments[MaskIndex] if (MaskIndex >= 0 and MaskIndex < 4) else null
