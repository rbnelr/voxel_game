#include "common.hpp"
#include "game.hpp"
#include "engine/window.hpp"
#include "kisslib/threadpool.hpp"

Game::Game () {
	ZoneScoped;

	//set_process_high_priority();
	set_thread_priority(TPRIO_MAIN);
	set_thread_preferred_core(0);
	set_thread_description(">> gameloop");
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

		ImGui::Checkbox("Logger", &g_logger.shown);

		ImGui::SameLine();
		ImGui::Checkbox("ImGui Demo", &g_imgui.show_demo_window);
	}

	{
		ImGui::Spacing();
		if (renderer)
			renderer->screenshot_imgui(I);
		ImGui::Spacing();

		if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
			fps_display.display_fps(window.input.real_dt, window.input.dt);

			//ImGui::Text("Chunks drawn %4d / %4d", world->chunks.chunks.count() - world->chunks.count_culled, world->chunks.chunks.count());
		}

		window.input.imgui();
		
		if (ImGui::CollapsingHeader("Graphics")) {
			if (ImGui::Combo("render_backend", (int*)&g_window.render_backend, "OPENGL\0VULKAN"))
				g_window.switch_render_backend = true;

			if (renderer)
				renderer->graphics_imgui(I);
		}
		//graphics.imgui(world->chunks);

		if (ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen)) {

			if (ImGui::Button("Recreate")) {
				world = std::make_unique<World>(world_gen);
			}

			world_gen.imgui();
			world->chunks.imgui(renderer);
			block_update.imgui();
		}

		{
			bool open = ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen);

			if (open) ImGui::Checkbox("Toggle Flycam [P]", &activate_flycam);
			if (window.input.buttons[KEY_P].went_down) {
				activate_flycam = !activate_flycam;

				if (activate_flycam) {
					float3x3 cam_to_world_rot;
					flycam.calc_world_to_cam_rot(&cam_to_world_rot);

					flycam.cam.pos = world->player.pos + world->player.head_pivot - cam_to_world_rot * float3(0,0,-1) * 2;
				}
			}

			if ((open && ImGui::Button("Respawn Player [Q]")) || window.input.buttons[KEY_Q].went_down) {
				world->player.pos = flycam.cam.pos;
			}

			if (open) ImGui::Checkbox("Creative Mode [C]", &creative_mode);
			if (window.input.buttons[KEY_C].went_down)
				creative_mode = !creative_mode;

			if (open) ImGui::Separator();

			if (open) flycam.imgui("flycam");
			if (open) world->player.imgui("player");

			if (open) ImGui::Separator();
		}

	}

	ImGui::PopItemWidth();
	ImGui::End();
}

void Game::update (Window& window, Input& I) {
	ImGui::Begin("Debug");

	g_debugdraw.clear();

	if (!activate_flycam) {
		world->player.update_movement_controls(I, *world);
	}

	physics.update_player(I.dt, *world, world->player);

	player_view = world->player.update_post_physics(I, *world);

	auto& sel = world->player.selected_block;
	if (sel)
		ImGui::Text("Selected Block: (%+4d, %+4d, %+4d) %s", sel.hit.pos.x, sel.hit.pos.y, sel.hit.pos.z, g_assets.block_types[sel.hit.bid].name.c_str());
	else
		ImGui::Text("Selected Block: None");

	if (activate_flycam) {
		view = flycam.update(I);
	} else {
		view = player_view;
	}

	update_block_edits(I, *world, view, creative_mode);

	block_update.update_blocks(I, world->chunks);

	world->chunks.update_chunk_loading(*world, world->player);
	world->chunks.update_chunk_meshing(*world);

	if (activate_flycam)
		g_debugdraw.cylinder(world->player.pos, world->player.radius, world->player.height, lrgba(1,0,1,0.5f));

	ImGui::End();
}
