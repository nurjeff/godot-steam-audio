#include "config.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "server.hpp"
#include <godot_cpp/variant/utility_functions.hpp>
#include "steam_audio.hpp"

SteamAudio::GodotSteamAudioLogLevel SteamAudioConfig::log_level = SteamAudio::log_info;
float SteamAudioConfig::hrtf_volume = 1.0f;
IPLHRTFNormType SteamAudioConfig::hrtf_normalization = IPL_HRTFNORMTYPE_NONE;
int SteamAudioConfig::max_ambisonics_order = 1;
int SteamAudioConfig::max_num_occ_samples = 64;
int SteamAudioConfig::max_num_refl_rays = 4096;
int SteamAudioConfig::num_diffuse_samples = 32;
float SteamAudioConfig::max_refl_duration = 2.0f;
int SteamAudioConfig::max_num_refl_srcs = 8;
int SteamAudioConfig::num_refl_threads = 2;
// Embree is faster, but Steam Audio 4.8 never detaches a released static mesh from the Embree
// scene and hands its geometry id to the next one, so the first level change leaves the ray
// tracer with a broken scene: no reflections, no occlusion, and usually a crash. Godot games
// change scenes, so the portable tracer is the safe default.
IPLSceneType SteamAudioConfig::scene_type = IPL_SCENETYPE_DEFAULT;
IPLReflectionEffectType SteamAudioConfig::reflection_type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
int SteamAudioConfig::path_vis_samples = 4;

void SteamAudioConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_global_log_level"), &SteamAudioConfig::get_global_log_level);
	ClassDB::bind_method(D_METHOD("set_global_log_level", "p_global_log_level"), &SteamAudioConfig::set_global_log_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "global_log_level", PROPERTY_HINT_ENUM, "Debug,Info,Warning,Error"), "set_global_log_level", "get_global_log_level");

	ADD_GROUP("Performance", "");
	ClassDB::bind_method(D_METHOD("get_hrtf_normalization"), &SteamAudioConfig::get_hrtf_normalization);
	ClassDB::bind_method(D_METHOD("set_hrtf_normalization", "p_hrtf_normalization"), &SteamAudioConfig::set_hrtf_normalization);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "hrtf_normalization", PROPERTY_HINT_ENUM, "None,RMS"), "set_hrtf_normalization", "get_hrtf_normalization");

	ClassDB::bind_method(D_METHOD("get_reflection_type"), &SteamAudioConfig::get_reflection_type);
	ClassDB::bind_method(D_METHOD("set_reflection_type", "p_reflection_type"), &SteamAudioConfig::set_reflection_type);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "reflection_type", PROPERTY_HINT_ENUM, "Convolution,Parametric,Hybrid"), "set_reflection_type", "get_reflection_type");

	ClassDB::bind_method(D_METHOD("get_scene_type"), &SteamAudioConfig::get_scene_type);
	ClassDB::bind_method(D_METHOD("set_scene_type", "p_scene_type"), &SteamAudioConfig::set_scene_type);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "scene_type", PROPERTY_HINT_ENUM, "Default,Embree"), "set_scene_type", "get_scene_type");

	ClassDB::bind_method(D_METHOD("get_hrtf_volume"), &SteamAudioConfig::get_hrtf_volume);
	ClassDB::bind_method(D_METHOD("set_hrtf_volume", "p_hrtf_volume"), &SteamAudioConfig::set_hrtf_volume);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hrtf_volume", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_hrtf_volume", "get_hrtf_volume");

	ClassDB::bind_method(D_METHOD("get_max_refl_duration"), &SteamAudioConfig::get_max_refl_duration);
	ClassDB::bind_method(D_METHOD("set_max_refl_duration", "p_max_refl_duration"), &SteamAudioConfig::set_max_refl_duration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_reflection_duration", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_max_refl_duration", "get_max_refl_duration");

	ClassDB::bind_method(D_METHOD("get_max_ambisonics_order"), &SteamAudioConfig::get_max_ambisonics_order);
	ClassDB::bind_method(D_METHOD("set_max_ambisonics_order", "p_max_ambisonics_order"), &SteamAudioConfig::set_max_ambisonics_order);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_ambisonics_order", PROPERTY_HINT_RANGE, "0,5,1"), "set_max_ambisonics_order", "get_max_ambisonics_order");

	ClassDB::bind_method(D_METHOD("get_path_vis_samples"), &SteamAudioConfig::get_path_vis_samples);
	ClassDB::bind_method(D_METHOD("set_path_vis_samples", "p_path_vis_samples"), &SteamAudioConfig::set_path_vis_samples);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pathing_visibility_samples", PROPERTY_HINT_RANGE, "1,32,1"), "set_path_vis_samples", "get_path_vis_samples");

	ClassDB::bind_method(D_METHOD("get_max_num_occ_samples"), &SteamAudioConfig::get_max_num_occ_samples);
	ClassDB::bind_method(D_METHOD("set_max_num_occ_samples", "p_max_num_occ_samples"), &SteamAudioConfig::set_max_num_occ_samples);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_occlusion_samples", PROPERTY_HINT_RANGE, "1,256,1"), "set_max_num_occ_samples", "get_max_num_occ_samples");

	ClassDB::bind_method(D_METHOD("get_max_num_refl_rays"), &SteamAudioConfig::get_max_num_refl_rays);
	ClassDB::bind_method(D_METHOD("set_max_num_refl_rays", "p_max_num_refl_rays"), &SteamAudioConfig::set_max_num_refl_rays);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_reflection_rays", PROPERTY_HINT_RANGE, "1,8192,1"), "set_max_num_refl_rays", "get_max_num_refl_rays");

	ClassDB::bind_method(D_METHOD("get_num_diffuse_samples"), &SteamAudioConfig::get_num_diffuse_samples);
	ClassDB::bind_method(D_METHOD("set_num_diffuse_samples", "p_num_diffuse_samples"), &SteamAudioConfig::set_num_diffuse_samples);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "diffuse_samples", PROPERTY_HINT_RANGE, "1,256,1"), "set_num_diffuse_samples", "get_num_diffuse_samples");

	ClassDB::bind_method(D_METHOD("get_max_num_refl_srcs"), &SteamAudioConfig::get_max_num_refl_srcs);
	ClassDB::bind_method(D_METHOD("set_max_num_refl_srcs", "p_max_num_refl_srcs"), &SteamAudioConfig::set_max_num_refl_srcs);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_reflection_sources", PROPERTY_HINT_RANGE, "1,512,1"), "set_max_num_refl_srcs", "get_max_num_refl_srcs");

	ClassDB::bind_method(D_METHOD("get_num_refl_threads"), &SteamAudioConfig::get_num_refl_threads);
	ClassDB::bind_method(D_METHOD("set_num_refl_threads", "p_num_refl_threads"), &SteamAudioConfig::set_num_refl_threads);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "reflection_threads", PROPERTY_HINT_RANGE, "1,64,1"), "set_num_refl_threads", "get_num_refl_threads");
}

SteamAudioConfig::SteamAudioConfig() {}
SteamAudioConfig::~SteamAudioConfig() {}

PackedStringArray SteamAudioConfig::_get_configuration_warnings() const {
	PackedStringArray res;
	if (count_nodes_of_class_in_scene(this, "SteamAudioConfig") > 1) {
		res.push_back("More than one SteamAudioConfig in this scene. Only the first to load is used.");
	}
	if (count_nodes_of_class_in_scene(this, "SteamAudioListener") == 0) {
		res.push_back("No SteamAudioListener in this scene. Add one, usually under the Camera3D.");
	}
	if (scene_type == IPL_SCENETYPE_EMBREE) {
		res.push_back("Embree traces faster, but Steam Audio leaves stale geometry behind when acoustic "
					  "geometry is freed. Only use it if the acoustic scene is built once and never torn down.");
	}
	return res;
}

void SteamAudioConfig::ready_internal() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	// These properties are read once, when the simulator is created. If something else already
	// forced that (a SteamAudioGeometry entering the tree first, or an earlier scene), this
	// node's settings are silently ignored, which is worth saying out loud.
	if (SteamAudioServer::get_singleton()->get_global_state(false) != nullptr) {
		UtilityFunctions::push_warning(
				"SteamAudioConfig entered the tree after Steam Audio was already initialized, so its "
				"settings have not been applied. Add the config before any geometry, and keep it for "
				"the lifetime of the process.");
	}

	// Initialize global state
	SteamAudioServer::get_singleton()->get_global_state();
	set_physics_process(true);
}

void SteamAudioConfig::process_internal(double delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	SteamAudioServer::get_singleton()->tick();
}

void SteamAudioConfig::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
			ready_internal();
			break;
		case NOTIFICATION_PHYSICS_PROCESS:
			process_internal(get_physics_process_delta_time());
			break;
	}
}

SteamAudio::GodotSteamAudioLogLevel SteamAudioConfig::get_global_log_level() { return log_level; }
void SteamAudioConfig::set_global_log_level(SteamAudio::GodotSteamAudioLogLevel p_global_log_level) { log_level = p_global_log_level; }

float SteamAudioConfig::get_hrtf_volume() { return hrtf_volume; }
void SteamAudioConfig::set_hrtf_volume(float p_hrtf_volume) { hrtf_volume = p_hrtf_volume; }

int SteamAudioConfig::get_max_ambisonics_order() { return max_ambisonics_order; }
void SteamAudioConfig::set_max_ambisonics_order(int p_max_ambisonics_order) { max_ambisonics_order = p_max_ambisonics_order; }

IPLHRTFNormType SteamAudioConfig::get_hrtf_normalization() { return hrtf_normalization; }
void SteamAudioConfig::set_hrtf_normalization(IPLHRTFNormType p_hrtf_normalization) { hrtf_normalization = p_hrtf_normalization; }

IPLReflectionEffectType SteamAudioConfig::get_reflection_type() { return reflection_type; }
void SteamAudioConfig::set_reflection_type(IPLReflectionEffectType p_reflection_type) { reflection_type = p_reflection_type; }

IPLSceneType SteamAudioConfig::get_scene_type() { return scene_type; }
void SteamAudioConfig::set_scene_type(IPLSceneType p_scene_type) { scene_type = p_scene_type; }

int SteamAudioConfig::get_num_refl_threads() { return num_refl_threads; }
void SteamAudioConfig::set_num_refl_threads(int p_num_refl_threads) { num_refl_threads = p_num_refl_threads; }

int SteamAudioConfig::get_max_num_refl_srcs() { return max_num_refl_srcs; }
void SteamAudioConfig::set_max_num_refl_srcs(int p_max_num_refl_srcs) { max_num_refl_srcs = p_max_num_refl_srcs; }

float SteamAudioConfig::get_max_refl_duration() { return max_refl_duration; }
void SteamAudioConfig::set_max_refl_duration(float p_max_refl_duration) { max_refl_duration = p_max_refl_duration; }

int SteamAudioConfig::get_num_diffuse_samples() { return num_diffuse_samples; }
void SteamAudioConfig::set_num_diffuse_samples(int p_num_diffuse_samples) { num_diffuse_samples = p_num_diffuse_samples; }

int SteamAudioConfig::get_max_num_refl_rays() { return max_num_refl_rays; }
void SteamAudioConfig::set_max_num_refl_rays(int p_max_num_refl_rays) { max_num_refl_rays = p_max_num_refl_rays; }

int SteamAudioConfig::get_max_num_occ_samples() { return max_num_occ_samples; }
void SteamAudioConfig::set_max_num_occ_samples(int p_max_num_occ_samples) { max_num_occ_samples = p_max_num_occ_samples; }
int SteamAudioConfig::get_path_vis_samples() { return path_vis_samples; }
void SteamAudioConfig::set_path_vis_samples(int p_path_vis_samples) { path_vis_samples = p_path_vis_samples; }
