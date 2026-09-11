extends SceneTree

func _fail(msg: String) -> void:
	push_error(msg)
	printerr("FAIL: ", msg)
	quit(1)

func _initialize() -> void:
	if not ClassDB.class_exists("SteamAudioSceneTools"):
		_fail("SteamAudioSceneTools is not registered")
		return

	var root := Node3D.new()
	root.name = "World"
	var cfg := ClassDB.instantiate("SteamAudioConfig") as Node
	root.add_child(cfg)

	var wall := MeshInstance3D.new()
	wall.name = "Wall"
	wall.mesh = BoxMesh.new()
	root.add_child(wall)

	var floor_mesh := MeshInstance3D.new()
	floor_mesh.name = "Floor"
	floor_mesh.mesh = BoxMesh.new()
	floor_mesh.add_child(ClassDB.instantiate("SteamAudioGeometry") as Node)
	root.add_child(floor_mesh)

	var body := StaticBody3D.new()
	var shape := CollisionShape3D.new()
	shape.name = "WallShape"
	shape.shape = BoxShape3D.new()
	body.add_child(shape)
	root.add_child(body)

	var targets := SteamAudioSceneTools.find_geometry_targets(root)
	var names: Array[String] = []
	for n in targets:
		names.append(n.name)
	names.sort()
	if names != ["Wall", "WallShape"]:
		_fail("unexpected targets: %s" % [names])
		return

	var geo := SteamAudioSceneTools.find_geometry(root)
	if geo.size() != 1:
		_fail("expected 1 geometry node, got %d" % geo.size())
		return

	var problems := SteamAudioSceneTools.validate_scene(root)
	var joined := "\n".join(problems)
	if not joined.contains("SteamAudioListener"):
		_fail("validate_scene missed the missing listener:\n%s" % joined)
		return
	if not joined.contains("2 mesh or collision shape"):
		_fail("validate_scene missed the untagged geometry:\n%s" % joined)
		return

	# A scene that is set up properly reports nothing.
	var ok := Node3D.new()
	ok.add_child(ClassDB.instantiate("SteamAudioConfig") as Node)
	ok.add_child(ClassDB.instantiate("SteamAudioListener") as Node)
	var ok_problems := SteamAudioSceneTools.validate_scene(ok)
	if not ok_problems.is_empty():
		_fail("clean scene reported problems: %s" % ["\n".join(ok_problems)])
		return

	# Null root must not crash.
	SteamAudioSceneTools.find_geometry_targets(null)
	SteamAudioSceneTools.find_geometry(null)
	if SteamAudioSceneTools.validate_scene(null).is_empty():
		_fail("validate_scene(null) should report something")
		return

	print("scene_tools: ok")
	print("validate_scene output:")
	for p in problems:
		print("  - ", p)
	ok.free()
	root.free()
	quit(0)
