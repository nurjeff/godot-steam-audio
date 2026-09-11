#ifndef STEAM_AUDIO_EDITOR_PLUGIN_H
#define STEAM_AUDIO_EDITOR_PLUGIN_H

#include <godot_cpp/classes/editor_node3d_gizmo_plugin.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
#include <godot_cpp/variant/typed_array.hpp>

using namespace godot;

// Draws the box a SteamAudioProbeBatch covers. Without it the volume is invisible until the
// game runs, which makes it guesswork to place.
class SteamAudioProbeGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(SteamAudioProbeGizmoPlugin, EditorNode3DGizmoPlugin);

protected:
	static void _bind_methods() {}

public:
	SteamAudioProbeGizmoPlugin();
	bool _has_gizmo(Node3D *node) const override;
	String _get_gizmo_name() const override;
	void _redraw(const Ref<EditorNode3DGizmo> &gizmo) override;
};

class SteamAudioEditorPlugin : public EditorPlugin {
	GDCLASS(SteamAudioEditorPlugin, EditorPlugin);

private:
	PopupMenu *menu = nullptr;
	Ref<SteamAudioProbeGizmoPlugin> gizmos;

	void on_menu_pressed(int p_id);
	void add_geometry(const TypedArray<Node> &targets);
	void remove_geometry();
	void validate();

protected:
	static void _bind_methods();

public:
	void _enter_tree() override;
	void _exit_tree() override;
};

#endif // STEAM_AUDIO_EDITOR_PLUGIN_H
