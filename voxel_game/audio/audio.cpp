#include "audio.hpp"
#include "read_wav.hpp"
#include "portaudio.h"
#include "stdio.h"

namespace audio {
	AudioData16 load_sound_data_from_file (const char* filepath) {
		return load_wav(filepath);
	}
}

AudioManager audio_manager;

#if 0
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
#endif
