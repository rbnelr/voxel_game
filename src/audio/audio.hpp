#pragma once
#include "kisslib/string.hpp"
using namespace kiss;
#include "imgui/dear_imgui.hpp"
#include "kisslib/stl_extensions.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include "assert.h"

namespace audio {
	inline int max (int a, int b) {
		return a >= b ? a : b;
	}
	inline int min (int a, int b) {
		return a <= b ? a : b;
	}

	inline int clamp (int x, int a, int b) {
		return min(max(x, a), b);
	}

	inline float lerp (float a, float b, float t) {
		return a * (1 - t) + b * t;
	}

	struct AudioSample {
		float left;
		float right;
	};

	struct AudioData16 {
		double sample_rate;
		int channels; // mono or sterio

		int count;
		std::unique_ptr<int16_t[]> samples = nullptr;

		static inline float sample_to_f (int16_t val) {
			return (float)val / (float)(1 << 15);
		}

		inline AudioSample sample (double t) {
			assert(channels == 1);

			t *= sample_rate;

			// just clamp the sample indices, to prevent out of bounds
			int ai = clamp((int)t, 0, count);
			int bi = min(ai + 1, count);
			float interp = (float)(t - (double)ai);

			auto test = samples.get();

			if (channels == 1) {
				float a = sample_to_f( samples[ai] );
				float b = sample_to_f( samples[bi] );

				float tmp = lerp(a, b, interp);
				return { tmp, tmp };
			} else {

				auto al = sample_to_f( samples[ai * 2    ] );
				auto ar = sample_to_f( samples[bi * 2    ] );
				auto bl = sample_to_f( samples[ai * 2 + 1] );
				auto br = sample_to_f( samples[bi * 2 + 1] );

				return {
					lerp(al, ar, interp),
					lerp(bl, br, interp),
				};
			}
		}
	};

	
	struct AudioDataF32 {
		double sample_rate;
		int channels = 0; // mono or sterio

		int count;
		std::vector<float> samples;

		bool valid () { return channels > 0; }

		inline AudioSample sample (double t) {
			//assert(channels == 1 || channels == 2);

			t *= sample_rate;

			// just clamp the sample indices, to prevent out of bounds
			int ai = clamp((int)t, 0, count-1);
			int bi = min(ai + 1, count-1);
			float interp = (float)(t - (double)ai);

			auto test = samples.data();

			if (channels == 1) {
				float a = samples[ai];
				float b = samples[bi];

				float tmp = lerp(a, b, interp);
				return { tmp, tmp };
			} else {

				auto al = samples[ai * 2    ];
				auto ar = samples[bi * 2    ];
				auto bl = samples[ai * 2 + 1];
				auto br = samples[bi * 2 + 1];

				return {
					lerp(al, ar, interp),
					lerp(bl, br, interp),
				};
			}
		}
	};

	//AudioData16 load_sound_data_from_file (const char* filepath);
	AudioDataF32 load_sound_data_from_file (std::string const& filepath);
}

class AudioManager {
public:
	const std::string sounds_directory = "sounds/";
	
	struct Sound {
		audio::AudioDataF32 data;
		float volume;
		float speed;

		bool valid () { return data.valid(); }
	};

	std::unordered_map<std::string, std::unique_ptr<Sound>> loaded_sounds;

	Sound* load_sound (std::string name, float volume, float speed) {
		auto it = loaded_sounds.find(name);
		if (it != loaded_sounds.end()) {
			return it->second.get();
		}

		auto filepath = prints("%s%s.wav", sounds_directory.c_str(), name.c_str());
		clog("Loading sound file '%s'...", filepath.c_str());
		auto ptr = std::make_unique<Sound>(std::move(Sound{ audio::load_sound_data_from_file(filepath), volume, speed }));

		auto* s = ptr.get();
		loaded_sounds.emplace(std::move(name), std::move(ptr));
		return s;
	}

	void play_sound (Sound* sound, float volume, float speed);
};

struct Sound {
	AudioManager::Sound* sound = nullptr;

	Sound () {}
	Sound (std::string name, float volume=1, float speed=1);

	void play (float volume=1, float speed=1);
};
struct SoundSet {
	std::vector<AudioManager::Sound*> sounds;

	SoundSet () {}
	SoundSet (std::string base_name, int max_index, float volume=1, float speed=1);

	void play (int idx, float volume=1, float speed=1);
	void play_random (float volume=1, float speed=1);
};
