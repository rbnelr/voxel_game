#pragma once
#include "common.hpp"
#include "engine/camera.hpp"

// Game is stored as a global such that systems needed everywhere can be accessed like g->assets->
//  (audio, assets, chunks, etc.)
// The systems are forward declared and put into Game as pointers (instead of included as header and used used by-value)
//  to allow headers to access them though game without game needing the system header
//  (causing circular reference which the C++ compiler throws a hissy fit about despite this being a common pattern in other languages...)
struct Assets;
class AudioManager;
struct Physics;
struct Chunks;
struct WorldGenerator;
struct BlockUpdate;
struct Player;

struct Game {
#define SERIALIZE_NORMAL world_gen, chunks, flycam, player, activate_flycam, imopen, lod_follow_flycam

	friend void to_json (nlohmann::ordered_json& j, const Game& t);
	friend void from_json (const nlohmann::ordered_json& j, Game& t);

	struct ImguiOpen {
		SERIALIZE(ImguiOpen, performance, world, chunks, entities, graphics, audio)
		bool performance=true, world=false, chunks=false, entities=false, graphics=false, audio=false;
	};
	ImguiOpen imopen;

	std::unique_ptr<AudioManager> audio;
	std::unique_ptr<Assets> assets;

	bool dbg_pause = false;
	Timing_Histogram fps_display;

	std::unique_ptr<WorldGenerator> world_gen; // modified by imgui etc.
	std::unique_ptr<WorldGenerator> _threads_world_gen; // used in threads, do not modify

	std::unique_ptr<Physics> physics;
	std::unique_ptr<Chunks> chunks;

	std::unique_ptr<Flycam> flycam;
	std::unique_ptr<Player> player;

	std::unique_ptr<BlockUpdate> block_update;

	bool activate_flycam = false;
	bool flycam_control_player = false;
	bool player_controls_active () { return !activate_flycam || flycam_control_player; }

	bool creative_mode = false;
	bool trigger_place_block = false;

	// Render data
	Camera_View player_view;
	Camera_View view;

	bool lod_follow_flycam = true;
	float3 lod_center ();

	Game ();
	void init (); // Called after renderer is inited, since renderer wants access to game.assets
	~Game ();
	
	void imgui (Window& window, Input& I, Renderer* renderer);
	void update (Window& window, Input& I);
};

extern Game* g;
