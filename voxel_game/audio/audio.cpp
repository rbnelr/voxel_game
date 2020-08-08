#include "stdafx.hpp"
#include "audio.hpp"
#include "read_wav.hpp"
#include "../input.hpp"

#include "portaudio.h"

#include <atomic>

namespace audio {
	bool load_sound_data_from_file (const char* filepath, AudioData16* data) {
		return load_wav(filepath, data);
	}
}

AudioManager audio_manager;

////
constexpr double SAMPLE_RATE = 44100; // output sample rate

std::atomic<bool> locked = false;
std::atomic<float> timescale = 1;
std::atomic<float> volume = .5f;

struct PlayingSound {
	AudioManager::Sound* sound;
	float volume;
	float speed;

	double t = 0;
};

constexpr int MAX_PLAYING_SOUNDS = 128;
PlayingSound playing_sounds[MAX_PLAYING_SOUNDS];
int playing_sounds_count = 0;

void AudioManager::update () {
	ZoneScopedN("AudioManager::update");

	while (locked)
		; // busy wait

	timescale = input.time_scale;
	volume = enabled ? master_volume : 0;
}

void AudioManager::play_sound (Sound* sound, float volume, float speed) {
	ZoneScopedN("AudioManager::play_sound");

	while (locked)
		; // busy wait

	if (playing_sounds_count < MAX_PLAYING_SOUNDS) {
		playing_sounds[playing_sounds_count] = { sound, volume * sound->volume, speed * sound->speed, 0 };
		playing_sounds_count++;
	}
}

audio::AudioSample mix_sounds () {
	audio::AudioSample total = { 0 };

	for (int i=0; i<playing_sounds_count;) {
		auto& sound = playing_sounds[i];

		auto sampl = sound.sound->data.sample( sound.t );

		sound.t += sound.speed / SAMPLE_RATE * (double)timescale;

		total.left  += sampl.left  * sound.volume;
		total.right += sampl.right * sound.volume;

		if (sound.t > (1.0f / sound.sound->data.sample_rate * (double)sound.sound->data.count)) {
			playing_sounds[i] = playing_sounds[playing_sounds_count - 1];
			playing_sounds_count--;
		} else {
			i++;
		}
	}

	total.left  = clamp(total.left,  0.0f,1.0f);
	total.right = clamp(total.right, 0.0f,1.0f);
	return total;
}

int portaudio_callback (
		const void *input,
		void *output,
		unsigned long frameCount,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData
	) {

	locked = true;

	float *out = (float*)output;

	for(unsigned i=0; i<frameCount; i++) {
		auto smpl = mix_sounds();

		float left  = smpl.left  * volume;
		float right = smpl.right * volume;

		*out++ = left; // left
		*out++ = right; // right
	}

	locked = false;
	return 0;
}

bool __audio_run = [] () {

	PaError err;

	err = Pa_Initialize();
	if(err != paNoError) {
		fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
		return false;
	}

	////

	PaStream *stream;
	/* Open an audio I/O stream. */
	err = Pa_OpenDefaultStream(&stream,
		0,          /* no input channels */
		2,          /* stereo output */
		paFloat32,  /* 32 bit floating point output */
		SAMPLE_RATE,
		64,        /* frames per buffer, i.e. the number
					of sample frames that PortAudio will
					request from the callback. Many apps
					may want to use
					paFramesPerBufferUnspecified, which
					tells PortAudio to pick the best,
					possibly changing, buffer size.*/
		portaudio_callback, /* this is your callback function */
		NULL ); /*This is a pointer that will be passed to
				 your callback*/
	if(err != paNoError) {
		fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
		return false;
	}

	err = Pa_StartStream(stream);
	if(err != paNoError) {
		fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
		return false;
	}

	//err = Pa_StopStream( stream );
	//if(err != paNoError) {
	//	fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
	//	return false;
	//}
	//
	//
	//////
	//
	//err = Pa_Terminate();
	//if(err != paNoError) {
	//	fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
	//	return false;
	//}

	return true;
} ();
