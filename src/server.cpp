#include "server.hpp"
#include "godot_cpp/classes/dir_access.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/project_settings.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/memory.hpp"
#include "godot_cpp/variant/callable_method_pointer.hpp"
#include "phonon.h"
#include "config.hpp"
#include "player.hpp"
#include "server_init.hpp"
#include "steam_audio.hpp"
#include <algorithm>
#include <godot_cpp/variant/utility_functions.hpp>

void SteamAudioServer::tick() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (!self->is_global_state_init.load()) {
		return;
	}
	std::lock_guard<std::mutex> tick_lock(self->tick_mux);
	if (self->listener == nullptr || !self->listener->is_inside_tree()) {
		return;
	}

	SteamAudio::log(SteamAudio::log_debug, "tick");

	// Steam Audio: "This function cannot be called concurrently with any simulation functions."
	// The reflection simulation is only ever started at the end of this function, on this same
	// thread, so observing it idle here means it stays idle for the rest of the window.
	if (!self->is_refl_thread_processing.load()) {
		self->flush_scene_changes();
	}

	SteamAudio::log(SteamAudio::log_debug, "tick: committed scene");

	self->global_state.listener_coords =
			ipl_coords_from(self->listener->get_global_transform());

	for (auto ls : self->local_states) {
		if (ls->src.player == nullptr || !ls->src.player->is_inside_tree()) {
			continue;
		}
		if (!ls->src.player->is_playing()) {
			continue;
		}

		Vector3 src_pos = ls->src.player->get_global_position();
		ls->dir_to_listener = src_pos - self->listener->get_global_position();

		IPLDistanceAttenuationModel attn_model{};
		attn_model.type = IPL_DISTANCEATTENUATIONTYPE_INVERSEDISTANCE;
		attn_model.minDistance = ls->cfg.min_attn_dist;

		IPLAirAbsorptionModel absorp_model{};
		absorp_model.type = ls->cfg.air_absorption_model_type;
		absorp_model.coefficients[0] = ls->cfg.air_absorption_low;
		absorp_model.coefficients[1] = ls->cfg.air_absorption_mid;
		absorp_model.coefficients[2] = ls->cfg.air_absorption_high;

		IPLCoordinateSpace3 src_coords = ipl_coords_from(ls->src.player->get_global_transform());

		IPLSimulationInputs inputs{};
		inputs.flags = IPL_SIMULATIONFLAGS_DIRECT;
		inputs.distanceAttenuationModel = attn_model;
		inputs.airAbsorptionModel = absorp_model;
		inputs.source = src_coords;
		inputs.occlusionType = IPL_OCCLUSIONTYPE_VOLUMETRIC;
		inputs.occlusionRadius = ls->cfg.occ_radius;
		inputs.numOcclusionSamples = ls->cfg.occ_samples;
		inputs.numTransmissionRays = ls->cfg.transm_rays;

		if (ls->cfg.is_air_absorp_on) {
			inputs.directFlags = static_cast<IPLDirectSimulationFlags>(
					inputs.directFlags |
					IPL_DIRECTSIMULATIONFLAGS_AIRABSORPTION);
		}

		if (ls->cfg.is_dist_attn_on) {
			inputs.directFlags = static_cast<IPLDirectSimulationFlags>(
					inputs.directFlags |
					IPL_DIRECTSIMULATIONFLAGS_DISTANCEATTENUATION);
		}
		if (ls->cfg.is_occlusion_on) {
			inputs.directFlags = static_cast<IPLDirectSimulationFlags>(
					inputs.directFlags |
					IPL_DIRECTSIMULATIONFLAGS_OCCLUSION |
					IPL_DIRECTSIMULATIONFLAGS_TRANSMISSION);
		}
		if (ls->cfg.is_directivity_on) {
			inputs.directivity.dipoleWeight = ls->cfg.dipole_weight;
			inputs.directivity.dipolePower = ls->cfg.dipole_power;
			inputs.directFlags = static_cast<IPLDirectSimulationFlags>(
					inputs.directFlags |
					IPL_DIRECTSIMULATIONFLAGS_DIRECTIVITY);
		}

		SteamAudio::log(SteamAudio::log_debug, "tick: setting inputs");
		iplSourceSetInputs(ls->src.src, IPL_SIMULATIONFLAGS_DIRECT, &inputs);
	}
	SteamAudio::log(SteamAudio::log_debug, "tick: direct inputs set");

	IPLSimulationSharedInputs shared_inputs{};
	shared_inputs.listener = self->global_state.listener_coords;
	iplSimulatorSetSharedInputs(self->global_state.sim,
			IPL_SIMULATIONFLAGS_DIRECT, &shared_inputs);
	iplSimulatorRunDirect(self->global_state.sim);

	SteamAudio::log(SteamAudio::log_debug, "tick: direct sim complete");

	for (auto ls : self->local_states) {
		if (ls->src.player == nullptr || !ls->src.player->is_inside_tree()) {
			continue;
		}
		if (!ls->src.player->is_playing()) {
			continue;
		}

		IPLSimulationOutputs outputs{};
		iplSourceGetOutputs(ls->src.src, IPL_SIMULATIONFLAGS_DIRECT, &outputs);
		ls->direct_outputs = outputs.direct;
	}

	if (is_refl_thread_processing.load()) {
		SteamAudio::log(SteamAudio::log_debug, "tick: done, skipping reflections");
		return;
	}

	int num_refl_srcs = 0;

	// Never block the game thread on the audio thread: a frame without fresh impulse responses
	// is better than a stall behind a convolution.
	
	if (global_state.refl_ir_lock.try_lock()) {
		for (auto ls : local_states) {
			if (ls->src.player == nullptr || !ls->src.player->is_inside_tree()) {
				continue;
			}
			if (!ls->src.player->is_playing()) {
				ls->refl_in_range.store(false);
				continue;
			}
			if (listener == nullptr || !listener->is_inside_tree()) {
				continue;
			}

			if (ls->src.player->get_global_position().distance_to(listener->get_global_position()) > ls->cfg.max_refl_dist) {
				ls->refl_in_range.store(false);
				continue;
			}

			IPLSimulationOutputs outputs;
			iplSourceGetOutputs(ls->src.src, IPL_SIMULATIONFLAGS_REFLECTIONS, &outputs);
			ls->refl_outputs = outputs.reflections;
			ls->refl_in_range.store(true);
		}
		global_state.refl_ir_lock.unlock();
	}

	for (auto ls : self->local_states) {
		if (ls->src.player == nullptr || !ls->src.player->is_inside_tree()) {
			continue;
		}
		if (!ls->src.player->is_playing()) {
			continue;
		}
		if (listener == nullptr || !listener->is_inside_tree()) {
			continue;
		}
		if (ls->src.player->get_global_position().distance_to(listener->get_global_position()) > ls->cfg.max_refl_dist) {
			continue;
		}

		auto player = dynamic_cast<SteamAudioPlayer *>(ls->src.player);
		if (player == nullptr || !player->is_reflection_on()) {
			continue;
		}

		IPLCoordinateSpace3 src_coords = ipl_coords_from(ls->src.player->get_global_transform());

		IPLSimulationInputs inputs{};
		inputs.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
		inputs.source = src_coords;
		// Zero here means "scale the simulated reverb to nothing", which left parametric and
		// hybrid reverb silent.
		inputs.reverbScale[0] = 1.0f;
		inputs.reverbScale[1] = 1.0f;
		inputs.reverbScale[2] = 1.0f;
		inputs.hybridReverbTransitionTime = 1.0f;
		inputs.hybridReverbOverlapPercent = 0.25f;

		iplSourceSetInputs(ls->src.src, IPL_SIMULATIONFLAGS_REFLECTIONS, &inputs);
		num_refl_srcs++;
	}

	// Nothing wants reflections, so do not wake the reflection thread at all. This keeps the
	// ray tracer off the CPU when the feature is unused, and leaves the scene free to be
	// mutated without waiting. Players stop applying the effect when their flag is off, so no
	// stale impulse response is left ringing.
	if (num_refl_srcs == 0) {
		SteamAudio::log(SteamAudio::log_debug, "tick: done, no sources want reflections");
		return;
	}

	if (num_refl_srcs > SteamAudioConfig::max_num_refl_srcs && !has_warned_refl_src_limit) {
		has_warned_refl_src_limit = true;
		UtilityFunctions::push_warning(vformat(
				"%d sources have reflections enabled, but SteamAudioConfig.max_reflection_sources is %d. "
				"Raise it before the extension initializes, or reduce the number of reflective sources.",
				num_refl_srcs, SteamAudioConfig::max_num_refl_srcs));
	}

	if (listener == nullptr || !listener->is_inside_tree()) {
		return;
	}
	shared_inputs = IPLSimulationSharedInputs{};
	shared_inputs.listener = global_state.listener_coords;
	shared_inputs.numRays = listener->get_num_refl_rays();
	shared_inputs.numBounces = listener->get_num_refl_bounces();
	shared_inputs.duration = listener->get_refl_duration();
	shared_inputs.order = listener->get_refl_ambisonics_order();
	shared_inputs.irradianceMinDistance = listener->get_irradiance_min_dist();
	iplSimulatorSetSharedInputs(global_state.sim, IPL_SIMULATIONFLAGS_REFLECTIONS, &shared_inputs);

	{
		// notify reflection thread and tell it it can start running again
		std::unique_lock<std::mutex> lock(refl_mux);
		is_refl_thread_processing.store(true);
		cv.notify_one();
	}

	SteamAudio::log(SteamAudio::log_debug, "tick: done");
}

GlobalSteamAudioState *SteamAudioServer::get_global_state(bool should_init) {
	self->init_mux.lock();
	if (self->is_global_state_init.load()) {
		self->init_mux.unlock();
		return &self->global_state;
	}

	if (!should_init) {
		self->init_mux.unlock();
		return nullptr;
	}

	SteamAudio::log(SteamAudio::log_info, "Initializing SteamAudioServer global state");

	global_state.audio_cfg = create_audio_cfg();
	global_state.ctx = create_ctx();

	IPLSceneSettings scene_cfg = create_scene_cfg(global_state.ctx);
	IPLerror err = iplSceneCreate(global_state.ctx, &scene_cfg, &global_state.scene);
	handleErr(err);
	for (auto m : static_meshes_to_add) {
		iplStaticMeshAdd(m, global_state.scene);
	}

	global_state.sim = create_simulator(
			global_state.ctx, global_state.audio_cfg, scene_cfg);
	global_state.hrtf = create_hrtf(global_state.ctx, global_state.audio_cfg);
	global_state.ambi_enc_effect = create_ambisonics_encode_effect(
			global_state.ctx, global_state.audio_cfg);
	global_state.ambi_dec_effect = create_ambisonics_decode_effect(
			global_state.ctx, global_state.audio_cfg, global_state.hrtf);

	iplSimulatorSetScene(global_state.sim, global_state.scene);
	iplSimulatorCommit(global_state.sim);

	is_global_state_init.store(true);
	init_mux.unlock();

	SteamAudio::log(SteamAudio::log_info, "Initialized SteamAudioServer global state");
	start_refl_sim();
	return &global_state;
}

void SteamAudioServer::start_refl_sim() {
	refl_thread.instantiate();
	refl_thread->start(callable_mp(this, &SteamAudioServer::run_refl_sim));
}

void SteamAudioServer::run_refl_sim() {
	while (this->is_running.load()) {
		{
			std::unique_lock<std::mutex> lock(this->refl_mux);
			cv.wait(lock, [&] { return is_refl_thread_processing.load() || !is_running.load(); });
		}
		// if someone removed a local state, then the reflection sim might crash, so
		// we need it to wait for another tick.
		// XXX: what happens if a local state is removed in the middle of a sim run...?
		if (local_states_have_changed.load()) {
			local_states_have_changed.store(false);
			mark_refl_idle();
			continue;
		}
		SteamAudio::log(SteamAudio::log_debug, "running reflection sim");
		iplSimulatorRunReflections(global_state.sim);
		mark_refl_idle();
	}
}

void SteamAudioServer::mark_refl_idle() {
	{
		// Taken so a waiter cannot miss the notification between the store and the wait.
		std::lock_guard<std::mutex> lock(refl_mux);
		is_refl_thread_processing.store(false);
	}
	refl_done_cv.notify_all();
}

void SteamAudioServer::wait_for_refl_idle() {
	if (!is_refl_thread_processing.load()) {
		return;
	}
	std::unique_lock<std::mutex> lock(refl_mux);
	refl_done_cv.wait(lock, [&] {
		return !is_refl_thread_processing.load() || !is_running.load();
	});
}

// Applies everything that changes the scene or the simulator, then commits once. Steam Audio
// asks for exactly this: "For best performance, call this function once after all changes have
// been made for a given frame." Only called from tick(), with the reflection thread parked.
void SteamAudioServer::flush_scene_changes() {
	std::vector<std::pair<IPLInstancedMesh, IPLMatrix4x4>> transforms;
	{
		std::lock_guard<std::mutex> lock(scene_mux);
		transforms.swap(pending_dynamic_transforms);
	}
	for (auto &pending : transforms) {
		iplInstancedMeshUpdateTransform(pending.first, global_state.scene, pending.second);
	}
	if (!transforms.empty()) {
		scene_needs_commit.store(true);
	}
	if (scene_needs_commit.exchange(false)) {
		iplSceneCommit(global_state.scene);
	}
	std::vector<IPLSource> sources;
	{
		std::lock_guard<std::mutex> lock(scene_mux);
		sources.swap(pending_sources_to_add);
	}
	for (auto source : sources) {
		iplSourceAdd(source, global_state.sim);
	}
	// Sources added or removed after startup only take part in simulations once committed.
	if (!sources.empty() || sim_needs_commit.exchange(false)) {
		iplSimulatorCommit(global_state.sim);
	}
}

void SteamAudioServer::add_listener(SteamAudioListener *lis) {
	std::lock_guard<std::mutex> lock(self->tick_mux);
	self->listener = lis;
}

void SteamAudioServer::add_local_state(LocalSteamAudioState *ls) {
	std::lock_guard<std::mutex> lock(self->tick_mux);
	self->local_states.push_back(ls);
}

void SteamAudioServer::remove_local_state(LocalSteamAudioState *ls) {
	std::lock_guard<std::mutex> lock(tick_mux);
	auto it = std::find(local_states.begin(), local_states.end(), ls);
	if (it == local_states.end()) {
		return;
	}
	local_states.erase(it);
	local_states_have_changed.store(true);
}

void SteamAudioServer::add_static_mesh(IPLStaticMesh mesh) {
	if (!is_global_state_init.load()) {
		static_meshes_to_add.push_back(mesh);
		return;
	}
	wait_for_refl_idle();
	iplStaticMeshAdd(mesh, global_state.scene);
	scene_needs_commit.store(true);
}

void SteamAudioServer::remove_static_mesh(IPLStaticMesh mesh) {
	if (!is_global_state_init.load()) {
		auto it = std::find(static_meshes_to_add.begin(), static_meshes_to_add.end(), mesh);
		if (it != static_meshes_to_add.end()) {
			static_meshes_to_add.erase(it);
		}
		return;
	}
	// Removing a mesh frees its acceleration structure, so this must never overlap a running
	// simulation. Unloading a level used to crash the process here.
	wait_for_refl_idle();
	iplStaticMeshRemove(mesh, global_state.scene);
	scene_needs_commit.store(true);
}

void SteamAudioServer::add_dynamic_mesh(IPLInstancedMesh mesh) {
	if (!is_global_state_init.load()) {
		SteamAudio::log(SteamAudio::log_error, "Adding a dynamic mesh, but SteamAudio is not initialized.");
		return;
	}
	wait_for_refl_idle();
	iplInstancedMeshAdd(mesh, global_state.scene);
	scene_needs_commit.store(true);
}

void SteamAudioServer::remove_dynamic_mesh(IPLInstancedMesh mesh) {
	if (!is_global_state_init.load()) {
		return; // We've probably already deleted the scene.
	}
	wait_for_refl_idle();
	iplInstancedMeshRemove(mesh, global_state.scene);
	// Drop any transform update still queued for a mesh that is going away.
	{
		std::lock_guard<std::mutex> lock(scene_mux);
		pending_dynamic_transforms.erase(
				std::remove_if(pending_dynamic_transforms.begin(), pending_dynamic_transforms.end(),
						[mesh](const std::pair<IPLInstancedMesh, IPLMatrix4x4> &pending) {
							return pending.first == mesh;
						}),
				pending_dynamic_transforms.end());
	}
	scene_needs_commit.store(true);
}

// Queued rather than applied: dynamic geometry moves every physics frame, and waiting for the
// ray tracer that often would stall the game thread. tick() applies the latest transform.
void SteamAudioServer::update_dynamic_mesh_transform(IPLInstancedMesh mesh, const IPLMatrix4x4 &transform) {
	if (!is_global_state_init.load()) {
		return;
	}
	std::lock_guard<std::mutex> lock(scene_mux);
	for (auto &pending : pending_dynamic_transforms) {
		if (pending.first == mesh) {
			pending.second = transform;
			return;
		}
	}
	pending_dynamic_transforms.emplace_back(mesh, transform);
}

// Queued rather than applied: this is reached from whichever thread first mixes a player,
// including the audio thread, which must never block waiting for the ray tracer.
void SteamAudioServer::add_source(IPLSource source) {
	if (!is_global_state_init.load()) {
		return;
	}
	std::lock_guard<std::mutex> lock(scene_mux);
	pending_sources_to_add.push_back(source);
}

void SteamAudioServer::remove_source(IPLSource source) {
	if (!is_global_state_init.load()) {
		return;
	}
	// The caller releases the source right after this, so the removal is committed here rather
	// than deferred: a released source must not still be committed in the simulator.
	wait_for_refl_idle();
	iplSourceRemove(source, global_state.sim);
	iplSimulatorCommit(global_state.sim);
	sim_needs_commit.store(false);
}

SteamAudioServer::SteamAudioServer() {
	self = this;
	is_global_state_init.store(false);
	is_refl_thread_processing.store(false);
	is_running.store(true);
	local_states_have_changed.store(false);
	scene_needs_commit.store(false);
	sim_needs_commit.store(false);
}

SteamAudioServer::~SteamAudioServer() {
	is_running.store(false);
	{
		std::unique_lock<std::mutex> lock(refl_mux);
		cv.notify_one();
	}
	if (refl_thread.is_valid()) {
		refl_thread->wait_to_finish();
		refl_thread.unref();
	}

	if (!self->is_global_state_init.load()) {
		return;
	}
	SteamAudio::log(SteamAudio::log_debug, "destroying steam audio server");

	// Cleared before anything is released: nodes are freed in an order this singleton does not
	// control, and a geometry or player destructor that runs afterwards must not call into a
	// released scene or simulator. Every mutator checks this flag first.
	self->is_global_state_init.store(false);

	iplAmbisonicsDecodeEffectRelease(&self->global_state.ambi_dec_effect);
	iplAmbisonicsEncodeEffectRelease(&self->global_state.ambi_enc_effect);
	iplHRTFRelease(&self->global_state.hrtf);
	iplSimulatorRelease(&self->global_state.sim);
	iplSceneRelease(&self->global_state.scene);
	iplContextRelease(&self->global_state.ctx);
}

bool SteamAudioServer::save_scene_obj(const String &path) {
	if (!self->is_global_state_init.load()) {
		UtilityFunctions::push_error("[godot-steam-audio] No acoustic scene to export; it only exists while the game is running.");
		return false;
	}
	String out = path.is_empty() ? String("user://steam_audio_scene.obj") : path;
	if (!out.to_lower().ends_with(".obj")) {
		out += ".obj";
	}
	String dir = out.get_base_dir();
	if (!dir.is_empty() && !DirAccess::dir_exists_absolute(dir)) {
		DirAccess::make_dir_recursive_absolute(dir);
	}
	// Steam Audio fprintf()s into whatever fopen returns without checking it, so make sure the
	// path is writable before handing it over.
	Ref<FileAccess> probe = FileAccess::open(out, FileAccess::WRITE);
	if (probe.is_null()) {
		UtilityFunctions::push_error("[godot-steam-audio] Cannot write ", out, ".");
		return false;
	}
	probe->close();

	CharString global = ProjectSettings::get_singleton()->globalize_path(out).utf8();
	std::lock_guard<std::mutex> tick_lock(self->tick_mux);
	self->wait_for_refl_idle();
	self->flush_scene_changes();
	iplSceneSaveOBJ(self->global_state.scene, const_cast<char *>(global.get_data()));
	return true;
}

void SteamAudioServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("tick"), &SteamAudioServer::tick);
	ClassDB::bind_method(D_METHOD("save_scene_obj", "path"), &SteamAudioServer::save_scene_obj, DEFVAL("user://steam_audio_scene.obj"));
	ClassDB::bind_static_method("SteamAudioServer", D_METHOD("get_singleton"),
			&SteamAudioServer::get_singleton);
}

SteamAudioServer *SteamAudioServer::get_singleton() {
	return self;
}

SteamAudioServer *SteamAudioServer::self = nullptr;
