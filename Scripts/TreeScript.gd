class_name TreeAsset extends Node3D

func update_mask() -> void:
	var meshes_node = %Meshes
	for i in meshes_node.get_child_count():
		meshes_node.get_child(i).visible = GlobalObject.CurrentMask == i
