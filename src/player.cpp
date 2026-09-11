#include "player.hpp"
#include "config.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/core/object.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include "server.hpp"
#include "server_init.hpp"
#include "steam_audio.hpp"
#include <algorithm>
#include <cmath>
#include "stream.hpp"

void SteamAudioPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("play_stream", "stream", "from_offset", "volume_db", "pitch_scale"), &SteamAudioPlayer::play_stream, DEFVAL(0), DEFVAL(0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("get_inner_stream"), &SteamAudioPlayer::get_inner_stream);
	ClassDB::bind_method(D_METHOD("get_inner_stream_playback"), &SteamAudioPlayer::get_inner_stream_playback);

	ClassDB::bind_method(D_METHOD("is_dist_attn_on"), &SteamAudioPlayer::is_dist_attn_on);
	ClassDB::bind_method(D_METHOD("set_dist_attn_on", "p_dist_attn_on"), &SteamAudioPlayer::set_dist_attn_on);
	ClassDB::bind_method(D_METHOD("get_min_attenuation_distance"), &SteamAudioPlayer::get_min_attenuation_dist);
	ClassDB::bind_method(D_METHOD("set_min_attenuation_distance", "p_min_attenuation_distance"), &SteamAudioPlayer::set_min_attenuation_dist);
	ClassDB::bind_method(D_METHOD("set_max_reflection_distance", "p_max_reflection_distance"), &SteamAudioPlayer::set_max_reflection_dist);
	ClassDB::bind_method(D_METHOD("get_max_reflection_distance"), &SteamAudioPlayer::get_max_reflection_dist);
	ClassDB::bind_method(D_METHOD("is_air_absorp_on"), &SteamAudioPlayer::is_air_absorp_on);
	ClassDB::bind_method(D_METHOD("set_air_absorp_on", "p_air_absorp_on"), &SteamAudioPlayer::set_air_absorp_on);
	ClassDB::bind_method(D_METHOD("set_air_absorption_low", "p_air_absorption_low"), &SteamAudioPlayer::set_air_absorption_low);
	ClassDB::bind_method(D_METHOD("get_air_absorption_low"), &SteamAudioPlayer::get_air_absorption_low);
	ClassDB::bind_method(D_METHOD("set_air_absorption_mid", "p_air_absorption_mid"), &SteamAudioPlayer::set_air_absorption_mid);
	ClassDB::bind_method(D_METHOD("get_air_absorption_mid"), &SteamAudioPlayer::get_air_absorption_mid);
	ClassDB::bind_method(D_METHOD("set_air_absorption_high", "p_air_absorption_high"), &SteamAudioPlayer::set_air_absorption_high);
	ClassDB::bind_method(D_METHOD("get_air_absorption_high"), &SteamAudioPlayer::get_air_absorption_high);
	ClassDB::bind_method(D_METHOD("get_air_absorption_model_type"), &SteamAudioPlayer::get_air_absorption_model_type);
	ClassDB::bind_method(D_METHOD("set_air_absorption_model_type", "p_air_absorption_model_type"), &SteamAudioPlayer::set_air_absorption_model_type);
	ClassDB::bind_method(D_METHOD("is_occlusion_on"), &SteamAudioPlayer::is_occlusion_on);
	ClassDB::bind_method(D_METHOD("set_occlusion_on", "p_occlusion_on"), &SteamAudioPlayer::set_occlusion_on);
	ClassDB::bind_method(D_METHOD("is_reflection_on"), &SteamAudioPlayer::is_reflection_on);
	ClassDB::bind_method(D_METHOD("set_reflection_on", "p_reflection_on"), &SteamAudioPlayer::set_reflection_on);
	ClassDB::bind_method(D_METHOD("is_directivity_on"), &SteamAudioPlayer::is_directivity_on);
	ClassDB::bind_method(D_METHOD("set_directivity_on", "p_directivity_on"), &SteamAudioPlayer::set_directivity_on);
	ClassDB::bind_method(D_METHOD("get_dipole_weight"), &SteamAudioPlayer::get_dipole_weight);
	ClassDB::bind_method(D_METHOD("set_dipole_weight", "p_dipole_weight"), &SteamAudioPlayer::set_dipole_weight);
	ClassDB::bind_method(D_METHOD("get_dipole_power"), &SteamAudioPlayer::get_dipole_power);
	ClassDB::bind_method(D_METHOD("set_dipole_power", "p_dipole_power"), &SteamAudioPlayer::set_dipole_power);
	ClassDB::bind_method(D_METHOD("get_occlusion_radius"), &SteamAudioPlayer::get_occlusion_radius);
	ClassDB::bind_method(D_METHOD("set_occlusion_radius", "p_occlusion_radius"), &SteamAudioPlayer::set_occlusion_radius);
	ClassDB::bind_method(D_METHOD("get_occlusion_samples"), &SteamAudioPlayer::get_occlusion_samples);
	ClassDB::bind_method(D_METHOD("set_occlusion_samples", "p_occlusion_samples"), &SteamAudioPlayer::set_occlusion_samples);
	ClassDB::bind_method(D_METHOD("get_transmission_rays"), &SteamAudioPlayer::get_transmission_rays);
	ClassDB::bind_method(D_METHOD("set_transmission_rays", "p_transmission_rays"), &SteamAudioPlayer::set_transmission_rays);
	ClassDB::bind_method(D_METHOD("get_ambisonics_order"), &SteamAudioPlayer::get_ambisonics_order);
	ClassDB::bind_method(D_METHOD("set_ambisonics_order", "p_ambisonics_order"), &SteamAudioPlayer::set_ambisonics_order);
	ClassDB::bind_method(D_METHOD("is_ambisonics_on"), &SteamAudioPlayer::is_ambisonics_on);
	ClassDB::bind_method(D_METHOD("set_ambisonics_on", "p_ambisonics_on"), &SteamAudioPlayer::set_ambisonics_on);
	ClassDB::bind_method(D_METHOD("is_pathing_on"), &SteamAudioPlayer::is_pathing_on);
	ClassDB::bind_method(D_METHOD("set_pathing_on", "p_pathing_on"), &SteamAudioPlayer::set_pathing_on);
	ClassDB::bind_method(D_METHOD("get_pathing_order"), &SteamAudioPlayer::get_pathing_order);
	ClassDB::bind_method(D_METHOD("set_pathing_order", "p_pathing_order"), &SteamAudioPlayer::set_pathing_order);
	ClassDB::bind_method(D_METHOD("is_path_validation_on"), &SteamAudioPlayer::is_path_validation_on);
	ClassDB::bind_method(D_METHOD("set_path_validation_on", "p_on"), &SteamAudioPlayer::set_path_validation_on);
	ClassDB::bind_method(D_METHOD("get_path_vis_radius"), &SteamAudioPlayer::get_path_vis_radius);
	ClassDB::bind_method(D_METHOD("set_path_vis_radius", "p_v"), &SteamAudioPlayer::set_path_vis_radius);
	ClassDB::bind_method(D_METHOD("get_path_vis_threshold"), &SteamAudioPlayer::get_path_vis_threshold);
	ClassDB::bind_method(D_METHOD("set_path_vis_threshold", "p_v"), &SteamAudioPlayer::set_path_vis_threshold);
	ClassDB::bind_method(D_METHOD("get_path_vis_range"), &SteamAudioPlayer::get_path_vis_range);
	ClassDB::bind_method(D_METHOD("set_path_vis_range", "p_v"), &SteamAudioPlayer::set_path_vis_range);
	ClassDB::bind_method(D_METHOD("is_path_active"), &SteamAudioPlayer::is_path_active);
	ClassDB::bind_method(D_METHOD("get_path_level"), &SteamAudioPlayer::get_path_level);
	ClassDB::bind_method(D_METHOD("is_baked_reverb_on"), &SteamAudioPlayer::is_baked_reverb_on);
	ClassDB::bind_method(D_METHOD("set_baked_reverb_on", "p_on"), &SteamAudioPlayer::set_baked_reverb_on);

	ADD_GROUP("Distance Attenuation", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "distance_attenuation"), "set_dist_attn_on", "is_dist_attn_on");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_attenuation_distance", PROPERTY_HINT_RANGE, "0.0,100.0,0.1"), "set_min_attenuation_distance", "get_min_attenuation_distance");

	ADD_GROUP("Air Absorption", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "air_absorption"), "set_air_absorp_on", "is_air_absorp_on");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "air_absorption_low", PROPERTY_HINT_RANGE, "0.0,1.0,0.1"), "set_air_absorption_low", "get_air_absorption_low");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "air_absorption_mid", PROPERTY_HINT_RANGE, "0.0,1.0,0.1"), "set_air_absorption_mid", "get_air_absorption_mid");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "air_absorption_high", PROPERTY_HINT_RANGE, "0.0,1.0,0.1"), "set_air_absorption_high", "get_air_absorption_high");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "air_absorption_model", PROPERTY_HINT_ENUM, "Default,Exponential"), "set_air_absorption_model_type", "get_air_absorption_model_type");

	ADD_GROUP("Occlusion and Transmission", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "occlusion"), "set_occlusion_on", "is_occlusion_on");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "occlusion_radius", PROPERTY_HINT_RANGE, "0.0,20.0,0.1"), "set_occlusion_radius", "get_occlusion_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "occlusion_samples", PROPERTY_HINT_RANGE, "0,512,1"), "set_occlusion_samples", "get_occlusion_samples");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "transmission_rays", PROPERTY_HINT_RANGE, "0,512,1"), "set_transmission_rays", "get_transmission_rays");
	ClassDB::bind_method(D_METHOD("get_transmission_type"), &SteamAudioPlayer::get_transmission_type);
	ClassDB::bind_method(D_METHOD("set_transmission_type", "p_transmission_type"), &SteamAudioPlayer::set_transmission_type);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "transmission_type", PROPERTY_HINT_ENUM, "Frequency-Independent,Frequency-Dependent"), "set_transmission_type", "get_transmission_type");

	ADD_GROUP("Ambisonics", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ambisonics"), "set_ambisonics_on", "is_ambisonics_on");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ambisonics_order", PROPERTY_HINT_RANGE, "0,5,1"), "set_ambisonics_order", "get_ambisonics_order");

	ADD_GROUP("Reflection", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reflection"), "set_reflection_on", "is_reflection_on");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_reflection_distance", PROPERTY_HINT_RANGE, "0.0,20000.0,0.1"), "set_max_reflection_distance", "get_max_reflection_distance");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "baked_reverb"), "set_baked_reverb_on", "is_baked_reverb_on");

	ADD_GROUP("Pathing", "pathing_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pathing"), "set_pathing_on", "is_pathing_on");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pathing_ambisonics_order", PROPERTY_HINT_RANGE, "0,3,1"), "set_pathing_order", "get_pathing_order");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pathing_validation"), "set_path_validation_on", "is_path_validation_on");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pathing_visibility_radius", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_path_vis_radius", "get_path_vis_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pathing_visibility_threshold", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_path_vis_threshold", "get_path_vis_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pathing_visibility_range", PROPERTY_HINT_RANGE, "1.0,500.0,1.0,or_greater"), "set_path_vis_range", "get_path_vis_range");

	ADD_GROUP("Directivity", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "directivity"), "set_directivity_on", "is_directivity_on");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dipole_weight", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_dipole_weight", "get_dipole_weight");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dipole_power", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_dipole_power", "get_dipole_power");
}

SteamAudioPlayer::SteamAudioPlayer() {
	is_local_state_init.store(false);
	can_load_local_state.store(true);

	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	auto str = dynamic_cast<SteamAudioStream *>(get_stream().ptr());
	if (str == nullptr) {
		Ref<SteamAudioStream> new_stream;
		new_stream.instantiate();
		new_stream->parent = this;
		if (get_stream().ptr() != nullptr) {
			new_stream->set_stream(get_stream());
		}
		this->set_stream(new_stream);
	}
}
SteamAudioPlayer::~SteamAudioPlayer() {
	SteamAudio::log(SteamAudio::log_debug, "destroying player");
	std::unique_lock lock(local_state.mux);

	// Unregister unconditionally and before anything else. The local state is registered by
	// whichever thread first mixes this player, which can happen while this destructor runs,
	// so gating removal on is_local_state_init could leave a freed player in the server's
	// list. tick() then dereferenced it and crashed the process.
	can_load_local_state.store(false);
	SteamAudioServer::get_singleton()->remove_local_state(&local_state);
	if (!is_local_state_init.load()) {
		return;
	}
	is_local_state_init.store(false);
	auto gs = SteamAudioServer::get_singleton()->get_global_state();

	// Removes and commits with the reflection simulation parked, so the source is no longer
	// part of any simulation before it is released.
	SteamAudioServer::get_singleton()->remove_source(local_state.src.src);
	iplSourceRelease(&local_state.src.src);
	iplDirectEffectRelease(&local_state.fx.direct);
	iplReflectionEffectRelease(&local_state.fx.refl);
	iplAmbisonicsDecodeEffectRelease(&local_state.fx.dec);
	iplAmbisonicsDecodeEffectRelease(&local_state.fx.refl_dec);
	iplAmbisonicsEncodeEffectRelease(&local_state.fx.enc);

	iplAudioBufferFree(gs->ctx, &local_state.bufs.in);
	iplAudioBufferFree(gs->ctx, &local_state.bufs.direct);
	iplAudioBufferFree(gs->ctx, &local_state.bufs.ambi);
	iplAudioBufferFree(gs->ctx, &local_state.bufs.out);
	iplAudioBufferFree(gs->ctx, &local_state.bufs.mono);
	iplAudioBufferFree(gs->ctx, &local_state.bufs.refl_ambi);
	iplAudioBufferFree(gs->ctx, &local_state.bufs.refl_out);

	if (!pb.is_null()) {
		auto playback = dynamic_cast<SteamAudioStreamPlayback *>(pb.ptr());
		playback->parent = nullptr;
	}
}

LocalSteamAudioState *SteamAudioPlayer::get_local_state() {
	if (!can_load_local_state.load()) {
		return nullptr;
	}
	if (!is_local_state_init.load()) {
		// Taken because the destructor holds it for the whole teardown: either the state is
		// initialized before this player starts going away, or this sees it is too late.
		std::unique_lock lock(local_state.mux);
		if (!can_load_local_state.load()) {
			return nullptr;
		}
		if (!is_local_state_init.load()) {
			init_local_state();
		}
	}
	return &local_state;
}

void SteamAudioPlayer::init_local_state() {
	SteamAudio::log(SteamAudio::log_debug, "init local state");
	auto gs = SteamAudioServer::get_singleton()->get_global_state();
	local_state.cfg = cfg;

	IPLSourceSettings src_cfg{};
	src_cfg.flags = static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS | IPL_SIMULATIONFLAGS_PATHING);
	// Registering a source changes the simulator, which Steam Audio forbids while a simulation
	// is running. The server queues it and commits in its own safe window: this can run on the
	// audio thread, which must never block on the ray tracer.
	handleErr(iplSourceCreate(gs->sim, &src_cfg, &local_state.src.src));
	SteamAudioServer::get_singleton()->add_source(local_state.src.src);

	// TODO: check if we can't create effects globally and use their Reset functions.
	// If we create these globally and use them for all sources, then strange things happen
	// (e.g. one source may start to play audio from all sources and positioning gets screwed)
	IPLDirectEffectSettings dir_effect_cfg;
	dir_effect_cfg.numChannels = 2;
	handleErr(iplDirectEffectCreate(gs->ctx, &gs->audio_cfg, &dir_effect_cfg, &local_state.fx.direct));

	IPLReflectionEffectSettings refl_effect_cfg{};
	refl_effect_cfg.type = SteamAudioConfig::reflection_type;
	refl_effect_cfg.irSize = int(SteamAudioConfig::max_refl_duration * float(gs->audio_cfg.samplingRate));
	refl_effect_cfg.numChannels = ambisonic_channels_from(SteamAudioConfig::max_ambisonics_order);
	handleErr(iplReflectionEffectCreate(gs->ctx, &gs->audio_cfg, &refl_effect_cfg, &local_state.fx.refl));

	IPLPathEffectSettings path_effect_cfg{};
	path_effect_cfg.maxOrder = SteamAudioConfig::max_ambisonics_order;
	path_effect_cfg.spatialize = IPL_FALSE;
	handleErr(iplPathEffectCreate(gs->ctx, &gs->audio_cfg, &path_effect_cfg, &local_state.fx.path));
	local_state.fx.path_dec = create_ambisonics_decode_effect(gs->ctx, gs->audio_cfg, gs->hrtf);
	local_state.path_sh.assign(ambisonic_channels_from(SteamAudioConfig::max_ambisonics_order), 0.0f);

	local_state.fx.dec = create_ambisonics_decode_effect(
			gs->ctx, gs->audio_cfg, gs->hrtf);
	local_state.fx.refl_dec = create_ambisonics_decode_effect(
			gs->ctx, gs->audio_cfg, gs->hrtf);
	local_state.fx.enc = create_ambisonics_encode_effect(
			gs->ctx, gs->audio_cfg);

	handleErr(iplAudioBufferAllocate(gs->ctx, 2, gs->audio_cfg.frameSize, &local_state.bufs.in));
	handleErr(iplAudioBufferAllocate(gs->ctx, 2, gs->audio_cfg.frameSize, &local_state.bufs.direct));
	handleErr(iplAudioBufferAllocate(gs->ctx, ambisonic_channels_from(SteamAudioConfig::max_ambisonics_order), gs->audio_cfg.frameSize, &local_state.bufs.ambi));
	handleErr(iplAudioBufferAllocate(gs->ctx, 2, gs->audio_cfg.frameSize, &local_state.bufs.out));
	handleErr(iplAudioBufferAllocate(gs->ctx, 1, gs->audio_cfg.frameSize, &local_state.bufs.mono));
	handleErr(iplAudioBufferAllocate(gs->ctx, ambisonic_channels_from(SteamAudioConfig::max_ambisonics_order), gs->audio_cfg.frameSize, &local_state.bufs.refl_ambi));
	handleErr(iplAudioBufferAllocate(gs->ctx, 2, gs->audio_cfg.frameSize, &local_state.bufs.refl_out));
	handleErr(iplAudioBufferAllocate(gs->ctx, ambisonic_channels_from(SteamAudioConfig::max_ambisonics_order), gs->audio_cfg.frameSize, &local_state.bufs.path_ambi));
	handleErr(iplAudioBufferAllocate(gs->ctx, 2, gs->audio_cfg.frameSize, &local_state.bufs.path_out));
	local_state.src.player = this;

	SteamAudio::log(SteamAudio::log_debug, "init local state done");

	SteamAudioServer::get_singleton()->add_local_state(&this->local_state);
	is_local_state_init.store(true);
}

void SteamAudioPlayer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
			ready_internal();
			break;
		case NOTIFICATION_EXIT_TREE:
			// Unconditional: see the destructor. Removal is idempotent.
			if (!Engine::get_singleton()->is_editor_hint()) {
				SteamAudioServer::get_singleton()->remove_local_state(&local_state);
			}
			break;
		case NOTIFICATION_PROCESS:
			process_internal(get_process_delta_time());
			break;
	}
}

void SteamAudioPlayer::ready_internal() {
	set_process(true);

	set_panning_strength(0.0f);
	if (cfg.is_dist_attn_on) {
		set_attenuation_model(ATTENUATION_DISABLED);
	}
	// Godot dampens 3D audio above 5 kHz by default, which sits on top of everything Steam
	// Audio renders and just sounds muffled. Air absorption is Steam Audio's job.
	set_attenuation_filter_cutoff_hz(20500.0f);

	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	auto str = dynamic_cast<SteamAudioStream *>(get_stream().ptr());
	if (str == nullptr) {
		if (is_autoplay_enabled()) {
			stop();
		}
		Ref<SteamAudioStream> new_stream;
		new_stream.instantiate();
		if (get_stream().ptr() != nullptr) {
			new_stream->set_stream(get_stream());
		}
		set_stream(new_stream);
		str = new_stream.ptr();
		str->parent = this;
		if (is_autoplay_enabled()) {
			play();
		}
	}

	if (cfg.ambisonics_order > SteamAudioConfig::max_ambisonics_order) {
		cfg.ambisonics_order = SteamAudioConfig::max_ambisonics_order;
	}
	if (cfg.occ_samples > SteamAudioConfig::max_num_occ_samples) {
		cfg.occ_samples = SteamAudioConfig::max_num_occ_samples;
	}
}

void SteamAudioPlayer::process_internal(double delta) {
	if (get_panning_strength() > 0.0f) {
		if (!has_warned_panning) {
			UtilityFunctions::push_warning("Panning strength is always zero on SteamAudioPlayer. You can control panning by enabling or disabling ambisonics.");
			has_warned_panning = true;
		}
		set_panning_strength(0.0f);
	}
	if (cfg.is_dist_attn_on && get_attenuation_model() != ATTENUATION_DISABLED) {
		if (!has_warned_attenuation) {
			UtilityFunctions::push_warning("You cannot enable Godot's and SteamAudio's distance attenuation features at the same time. Disable SteamAudio's attenuation before adjusting Godot's.");
			has_warned_attenuation = true;
		}
		set_attenuation_model(ATTENUATION_DISABLED);
	}

	if (is_playing() && !get_stream_playback().is_null()) {
		pb = get_stream_playback();
	}

	// Sync cfg changes (e.g. runtime property toggles) into local_state.cfg.
	// local_state.cfg is a copy made at init time, so setters must be reflected here.
	if (is_local_state_init.load() && cfg_dirty.exchange(false)) {
		std::unique_lock lock(local_state.mux);
		local_state.cfg = cfg;
	}
}

void SteamAudioPlayer::play_stream(const Ref<AudioStream> &p_stream, float p_from_offset, float p_volume_db, float p_pitch_scale) {
	if (p_stream.is_null()) {
		SteamAudio::log(SteamAudio::log_warn, "Tried to play a null stream, won't play anything.");
		return;
	}

	if (this->is_playing()) {
		this->stop();
	}
	// The volume and pitch arguments used to be accepted and then dropped on the floor.
	this->set_volume_db(p_volume_db);
	this->set_pitch_scale(p_pitch_scale);
	this->play();

	auto str = dynamic_cast<SteamAudioStream *>(get_stream().ptr());
	if (str == nullptr) {
		SteamAudio::log(SteamAudio::log_warn,
						"Tried to get an inner stream from a SteamAudioPlayer, but its outer stream is not a SteamAudioStream. Returning null.");
		return;
	}
	str->set_stream(p_stream);

	auto playback_ptr = dynamic_cast<SteamAudioStreamPlayback *>(get_stream_playback().ptr());
	if (playback_ptr == nullptr) {
		SteamAudio::log(SteamAudio::log_warn,
						"Tried to play a new stream on SteamAudioPlayer, but this player's outer stream was not a SteamAudioStream. Will not play anything.");
		this->stop();
		return;
	}

	playback_ptr->play_stream(p_stream, p_from_offset, p_volume_db, p_pitch_scale);
}

Ref<AudioStream> SteamAudioPlayer::get_inner_stream() {
	auto str = dynamic_cast<SteamAudioStream *>(get_stream().ptr());
	if (str == nullptr) {
		SteamAudio::log(SteamAudio::log_warn,
						"Tried to get an inner stream from a SteamAudioPlayer, but its outer stream is not a SteamAudioStream. Returning null.");
		Ref<AudioStream> null_str;
		return null_str;
	}

	return str->get_stream();
}

Ref<AudioStreamPlayback> SteamAudioPlayer::get_inner_stream_playback() {
	auto spb = dynamic_cast<SteamAudioStreamPlayback *>(get_stream_playback().ptr());
	if (spb == nullptr) {
		SteamAudio::log(SteamAudio::log_warn,
						"Tried to get an inner stream playback from a SteamAudioPlayer, but its outer stream playback is not a SteamAudioStreamPlayback (or the player may not be playing audio). Returning null.");
		Ref<AudioStreamPlayback> null_pb;
		return null_pb;
	}

	return spb->get_stream_playback();
}

float SteamAudioPlayer::get_occlusion_radius() { return cfg.occ_radius; }
void SteamAudioPlayer::set_occlusion_radius(float p_occlusion_radius) { cfg.occ_radius = p_occlusion_radius; cfg_dirty.store(true); }
int SteamAudioPlayer::get_occlusion_samples() { return cfg.occ_samples; }
void SteamAudioPlayer::set_occlusion_samples(int p_occlusion_samples) { cfg.occ_samples = p_occlusion_samples; cfg_dirty.store(true); }
int SteamAudioPlayer::get_transmission_rays() { return cfg.transm_rays; }
void SteamAudioPlayer::set_transmission_rays(int p_transmission_rays) { cfg.transm_rays = p_transmission_rays; cfg_dirty.store(true); }
float SteamAudioPlayer::get_min_attenuation_dist() { return cfg.min_attn_dist; }
void SteamAudioPlayer::set_min_attenuation_dist(float p_min_attenuation_dist) { cfg.min_attn_dist = p_min_attenuation_dist; cfg_dirty.store(true); }
int SteamAudioPlayer::get_ambisonics_order() { return cfg.ambisonics_order; }
void SteamAudioPlayer::set_ambisonics_order(int p_ambisonics_order) {
	cfg.ambisonics_order = std::min(p_ambisonics_order, SteamAudioConfig::max_ambisonics_order);
	cfg_dirty.store(true);
}
float SteamAudioPlayer::get_max_reflection_dist() { return cfg.max_refl_dist; }
void SteamAudioPlayer::set_max_reflection_dist(float p_max_reflection_dist) { cfg.max_refl_dist = p_max_reflection_dist; cfg_dirty.store(true); }

bool SteamAudioPlayer::is_dist_attn_on() { return cfg.is_dist_attn_on; }
void SteamAudioPlayer::set_dist_attn_on(bool p_dist_attn_on) { cfg.is_dist_attn_on = p_dist_attn_on; cfg_dirty.store(true); }

bool SteamAudioPlayer::is_air_absorp_on() { return cfg.is_air_absorp_on; }
void SteamAudioPlayer::set_air_absorp_on(bool p_air_absorp_on) { cfg.is_air_absorp_on = p_air_absorp_on; cfg_dirty.store(true); }
float SteamAudioPlayer::get_air_absorption_low() { return cfg.air_absorption_low; }
void SteamAudioPlayer::set_air_absorption_low(float p_air_absorption_low) { cfg.air_absorption_low = p_air_absorption_low; cfg_dirty.store(true); }
float SteamAudioPlayer::get_air_absorption_mid() { return cfg.air_absorption_mid; }
void SteamAudioPlayer::set_air_absorption_mid(float p_air_absorption_mid) { cfg.air_absorption_mid = p_air_absorption_mid; cfg_dirty.store(true); }
float SteamAudioPlayer::get_air_absorption_high() { return cfg.air_absorption_high; }
void SteamAudioPlayer::set_air_absorption_high(float p_air_absorption_high) { cfg.air_absorption_high = p_air_absorption_high; cfg_dirty.store(true); }
IPLAirAbsorptionModelType SteamAudioPlayer::get_air_absorption_model_type() { return cfg.air_absorption_model_type; }
void SteamAudioPlayer::set_air_absorption_model_type(IPLAirAbsorptionModelType p_air_absorption_model_type) { cfg.air_absorption_model_type = p_air_absorption_model_type; cfg_dirty.store(true); }

bool SteamAudioPlayer::is_reflection_on() { return cfg.is_reflection_on; }
void SteamAudioPlayer::set_reflection_on(bool p_reflection_on) { cfg.is_reflection_on = p_reflection_on; cfg_dirty.store(true); }
bool SteamAudioPlayer::is_occlusion_on() { return cfg.is_occlusion_on; }
void SteamAudioPlayer::set_occlusion_on(bool p_occlusion_on) { cfg.is_occlusion_on = p_occlusion_on; cfg_dirty.store(true); }

bool SteamAudioPlayer::is_pathing_on() { return cfg.is_pathing_on; }
void SteamAudioPlayer::set_pathing_on(bool p_pathing_on) { cfg.is_pathing_on = p_pathing_on; cfg_dirty.store(true); }
int SteamAudioPlayer::get_pathing_order() { return cfg.pathing_order; }
void SteamAudioPlayer::set_pathing_order(int p_pathing_order) {
	cfg.pathing_order = std::clamp(p_pathing_order, 0, SteamAudioConfig::max_ambisonics_order);
	cfg_dirty.store(true);
}
bool SteamAudioPlayer::is_path_validation_on() { return cfg.path_validation; }
void SteamAudioPlayer::set_path_validation_on(bool p_on) { cfg.path_validation = p_on; cfg_dirty.store(true); }
float SteamAudioPlayer::get_path_vis_radius() { return cfg.path_vis_radius; }
void SteamAudioPlayer::set_path_vis_radius(float p_v) { cfg.path_vis_radius = p_v; cfg_dirty.store(true); }
float SteamAudioPlayer::get_path_vis_threshold() { return cfg.path_vis_threshold; }
void SteamAudioPlayer::set_path_vis_threshold(float p_v) { cfg.path_vis_threshold = p_v; cfg_dirty.store(true); }
float SteamAudioPlayer::get_path_vis_range() { return cfg.path_vis_range; }
void SteamAudioPlayer::set_path_vis_range(float p_v) { cfg.path_vis_range = p_v; cfg_dirty.store(true); }
bool SteamAudioPlayer::is_path_active() {
	return is_local_state_init.load() && local_state.path_active.load();
}

float SteamAudioPlayer::get_path_level() {
	if (!is_local_state_init.load()) {
		return 0.0f;
	}
	std::lock_guard<std::mutex> lock(local_state.path_mux);
	float sum = 0.0f;
	for (float coefficient : local_state.path_sh) {
		sum += coefficient * coefficient;
	}
	return std::sqrt(sum);
}

bool SteamAudioPlayer::is_baked_reverb_on() { return cfg.is_baked_reverb_on; }
void SteamAudioPlayer::set_baked_reverb_on(bool p_on) { cfg.is_baked_reverb_on = p_on; cfg_dirty.store(true); }

IPLTransmissionType SteamAudioPlayer::get_transmission_type() { return cfg.transmission_type; }
void SteamAudioPlayer::set_transmission_type(IPLTransmissionType p_transmission_type) { cfg.transmission_type = p_transmission_type; cfg_dirty.store(true); }

bool SteamAudioPlayer::is_directivity_on() { return cfg.is_directivity_on; }
void SteamAudioPlayer::set_directivity_on(bool p_directivity_on) { cfg.is_directivity_on = p_directivity_on; cfg_dirty.store(true); }
float SteamAudioPlayer::get_dipole_weight() { return cfg.dipole_weight; }
void SteamAudioPlayer::set_dipole_weight(float p_dipole_weight) { cfg.dipole_weight = p_dipole_weight; cfg_dirty.store(true); }
float SteamAudioPlayer::get_dipole_power() { return cfg.dipole_power; }
void SteamAudioPlayer::set_dipole_power(float p_dipole_power) { cfg.dipole_power = p_dipole_power; cfg_dirty.store(true); }

bool SteamAudioPlayer::is_ambisonics_on() { return cfg.is_ambisonics_on; }
void SteamAudioPlayer::set_ambisonics_on(bool p_ambisonics_on) { cfg.is_ambisonics_on = p_ambisonics_on; cfg_dirty.store(true); }

PackedStringArray SteamAudioPlayer::_get_configuration_warnings() const {
	PackedStringArray res;

	if (count_nodes_of_class_in_scene(this, "SteamAudioConfig") == 0) {
		res.push_back("No SteamAudioConfig in this scene. Steam Audio will not run without exactly one.");
	}
	if (count_nodes_of_class_in_scene(this, "SteamAudioListener") == 0) {
		res.push_back("No SteamAudioListener in this scene. Add one, usually under the Camera3D.");
	}
	if (cfg.ambisonics_order > SteamAudioConfig::max_ambisonics_order) {
		res.push_back("Ambisonics order exceeds the maximum set in SteamAudioConfig, and will be clamped at runtime.");
	}
	if (cfg.occ_samples > SteamAudioConfig::max_num_occ_samples) {
		res.push_back("Occlusion samples exceed the maximum set in SteamAudioConfig, and will be clamped at runtime.");
	}
	if (cfg.is_dist_attn_on && get_attenuation_model() != ATTENUATION_DISABLED) {
		res.push_back("Steam Audio distance attenuation is on, so Godot's attenuation model is ignored and will be set to Disabled.");
	}
	if (get_panning_strength() > 0.0f) {
		res.push_back("Panning strength is ignored on a SteamAudioPlayer; use the ambisonics settings instead.");
	}
	if (get_attenuation_filter_cutoff_hz() < 20500.0f) {
		res.push_back("Godot's attenuation filter dampens everything above its cutoff. Steam Audio models air absorption itself, so this is set to 20500 at runtime.");
	}

	return res;
}
