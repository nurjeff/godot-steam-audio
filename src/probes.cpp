#include "probes.hpp"
#include "config.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/os.hpp"
#include "godot_cpp/classes/time.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include "server.hpp"
#include <algorithm>

// iplPathBakerBake calls this without checking it for null, so it cannot be omitted.
static void IPLCALL bake_progress(IPLfloat32 progress, void *user_data) {
	(void)progress;
	(void)user_data;
}

void SteamAudioProbeBatch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("bake"), &SteamAudioProbeBatch::bake);
	ClassDB::bind_method(D_METHOD("save_data", "path"), &SteamAudioProbeBatch::save_data);
	ClassDB::bind_method(D_METHOD("load_data", "path"), &SteamAudioProbeBatch::load_data);
	ClassDB::bind_method(D_METHOD("get_probe_count"), &SteamAudioProbeBatch::get_probe_count);
	ClassDB::bind_method(D_METHOD("is_built"), &SteamAudioProbeBatch::is_built);
	ADD_SIGNAL(MethodInfo("baked", PropertyInfo(Variant::INT, "probe_count")));

	ClassDB::bind_method(D_METHOD("get_size"), &SteamAudioProbeBatch::get_size);
	ClassDB::bind_method(D_METHOD("set_size", "value"), &SteamAudioProbeBatch::set_size);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size"), "set_size", "get_size");

	ClassDB::bind_method(D_METHOD("get_spacing"), &SteamAudioProbeBatch::get_spacing);
	ClassDB::bind_method(D_METHOD("set_spacing", "value"), &SteamAudioProbeBatch::set_spacing);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spacing", PROPERTY_HINT_RANGE, "0.25,20.0,0.25,or_greater"), "set_spacing", "get_spacing");

	ClassDB::bind_method(D_METHOD("get_height"), &SteamAudioProbeBatch::get_height);
	ClassDB::bind_method(D_METHOD("set_height", "value"), &SteamAudioProbeBatch::set_height);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.0,10.0,0.1,or_greater"), "set_height", "get_height");

	ClassDB::bind_method(D_METHOD("get_data_path"), &SteamAudioProbeBatch::get_data_path);
	ClassDB::bind_method(D_METHOD("set_data_path", "value"), &SteamAudioProbeBatch::set_data_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "data_path", PROPERTY_HINT_FILE, "*.probes"), "set_data_path", "get_data_path");

	ClassDB::bind_method(D_METHOD("is_prepare_on_ready"), &SteamAudioProbeBatch::is_prepare_on_ready);
	ClassDB::bind_method(D_METHOD("set_prepare_on_ready", "value"), &SteamAudioProbeBatch::set_prepare_on_ready);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "prepare_on_ready"), "set_prepare_on_ready", "is_prepare_on_ready");

	ClassDB::bind_method(D_METHOD("is_save_after_bake"), &SteamAudioProbeBatch::is_save_after_bake);
	ClassDB::bind_method(D_METHOD("set_save_after_bake", "value"), &SteamAudioProbeBatch::set_save_after_bake);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "save_after_bake"), "set_save_after_bake", "is_save_after_bake");

	ADD_GROUP("Pathing", "path_");

	ClassDB::bind_method(D_METHOD("is_bake_pathing"), &SteamAudioProbeBatch::is_bake_pathing);
	ClassDB::bind_method(D_METHOD("set_bake_pathing", "value"), &SteamAudioProbeBatch::set_bake_pathing);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "path_bake"), "set_bake_pathing", "is_bake_pathing");

	ClassDB::bind_method(D_METHOD("get_path_samples"), &SteamAudioProbeBatch::get_path_samples);
	ClassDB::bind_method(D_METHOD("set_path_samples", "value"), &SteamAudioProbeBatch::set_path_samples);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "path_samples", PROPERTY_HINT_RANGE, "1,16,1"), "set_path_samples", "get_path_samples");

	ClassDB::bind_method(D_METHOD("get_path_radius"), &SteamAudioProbeBatch::get_path_radius);
	ClassDB::bind_method(D_METHOD("set_path_radius", "value"), &SteamAudioProbeBatch::set_path_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "path_radius", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_path_radius", "get_path_radius");

	ClassDB::bind_method(D_METHOD("get_path_threshold"), &SteamAudioProbeBatch::get_path_threshold);
	ClassDB::bind_method(D_METHOD("set_path_threshold", "value"), &SteamAudioProbeBatch::set_path_threshold);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "path_threshold", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_path_threshold", "get_path_threshold");

	ClassDB::bind_method(D_METHOD("get_path_visibility_range"), &SteamAudioProbeBatch::get_path_visibility_range);
	ClassDB::bind_method(D_METHOD("set_path_visibility_range", "value"), &SteamAudioProbeBatch::set_path_visibility_range);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "path_visibility_range", PROPERTY_HINT_RANGE, "1.0,500.0,1.0,or_greater"), "set_path_visibility_range", "get_path_visibility_range");

	ClassDB::bind_method(D_METHOD("get_path_range"), &SteamAudioProbeBatch::get_path_range);
	ClassDB::bind_method(D_METHOD("set_path_range", "value"), &SteamAudioProbeBatch::set_path_range);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "path_range", PROPERTY_HINT_RANGE, "1.0,5000.0,1.0,or_greater"), "set_path_range", "get_path_range");

	ADD_GROUP("Baked Reverb", "reverb_");

	ClassDB::bind_method(D_METHOD("is_bake_reverb"), &SteamAudioProbeBatch::is_bake_reverb);
	ClassDB::bind_method(D_METHOD("set_bake_reverb", "value"), &SteamAudioProbeBatch::set_bake_reverb);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reverb_bake"), "set_bake_reverb", "is_bake_reverb");

	ClassDB::bind_method(D_METHOD("get_reflection_rays"), &SteamAudioProbeBatch::get_reflection_rays);
	ClassDB::bind_method(D_METHOD("set_reflection_rays", "value"), &SteamAudioProbeBatch::set_reflection_rays);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "reverb_rays", PROPERTY_HINT_RANGE, "512,65536,512"), "set_reflection_rays", "get_reflection_rays");

	ClassDB::bind_method(D_METHOD("get_reflection_bounces"), &SteamAudioProbeBatch::get_reflection_bounces);
	ClassDB::bind_method(D_METHOD("set_reflection_bounces", "value"), &SteamAudioProbeBatch::set_reflection_bounces);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "reverb_bounces", PROPERTY_HINT_RANGE, "1,128,1"), "set_reflection_bounces", "get_reflection_bounces");

	ClassDB::bind_method(D_METHOD("get_reflection_duration"), &SteamAudioProbeBatch::get_reflection_duration);
	ClassDB::bind_method(D_METHOD("set_reflection_duration", "value"), &SteamAudioProbeBatch::set_reflection_duration);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reverb_duration", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_reflection_duration", "get_reflection_duration");

	ClassDB::bind_method(D_METHOD("get_reflection_order"), &SteamAudioProbeBatch::get_reflection_order);
	ClassDB::bind_method(D_METHOD("set_reflection_order", "value"), &SteamAudioProbeBatch::set_reflection_order);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "reverb_ambisonics_order", PROPERTY_HINT_RANGE, "0,3,1"), "set_reflection_order", "get_reflection_order");

	ClassDB::bind_method(D_METHOD("get_reflection_diffuse_samples"), &SteamAudioProbeBatch::get_reflection_diffuse_samples);
	ClassDB::bind_method(D_METHOD("set_reflection_diffuse_samples", "value"), &SteamAudioProbeBatch::set_reflection_diffuse_samples);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "reverb_diffuse_samples", PROPERTY_HINT_RANGE, "32,4096,32"), "set_reflection_diffuse_samples", "get_reflection_diffuse_samples");

	ClassDB::bind_method(D_METHOD("get_irradiance_min_distance"), &SteamAudioProbeBatch::get_irradiance_min_distance);
	ClassDB::bind_method(D_METHOD("set_irradiance_min_distance", "value"), &SteamAudioProbeBatch::set_irradiance_min_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reverb_irradiance_min_distance", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_irradiance_min_distance", "get_irradiance_min_distance");

	ADD_GROUP("", "");

	ClassDB::bind_method(D_METHOD("is_bake_static_only"), &SteamAudioProbeBatch::is_bake_static_only);
	ClassDB::bind_method(D_METHOD("set_bake_static_only", "value"), &SteamAudioProbeBatch::set_bake_static_only);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bake_static_only"), "set_bake_static_only", "is_bake_static_only");

	ClassDB::bind_method(D_METHOD("get_bake_threads"), &SteamAudioProbeBatch::get_bake_threads);
	ClassDB::bind_method(D_METHOD("set_bake_threads", "value"), &SteamAudioProbeBatch::set_bake_threads);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bake_threads", PROPERTY_HINT_RANGE, "0,32,1"), "set_bake_threads", "get_bake_threads");
}

SteamAudioProbeBatch::~SteamAudioProbeBatch() {
	if (batch != nullptr) {
		iplProbeBatchRelease(&batch);
	}
}

void SteamAudioProbeBatch::_notification(int p_what) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
			if (prepare_on_ready) {
				SteamAudioServer::get_singleton()->add_probe_batch(this);
			}
			break;
		case NOTIFICATION_EXIT_TREE:
			SteamAudioServer::get_singleton()->remove_probe_batch(this);
			break;
	}
}

int SteamAudioProbeBatch::threads() const {
	if (bake_threads > 0) {
		return bake_threads;
	}
	return std::max(1, int(OS::get_singleton()->get_processor_count()) / 2);
}

// Steam Audio maps a unit cube centred on the origin through this matrix, and reads the matrix
// transposed, so the basis vectors go across the rows.
IPLMatrix4x4 SteamAudioProbeBatch::volume_transform() const {
	Transform3D trf = get_global_transform();
	trf.basis = trf.basis.scaled_local(size);
	return ipl_matrix_from(trf);
}

bool SteamAudioProbeBatch::build() {
	if (is_prepared) {
		return batch != nullptr;
	}
	bool ok = (!data_path.is_empty() && FileAccess::file_exists(data_path)) ? load_internal(data_path) : bake_internal();
	is_prepared = ok;
	return ok;
}

// Re-generating or re-loading swaps the IPLProbeBatch out from under the simulator, so it has
// to happen where nothing is reading it. An empty path means bake.
bool SteamAudioProbeBatch::rebuild(const String &path) {
	bool ok = path.is_empty() ? bake_internal() : load_internal(path);
	is_prepared = is_prepared || ok;
	return ok;
}

bool SteamAudioProbeBatch::bake() {
	return SteamAudioServer::get_singleton()->rebuild_probe_batch(this, String());
}

bool SteamAudioProbeBatch::load_data(const String &path) {
	return SteamAudioServer::get_singleton()->rebuild_probe_batch(this, path);
}

bool SteamAudioProbeBatch::bake_internal() {
	auto gs = SteamAudioServer::get_singleton()->get_global_state(false);
	if (gs == nullptr) {
		UtilityFunctions::push_error("[godot-steam-audio] Cannot bake probes before Steam Audio is running.");
		return false;
	}
	if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
		UtilityFunctions::push_error("[godot-steam-audio] ", get_name(), ": the probe volume needs a positive size on every axis.");
		return false;
	}

	uint64_t started = Time::get_singleton()->get_ticks_msec();

	IPLProbeArray probes = nullptr;
	handleErr(iplProbeArrayCreate(gs->ctx, &probes));
	if (probes == nullptr) {
		return false;
	}
	// Baked data describes the level. A door that happened to be shut when the bake ran would
	// otherwise be baked in as a wall, and no path would ever be found through its doorway.
	if (bake_static_only) {
		SteamAudioServer::get_singleton()->set_dynamic_geometry_present(false);
	}
	// Probe generation ray-casts the scene, which needs an up to date acceleration structure.
	iplSceneCommit(gs->scene);

	IPLProbeGenerationParams gen{};
	gen.type = IPL_PROBEGENERATIONTYPE_UNIFORMFLOOR;
	gen.spacing = spacing;
	gen.height = height;
	gen.transform = volume_transform();
	iplProbeArrayGenerateProbes(probes, gs->scene, &gen);
	probe_count = iplProbeArrayGetNumProbes(probes);

	if (batch != nullptr) {
		iplProbeBatchRelease(&batch);
	}
	handleErr(iplProbeBatchCreate(gs->ctx, &batch));
	if (batch == nullptr) {
		iplProbeArrayRelease(&probes);
		restore_dynamic_geometry();
		return false;
	}
	iplProbeBatchAddProbeArray(batch, probes);
	iplProbeArrayRelease(&probes);
	iplProbeBatchCommit(batch);

	if (probe_count == 0) {
		restore_dynamic_geometry();
		UtilityFunctions::push_warning(vformat(
				"[godot-steam-audio] %s: no probes were generated. Probes sit on floors, so the volume "
				"has to cover acoustic geometry. Volume centre %v, size %v, %d meshes in the scene.",
				get_name(), get_global_position(), size, SteamAudioServer::get_singleton()->get_static_mesh_count()));
		return true;
	}

	if (bake_pathing) {
		IPLPathBakeParams params{};
		params.scene = gs->scene;
		params.probeBatch = batch;
		params.identifier.type = IPL_BAKEDDATATYPE_PATHING;
		params.identifier.variation = IPL_BAKEDDATAVARIATION_DYNAMIC;
		params.numSamples = path_samples;
		params.radius = path_radius;
		params.threshold = path_threshold;
		params.visRange = path_visibility_range;
		params.pathRange = path_range;
		params.numThreads = threads();
		iplPathBakerBake(gs->ctx, &params, bake_progress, nullptr);
	}

	if (bake_reverb) {
		IPLReflectionsBakeParams params{};
		params.scene = gs->scene;
		params.probeBatch = batch;
		params.sceneType = SteamAudioConfig::scene_type;
		params.identifier.type = IPL_BAKEDDATATYPE_REFLECTIONS;
		params.identifier.variation = IPL_BAKEDDATAVARIATION_REVERB;
		params.bakeFlags = IPL_REFLECTIONSBAKEFLAGS_BAKECONVOLUTION;
		if (SteamAudioConfig::reflection_type != IPL_REFLECTIONEFFECTTYPE_CONVOLUTION) {
			params.bakeFlags = static_cast<IPLReflectionsBakeFlags>(params.bakeFlags | IPL_REFLECTIONSBAKEFLAGS_BAKEPARAMETRIC);
		}
		params.numRays = reflection_rays;
		params.numDiffuseSamples = reflection_diffuse_samples;
		params.numBounces = reflection_bounces;
		params.simulatedDuration = reflection_duration;
		params.savedDuration = reflection_duration;
		params.order = reflection_order;
		params.numThreads = threads();
		params.irradianceMinDistance = irradiance_min_distance;
		params.bakeBatchSize = 1;
		iplReflectionsBakerBake(gs->ctx, &params, bake_progress, nullptr);
	}

	restore_dynamic_geometry();
	uint64_t took = Time::get_singleton()->get_ticks_msec() - started;
	SteamAudio::log(SteamAudio::log_info, vformat("Baked %d probes in %d ms.", probe_count, int(took)).utf8().get_data());

	if (save_after_bake && !data_path.is_empty()) {
		save_data(data_path);
	}
	emit_signal("baked", probe_count);
	return true;
}

void SteamAudioProbeBatch::restore_dynamic_geometry() {
	if (bake_static_only) {
		SteamAudioServer::get_singleton()->set_dynamic_geometry_present(true);
	}
}

bool SteamAudioProbeBatch::save_data(const String &path) {
	auto gs = SteamAudioServer::get_singleton()->get_global_state(false);
	if (gs == nullptr || batch == nullptr) {
		UtilityFunctions::push_error("[godot-steam-audio] Nothing to save: bake the probe batch first.");
		return false;
	}
	IPLSerializedObjectSettings cfg{};
	IPLSerializedObject obj = nullptr;
	handleErr(iplSerializedObjectCreate(gs->ctx, &cfg, &obj));
	if (obj == nullptr) {
		return false;
	}
	iplProbeBatchSave(batch, obj);

	PackedByteArray bytes;
	IPLsize len = iplSerializedObjectGetSize(obj);
	bytes.resize(int64_t(len));
	memcpy(bytes.ptrw(), iplSerializedObjectGetData(obj), size_t(len));
	iplSerializedObjectRelease(&obj);

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null()) {
		UtilityFunctions::push_error("[godot-steam-audio] Cannot write ", path, ".");
		return false;
	}
	file->store_buffer(bytes);
	file->close();
	SteamAudio::log(SteamAudio::log_info, vformat("Saved %d probes to %s.", probe_count, path).utf8().get_data());
	return true;
}

bool SteamAudioProbeBatch::load_internal(const String &path) {
	auto gs = SteamAudioServer::get_singleton()->get_global_state(false);
	if (gs == nullptr) {
		UtilityFunctions::push_error("[godot-steam-audio] Cannot load probes before Steam Audio is running.");
		return false;
	}
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		UtilityFunctions::push_error("[godot-steam-audio] Cannot read ", path, ".");
		return false;
	}
	PackedByteArray bytes = file->get_buffer(int64_t(file->get_length()));
	file->close();
	if (bytes.is_empty()) {
		UtilityFunctions::push_error("[godot-steam-audio] ", path, " is empty.");
		return false;
	}

	IPLSerializedObjectSettings cfg{};
	cfg.data = const_cast<IPLbyte *>(bytes.ptr());
	cfg.size = IPLsize(bytes.size());
	IPLSerializedObject obj = nullptr;
	handleErr(iplSerializedObjectCreate(gs->ctx, &cfg, &obj));
	if (obj == nullptr) {
		return false;
	}
	if (batch != nullptr) {
		iplProbeBatchRelease(&batch);
	}
	IPLerror err = iplProbeBatchLoad(gs->ctx, obj, &batch);
	iplSerializedObjectRelease(&obj);
	handleErr(err);
	if (batch == nullptr) {
		return false;
	}
	iplProbeBatchCommit(batch);
	probe_count = iplProbeBatchGetNumProbes(batch);
	SteamAudio::log(SteamAudio::log_info, vformat("Loaded %d probes from %s.", probe_count, path).utf8().get_data());
	return true;
}

PackedStringArray SteamAudioProbeBatch::_get_configuration_warnings() const {
	PackedStringArray res;
	if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
		res.push_back("The probe volume needs a positive size on every axis.");
	}
	if (spacing <= 0.0f) {
		res.push_back("Probe spacing has to be greater than zero.");
	}
	if (!bake_pathing && !bake_reverb && data_path.is_empty()) {
		res.push_back("Nothing is baked and no data file is set, so this batch will have no effect.");
	}
	return res;
}
