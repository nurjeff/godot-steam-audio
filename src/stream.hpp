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

	SteamAudioPlayer *parent = nullptr;
};

class SteamAudioStreamPlayback : public AudioStreamPlayback {
	GDCLASS(SteamAudioStreamPlayback, AudioStreamPlayback);
	friend class SteamAudioStream;

private:
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

	virtual int32_t _mix(AudioFrame *buffer, float rate_scale, int32_t frames) override;
	int play_stream(const Ref<AudioStream> &p_stream, float p_from_offset,
			float p_volume_db, float p_pitch_scale);
	void _start(double from_pos) override;
	void _stop() override;
	bool _is_playing() const override;

	SteamAudioPlayer *parent = nullptr;
};

#endif
