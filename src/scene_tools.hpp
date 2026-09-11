#ifndef STEAM_AUDIO_SCENE_TOOLS_H
#define STEAM_AUDIO_SCENE_TOOLS_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>

using namespace godot;

// Scene inspection shared by the editor plugin and available to tool scripts.
class SteamAudioSceneTools : public RefCounted {
	GDCLASS(SteamAudioSceneTools, RefCounted);

protected:
	static void _bind_methods();

public:
	// Meshes and collision shapes under root that have no acoustic geometry yet.
	static TypedArray<Node> find_geometry_targets(Node *root);
	// Every SteamAudioGeometry and SteamAudioDynamicGeometry under root.
	static TypedArray<Node> find_geometry(Node *root);
	// One line per problem; empty means the scene looks usable.
	static PackedStringArray validate_scene(Node *root);
};

#endif // STEAM_AUDIO_SCENE_TOOLS_H
