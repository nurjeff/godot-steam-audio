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

SteamAudioStream::SteamAudioStream() {}
SteamAudioStream::~SteamAudioStream() {}

void SteamAudioStream::_bind_methods() {}

Ref<AudioStreamPlayback> SteamAudioStream::_instantiate_playback() const {
	Ref<SteamAudioStreamPlayback> playback;
	playback.instantiate();
	playback->set_stream(stream);
	playback->parent = parent;

	return playback;
}

void SteamAudioStream::set_stream(Ref<AudioStream> p_stream) { stream = p_stream; }
Ref<AudioStream> SteamAudioStream::get_stream() { return this->stream; }

// ----------------------------------------------------
// SteamAudioStreamPlayback

SteamAudioStreamPlayback::SteamAudioStreamPlayback() {}
SteamAudioStreamPlayback::~SteamAudioStreamPlayback() {}

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

static void scale_buffer(IPLAudioBuffer &buffer, float gain) {
	for (int i = 0; i < buffer.numChannels; i++) {
		for (int j = 0; j < buffer.numSamples; j++) {
			buffer.data[i][j] *= gain;
		}
	}
}

int32_t SteamAudioStreamPlayback::_mix(AudioFrame *buffer, float rate_scale, int32_t frames) {
	if (parent == nullptr || stream_playback.is_null() || Engine::get_singleton()->is_editor_hint()) {
		fill_silence(buffer, 0, frames);
		return frames;
	}

	auto gs = SteamAudioServer::get_singleton()->get_global_state(false);
	if (gs == nullptr) {
		fill_silence(buffer, 0, frames);
		return frames;
	}

	LocalSteamAudioState *ls = parent->get_local_state();
	if (ls == nullptr) { // probably being destroyed
		fill_silence(buffer, 0, frames);
		return frames;
	}
	std::unique_lock lock(ls->mux);

	// parent may have been deleted while we waited for the lock
	if (parent == nullptr || (ls = parent->get_local_state()) == nullptr || !ls->src.player) {
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

	IPLAmbisonicsDecodeEffectParams dec_params{};
	dec_params.orientation = gs->listener_coords;
	dec_params.order = ls->cfg.ambisonics_order;
	dec_params.hrtf = gs->hrtf;
	dec_params.binaural = IPL_TRUE;

	if (ls->cfg.is_ambisonics_on) {
		IPLAmbisonicsEncodeEffectParams enc_params{};
		enc_params.direction = ipl_vec3_from(ls->dir_to_listener);
		enc_params.order = ls->cfg.ambisonics_order;
		iplAmbisonicsEncodeEffectApply(
				ls->fx.enc, &enc_params,
				&ls->bufs.direct, &ls->bufs.ambi);

		iplAmbisonicsDecodeEffectApply(
				ls->fx.dec, &dec_params,
				&ls->bufs.ambi, &ls->bufs.out);
	} else {
		iplAudioBufferMix(gs->ctx, &ls->bufs.direct, &ls->bufs.out);
	}

	// Parametric reverb carries decay times rather than an impulse response. Out of reflection
	// range the simulator stops updating this source, so reusing its last result would both cost
	// CPU and freeze the reverb in place.
	const bool needs_ir = SteamAudioConfig::reflection_type != IPL_REFLECTIONEFFECTTYPE_PARAMETRIC;
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
		iplAudioBufferMix(gs->ctx, &ls->bufs.path_out, &ls->bufs.out);
	}

	gs->refl_ir_lock.lock();
	const bool have_ir = ls->refl_outputs.ir != nullptr && ls->refl_outputs.irSize > 0 && ls->refl_outputs.numChannels > 0;
	if (wants_refl && (!needs_ir || have_ir)) {
		// numChannels and irSize describe the impulse response the simulator actually produced,
		// from the listener's reflection order and duration. Substituting the source's own
		// settings here convolved past the end of it whenever the two disagreed.
		ls->refl_outputs.type = SteamAudioConfig::reflection_type;
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

		if (ls->cfg.is_baked_reverb_on) {
			scale_buffer(ls->bufs.refl_out, baked_reverb_send(ls));
		}
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
	for (int i = 0; i < ls->bufs.out.numChannels; i++) {
		for (int j = 0; j < ls->bufs.out.numSamples; j++) {
			ls->bufs.out.data[i][j] = 0.0f;
		}
	}

	if (iplDirectEffectGetTail(ls->fx.direct, &ls->bufs.direct) == IPL_AUDIOEFFECTSTATE_TAILREMAINING) {
		remaining = true;
	}
	if (ls->cfg.is_ambisonics_on) {
		IPLAmbisonicsEncodeEffectParams enc_params{};
		enc_params.direction = ipl_vec3_from(ls->dir_to_listener);
		enc_params.order = ls->cfg.ambisonics_order;
		iplAmbisonicsEncodeEffectApply(ls->fx.enc, &enc_params, &ls->bufs.direct, &ls->bufs.ambi);
		iplAmbisonicsDecodeEffectApply(ls->fx.dec, &dec_params, &ls->bufs.ambi, &ls->bufs.out);
	} else {
		iplAudioBufferMix(gs->ctx, &ls->bufs.direct, &ls->bufs.out);
	}

	if (ls->cfg.is_pathing_on) {
		if (iplPathEffectGetTail(ls->fx.path, &ls->bufs.path_ambi) == IPL_AUDIOEFFECTSTATE_TAILREMAINING) {
			remaining = true;
		}
		IPLAmbisonicsDecodeEffectParams path_dec_params = dec_params;
		path_dec_params.order = ls->cfg.pathing_order;
		iplAmbisonicsDecodeEffectApply(ls->fx.path_dec, &path_dec_params, &ls->bufs.path_ambi, &ls->bufs.path_out);
		iplAudioBufferMix(gs->ctx, &ls->bufs.path_out, &ls->bufs.out);
	}

	if (ls->cfg.is_reflection_on) {
		if (iplReflectionEffectGetTail(ls->fx.refl, &ls->bufs.refl_ambi, nullptr) == IPL_AUDIOEFFECTSTATE_TAILREMAINING) {
			remaining = true;
		}
		IPLAmbisonicsDecodeEffectParams refl_dec_params = dec_params;
		refl_dec_params.order = ls->last_refl_order;
		iplAmbisonicsDecodeEffectApply(ls->fx.refl_dec, &refl_dec_params, &ls->bufs.refl_ambi, &ls->bufs.refl_out);
		if (ls->cfg.is_baked_reverb_on) {
			scale_buffer(ls->bufs.refl_out, baked_reverb_send(ls));
		}
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

	stream = p_stream;
	stream_playback = stream->instantiate_playback();
	stream_playback->start(p_from_offset);
	pending_pos = 0;
	pending_len = 0;
	source_done = false;
	tail_active = false;
	tail_done = false;
	tail_requested.store(false);
}

void SteamAudioStreamPlayback::_start(double from_pos) {
	if (stream_playback == nullptr) {
		if (stream.is_valid()) {
			is_active.store(true);
			restart(stream, float(from_pos));
		}
		return;
	} else if (stream_playback->is_playing()) {
		return;
	}
	stream_playback->start(from_pos);
	is_active.store(true);
}

void SteamAudioStreamPlayback::release_inner() {
	_stop();
	stream_playback.unref();
	stream.unref();
}

void SteamAudioStreamPlayback::_stop() {
	is_active.store(false);
	tail_active = false;
	tail_done = true;
	if (stream_playback == nullptr || !stream_playback->is_playing()) {
		return;
	}
	stream_playback->stop();
}

bool SteamAudioStreamPlayback::_is_playing() const { return is_active; }
void SteamAudioStreamPlayback::set_stream(Ref<AudioStream> p_stream) { stream = p_stream; }
Ref<AudioStreamPlayback> SteamAudioStreamPlayback::get_stream_playback() { return this->stream_playback; }
