#include "audio.hpp"
#include "read_wav.hpp"
#include "../input.hpp"
#include "../kissmath/float.hpp"
using namespace kissmath;

#include "portaudio.h"
#include "stdio.h"

#include <atomic>

namespace audio {
	AudioData16 load_sound_data_from_file (const char* filepath) {
		return load_wav(filepath);
	}
}

AudioManager audio_manager;

////
constexpr double SAMPLE_RATE = 44100; // output sample rate
float volume = 0.5f;

std::atomic<bool> locked = false;
std::atomic<float> _timescale = 1;

struct PlayingSound {
	AudioManager::Sound* sound;
	float volume;
	float speed;

	double t = 0;
};

constexpr int MAX_PLAYING_SOUNDS = 128;
PlayingSound playing_sounds[MAX_PLAYING_SOUNDS];
int playing_sounds_count = 0;

void AudioManager::play_sound (Sound* sound, float volume, float speed) {
	while (locked)
		; // busy wait

	if (playing_sounds_count < MAX_PLAYING_SOUNDS) {
		playing_sounds[playing_sounds_count] = { sound, volume * sound->volume, speed * sound->speed, 0 };
		playing_sounds_count++;
	}
	_timescale = input.time_scale;
}

audio::AudioSample mix_sounds () {
	audio::AudioSample total = { 0 };

	for (int i=0; i<playing_sounds_count;) {
		auto& sound = playing_sounds[i];

		auto sampl = sound.sound->data.sample( sound.t );

		sound.t += sound.speed / SAMPLE_RATE * (double)_timescale;

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

bool test = [] () {

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
