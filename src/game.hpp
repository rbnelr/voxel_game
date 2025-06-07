#pragma once
#include "common.hpp"
#include "engine/camera.hpp"
#include "world_generator.hpp"
#include "block_update.hpp"
#include "player.hpp"
#include "assets.hpp"

struct Game {
#define SERIALIZE_NORMAL     world_gen, chunks, flycam, player, activate_flycam, imopen

	friend void to_json (nlohmann::ordered_json& j, const Game& t);
	friend void from_json (const nlohmann::ordered_json& j, Game& t);

	struct ImguiOpen {
		SERIALIZE(ImguiOpen, performance, graphics, world, chunks, entities)
		bool performance=true, graphics=true, world=true, chunks=true, entities=true;
	};
	ImguiOpen imopen;

	bool dbg_pause = false;
	Timing_Histogram fps_display;

	WorldGenerator world_gen; // modified by imgui etc.
	WorldGenerator _threads_world_gen; // used in threads, do not modify

	Chunks chunks;

	Flycam flycam = { float3(-5, -10, 50), float3(0, deg(-20), 0), 12 };
	Player player = { float3(0.5f,0.5f,34) };

	BlockUpdate block_update;

	bool activate_flycam = false;
	bool flycam_control_player = false;
	bool player_controls_active () { return !activate_flycam || flycam_control_player; }

	bool creative_mode = false;
	bool trigger_place_block = false;

	// Render data
	Camera_View player_view;
	Camera_View view;

	bool chunk_loading_follow_flycam = true;
	float3 chunk_loading_center () {
		return chunk_loading_follow_flycam && activate_flycam ? flycam.cam.pos : player.pos;
	}

	Game ();
	~Game ();

	void imgui (Window& window, Input& I, Renderer* renderer);
	void update (Window& window, Input& I);

};
