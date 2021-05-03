#include "common.hpp"
#include "game.hpp"
#include "engine/window.hpp"
#include "kisslib/threadpool.hpp"

void to_json (nlohmann::ordered_json& j, const Game& t) {
	_JSON_EXPAND(_JSON_PASTE(_JSON_TO, SERIALIZE_NORMAL))
		if (g_window.render_backend == RenderBackend::OPENGL)
			g_window.renderer->serialize(j["renderer_opengl"]);
}
void from_json (const nlohmann::ordered_json& j, Game& t) {
	_JSON_EXPAND(_JSON_PASTE(_JSON_FROM, SERIALIZE_NORMAL))
		if (g_window.render_backend == RenderBackend::OPENGL && j.contains("renderer_opengl"))
				g_window.renderer->deserialize(j["renderer_opengl"]);
}

Game::Game () {
	ZoneScoped;
	load("debug.json", this); 
	
	//set_process_high_priority();
	set_thread_priority(TPRIO_MAIN);
	set_thread_preferred_core(0);
	set_thread_description(">> gameloop");

	_threads_world_gen = world_gen; // apply changes loaded by load("debug.json")
}
Game::~Game () {
	ZoneScoped;
	//save("debug.json", *this); 
}

// try to do all debug guis in here,
// but don't call ImGui::End() yet so we can quickly display values from inside algorithms if we want
void Game::imgui (Window& window, Input& I, Renderer* renderer) {
	ZoneScoped;

	ImGui::Begin("Debug");
	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);

	{ // Various controls at the top
		{
			bool fullscreen = window.fullscreen;
			bool borderless_fullscreen = window.borderless_fullscreen;
			
			bool changed = ImGui::Checkbox("fullscreen", &fullscreen);
			ImGui::SameLine();
			changed = ImGui::Checkbox("borderless", &borderless_fullscreen) || changed;

			if (changed)
				window.switch_fullscreen(fullscreen, borderless_fullscreen);
		}

		ImGui::SameLine();
		bool vsync = window.renderer->get_vsync();
		if (ImGui::Checkbox("Vsync", &vsync)) {
			window.renderer->set_vsync(vsync);
		}

		ImGui::SameLine();
		if (ImGui::Button("exit"))
			window.close();

		ImGui::SameLine();
		ImGui::Checkbox("Logger", &g_logger.shown);

		ImGui::SameLine();
		ImGui::Checkbox("ImGui Demo", &g_imgui.show_demo_window);

		//
		ImGui::Text("debug.json:");
		ImGui::SameLine();
		if (ImGui::Button("Load [;]") || I.buttons[KEY_SEMICOLON].went_down)
			load("debug.json", this); 
		ImGui::SameLine();
		if (ImGui::Button("Save [']") || I.buttons[KEY_APOSTROPHE].went_down)
			save("debug.json", *this); 
	}

	{
		ImGui::Spacing();
		if (renderer)
			renderer->screenshot_imgui(I);
		ImGui::Spacing();

		if (imgui_header("Performance", &imopen.performance)) {
			fps_display.display_fps(window.input.real_dt, window.input.dt);

			//ImGui::Text("Chunks drawn %4d / %4d", world->chunks.chunks.count() - world->chunks.count_culled, world->chunks.chunks.count());
		}

		window.input.imgui();
		
		if (imgui_header("Graphics", &imopen.graphics)) {
			//if (ImGui::Combo("render_backend", (int*)&g_window.render_backend, "OPENGL"))
			//	g_window.switch_render_backend = true;

			if (renderer)
				renderer->graphics_imgui(I);
		}

		if (imgui_header("World", &imopen.world)) {

			if (ImGui::Button("Recreate")) {
				chunks.destroy();
				_threads_world_gen = world_gen;
			}

			world_gen.imgui();
		}

		if (imgui_header("Chunks", &imopen.chunks)) {
			chunks.imgui(renderer);
			block_update.imgui();
		}

		{
			bool open = imgui_header("Misc", &imopen.entities);

			if (open) ImGui::Checkbox("Toggle Flycam [P]", &activate_flycam);
			if (window.input.buttons[KEY_P].went_down) {
				activate_flycam = !activate_flycam;

				if (activate_flycam) {
					float3x3 cam_to_world_rot;
					flycam.calc_world_to_cam_rot(&cam_to_world_rot);

					flycam.cam.pos = player.pos + player.head_pivot - cam_to_world_rot * float3(0,0,-1) * 2;
				}
			}

			if ((open && ImGui::Button("Respawn Player [Q]")) || window.input.buttons[KEY_Q].went_down) {
				player.pos = flycam.cam.pos;
			}

			if (open) ImGui::Checkbox("Creative Mode [C]", &creative_mode);
			if (window.input.buttons[KEY_C].went_down)
				creative_mode = !creative_mode;

			if (open) ImGui::Separator();

			if (open) flycam.imgui("flycam");
			if (open) player.imgui("player");

			if (open) ImGui::Separator();
		}

	}

	ImGui::PopItemWidth();
	ImGui::End();
}

void Game::update (Window& window, Input& I) {
	ImGui::Begin("Debug");

	g_debugdraw.clear();

	player.update_controls(I, *this);

	player.update_movement(I, *this);
	physics.update_player(I.dt, chunks, player);

	player.update(I, I.window_size, *this);

	auto& sel = player.selected_block;
	if (sel)
		ImGui::Text("Selected Block: (%+4d, %+4d, %+4d) %s", sel.hit.pos.x, sel.hit.pos.y, sel.hit.pos.z, g_assets.block_types[sel.hit.bid].name.c_str());
	else
		ImGui::Text("Selected Block: None");

	//_dev_raycast(chunks, player_view);

	block_update.update_blocks(I, chunks);

	chunks.update_chunk_loading(*this);
	chunks.update_chunk_meshing(*this);

	if (activate_flycam || player.third_person)
		g_debugdraw.cylinder(player.pos, player.radius, player.height, lrgba(1,0,1,0.5f));
	if (activate_flycam)
		g_debugdraw.axis_gizmo(view, I.window_size);

	ImGui::End();
}

//
bool Game::raycast_breakable_blocks (Ray const& ray, float max_dist, VoxelHit& hit, bool hit_at_max_dist) {
	ZoneScoped;

	bool did_hit = false;

	raycast_voxels(chunks, ray, [&] (int3 const& pos, int axis, float dist) -> bool {
		//g_debugdraw.wire_cube((float3)pos+0.5f, 1, lrgba(1,0,0,1));

		hit.pos = pos;
		hit.bid = chunks.read_block(pos.x, pos.y, pos.z);

		if (dist > max_dist) {
			if (hit_at_max_dist) {
				hit.face = (BlockFace)-1; // select block itself instead of face (for creative mode block placing)
				did_hit = true;
			}
			return true;
		}

		if ((g_assets.block_types.block_breakable(hit.bid))) {
			hit.face = (BlockFace)face_from_stepmask(axis, ray.dir);
			did_hit = true;
			return true;
		}
		return false;
	});

	return did_hit;
}
