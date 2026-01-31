class_name TreeAsset extends Node3D

func update_mask() -> void:
	var meshes_node = %Meshes
	for i in meshes_node.get_child_count():
		meshes_node.get_child(i).visible = GlobalObject.CurrentMask == i
	%StaticBody3D.process_mode = Node.PROCESS_MODE_INHERIT if GlobalObject.CurrentMask != 3 else Node.PROCESS_MODE_DISABLED
