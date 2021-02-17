#pragma once
#include "common.hpp"
#include "world_generator.hpp"
#include "block_update.hpp"
#include "engine/camera.hpp"
#include "player.hpp"
#include "assets.hpp"

struct Game {
	bool dbg_pause = false;
	FPS_Display fps_display;

	WorldGenerator world_gen;
	WorldGenerator _threads_world_gen = world_gen; // used in threads, do not modify

	Chunks chunks;

	Flycam flycam = { float3(-5, -10, 50), float3(0, deg(-20), 0), 12 };
	Player player = { float3(0.5f,0.5f,34) };

	BlockUpdate block_update;

	Sound break_sound = { "dig1", 1.2f, 0.8f };

	SERIALIZE(Game, flycam, player, activate_flycam, chunks)

	bool activate_flycam = false;
	bool creative_mode = false;
	bool trigger_place_block = false;

	// Render data
	Camera_View player_view;
	Camera_View view;

	Game ();
	~Game ();

	void imgui (Window& window, Input& I, Renderer* renderer);
	void update (Window& window, Input& I);

	//
	void raycast_breakable_blocks (SelectedBlock& block, Ray const& ray, float max_dist, bool hit_at_max_dist=false);

	void apply_damage (SelectedBlock& block, Item& item, bool creative_mode);
	bool try_place_block (int3 pos, block_id id);

};
