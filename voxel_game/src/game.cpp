#include "stdafx.hpp"
#include "glfw_window.hpp"
#include "game.hpp"
#include "graphics/gl.hpp"
#include "util/threadpool.hpp"

//
Game::Game () {
	set_process_priority();
	set_thread_priority(ThreadPriority::HIGH);
	set_thread_preferred_core(0);
	set_thread_description(">> gameloop");
}

void Game::frame () {

	ImGui::Begin("Debug");
	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);

	{
		bool borderless_fullscreen;
		bool fullscreen = get_fullscreen(&borderless_fullscreen);
		bool changed = false;

		changed = ImGui::Checkbox("fullscreen", &fullscreen);
		ImGui::SameLine();
		changed = ImGui::Checkbox("borderless", &borderless_fullscreen) || changed;
		
		if (changed)
			switch_fullscreen(fullscreen, borderless_fullscreen);

		ImGui::SameLine();
		bool vsync = get_vsync();
		if (ImGui::Checkbox("Vsync", &vsync)) {
			set_vsync(vsync);
		}

		ImGui::SameLine();
		if (ImGui::Button("exit")) {
			glfwSetWindowShouldClose(window, 1);
		}

		ImGui::Checkbox("Debug Pause", &dbg_pause);

		ImGui::SameLine();
		ImGui::Checkbox("Console", &gui_console.shown);

		ImGui::SameLine();
		ImGui::Checkbox("ImGui Demo", &imgui.show_demo_window);
	}

	if (!dbg_pause) {
		auto changed_files = directory_watcher.poll_changes();

		shaders->reload_shaders_on_change();

		{
			ZoneScopedN("Imgui stuff");

			ImGui::Separator();

			graphics.trigger_screenshot = ImGui::Button("Screenshot [F8]") || input.buttons[GLFW_KEY_F8].went_down;
			ImGui::SameLine();
			ImGui::Checkbox("With HUD", &graphics.screenshot_hud);

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
				fps_display.display_fps();
			}

			input.imgui();
			audio_manager.imgui();
			graphics.imgui(/*world->chunks*/);

			bool world_open = ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen);

				
			// Reload world on button, worldgen hot-reload or intially
			bool trigger_reload = changed_files.contains("hot_reload.txt", kiss::FILE_MODIFIED);

			if (	(world_open && ImGui::Button("Recreate")) ||
					worldgen_dll.h == NULL || trigger_reload) {
				clog(INFO, "Recreating world%s", trigger_reload ? "" : " due to worldgen.dll recompile");
				world = nullptr; // unload world before reloading dll, this shuts down the threadpool which might still be calling the worldgen function in the dll

				world = std::make_unique<World>( WorldGenerator::load("test_world") );

				// reload worldgen dll and get the function dynamically
				worldgen_dll.reload();
				world->world_gen.generate_chunk_dll = worldgen_dll.h ?
					(generate_chunk_dll_fp)GetProcAddress(worldgen_dll.h, "generate_chunk_dll") : nullptr;
				if (world->world_gen.generate_chunk_dll == nullptr) {
					clog(ERROR, "GetProcAddress failed");
				}

				// reload the world using the potentially new worldgen dll
			}

			if (world_open) {
				world->world_gen.imgui();
				world->imgui();
				block_update.imgui();
			}

			{
				bool open = ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen);
		
				if (open) ImGui::Checkbox("Toggle Flycam [P]", &activate_flycam);
				if (input.buttons[GLFW_KEY_P].went_down) {
					activate_flycam = !activate_flycam;

					if (activate_flycam) {
						float3x3 cam_to_world_rot;
						flycam.calc_world_to_cam_rot(&cam_to_world_rot);

						flycam.pos = world->player.pos + world->player.head_pivot - cam_to_world_rot * float3(0,0,-1) * 2;
					}
				}

				if (open) ImGui::Checkbox("Toggle Creative Mode [C]", &creative_mode);
				if (input.buttons[GLFW_KEY_C].went_down)
					creative_mode = !creative_mode;

				if ((open && ImGui::Button("Teleport Player to Flycam [Q]")) || input.buttons[GLFW_KEY_Q].went_down) {
					world->player.pos = flycam.pos;
				}

				if (open) ImGui::Separator();

				if (open) flycam.imgui("flycam");
				if (open) world->player.imgui("player");

				if (open) ImGui::Separator();
			}

		}

		audio_manager.update();

		if (!activate_flycam) {
			world->player.update_movement_controls(*world);
		}

		physics.update_player(*world, world->player);

		Camera_View player_view = world->player.update_post_physics(*world);

		Camera_View view;
		if (activate_flycam) {
			view = flycam.update();
		} else {
			view = player_view;
		}

		if (!activate_flycam || creative_mode)
			update_block_edits(*world, view, graphics.player, creative_mode);

		auto& sel = world->player.selected_block;
		if (sel)
			ImGui::Text("Selected Block: (%+4d, %+4d, %+4d) [%3d] %s %.3f damage", sel.pos.x, sel.pos.y,sel.pos.z, sel.block, blocks.name[sel.block], sel.damage);
		else
			ImGui::Text("Selected Block: None");

		//block_update.update_blocks(world->chunks);

		world->voxels.svo.update_chunk_loading_and_meshing(*world, graphics);

		//// Draw
		graphics.draw(*world, view, player_view, activate_flycam, creative_mode, sel);
	}
	ImGui::PopItemWidth();
	ImGui::End();

	gui_console.imgui();
}
