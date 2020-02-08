#include "audio.hpp"
#include "portaudio.h"
#include "math.h"
#include "stdio.h"

constexpr float TWO_PI = 3.141592653589793238462643383279f;
constexpr float SAMPLE_RATE = 44100;

float t = 0;
float freq = 261.626f;
float amp = 0.1f;

float min_freq = 200;
float max_freq = 2000;
float freq_vel = 1000;

float get_sample () {
	float val = sinf(t * TWO_PI) * amp;

	t += freq / SAMPLE_RATE;

	if (freq >= max_freq || freq <= min_freq) {
		freq_vel = -freq_vel;
	}
	freq += freq_vel / SAMPLE_RATE;

	return val;
}

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
		float sample = get_sample();

		*out++ = sample; // left
		*out++ = sample; // right
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
		256,        /* frames per buffer, i.e. the number
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
