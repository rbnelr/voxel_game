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
		{
			ZoneScopedN("Imgui stuff");

			if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
				fps_display.display_fps();
			}

			input.imgui();
			audio_manager.imgui();
			graphics.imgui(/*world->chunks*/);
			debug_graphics->imgui();

			if (ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen)) {
		
				if (ImGui::Button("Recreate")) { // TODO: not safe right now. doing this while chunks are still being generated async will crash or worse
					world = std::make_unique<World>(world_gen);
				}

				world_gen.imgui();
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

				if (open) ImGui::DragFloat3("player_spawn_point", &player_spawn_point.x, 0.2f);
				if ((open && ImGui::Button("Respawn Player [Q]")) || input.buttons[GLFW_KEY_Q].went_down) {
					world->player.respawn();
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

		world->voxels.svo.update_chunk_loading_and_meshing(world->voxels, world->player, world_gen, graphics);

		//// Draw
		graphics.draw(*world, view, player_view, activate_flycam, creative_mode, sel);
	}
	ImGui::PopItemWidth();
	ImGui::End();

	gui_console.imgui();
}
