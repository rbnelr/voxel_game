#pragma once
#include "kisslib/string.hpp"
#include "imgui/dear_imgui.hpp"
#include "kisslib/serialization.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include "kisslib/random.hpp"
using namespace kiss;

// Opaque pointer to hide audio engine from header
class AudioEngine;

// TODO: Test out spatialization

class AudioManager {
public:
	SERIALIZE(AudioManager, global_volume)

	std::unique_ptr<AudioEngine> engine;

	const std::string sounds_directory = "sounds/";

	float global_volume = 1;

	AudioManager ();
	~AudioManager ();

	void update_volumes ();

	void imgui () {
		ImGui::SliderFloat("global_volume", &global_volume, 0, 3);
	}
};

struct ma_sound;

class Sound {
	// address needs to stay stable, so need to heap alloc and make sound class move-only
	MOVE_ONLY_CLASS(Sound)
	ma_sound* sound = nullptr;
public:

	Sound () {}
	// filepath with file extension relative to sound directory
	// looping, volume, pitch: initial values, can be set using set_*()
	Sound (std::string const& name, bool looping=false, float volume=1, float pitch=1);
	~Sound ();

	void play_once (float volume=1, float pitch=1);

	void set_volume (float volume);
	void set_pitch (float pitch);
	void set_looping (bool looping);
	
	void play ();
	// -1 means don't modify
	void play (float set_volume, float set_pitch=-1);
	void stop ();
	void set_playing (bool playing);
};
inline void swap (Sound& l, Sound& r) {
	std::swap(l.sound, r.sound);
}

class SoundSet {
	std::vector<Sound> sounds;
public:

	SoundSet () {}
	// expect name_format like step%d.wav where %d becomes [0,max_index)
	SoundSet (std::string const& name_format, int max_index=-1);

	void play_once (int idx, float volume=1, float pitch=1) {
		if (idx < 0 || idx >= (int)sounds.size()) return;
		sounds[idx].play_once(volume, pitch);
	}
	void play_random_once (float volume=1, float pitch=1) {
		get_random()->play_once(volume, pitch);
	}

	Sound* get_random () {
		if (sounds.empty()) return nullptr;
		int idx = random.uniformi(0, (int)sounds.size());
		return &sounds[idx];
	}
};
