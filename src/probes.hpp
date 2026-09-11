#ifndef STEAM_AUDIO_PROBES_H
#define STEAM_AUDIO_PROBES_H

#include "godot_cpp/classes/node3d.hpp"
#include "phonon.h"
#include "steam_audio.hpp"
#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

// A box volume filled with sound probes. Probes carry the baked data that pathing and baked
// reflections read at runtime. Bake once, save the result next to the level, and ship it.
class SteamAudioProbeBatch : public Node3D {
	GDCLASS(SteamAudioProbeBatch, Node3D);

private:
	IPLProbeBatch batch = nullptr;
	int probe_count = 0;
	bool is_registered = false;
	bool is_prepared = false;

	Vector3 size{ 30.0f, 10.0f, 30.0f };
	float spacing = 3.0f;
	float height = 1.5f;
	String data_path;
	bool prepare_on_ready = true;
	bool save_after_bake = false;

	bool bake_pathing = true;
	int path_samples = 1;
	float path_radius = 1.0f;
	float path_threshold = 0.1f;
	float path_visibility_range = 50.0f;
	float path_range = 1000.0f;

	bool bake_reverb = false;
	int reflection_rays = 16384;
	int reflection_bounces = 16;
	float reflection_duration = 2.0f;
	int reflection_order = 1;
	int reflection_diffuse_samples = 1024;
	float irradiance_min_distance = 1.0f;

	bool bake_static_only = true;
	int bake_threads = 0;

	IPLMatrix4x4 volume_transform() const;
	int threads() const;
	bool bake_internal();
	void restore_dynamic_geometry();
	bool load_internal(const String &path);

protected:
	static void _bind_methods();

public:
	~SteamAudioProbeBatch();
	void _notification(int p_what);

	// Called by the server inside its safe window. Loads the batch from disk when it can, and
	// generates and bakes it otherwise.
	bool build();
	IPLProbeBatch get_batch() const { return batch; }
	bool is_built() const { return batch != nullptr; }
	void mark_registered(bool p_registered) { is_registered = p_registered; }

	// Both go through the server, which runs them with the reflection simulation parked.
	bool bake();
	bool load_data(const String &path);
	bool save_data(const String &path);
	bool rebuild(const String &path);
	int get_probe_count() const { return probe_count; }

	Vector3 get_size() const { return size; }
	void set_size(Vector3 p_size) {
		size = p_size;
		update_gizmos();
	}
	float get_spacing() const { return spacing; }
	void set_spacing(float p_spacing) { spacing = p_spacing; }
	float get_height() const { return height; }
	void set_height(float p_height) { height = p_height; }
	String get_data_path() const { return data_path; }
	void set_data_path(String p_data_path) { data_path = p_data_path; }
	bool is_prepare_on_ready() const { return prepare_on_ready; }
	void set_prepare_on_ready(bool p_on) { prepare_on_ready = p_on; }
	bool is_save_after_bake() const { return save_after_bake; }
	void set_save_after_bake(bool p_on) { save_after_bake = p_on; }

	bool is_bake_pathing() const { return bake_pathing; }
	void set_bake_pathing(bool p_on) { bake_pathing = p_on; }
	int get_path_samples() const { return path_samples; }
	void set_path_samples(int p_v) { path_samples = p_v; }
	float get_path_radius() const { return path_radius; }
	void set_path_radius(float p_v) { path_radius = p_v; }
	float get_path_threshold() const { return path_threshold; }
	void set_path_threshold(float p_v) { path_threshold = p_v; }
	float get_path_visibility_range() const { return path_visibility_range; }
	void set_path_visibility_range(float p_v) { path_visibility_range = p_v; }
	float get_path_range() const { return path_range; }
	void set_path_range(float p_v) { path_range = p_v; }

	bool is_bake_reverb() const { return bake_reverb; }
	void set_bake_reverb(bool p_on) { bake_reverb = p_on; }
	int get_reflection_rays() const { return reflection_rays; }
	void set_reflection_rays(int p_v) { reflection_rays = p_v; }
	int get_reflection_bounces() const { return reflection_bounces; }
	void set_reflection_bounces(int p_v) { reflection_bounces = p_v; }
	float get_reflection_duration() const { return reflection_duration; }
	void set_reflection_duration(float p_v) { reflection_duration = p_v; }
	int get_reflection_order() const { return reflection_order; }
	void set_reflection_order(int p_v) { reflection_order = p_v; }
	int get_reflection_diffuse_samples() const { return reflection_diffuse_samples; }
	void set_reflection_diffuse_samples(int p_v) { reflection_diffuse_samples = p_v; }
	float get_irradiance_min_distance() const { return irradiance_min_distance; }
	void set_irradiance_min_distance(float p_v) { irradiance_min_distance = p_v; }
	bool is_bake_static_only() const { return bake_static_only; }
	void set_bake_static_only(bool p_on) { bake_static_only = p_on; }
	int get_bake_threads() const { return bake_threads; }
	void set_bake_threads(int p_v) { bake_threads = p_v; }

	PackedStringArray _get_configuration_warnings() const override;
};

#endif // STEAM_AUDIO_PROBES_H
