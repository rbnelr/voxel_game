#include "common.hpp"
#include "game.hpp"
#include "engine.hpp"
#include "kisslib/threadpool.hpp"
#include "world_generator.hpp"
#include "block_update.hpp"
#include "chunks.hpp"
#include "player.hpp"

void Game::json_load () {
	serialize::json_load("config.json", [&] (nlohmann::ordered_json& j) {
		auto& t = *this;
		_JSON_EXPAND(_JSON_PASTE(_JSON_FROM, SERIALIZE_NORMAL))
		if (j.contains("renderer_opengl")) renderer->deserialize(j["renderer_opengl"]);
		// Dereference causes object values to be deserialized instead of ptr deserialized, which causes object to be recreated, breaking sound references
		if (j.contains("audio")) j.at("audio").get_to(*t.audio);
	});
}
void Game::json_save () {
	serialize::json_save("config.json", [&] (nlohmann::ordered_json& j) {
		auto& t = *this;
		_JSON_EXPAND(_JSON_PASTE(_JSON_TO, SERIALIZE_NORMAL))
		renderer->serialize(j["renderer_opengl"]);
		j["audio"] = *t.audio;
	});
}

Game::Game (): Engine{"Voxel Project"} {
	ZoneScoped;
	g = this;
	
	audio = std::make_unique<AudioManager>();
	assets = std::make_unique<Assets>( Assets::load() );
	renderer = start_renderer(RenderBackend::OPENGL, *this);

	physics = std::make_unique<Physics>();
	chunks = std::make_unique<Chunks>();

	flycam = std::make_unique<Flycam>();
	player = std::make_unique<Player>();

	world_gen = std::make_unique<WorldGenerator>();
	
	//set_process_high_priority();
	set_thread_priority(TPRIO_MAIN);
	set_thread_preferred_core(0);
	set_thread_description(">> gameloop");

	json_load();

	// apply changes loaded by load("debug.json")
	_threads_world_gen = std::make_unique<WorldGenerator>(*world_gen);
	_threads_world_gen->seed = get_seed(_threads_world_gen->seed_str);
}
Game::~Game () {
	ZoneScoped;
	//save("debug.json", *this);
	g = nullptr;
}

float3 Game::lod_center () {
	return lod_follow_flycam && activate_flycam ? flycam->cam.pos : player->pos;
}

void Game::imgui () {
	ZoneScoped;

	{
		bool open = imgui_Header("Misc", &imopen.entities);

		if (open) ImGui::Checkbox("Toggle Flycam [P]", &activate_flycam);
		if (input.buttons[KEY_P].went_down) {
			activate_flycam = !activate_flycam;

			if (activate_flycam) {
				float3x3 cam_to_world_rot;
				flycam->calc_world_to_cam_rot(&cam_to_world_rot);

				flycam->cam.pos = player->pos + player->head_pivot() - cam_to_world_rot * float3(0,0,-1) * 2;
			}
		}
		if (open) ImGui::Checkbox("flycam_control_player", &flycam_control_player);

		if ((open && ImGui::Button("Respawn Player [Q]")) || input.buttons[KEY_Q].went_down) {
			player->pos = flycam->cam.pos;
			//player->pos = float3(
			//-78.52836608886719f,
			//-46.81139373779297f,
			//-112.0999984741211f
			//);
			player->vel = 0;
		}

		if (open) ImGui::Checkbox("Creative Mode [C]", &creative_mode);
		if (input.buttons[KEY_C].went_down)
			creative_mode = !creative_mode;

		if (open) ImGui::Separator();

		if (open) flycam->imgui("flycam");
		if (open) player->imgui("player");

		if (open) ImGui::Checkbox("lod_follow_flycam", &lod_follow_flycam);

		if (open) ImGui::Separator();
		if (open) ImGui::PopID();
	}

	if (imgui_Header("World", &imopen.world)) {

		if (ImGui::Button("Recreate")) {
			chunks->destroy();
			_threads_world_gen = std::make_unique<WorldGenerator>(*world_gen); // make copy that can safely be used in threads while main version is edited by imgui
			_threads_world_gen->seed = get_seed(_threads_world_gen->seed_str);
		}

		ImGui::InputText("savefile", &world_gen->savefile);
		ImGui::SameLine();
		ImGui::Checkbox("Load", &chunks->load_from_disk);
		ImGui::SameLine();
		if (ImGui::Button("Save"))
			chunks->save_chunks_to_disk(world_gen->savefile.c_str());

		world_gen->imgui();

		ImGui::PopID();
	}

	if (imgui_Header("Physics", &imopen.physics)) {
		physics->imgui();
		ImGui::PopID();
	}
		
	if (imgui_Header("Chunks", &imopen.chunks)) {
		chunks->edits.imgui(input);
		chunks->imgui(renderer.get());
		block_update->imgui();
		ImGui::PopID();
	}

	if (imgui_Header("Graphics", &imopen.graphics)) {
		//if (ImGui::Combo("render_backend", (int*)&g_window.render_backend, "OPENGL"))
		//	g_window.switch_render_backend = true;

		if (renderer)
			renderer->graphics_imgui();
		ImGui::PopID();
	}
		
	if (imgui_Header("Audio", &imopen.audio)) {
		audio->imgui();
		ImGui::PopID();
	}
}

void Game::frame () {
	audio->update_volumes();

	g_debugdraw.clear();

	player->update(input);

	g_debugdraw.prepare_selectables(view, input, input.cursor_enabled);

	auto& sel = player->selected_block;
	if (sel)
		ImGui::Text("Selected Block: (%+4d, %+4d, %+4d) %s", sel.hit.pos.x, sel.hit.pos.y, sel.hit.pos.z, g->assets->block_types[sel.hit.bid].name.c_str());
	else
		ImGui::Text("Selected Block: None");

	//_dev_raycast(chunks, player_view);

	block_update->update_blocks(input);

	chunks->update_chunk_loading();
	
	if (chunks->edits.open)
		chunks->edits.update(input);
	
	chunks->update_chunk_meshing();

	if (activate_flycam) {
		g_debugdraw.axis_gizmo(view, input.window_size);
		g_debugdraw.movable("player", &player->pos, 0.4f, lrgba(0.7f,0,0.7f,1), &player->vel);
	}

	g_debugdraw.finish_selectables();
	g_debugdraw.update(input);

	renderer->render_frame(*this);
}

Game* g = nullptr;

// Note: for windows /subsystem:windows to work we need to also set /entry:mainCRTStartup
// or turn main into WinMain
int main (int argc, char* argv[]) {
	log("Starting application...");
	auto* game = new Game();
	assert(g = game);
	int ret = g->main_loop();
	delete g;
	return ret;
}
