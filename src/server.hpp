#ifndef STEAM_AUDIO_SERVER_H
#define STEAM_AUDIO_SERVER_H

#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/classes/thread.hpp"
#include "listener.hpp"
#include "steam_audio.hpp"
#include <godot_cpp/variant/string.hpp>
#include <atomic>
#include <condition_variable>
#include <mutex>

using namespace godot;

class SteamAudioServer : public Object {
	GDCLASS(SteamAudioServer, Object)

private:
	static SteamAudioServer *self;
	GlobalSteamAudioState global_state{};
	std::vector<LocalSteamAudioState *> local_states;

	std::atomic<bool> is_global_state_init;
	std::atomic<bool> is_refl_thread_processing;
	std::atomic<bool> is_running;
	std::atomic<bool> local_states_have_changed;
	// Set when the scene or the simulator needs a commit; both are flushed from tick(), which
	// is the only place where the reflection simulation is known to be parked.
	std::atomic<bool> scene_needs_commit;
	std::atomic<bool> sim_needs_commit;
	std::mutex init_mux;
	std::mutex refl_mux;
	std::mutex tick_mux;
	std::mutex scene_mux;
	std::condition_variable cv;
	std::condition_variable refl_done_cv;
	bool has_warned_refl_src_limit = false;

	// Latest pending transform per dynamic mesh. Applying these touches the scene, so they are
	// queued on the game thread and flushed in tick() instead of being applied immediately.
	std::vector<std::pair<IPLInstancedMesh, IPLMatrix4x4>> pending_dynamic_transforms;
	std::vector<IPLSource> pending_sources_to_add;

	void flush_scene_changes();
	void mark_refl_idle();

	// meshes to add to the global state scene after it's initialized.
	std::vector<IPLStaticMesh> static_meshes_to_add;
	std::vector<IPLStaticMesh> dynamic_meshes_to_add;

	// TODO: allow for multiple
	SteamAudioListener *listener = nullptr;

	void init_scene(IPLSceneSettings *scene_cfg);
	void start_refl_sim();
	void run_refl_sim();
	Ref<Thread> refl_thread;

protected:
	static void _bind_methods();

public:
	SteamAudioServer();
	~SteamAudioServer();

	static SteamAudioServer *get_singleton();
	GlobalSteamAudioState *get_global_state(bool should_init = true);

	void add_listener(SteamAudioListener *listener);
	void add_local_state(LocalSteamAudioState *ls);
	void remove_local_state(LocalSteamAudioState *ls);
	void add_static_mesh(IPLStaticMesh mesh);
	void remove_static_mesh(IPLStaticMesh mesh);
	void add_dynamic_mesh(IPLInstancedMesh mesh);
	void remove_dynamic_mesh(IPLInstancedMesh mesh);
	void update_dynamic_mesh_transform(IPLInstancedMesh mesh, const IPLMatrix4x4 &transform);
	void add_source(IPLSource source);
	void remove_source(IPLSource source);

	// Blocks until the reflection simulation is not running. Steam Audio forbids committing
	// scene or simulator changes concurrently with a simulation, so anything that mutates
	// either must call this first. Game thread only: the simulation is started exclusively by
	// tick() on that same thread, so once this returns the caller owns the window.
	void wait_for_refl_idle();

	// Writes the committed acoustic scene to an OBJ (plus a sibling MTL) so you can open it in
	// a modelling tool and see what Steam Audio actually got. Runtime only.
	bool save_scene_obj(const String &path);

	void tick();
};

#endif // STEAM_AUDIO_SERVER_H
