#ifndef STEAM_AUDIO_H
#define STEAM_AUDIO_H

#include "godot_cpp/variant/transform3d.hpp"
#include <phonon.h>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <atomic>
#include <shared_mutex>

using namespace godot;

VARIANT_ENUM_CAST(IPLAirAbsorptionModelType);
VARIANT_ENUM_CAST(IPLTransmissionType);
VARIANT_ENUM_CAST(IPLReflectionEffectType);
VARIANT_ENUM_CAST(IPLHRTFNormType);

class SteamAudio {
public:
	typedef enum {
		log_debug,
		log_info,
		log_warn,
		log_error
	} GodotSteamAudioLogLevel;

	static void log(GodotSteamAudioLogLevel lvl, const char *str);
};

// Every handle starts null so a failed creation is detectable rather than garbage.
struct GlobalSteamAudioState {
	IPLScene scene = nullptr;
	IPLAudioSettings audio_cfg{};
	IPLContext ctx = nullptr;
	IPLHRTF hrtf = nullptr;
	IPLAmbisonicsEncodeEffect ambi_enc_effect = nullptr;
	IPLAmbisonicsDecodeEffect ambi_dec_effect = nullptr;
	IPLSimulationSettings sim_cfg{};
	IPLSimulator sim = nullptr;
	IPLCoordinateSpace3 listener_coords{};
	std::mutex refl_ir_lock;
};

struct SteamAudioSource {
	AudioStreamPlayer3D *player = nullptr;
	IPLSource src;
};

struct SteamAudioSourceConfig {
	float occ_radius;
	int occ_samples;
	int transm_rays;
	float min_attn_dist;
	int ambisonics_order;
	float max_refl_dist;
	bool is_dist_attn_on;
	bool is_air_absorp_on;
	float air_absorption_low;
	float air_absorption_mid;
	float air_absorption_high;
	IPLAirAbsorptionModelType air_absorption_model_type;
	bool is_ambisonics_on;
	bool is_occlusion_on;
	bool is_reflection_on;
	IPLTransmissionType transmission_type;
	bool is_directivity_on;
	float dipole_weight;
	float dipole_power;
	bool is_pathing_on;
	int pathing_order;
	float path_vis_radius;
	float path_vis_threshold;
	float path_vis_range;
	bool path_validation;
	bool is_baked_reverb_on;
	float reflection_mix;
	float direct_mix;
	bool reflection_parametric;
};

// Parametric reverb needs the decay times only a Parametric or Hybrid simulator computes, so
// under a Convolution config the request is ignored rather than left silent.
inline IPLReflectionEffectType effective_reflection_type(const SteamAudioSourceConfig &cfg, IPLReflectionEffectType global) {
	return cfg.reflection_parametric && global != IPL_REFLECTIONEFFECTTYPE_CONVOLUTION
			? IPL_REFLECTIONEFFECTTYPE_PARAMETRIC
			: global;
}

struct SteamAudioEffects {
	IPLDirectEffect direct;
	IPLReflectionEffect refl;
	IPLAmbisonicsDecodeEffect dec;
	IPLAmbisonicsDecodeEffect refl_dec;
	IPLAmbisonicsEncodeEffect enc;
	IPLPathEffect path;
	IPLAmbisonicsDecodeEffect path_dec;
};

struct LocalSteamAudioBuffers {
	IPLAudioBuffer in;
	IPLAudioBuffer direct;
	IPLAudioBuffer mono;
	IPLAudioBuffer refl_ambi;
	IPLAudioBuffer refl_out;
	IPLAudioBuffer path_ambi;
	IPLAudioBuffer path_out;
	IPLAudioBuffer ambi;
	IPLAudioBuffer out;
};

struct LocalSteamAudioState {
	SteamAudioSource src;
	Vector3 dir_to_listener;
	IPLDirectEffectParams direct_outputs{ {} };
	IPLReflectionEffectParams refl_outputs{ {} };
	LocalSteamAudioBuffers bufs;
	SteamAudioEffects fx;
	SteamAudioSourceConfig cfg;
	std::atomic<bool> refl_in_range{ false };
	// Mid-band RT60 from the last reflection pass, for callers that want to know how much room
	// there is. Only a Parametric or Hybrid simulator produces it; otherwise it stays zero.
	std::atomic<float> reverb_time{ 0.0f };
	std::shared_mutex mux;
	// Pathing coefficients are owned by the simulator and rewritten every tick, so the game
	// thread copies them here instead of handing the audio thread a pointer into that memory.
	IPLPathEffectParams path_outputs{};
	std::vector<float> path_sh;
	std::atomic<bool> path_active{ false };
	// Whether the simulator still has pathing inputs for this source. Game thread only.
	bool path_in_sim = false;
	// Ambisonic order of the last reflection block, so its tail is decoded the same way.
	int last_refl_order = 0;
	std::mutex path_mux;
};

constexpr int ambisonic_channels_from(int order) {
	return (order + 1) * (order + 1);
}

// The order an ambisonic buffer of this many channels carries. Reflections come back at the
// order the listener asked the simulator for, which need not be the source's own setting.
inline int ambisonic_order_from(int channels) {
	int order = 0;
	while (ambisonic_channels_from(order + 1) <= channels) {
		order++;
	}
	return order;
}

inline IPLVector3 ipl_vec3_from(Vector3 v) { return IPLVector3{ v.x, v.y, v.z }; }

inline IPLCoordinateSpace3 ipl_coords_from(const Transform3D &trf) {
	auto orig = trf.origin;
	auto right = trf.get_basis().get_column(0);
	auto up = trf.get_basis().get_column(1);
	auto fwd = -trf.get_basis().get_column(2);

	IPLCoordinateSpace3 coords;
	coords.origin = ipl_vec3_from(orig);
	coords.right = ipl_vec3_from(right);
	coords.up = ipl_vec3_from(up);
	coords.ahead = ipl_vec3_from(fwd);

	return coords;
}

// Steam Audio reads IPLMatrix4x4 transposed and multiplies column vectors, so the basis
// vectors go across the rows and the translation into the last column. Geometry keeps Godot's
// axes: only orientations are converted, and that is what ipl_coords_from is for.
inline IPLMatrix4x4 ipl_matrix_from(const Transform3D &trf) {
	Vector3 x = trf.basis.get_column(0);
	Vector3 y = trf.basis.get_column(1);
	Vector3 z = trf.basis.get_column(2);
	Vector3 o = trf.origin;
	return IPLMatrix4x4{ {
			{ x.x, y.x, z.x, o.x },
			{ x.y, y.y, z.y, o.y },
			{ x.z, y.z, z.z, o.z },
			{ 0.0f, 0.0f, 0.0f, 1.0f },
	} };
}

inline void handleErr(IPLerror err) {
	switch (err) {
		case IPL_STATUS_SUCCESS:
			return;
		case IPL_STATUS_FAILURE:
			SteamAudio::log(SteamAudio::log_error, "Unspecified error in init");
			return;
		case IPL_STATUS_OUTOFMEMORY:
			SteamAudio::log(SteamAudio::log_error, "Out of memory in init");
			return;
		case IPL_STATUS_INITIALIZATION:
			SteamAudio::log(SteamAudio::log_error, "Failed to handle external dependency in init");
			return;
	}
}

inline int count_nodes_of_class(Node *from, const char *cls) {
	if (from == nullptr) {
		return 0;
	}
	int found = from->get_class() == cls ? 1 : 0;
	for (int i = 0; i < from->get_child_count(); i++) {
		found += count_nodes_of_class(from->get_child(i), cls);
	}
	return found;
}

// The edited scene in the editor, the live tree at runtime.
inline int count_nodes_of_class_in_scene(const Node *node, const char *cls) {
	if (node == nullptr || !node->is_inside_tree()) {
		return 0;
	}
	SceneTree *tree = node->get_tree();
	if (tree == nullptr) {
		return 0;
	}
	Node *root = Engine::get_singleton()->is_editor_hint() ? tree->get_edited_scene_root() : (Node *)tree->get_root();
	return count_nodes_of_class(root, cls);
}

inline void log_callback(IPLLogLevel level, const char *message) {
	SteamAudio::GodotSteamAudioLogLevel godot_log_level;
	switch (level) {
		case IPL_LOGLEVEL_INFO:
			godot_log_level = SteamAudio::GodotSteamAudioLogLevel::log_info;
			break;
		case IPL_LOGLEVEL_WARNING:
			godot_log_level = SteamAudio::GodotSteamAudioLogLevel::log_warn;
			break;
		case IPL_LOGLEVEL_ERROR:
			godot_log_level = SteamAudio::GodotSteamAudioLogLevel::log_error;
			break;
		case IPL_LOGLEVEL_DEBUG:
			godot_log_level = SteamAudio::GodotSteamAudioLogLevel::log_debug;
			break;
		default:
			godot_log_level = SteamAudio::GodotSteamAudioLogLevel::log_info;
			break;
	}
	SteamAudio::log(godot_log_level, String(message).strip_edges().utf8().get_data());
}

#endif
