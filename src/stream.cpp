#include "stream.hpp"
#include "config.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/variant/packed_vector2_array.hpp"
#include "server.hpp"
#include "steam_audio.hpp"
#include <phonon.h>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <algorithm>
#include <cmath>

SteamAudioStream::SteamAudioStream() {}
SteamAudioStream::~SteamAudioStream() {}

void SteamAudioStream::_bind_methods() {}

Ref<AudioStreamPlayback> SteamAudioStream::_instantiate_playback() const {
	Ref<SteamAudioStreamPlayback> playback;
	playback.instantiate();
	playback->set_stream(stream);
	playback->link = link;

	return playback;
}

void SteamAudioStream::set_stream(Ref<AudioStream> p_stream) { stream = p_stream; }
Ref<AudioStream> SteamAudioStream::get_stream() { return this->stream; }

// ----------------------------------------------------
// SteamAudioStreamPlayback

std::mutex SteamAudioStreamPlayback::live_mux;
std::unordered_set<SteamAudioStreamPlayback *> SteamAudioStreamPlayback::live;

SteamAudioStreamPlayback::SteamAudioStreamPlayback() {
	std::lock_guard<std::mutex> lock(live_mux);
	live.insert(this);
}

SteamAudioStreamPlayback::~SteamAudioStreamPlayback() {
	std::lock_guard<std::mutex> lock(live_mux);
	live.erase(this);
}

void SteamAudioStreamPlayback::pin_for_shutdown() {
	std::lock_guard<std::mutex> lock(live_mux);
	for (SteamAudioStreamPlayback *playback : live) {
		// Two, because Godot already consults the class when a release leaves one reference.
		playback->reference();
		playback->reference();
	}
}

static void fill_silence(AudioFrame *buffer, int32_t from, int32_t to) {
	for (int32_t i = from; i < to; i++) {
		buffer[i].left = 0.0f;
		buffer[i].right = 0.0f;
	}
}

// Baked reverb is the response of the room the listener is standing in, looked up at the
// listener's probes. It carries nothing about where the source is, so on its own a source on the
// far side of a closed door arrives at full strength, louder than it would be in the same room.
// Send it in at the gain its direct path would have had, which is what a reverb send is.
static float baked_reverb_send(const LocalSteamAudioState *ls) {
	float send = 1.0f;
	if (ls->cfg.is_dist_attn_on) {
		send *= ls->direct_outputs.distanceAttenuation;
	}
	if (ls->cfg.is_occlusion_on) {
		const float *transmission = ls->direct_outputs.transmission;
		float through = (transmission[0] + transmission[1] + transmission[2]) / 3.0f;
		float occlusion = ls->direct_outputs.occlusion;
		send *= occlusion + (1.0f - occlusion) * through;
	}
	return std::clamp(send, 0.0f, 1.0f);
}

// A path's EQ describes what bending around obstacles costs, not what travelling the distance
// costs, and its SH coefficients come back at much the same magnitude however far the route ran.
// Measured across a two-storey house, the hall, the room directly above the source and the
// landing all read within 0.8 dB of each other. Send the path in at the distance gain the source
// already has, so a room away is quieter than next door.
// With the source in sight, the path simulator returns the direct line itself as a path
// (path_simulator.cpp), a second copy of the direct sound. Weighted by how occluded the source is,
// the path takes over as the direct sound fades instead of doubling it.
static float path_send(const LocalSteamAudioState *ls) {
	float send = ls->cfg.is_occlusion_on ? 1.0f - ls->direct_outputs.occlusion : 1.0f;
	if (ls->cfg.is_dist_attn_on) {
		send *= ls->direct_outputs.distanceAttenuation;
	}
	return std::clamp(send, 0.0f, 1.0f);
}

// Baked reverb is sent at the direct path's gain; simulated reflections already carry their
// own. Both are then scaled by the player's mix, which is a taste setting, not physics.
static float reflection_send(const LocalSteamAudioState *ls) {
	return (ls->cfg.is_baked_reverb_on ? baked_reverb_send(ls) : 1.0f) * ls->cfg.reflection_mix;
}

static void scale_buffer(IPLAudioBuffer &buffer, float gain) {
	for (int i = 0; i < buffer.numChannels; i++) {
		for (int j = 0; j < buffer.numSamples; j++) {
			buffer.data[i][j] *= gain;
		}
	}
}

static IPLVector3 unit_or_ahead(IPLVector3 v) {
	const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	// Also catches NaN: a source at the listener has no direction to speak of.
	if (!(length > 1e-6f)) {
		return IPLVector3{ 0.0f, 0.0f, -1.0f };
	}
	return IPLVector3{ v.x / length, v.y / length, v.z / length };
}

// Writes the direct path into bufs.out. It goes through the HRTF as a point, as Steam Audio's own
// engine plugins render it: encoding it to first-order ambisonics and decoding that binaurally took
// 6 to 15 dB off everything above 1 kHz.
static void render_direct(GlobalSteamAudioState *gs, LocalSteamAudioState *ls) {
	if (!ls->cfg.is_ambisonics_on) {
		for (int i = 0; i < ls->bufs.out.numChannels; i++) {
			for (int j = 0; j < ls->bufs.out.numSamples; j++) {
				ls->bufs.out.data[i][j] = ls->bufs.direct.data[i][j];
			}
		}
		return;
	}
	iplAudioBufferDownmix(gs->ctx, &ls->bufs.direct, &ls->bufs.mono);
	IPLBinauralEffectParams params{};
	params.direction = unit_or_ahead(ls->hrtf_direction);
	params.interpolation = IPL_HRTFINTERPOLATION_BILINEAR;
	params.spatialBlend = 1.0f;
	params.hrtf = gs->hrtf;
	iplBinauralEffectApply(ls->fx.binaural, &params, &ls->bufs.mono, &ls->bufs.out);
}

int32_t SteamAudioStreamPlayback::_mix(AudioFrame *buffer, float rate_scale, int32_t frames) {
	if (link == nullptr || Engine::get_singleton()->is_editor_hint()) {
		fill_silence(buffer, 0, frames);
		return frames;
	}

	auto gs = SteamAudioServer::get_singleton()->get_global_state(false);
	if (gs == nullptr) {
		fill_silence(buffer, 0, frames);
		return frames;
	}

	std::lock_guard<std::mutex> inner_lock(mux);
	if (stream_playback.is_null()) {
		fill_silence(buffer, 0, frames);
		return frames;
	}
	// The player's destructor takes this one, so either it is gone or it stays until this block is out.
	std::lock_guard<std::mutex> lock(link->mux);
	LocalSteamAudioState *ls = link->player == nullptr ? nullptr : link->player->get_local_state();
	if (ls == nullptr || !ls->src.player) {
		fill_silence(buffer, 0, frames);
		return frames;
	}

	int32_t written = 0;
	while (written < frames) {
		if (pending_pos >= pending_len) {
			pending_len = process_block(gs, ls, rate_scale);
			pending_pos = 0;
			if (pending_len <= 0) {
				break;
			}
		}
		int32_t take = std::min(frames - written, pending_len - pending_pos);
		for (int32_t i = 0; i < take; i++) {
			buffer[written + i] = pending[pending_pos + i];
		}
		written += take;
		pending_pos += take;
	}

	fill_silence(buffer, written, frames);
	return written > 0 ? written : 0;
}

// Runs one fixed-size Steam Audio block. Returns how many frames of it are valid.
int SteamAudioStreamPlayback::process_block(GlobalSteamAudioState *gs, LocalSteamAudioState *ls, float rate_scale) {
	if (tail_requested.load()) {
		source_done = true;
	}
	if (source_done) {
		return process_tail_block(gs, ls);
	}

	const int block = gs->audio_cfg.frameSize;
	if (int(pending.size()) < block) {
		pending.resize(block);
	}

	scratch = stream_playback->mix_audio(rate_scale, block);
	int available = std::min(int(scratch.size()), block);
	if (available <= 0) {
		source_done = true;
		return process_tail_block(gs, ls);
	}
	if (available < block) {
		source_done = true;
	}

	const Vector2 *src = scratch.ptr();
	for (int i = 0; i < available; i++) {
		ls->bufs.in.data[0][i] = src[i].x;
		ls->bufs.in.data[1][i] = src[i].y;
	}
	// The effects always consume a full block; never let them see the previous one's tail.
	for (int i = available; i < block; i++) {
		ls->bufs.in.data[0][i] = 0.0f;
		ls->bufs.in.data[1][i] = 0.0f;
	}

	if (ls->cfg.is_air_absorp_on) {
		ls->direct_outputs.flags = static_cast<IPLDirectEffectFlags>(
				ls->direct_outputs.flags |
				IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION);
	}

	if (ls->cfg.is_dist_attn_on) {
		ls->direct_outputs.flags = static_cast<IPLDirectEffectFlags>(
				ls->direct_outputs.flags |
				IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION);
	}
	if (ls->cfg.is_occlusion_on) {
		ls->direct_outputs.flags = static_cast<IPLDirectEffectFlags>(
				ls->direct_outputs.flags |
				IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION |
				IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION);
		ls->direct_outputs.transmissionType = ls->cfg.transmission_type;
	}
	if (ls->cfg.is_directivity_on) {
		ls->direct_outputs.flags = static_cast<IPLDirectEffectFlags>(
				ls->direct_outputs.flags |
				IPL_DIRECTEFFECTFLAGS_APPLYDIRECTIVITY);
	}

	if (ls->direct_outputs.flags != 0) {
		iplDirectEffectApply(
				ls->fx.direct, &ls->direct_outputs,
				&ls->bufs.in, &ls->bufs.direct);
	} else {
		for (int i = 0; i < ls->bufs.direct.numChannels; i++) {
			for (int j = 0; j < ls->bufs.direct.numSamples; j++) {
				ls->bufs.direct.data[i][j] = 0.0f;
			}
		}

		iplAudioBufferMix(gs->ctx, &ls->bufs.in, &ls->bufs.direct);
	}
	// Only the direct path; reflections and paths are fed from the dry input.
	scale_buffer(ls->bufs.direct, ls->cfg.direct_mix);
	render_direct(gs, ls);

	IPLAmbisonicsDecodeEffectParams dec_params{};
	dec_params.orientation = gs->listener_coords;
	dec_params.order = ls->cfg.ambisonics_order;
	dec_params.hrtf = gs->hrtf;
	dec_params.binaural = IPL_TRUE;

	// Parametric reverb carries decay times rather than an impulse response. Out of reflection
	// range the simulator stops updating this source, so reusing its last result would both cost
	// CPU and freeze the reverb in place.
	const IPLReflectionEffectType refl_type = effective_reflection_type(ls->cfg, SteamAudioConfig::reflection_type);
	const bool needs_ir = refl_type != IPL_REFLECTIONEFFECTTYPE_PARAMETRIC;
	const bool wants_paths = ls->cfg.is_pathing_on && ls->path_active.load();
	const bool wants_refl = ls->cfg.is_reflection_on && ls->refl_in_range.load();
	if (wants_paths || wants_refl) {
		iplAudioBufferDownmix(gs->ctx, &ls->bufs.in, &ls->bufs.mono);
	}

	if (wants_paths) {
		// Copied off the shared vector so the simulation thread can keep writing into it.
		// Sized for the highest order the config allows, so the copy is never short.
		float sh[ambisonic_channels_from(5)] = {};
		IPLPathEffectParams path_params{};
		{
			std::lock_guard<std::mutex> lock(ls->path_mux);
			for (int i = 0; i < IPL_NUM_BANDS; i++) {
				path_params.eqCoeffs[i] = ls->path_outputs.eqCoeffs[i];
			}
			int copied = std::min(int(ls->path_sh.size()), int(sizeof(sh) / sizeof(sh[0])));
			for (int i = 0; i < copied; i++) {
				sh[i] = ls->path_sh[i];
			}
		}
		path_params.shCoeffs = sh;
		path_params.order = ls->cfg.pathing_order;
		path_params.binaural = IPL_FALSE;
		path_params.normalizeEQ = IPL_TRUE;
		iplPathEffectApply(ls->fx.path, &path_params, &ls->bufs.mono, &ls->bufs.path_ambi);
		// Decoded at the order the path effect wrote, not the source's own ambisonics setting.
		IPLAmbisonicsDecodeEffectParams path_dec_params = dec_params;
		path_dec_params.order = ls->cfg.pathing_order;
		iplAmbisonicsDecodeEffectApply(ls->fx.path_dec, &path_dec_params, &ls->bufs.path_ambi, &ls->bufs.path_out);
		scale_buffer(ls->bufs.path_out, path_send(ls));
		iplAudioBufferMix(gs->ctx, &ls->bufs.path_out, &ls->bufs.out);
	}

	gs->refl_ir_lock.lock();
	const bool have_ir = ls->refl_outputs.ir != nullptr && ls->refl_outputs.irSize > 0 && ls->refl_outputs.numChannels > 0;
	if (wants_refl && (!needs_ir || have_ir)) {
		// numChannels and irSize describe the impulse response the simulator actually produced,
		// from the listener's reflection order and duration. Substituting the source's own
		// settings here convolved past the end of it whenever the two disagreed.
		ls->refl_outputs.type = refl_type;
		if (!needs_ir) {
			// Parametric reverb is synthesised from decay times, so the simulator reports no
			// impulse response dimensions and the effect needs to be told what to render into.
			ls->refl_outputs.numChannels = ambisonic_channels_from(ls->cfg.ambisonics_order);
			ls->refl_outputs.irSize = int(SteamAudioConfig::max_refl_duration * float(gs->audio_cfg.samplingRate));
		}
		iplReflectionEffectApply(ls->fx.refl, &ls->refl_outputs, &ls->bufs.mono, &ls->bufs.refl_ambi, nullptr);

		IPLAmbisonicsDecodeEffectParams refl_dec_params = dec_params;
		if (needs_ir) {
			refl_dec_params.order = ambisonic_order_from(ls->refl_outputs.numChannels);
		}
		ls->last_refl_order = refl_dec_params.order;
		iplAmbisonicsDecodeEffectApply(
				ls->fx.refl_dec, &refl_dec_params,
				&ls->bufs.refl_ambi, &ls->bufs.refl_out);
		scale_buffer(ls->bufs.refl_out, reflection_send(ls));
		iplAudioBufferMix(gs->ctx, &ls->bufs.refl_out, &ls->bufs.out);
	}
	gs->refl_ir_lock.unlock();

	for (int i = 0; i < block; i++) {
		pending[i].left = ls->bufs.out.data[0][i];
		pending[i].right = ls->bufs.out.data[1][i];
	}
	return block;
}

// Drains what the effects are still holding after the source has run out. Each effect reports
// whether it has more; the extra blocks past that flush the HRTF decoders, which have no tail of
// their own to ask for.
int SteamAudioStreamPlayback::process_tail_block(GlobalSteamAudioState *gs, LocalSteamAudioState *ls) {
	if (tail_done) {
		return 0;
	}
	if (!tail_active) {
		tail_active = true;
		tail_blocks = 0;
		tail_drain = 2;
	}
	const int block = gs->audio_cfg.frameSize;
	if (int(pending.size()) < block) {
		pending.resize(block);
	}
	const int limit = int(SteamAudioConfig::max_refl_duration * float(gs->audio_cfg.samplingRate)) / block + 16;
	if (tail_blocks++ > limit) {
		tail_active = false;
		tail_done = true;
		return 0;
	}

	IPLAmbisonicsDecodeEffectParams dec_params{};
	dec_params.orientation = gs->listener_coords;
	dec_params.order = ls->cfg.ambisonics_order;
	dec_params.hrtf = gs->hrtf;
	dec_params.binaural = IPL_TRUE;

	bool remaining = false;
	if (iplDirectEffectGetTail(ls->fx.direct, &ls->bufs.direct) == IPL_AUDIOEFFECTSTATE_TAILREMAINING) {
		remaining = true;
	}
	scale_buffer(ls->bufs.direct, ls->cfg.direct_mix);
	// The binaural effect's own convolution tail drains through the extra blocks fed below.
	render_direct(gs, ls);

	if (ls->cfg.is_pathing_on) {
		if (iplPathEffectGetTail(ls->fx.path, &ls->bufs.path_ambi) == IPL_AUDIOEFFECTSTATE_TAILREMAINING) {
			remaining = true;
		}
		IPLAmbisonicsDecodeEffectParams path_dec_params = dec_params;
		path_dec_params.order = ls->cfg.pathing_order;
		iplAmbisonicsDecodeEffectApply(ls->fx.path_dec, &path_dec_params, &ls->bufs.path_ambi, &ls->bufs.path_out);
		scale_buffer(ls->bufs.path_out, path_send(ls));
		iplAudioBufferMix(gs->ctx, &ls->bufs.path_out, &ls->bufs.out);
	}

	if (ls->cfg.is_reflection_on) {
		if (iplReflectionEffectGetTail(ls->fx.refl, &ls->bufs.refl_ambi, nullptr) == IPL_AUDIOEFFECTSTATE_TAILREMAINING) {
			remaining = true;
		}
		IPLAmbisonicsDecodeEffectParams refl_dec_params = dec_params;
		refl_dec_params.order = ls->last_refl_order;
		iplAmbisonicsDecodeEffectApply(ls->fx.refl_dec, &refl_dec_params, &ls->bufs.refl_ambi, &ls->bufs.refl_out);
		scale_buffer(ls->bufs.refl_out, reflection_send(ls));
		iplAudioBufferMix(gs->ctx, &ls->bufs.refl_out, &ls->bufs.out);
	}

	if (remaining) {
		tail_drain = 2;
	} else if (--tail_drain <= 0) {
		tail_active = false;
		tail_done = true;
	}

	for (int i = 0; i < block; i++) {
		pending[i].left = ls->bufs.out.data[0][i];
		pending[i].right = ls->bufs.out.data[1][i];
	}
	return block;
}

void SteamAudioStreamPlayback::_bind_methods() {
	ClassDB::bind_method(D_METHOD("play_stream", "stream", "from_offset", "volume_db", "pitch_scale"), &SteamAudioStreamPlayback::play_stream, DEFVAL(0), DEFVAL(0), DEFVAL(1.0));
}

int SteamAudioStreamPlayback::play_stream(const Ref<AudioStream> &p_stream, float p_from_offset, float p_volume_db, float p_pitch_scale) {
	// Volume and pitch live on the player, which has already applied them by the time this runs.
	(void)p_volume_db;
	(void)p_pitch_scale;
	restart(p_stream, p_from_offset);
	return 0;
}

void SteamAudioStreamPlayback::restart(const Ref<AudioStream> &p_stream, float p_from_offset) {
	if (Engine::get_singleton()->is_editor_hint() || p_stream.is_null()) {
		return;
	}

	Ref<AudioStreamPlayback> playback = p_stream->instantiate_playback();
	playback->start(p_from_offset);
	Ref<AudioStream> old_stream = p_stream;
	{
		std::lock_guard<std::mutex> lock(mux);
		std::swap(stream, old_stream);
		std::swap(stream_playback, playback);
		pending_pos = 0;
		pending_len = 0;
		source_done = false;
		tail_active = false;
		tail_done = false;
		tail_requested.store(false);
	}
	// The previous stream and playback are dropped here, outside the lock.
}

void SteamAudioStreamPlayback::_start(double from_pos) {
	Ref<AudioStream> to_start;
	{
		std::lock_guard<std::mutex> lock(mux);
		if (stream_playback.is_valid()) {
			if (!stream_playback->is_playing()) {
				stream_playback->start(from_pos);
				is_active.store(true);
			}
			return;
		}
		to_start = stream;
	}
	if (to_start.is_valid()) {
		is_active.store(true);
		restart(to_start, float(from_pos));
	}
}

void SteamAudioStreamPlayback::release_inner() {
	Ref<AudioStreamPlayback> playback;
	Ref<AudioStream> inner;
	{
		std::lock_guard<std::mutex> lock(mux);
		is_active.store(false);
		tail_active = false;
		tail_done = true;
		std::swap(stream_playback, playback);
		std::swap(stream, inner);
	}
}

void SteamAudioStreamPlayback::_stop() {
	std::lock_guard<std::mutex> lock(mux);
	is_active.store(false);
	tail_active = false;
	tail_done = true;
	if (stream_playback.is_valid() && stream_playback->is_playing()) {
		stream_playback->stop();
	}
}

bool SteamAudioStreamPlayback::_is_playing() const { return is_active; }

void SteamAudioStreamPlayback::set_stream(Ref<AudioStream> p_stream) {
	std::lock_guard<std::mutex> lock(mux);
	stream = p_stream;
}

Ref<AudioStreamPlayback> SteamAudioStreamPlayback::get_stream_playback() {
	std::lock_guard<std::mutex> lock(mux);
	return stream_playback;
}
