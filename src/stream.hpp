#ifndef STEAM_AUDIO_STREAM_H
#define STEAM_AUDIO_STREAM_H

#include "godot_cpp/classes/ref.hpp"
#include "player.hpp"
#include <phonon.h>
#include <godot_cpp/classes/audio_frame.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback.hpp>
#include <godot_cpp/templates/safe_refcount.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <unordered_set>
#include <vector>

using namespace godot;

class SteamAudio;

class SteamAudioStream : public AudioStream {
	GDCLASS(SteamAudioStream, AudioStream)
	friend class SteamAudioStreamPlayback;
	Ref<AudioStream> stream;

protected:
	static void _bind_methods();

public:
	SteamAudioStream();
	~SteamAudioStream();

	Ref<AudioStreamPlayback> _instantiate_playback() const override;
	void set_stream(Ref<AudioStream> p_stream);
	Ref<AudioStream> get_stream();

	std::shared_ptr<SteamAudioPlayerLink> link;
};

class SteamAudioStreamPlayback : public AudioStreamPlayback {
	GDCLASS(SteamAudioStreamPlayback, AudioStreamPlayback);
	friend class SteamAudioStream;

private:
	// The audio thread mixes the inner playback while the game thread swaps, stops or releases it.
	// Everything below is touched under this lock; engine objects are made and dropped outside it,
	// since the audio thread holds the driver's lock while it waits here.
	std::mutex mux;
	Ref<AudioStream> stream;
	Ref<AudioStreamPlayback> stream_playback;

	std::atomic<bool> is_active{false};

	// Steam Audio needs exactly frameSize samples per call, Godot asks for whatever it likes.
	std::vector<AudioFrame> pending;
	int pending_pos = 0;
	int pending_len = 0;
	bool source_done = false;
	// Once the source runs out, the effects keep ringing for as long as their impulse responses
	// last. Cutting there is what makes a sound stop dead in a cathedral.
	std::atomic<bool> tail_requested{ false };
	bool tail_active = false;
	bool tail_done = false;
	int tail_blocks = 0;
	int tail_drain = 2;
	PackedVector2Array scratch;

	int process_block(GlobalSteamAudioState *gs, LocalSteamAudioState *ls, float rate_scale);
	int process_tail_block(GlobalSteamAudioState *gs, LocalSteamAudioState *ls);

protected:
	static void _bind_methods();

public:
	SteamAudioStreamPlayback();
	~SteamAudioStreamPlayback();

	void set_stream(Ref<AudioStream> p_stream);
	Ref<AudioStreamPlayback> get_stream_playback();
	// Drops the inner stream and its playback. Called when the player is being destroyed.
	void release_inner();
	// Stop pulling from the source but keep mixing until the effects have nothing left, so the
	// reverb of a sound that ends rings out instead of being cut with it.
	void begin_tail() { tail_requested.store(true); }

	virtual int32_t _mix(AudioFrame *buffer, float rate_scale, int32_t frames) override;
	int play_stream(const Ref<AudioStream> &p_stream, float p_from_offset,
			float p_volume_db, float p_pitch_scale);
	// Starts the inner stream and clears the block and tail state. Volume and pitch are the
	// player's, not the playback's, which is why play_stream ignores those two arguments.
	void restart(const Ref<AudioStream> &p_stream, float p_from_offset);
	void _start(double from_pos) override;
	void _stop() override;
	bool _is_playing() const override;

	std::shared_ptr<SteamAudioPlayerLink> link;

	// At shutdown Godot frees an extension's classes before the audio server lets go of its
	// playbacks, and the last release of one then reads the freed class. Called from the
	// extension's first deinit, this keeps every live playback referenced past that release.
	// The process reclaims them.
	static void pin_for_shutdown();

private:
	static std::mutex live_mux;
	static std::unordered_set<SteamAudioStreamPlayback *> live;
};

#endif
