#pragma once
#include "common.hpp"
#include "camera.hpp"

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
class Player;
class Renderer;

class Game : public Engine {
	#define SERIALIZE_NORMAL world_gen, chunks,\
		cam_binds, flycam, player, activate_flycam, lod_follow_flycam,\
		imopen
public:
	Game ();
	~Game ();

	virtual void json_load ();
	virtual void json_save ();

	struct ImguiOpen {
		SERIALIZE(ImguiOpen, performance, world, chunks, physics, entities, graphics, audio)
		bool performance=true, world=false, chunks=false, physics=false, entities=false, graphics=false, audio=false;
	};
	ImguiOpen imopen;
	
	std::unique_ptr<AudioManager> audio; // has to go before assets
	std::unique_ptr<Assets> assets;      // has to go before renderer
	std::unique_ptr<Renderer> renderer;

	bool dbg_pause = false;
	Timing_Histogram fps_display;

	std::unique_ptr<WorldGenerator> world_gen; // modified by imgui etc.
	std::unique_ptr<WorldGenerator> _threads_world_gen; // used in threads, do not modify

	std::unique_ptr<Physics> physics;
	std::unique_ptr<Chunks> chunks;

	CameraBinds cam_binds;
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

	virtual void imgui ();
	virtual void frame ();
	
	virtual bool update_files_changed (kiss::ChangedFiles& changed_files) {
		bool success = renderer->update_files_changed(changed_files);
		//if (changed_files.any_starts_with("assets")) {
		//	//assets.reload_all();
		//	//renderer->reload_textures(changed_files);
		//}
		//
		return success;
	}
};

extern Game* g;
