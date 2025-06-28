#include "common.hpp"
#include "audio.hpp"
#include "game.hpp"

// Include implementation directly to minimize TUs
#include "miniaudio.c"

// Core problem: miniaudio does not have any way to play the same loaded sound multiple times
// Likely because each ma_sound structure represents a playing sound instance with associated volume, pitch etc data
// Also a complication is that ma_sound needs a stable address (I believe it's linked into a graph that the audio thread reads)
// Manually having to create unique sound instances for each sound that may play at once is not convenient, so I'd like this API:
// LoadedSound.play_once(volume, pitch etc.) // Plays it once and can be called as many times as needed
// LoadedSound.play(set_volume=1, set_pitch=1, loop=false) // etc.
// LoadedSound.stop()
// LoadedSound.set_volume() // change volume of playing sound etc.
// If looping sound needs to exist twice simply specify make a copy

// The only reasonable way to automatically handle overlapping sound using a single object is to allocate the sound structures somewhere else,
// play them and recycle them when they are done, this is what ma_engine_play_sound already implements though I'm not sure how efficient it is
// I'd prefer to add my own wrapper, but I'm really unsure how the library handles the thread safety

// Derived from ma_engine_play_sound_ex
// For some reason despite essentially just pooling memory for ma_sounds, which support volume/pitch etc.
// they decided to not support setting volume/pitch on these 'inlined' sounds
MA_API ma_result sound_play_once (ma_engine* pEngine, ma_sound* copy_from_sound, float volume, float pitch)
{
    ma_result result = MA_SUCCESS;
    ma_sound_inlined* pSound = NULL;
    ma_sound_inlined* pNextSound = NULL;

    ma_node* pNode = ma_node_graph_get_endpoint(&pEngine->nodeGraph);
    ma_uint32 nodeInputBusIndex = 0;

    /*
    We want to check if we can recycle an already-allocated inlined sound. Since this is just a
    helper I'm not *too* concerned about performance here and I'm happy to use a lock to keep
    the implementation simple. Maybe this can be optimized later if there's enough demand, but
    if this function is being used it probably means the caller doesn't really care too much.

    What we do is check the atEnd flag. When this is true, we can recycle the sound. Otherwise
    we just keep iterating. If we reach the end without finding a sound to recycle we just
    allocate a new one. This doesn't scale well for a massive number of sounds being played
    simultaneously as we don't ever actually free the sound objects. Some kind of garbage
    collection routine might be valuable for this which I'll think about.
    */
    ma_spinlock_lock(&pEngine->inlinedSoundLock);
    {
        ma_uint32 soundFlags = 0;

        for (pNextSound = pEngine->pInlinedSoundHead; pNextSound != NULL; pNextSound = pNextSound->pNext) {
            if (ma_sound_at_end(&pNextSound->sound)) {
                /*
                The sound is at the end which means it's available for recycling. All we need to do
                is uninitialize it and reinitialize it. All we're doing is recycling memory.
                */
                pSound = pNextSound;
                ma_atomic_fetch_sub_32(&pEngine->inlinedSoundCount, 1);
                break;
            }
        }

        if (pSound != NULL) {
            /*
            We actually want to detach the sound from the list here. The reason is because we want the sound
            to be in a consistent state at the non-recycled case to simplify the logic below.
            */
            if (pEngine->pInlinedSoundHead == pSound) {
                pEngine->pInlinedSoundHead =  pSound->pNext;
            }

            if (pSound->pPrev != NULL) {
                pSound->pPrev->pNext = pSound->pNext;
            }
            if (pSound->pNext != NULL) {
                pSound->pNext->pPrev = pSound->pPrev;
            }

            /* Now the previous sound needs to be uninitialized. */
            ma_sound_uninit(&pNextSound->sound);
        } else {
            /* No sound available for recycling. Allocate one now. */
            pSound = (ma_sound_inlined*)ma_malloc(sizeof(*pSound), &pEngine->allocationCallbacks);
        }

        if (pSound != NULL) {   /* Safety check for the allocation above. */
            /*
            At this point we should have memory allocated for the inlined sound. We just need
            to initialize it like a normal sound now.
            */
            //soundFlags |= MA_SOUND_FLAG_ASYNC;                 /* For inlined sounds we don't want to be sitting around waiting for stuff to load so force an async load. */
            soundFlags |= MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT; /* We want specific control over where the sound is attached in the graph. We'll attach it manually just before playing the sound. */
            //soundFlags |= MA_SOUND_FLAG_NO_PITCH;              /* Pitching isn't usable with inlined sounds, so disable it to save on speed. */
            //soundFlags |= MA_SOUND_FLAG_NO_SPATIALIZATION;     /* Not currently doing spatialization with inlined sounds, but this might actually change later. For now disable spatialization. Will be removed if we ever add support for spatialization here. */

            result = ma_sound_init_copy(pEngine, copy_from_sound, soundFlags, NULL, &pSound->sound);
            if (result == MA_SUCCESS) {
                /* Now attach the sound to the graph. */
                result = ma_node_attach_output_bus(pSound, 0, pNode, nodeInputBusIndex);
                if (result == MA_SUCCESS) {
                    /* At this point the sound should be loaded and we can go ahead and add it to the list. The new item becomes the new head. */
                    pSound->pNext = pEngine->pInlinedSoundHead;
                    pSound->pPrev = NULL;

                    pEngine->pInlinedSoundHead = pSound;    /* <-- This is what attaches the sound to the list. */
                    if (pSound->pNext != NULL) {
                        pSound->pNext->pPrev = pSound;
                    }
                } else {
                    ma_free(pSound, &pEngine->allocationCallbacks);
                }
            } else {
                ma_free(pSound, &pEngine->allocationCallbacks);
            }
        } else {
            result = MA_OUT_OF_MEMORY;
        }
    }
    ma_spinlock_unlock(&pEngine->inlinedSoundLock);

    if (result != MA_SUCCESS) {
        return result;
    }

	ma_sound_set_volume(&pSound->sound, volume);
	ma_sound_set_pitch(&pSound->sound, pitch);

    /* Finally we can start playing the sound. */
    result = ma_sound_start(&pSound->sound);
    if (result != MA_SUCCESS) {
        /* Failed to start the sound. We need to mark it for recycling and return an error. */
        ma_atomic_exchange_32(&pSound->sound.atEnd, MA_TRUE);
        return result;
    }

    ma_atomic_fetch_add_32(&pEngine->inlinedSoundCount, 1);
    return result;
}

////
class AudioEngine {
friend class AudioManager;
friend class Sound;
	ma_engine engine;

public:
	AudioEngine () {
		auto engineConfig = ma_engine_config_init();
		//engineConfig.listenerCount = 1; // default

		if (ma_engine_init(&engineConfig, &engine) != MA_SUCCESS) {
			log("Audio Engine failed to init! (ma_engine_init)");
			return;
		}
	}
	~AudioEngine () {
		ma_engine_uninit(&engine);
	}
};

AudioManager::AudioManager () {
	engine = std::make_unique<AudioEngine>();

	update_volumes();
}
AudioManager::~AudioManager () {}

void AudioManager::update_volumes () {
	ma_engine_set_volume(&engine->engine, global_volume);
}


Sound::Sound (std::string const& name, bool looping, float volume, float pitch) {
	ZoneScoped;
	log("Loading sound %s", name.c_str());
	
	auto* engine = &g->audio->engine->engine;
	auto filepath = g->audio->sounds_directory + name;
	sound = new ma_sound();
	auto res = ma_sound_init_from_file(engine, filepath.c_str(), MA_SOUND_FLAG_DECODE, NULL, NULL, sound);
	if (res != MA_SUCCESS) {
		delete sound;
		sound = nullptr;
		return;
	}

	set_volume(volume);
	set_pitch(pitch);
	set_looping(looping);
}
Sound::~Sound () {
	if (sound) ma_sound_uninit(sound);
}

void Sound::play_once (float volume, float pitch) {
	if (!sound) return;
	ZoneScoped;
	auto* engine = &g->audio->engine->engine;
	sound_play_once(engine, sound, volume, pitch);
}

void Sound::set_volume (float volume) {
	if (!sound) return;
	ma_sound_set_volume(sound, volume);
}
void Sound::set_pitch (float pitch) {
	if (!sound) return;
	ma_sound_set_pitch(sound, pitch);
}
void Sound::set_looping (bool looping) {
	if (!sound) return;
	ma_sound_set_looping(sound, looping);
}

void Sound::play () {
	if (!sound) return;
	ma_sound_start(sound);
}
void Sound::play (float volume, float pitch) {
	if (!sound) return;
	if (volume >= 0) set_volume(volume);
	if (pitch >= 0) set_pitch(pitch);
	play();
}
void Sound::stop () {
	if (!sound) return;
	ma_sound_stop(sound);
}
void Sound::set_playing (bool playing) {
	if (!sound) return;
	if ((ma_sound_is_playing(sound) != 0) != playing) {
		if (playing) ma_sound_start(sound);
		else         ma_sound_stop(sound);
	}
}

SoundSet::SoundSet (std::string const& name_format, int max_index) {
	if (max_index < 0) {
		Directory dir;
		if (kiss::read_directory(g->audio->sounds_directory, &dir, name_format + "*.wav")) {
			for (auto& file : dir.filenames) {
				sounds.push_back(Sound( file ));
			}
		}
	}
	else {
		for (int i=0; i<max_index; i++) {
			sounds.push_back(Sound( prints("%s%d", name_format.c_str()) ));
		}
	}
}
