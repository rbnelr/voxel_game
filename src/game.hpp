#pragma once
#include "common.hpp"
#include "world_generator.hpp"
#include "world.hpp"
#include "block_update.hpp"
#include "engine/camera.hpp"
#include "graphics.hpp"

struct Game {
	bool dbg_pause = false;

	FPS_Display fps_display;

	// Global world gen I can tweak (changes are only visible on world recreate)
	WorldGenerator world_gen;

	// World gets world gen copy on create 
	std::unique_ptr<World> world = std::make_unique<World>(world_gen);

	BlockUpdate block_update;

	Flycam flycam = { float3(-5, -10, 50), float3(0, deg(-20), 0), 12 };

	bool activate_flycam = false;
	bool trigger_place_block = false;

	Assets assets;
	
	Game ();

	void imgui (Window& window, Input& I);
	RenderData update (Window& window, Input& I);
};
