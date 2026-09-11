#ifndef STEAM_AUDIO_EDITOR_PLUGIN_H
#define STEAM_AUDIO_EDITOR_PLUGIN_H

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/popup_menu.hpp>
#include <godot_cpp/variant/typed_array.hpp>

using namespace godot;

class SteamAudioEditorPlugin : public EditorPlugin {
	GDCLASS(SteamAudioEditorPlugin, EditorPlugin);

private:
	PopupMenu *menu = nullptr;

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
