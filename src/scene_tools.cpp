#include "scene_tools.hpp"
#include "config.hpp"
#include "geometry.hpp"
#include "geometry_dynamic.hpp"
#include "listener.hpp"
#include "player.hpp"
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/core/class_db.hpp>

static bool can_hold_geometry(Node *node) {
	return Object::cast_to<MeshInstance3D>(node) != nullptr || Object::cast_to<CollisionShape3D>(node) != nullptr;
}

static bool has_geometry(Node *node) {
	for (int i = 0; i < node->get_child_count(); i++) {
		Node *child = node->get_child(i);
		if (Object::cast_to<SteamAudioGeometry>(child) || Object::cast_to<SteamAudioDynamicGeometry>(child)) {
			return true;
		}
	}
	return false;
}

template <typename T>
static void collect_class(Node *from, std::vector<T *> &out) {
	if (from == nullptr) {
		return;
	}
	if (T *found = Object::cast_to<T>(from)) {
		out.push_back(found);
	}
	for (int i = 0; i < from->get_child_count(); i++) {
		collect_class<T>(from->get_child(i), out);
	}
}

static void collect_targets(Node *from, TypedArray<Node> &out) {
	if (from == nullptr) {
		return;
	}
	if (can_hold_geometry(from) && !has_geometry(from)) {
		out.push_back(from);
	}
	for (int i = 0; i < from->get_child_count(); i++) {
		collect_targets(from->get_child(i), out);
	}
}

static void collect_geometry(Node *from, TypedArray<Node> &out) {
	if (from == nullptr) {
		return;
	}
	if (Object::cast_to<SteamAudioGeometry>(from) || Object::cast_to<SteamAudioDynamicGeometry>(from)) {
		out.push_back(from);
	}
	for (int i = 0; i < from->get_child_count(); i++) {
		collect_geometry(from->get_child(i), out);
	}
}

void SteamAudioSceneTools::_bind_methods() {
	ClassDB::bind_static_method("SteamAudioSceneTools", D_METHOD("find_geometry_targets", "root"), &SteamAudioSceneTools::find_geometry_targets);
	ClassDB::bind_static_method("SteamAudioSceneTools", D_METHOD("find_geometry", "root"), &SteamAudioSceneTools::find_geometry);
	ClassDB::bind_static_method("SteamAudioSceneTools", D_METHOD("validate_scene", "root"), &SteamAudioSceneTools::validate_scene);
}

TypedArray<Node> SteamAudioSceneTools::find_geometry_targets(Node *root) {
	TypedArray<Node> out;
	collect_targets(root, out);
	return out;
}

TypedArray<Node> SteamAudioSceneTools::find_geometry(Node *root) {
	TypedArray<Node> out;
	collect_geometry(root, out);
	return out;
}

PackedStringArray SteamAudioSceneTools::validate_scene(Node *root) {
	PackedStringArray out;
	if (root == nullptr) {
		out.push_back("No scene to validate.");
		return out;
	}

	std::vector<SteamAudioConfig *> configs;
	std::vector<SteamAudioListener *> listeners;
	std::vector<SteamAudioPlayer *> players;
	collect_class<SteamAudioConfig>(root, configs);
	collect_class<SteamAudioListener>(root, listeners);
	collect_class<SteamAudioPlayer>(root, players);
	TypedArray<Node> geometry = find_geometry(root);
	TypedArray<Node> missing = find_geometry_targets(root);

	if (configs.size() != 1) {
		out.push_back(vformat("Needs exactly one SteamAudioConfig, found %d. It must enter the tree before any geometry.", int(configs.size())));
	}
	if (listeners.size() != 1) {
		out.push_back(vformat("Needs exactly one SteamAudioListener, found %d. It usually goes under the Camera3D.", int(listeners.size())));
	}
	if (geometry.is_empty() && !players.empty()) {
		out.push_back("No acoustic geometry, so occlusion and reflections have nothing to work with.");
	}
	if (!missing.is_empty()) {
		out.push_back(vformat("%d mesh or collision shape(s) have no acoustic geometry.", int(missing.size())));
	}
	for (SteamAudioPlayer *player : players) {
		if (player->get_attenuation_filter_cutoff_hz() < 20500.0f) {
			out.push_back(vformat("%s: Godot's attenuation filter is set to %d Hz. It is disabled at runtime, since Steam Audio models air absorption itself.",
					player->get_name(), int(player->get_attenuation_filter_cutoff_hz())));
		}
		if (player->get_ambisonics_order() > SteamAudioConfig::max_ambisonics_order) {
			out.push_back(vformat("%s: ambisonics order %d exceeds the config maximum and will be clamped.",
					player->get_name(), player->get_ambisonics_order()));
		}
	}
	return out;
}
