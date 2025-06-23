#include "audio.hpp"
#include "game.hpp"
#include "kisslib/kissmath.hpp"
#include "kisslib/string.hpp"
#include "kisslib/random.hpp"
#include "portaudio.h"
// audio loading with libsoundwave
#include "AudioDecoder.h"
#include "WavEncoder.h"
#include "PostProcess.h"

using namespace kissmath;

namespace audio {
	AudioDataF32 load_sound_data_from_file (std::string const& filepath) {
		soundwave::SoundwaveIO loader;
		soundwave::AudioData data;
		loader.Load(&data, filepath);

		if (data.channelCount < 1 || data.channelCount > 2)
			return {};

		AudioDataF32 res;
		res.sample_rate = (double)data.sampleRate;
		res.channels = data.channelCount;
		res.count = (int)data.samples.size() / data.channelCount;
		res.samples = std::move(data.samples);
		return res;
	}
}

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
	if (!sound->valid()) return; // Simply don't play if not valid

	while (locked)
		; // busy wait

	if (playing_sounds_count < MAX_PLAYING_SOUNDS) {
		playing_sounds[playing_sounds_count] = { sound,
			global_volume * volume * sound->volume,
			speed * sound->speed, 0 };
		playing_sounds_count++;
	}
	//_timescale = input.time_scale;
	_timescale = 1; // fix 
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

struct PortAudio {
	PaStream *stream;

	static int portaudio_callback (
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
	
	PortAudio () {

		PaError err = Pa_Initialize();
		if(err != paNoError) {
			fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
			return;
		}

		////

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
			return;
		}

		err = Pa_StartStream(stream);
		if(err != paNoError) {
			fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
			return;
		}
	}
	~PortAudio () {
		PaError err = Pa_StopStream( stream );
		if(err != paNoError) {
			fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
			return;
		}
		
		
		err = Pa_Terminate();
		if(err != paNoError) {
			fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
			return;
		}
	}
};

PortAudio g_portaudio;

Sound::Sound (std::string name, float volume, float speed) {
	sound = g->audio->load_sound(std::move(name), volume, speed);
}

void Sound::play (float volume, float speed) {
	g->audio->play_sound(sound, volume, speed);
}

SoundSet::SoundSet (std::string base_name, int max_index, float volume, float speed) {
	if (max_index < 0) {
		Directory dir;
		if (kiss::read_directory(g->audio->sounds_directory, &dir, base_name + "*.wav")) {
			for (auto& files : dir.filenames) {
				sounds.push_back( g->audio->load_sound(std::move(files), volume, speed) );
			}
		}
		return;
	}

	for (int i=0; i<max_index; i++) {
		auto name = prints("%s%d", base_name.c_str(), i);
		sounds.push_back( g->audio->load_sound(std::move(name), volume, speed) );
	}
}

void SoundSet::play (int idx, float volume, float speed) {
	assert(idx >= 0 && idx < (int)sounds.size());
	g->audio->play_sound(sounds[idx], volume, speed);
}
void SoundSet::play_random (float volume, float speed) {
	int idx = random.uniformi(0, (int)sounds.size());
	play(idx, volume, speed);
}
