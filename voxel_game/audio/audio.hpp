#pragma once
#include "stdafx.hpp"

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

	bool load_sound_data_from_file (const char* filepath, AudioData16* data);
}

class AudioManager {
public:
	const std::string sounds_directory = "sounds/";
	
	struct Sound {
		audio::AudioData16 data;
		float volume;
		float speed;
	};

	std::unordered_map<std::string, std::unique_ptr<Sound>> loaded_sounds;

	bool enabled = true;
	float master_volume = .5f;

	std::string filename_to_load;
	std::string errtext;

	void imgui () {
		if (!ImGui::CollapsingHeader("Audio")) return;

		ImGui::Checkbox("enable", &enabled);
		ImGui::SliderFloat("master_volume", &master_volume, 0, 1.2f);

		if (ImGui::TreeNode("loaded_sounds")) {
			for (auto& kv : loaded_sounds) {
				ImGui::Text(kv.first.c_str());
				ImGui::SameLine();
				if (ImGui::Button("Play"))
					play_sound(kv.second.get(), 1, 1);
			}
			ImGui::TreePop();
		}

		ImGui::InputText("filename_to_load", &filename_to_load);
		if (ImGui::Button("Load")) {
			bool success = load_sound(filename_to_load, 1, 1) != nullptr;
			errtext = success ? "success" : "error";
		}
		ImGui::SameLine();
		ImGui::Text(errtext.c_str());
	}

	Sound* load_sound (std::string name, float volume, float speed) {
		auto it = loaded_sounds.find(name);
		if (it != loaded_sounds.end()) {
			return it->second.get();
		}

		bool abs_path = name.size() > 0 && name[0] == '/';

		auto filepath = prints("%s%s.wav", abs_path ? "" : sounds_directory.c_str(), name.c_str() + (abs_path ? 1:0));
		audio::AudioData16 data;
		if (!audio::load_sound_data_from_file(filepath.c_str(), &data)) {
			return nullptr;
		}

		auto ptr = std::make_unique<Sound>(std::move(Sound{ std::move(data), volume, speed }));

		auto* s = ptr.get();
		loaded_sounds.emplace(std::move(name), std::move(ptr));
		return s;
	}

	void update ();
	void play_sound (Sound* sound, float volume, float speed);
};

// Global audio manager
extern AudioManager audio_manager;

struct Sound {
	AudioManager::Sound* sound = nullptr;

	Sound () {}
	Sound (std::string name, float volume=1, float speed=1) {
		sound = audio_manager.load_sound(std::move(name), volume, speed);
	}

	void play (float volume=1, float speed=1) {
		audio_manager.play_sound(sound, volume, speed);
	}
};
