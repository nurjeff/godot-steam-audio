#include "editor_plugin.hpp"
#include "geometry.hpp"
#include "material.hpp"
#include "probes.hpp"
#include "scene_tools.hpp"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_node3d_gizmo.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

enum MenuItem {
	MENU_ADD_SELECTION,
	MENU_ADD_SCENE,
	MENU_REMOVE_SCENE,
	MENU_VALIDATE,
};

static const char *DEFAULT_MATERIAL = "res://addons/godot-steam-audio/materials/default_material.tres";

SteamAudioProbeGizmoPlugin::SteamAudioProbeGizmoPlugin() {
	create_material("probe_volume", Color(0.98f, 0.14f, 0.29f, 0.8f));
}

bool SteamAudioProbeGizmoPlugin::_has_gizmo(Node3D *node) const {
	return Object::cast_to<SteamAudioProbeBatch>(node) != nullptr;
}

String SteamAudioProbeGizmoPlugin::_get_gizmo_name() const {
	return "SteamAudioProbeBatch";
}

void SteamAudioProbeGizmoPlugin::_redraw(const Ref<EditorNode3DGizmo> &gizmo) {
	gizmo->clear();
	SteamAudioProbeBatch *batch = Object::cast_to<SteamAudioProbeBatch>(gizmo->get_node_3d());
	if (batch == nullptr) {
		return;
	}
	Vector3 half = batch->get_size() * 0.5f;
	Vector3 corner[8];
	for (int i = 0; i < 8; i++) {
		corner[i] = Vector3(
				(i & 1) ? half.x : -half.x,
				(i & 2) ? half.y : -half.y,
				(i & 4) ? half.z : -half.z);
	}
	static const int edges[12][2] = {
		{ 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },
		{ 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
	};
	PackedVector3Array lines;
	for (auto &edge : edges) {
		lines.push_back(corner[edge[0]]);
		lines.push_back(corner[edge[1]]);
	}
	gizmo->add_lines(lines, get_material("probe_volume", gizmo));
}

void SteamAudioEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("on_menu_pressed", "id"), &SteamAudioEditorPlugin::on_menu_pressed);
}

void SteamAudioEditorPlugin::_enter_tree() {
	menu = memnew(PopupMenu);
	menu->add_item("Add Acoustic Geometry to Selection", MENU_ADD_SELECTION);
	menu->add_item("Add Acoustic Geometry to Scene", MENU_ADD_SCENE);
	menu->add_item("Remove Acoustic Geometry from Scene", MENU_REMOVE_SCENE);
	menu->add_separator();
	menu->add_item("Validate Scene", MENU_VALIDATE);
	menu->connect("id_pressed", Callable(this, "on_menu_pressed"));
	add_tool_submenu_item("Steam Audio", menu);

	gizmos.instantiate();
	add_node_3d_gizmo_plugin(gizmos);
}

void SteamAudioEditorPlugin::_exit_tree() {
	remove_tool_menu_item("Steam Audio");
	menu = nullptr;
	remove_node_3d_gizmo_plugin(gizmos);
	gizmos.unref();
}

void SteamAudioEditorPlugin::on_menu_pressed(int p_id) {
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	if (root == nullptr) {
		UtilityFunctions::print("[godot-steam-audio] No scene is open.");
		return;
	}

	switch (p_id) {
		case MENU_ADD_SELECTION: {
			TypedArray<Node> targets;
			TypedArray<Node> selected = EditorInterface::get_singleton()->get_selection()->get_selected_nodes();
			for (int i = 0; i < selected.size(); i++) {
				targets.append_array(SteamAudioSceneTools::find_geometry_targets(Object::cast_to<Node>(selected[i])));
			}
			add_geometry(targets);
		} break;
		case MENU_ADD_SCENE:
			add_geometry(SteamAudioSceneTools::find_geometry_targets(root));
			break;
		case MENU_REMOVE_SCENE:
			remove_geometry();
			break;
		case MENU_VALIDATE:
			validate();
			break;
	}
}

void SteamAudioEditorPlugin::add_geometry(const TypedArray<Node> &targets) {
	if (targets.is_empty()) {
		UtilityFunctions::print("[godot-steam-audio] Nothing to add: every mesh and collision shape already has acoustic geometry.");
		return;
	}

	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	Ref<SteamAudioMaterial> material = ResourceLoader::get_singleton()->load(DEFAULT_MATERIAL);
	EditorUndoRedoManager *undo = get_undo_redo();
	undo->create_action("Add Steam Audio geometry");
	for (int i = 0; i < targets.size(); i++) {
		Node *parent = Object::cast_to<Node>(targets[i]);
		SteamAudioGeometry *geometry = memnew(SteamAudioGeometry);
		geometry->set_name("SteamAudioGeometry");
		if (material.is_valid()) {
			geometry->set_material(material);
		}
		undo->add_do_method(parent, "add_child", geometry, true);
		undo->add_do_method(geometry, "set_owner", root);
		undo->add_do_reference(geometry);
		undo->add_undo_method(parent, "remove_child", geometry);
	}
	undo->commit_action();
	UtilityFunctions::print("[godot-steam-audio] Added acoustic geometry to ", targets.size(), " node(s).");
}

void SteamAudioEditorPlugin::remove_geometry() {
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	TypedArray<Node> found = SteamAudioSceneTools::find_geometry(root);
	if (found.is_empty()) {
		UtilityFunctions::print("[godot-steam-audio] No acoustic geometry in this scene.");
		return;
	}

	EditorUndoRedoManager *undo = get_undo_redo();
	undo->create_action("Remove Steam Audio geometry");
	for (int i = 0; i < found.size(); i++) {
		Node *geometry = Object::cast_to<Node>(found[i]);
		Node *parent = geometry->get_parent();
		undo->add_do_method(parent, "remove_child", geometry);
		undo->add_undo_method(parent, "add_child", geometry, true);
		undo->add_undo_method(geometry, "set_owner", root);
		undo->add_undo_reference(geometry);
	}
	undo->commit_action();
	UtilityFunctions::print("[godot-steam-audio] Removed ", found.size(), " acoustic geometry node(s).");
}

void SteamAudioEditorPlugin::validate() {
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	PackedStringArray problems = SteamAudioSceneTools::validate_scene(root);
	UtilityFunctions::print("[godot-steam-audio] ", root->get_name(), ": ",
			SteamAudioSceneTools::find_geometry(root).size(), " geometry node(s).");
	if (problems.is_empty()) {
		UtilityFunctions::print("  - Looks good.");
		return;
	}
	for (int i = 0; i < problems.size(); i++) {
		UtilityFunctions::print("  - ", problems[i]);
	}
}
