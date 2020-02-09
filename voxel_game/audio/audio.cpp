#include "audio.hpp"
#include "portaudio.h"
#include "math.h"
#include "assert.h"
#include "stdio.h"
#include "../util/file_io.hpp"
#include "../kissmath.hpp"

struct Samplef {
	float left;
	float right;
};

constexpr float SAMPLE_RATE = 44100;

struct AudioFile {
	typedef int16_t sample_t;
	struct Sample {
		sample_t left;
		sample_t right;

		Samplef tof () {
			return {
				(float)left / (float)(1 << 15),
				(float)right / (float)(1 << 15)
			};
		}
	};

	uint64_t count;
	std::unique_ptr<Sample[]> samples = nullptr;

	AudioFile (const char* filepath) {
		uint64_t size;
		samples = std::unique_ptr<Sample[]>( (Sample*)kiss::read_binary_file(filepath, &size).release() ); // is this safe?
		assert(samples && size > sizeof(Sample));

		count = size / sizeof(Sample);
	}

	double playback_speed = 1;
	double t = 0;

	Samplef get_sample (double t) {
		t *= SAMPLE_RATE;

		auto ai = (uint64_t)t;
		auto bi = ai + 1;
		auto interp = (float)(t - (double)ai);

		auto a = samples[ai].tof();
		auto b = samples[bi].tof();

		return {
			lerp(a.left , b.left , interp),
			lerp(a.right, b.right, interp),
		};
	}

	static double max (double a, double b) {
		return a >= b ? a : b;
	}
	static double min (double a, double b) {
		return a <= b ? a : b;
	}

	static double clamp (double x, double a, double b) {
		return min(max(x, a), b);
	}

	Samplef get_sample () {
		auto s = get_sample(t);

		t += playback_speed / SAMPLE_RATE;
		t = clamp(t, 0, count -1);

		return s;
	}
};

AudioFile audio1 = { "D:/test_audio2.raw" };

float amp = 0.75f;

Samplef get_sample () {
	return audio1.get_sample();
}

//float get_sample () {
//	float val = get_sample() * amp;
//
//	//if (freq >= max_freq || freq <= min_freq) {
//	//	freq_vel = -freq_vel;
//	//}
//	//freq += freq_vel / SAMPLE_RATE;
//
//	return val;
//}

int portaudio_callback (
		const void *input,
		void *output,
		unsigned long frameCount,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData
	) {

	float *out = (float*)output;

	for(unsigned i=0; i<frameCount; i++) {
		auto smpl = get_sample();

		float left  = smpl.left  * amp;
		float right = smpl.right * amp;

		*out++ = left; // left
		*out++ = right; // right
	}
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
		paFramesPerBufferUnspecified,        /* frames per buffer, i.e. the number
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
