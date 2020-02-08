#include "glfw_window.hpp"
#include "game.hpp"
#include "graphics/gl.hpp"

//
bool FileExists (const char* path) {
	DWORD dwAttrib = GetFileAttributes(path);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
bool _need_potatomode () {
	return FileExists("../../._need_potatomode.txt");
}
bool _use_potatomode = _need_potatomode();

//
Game::Game () {
	{ // GL state
		glEnable(GL_FRAMEBUFFER_SRGB);

		glEnable(GL_DEPTH_TEST);
		glClearDepth(1.0f);
		glDepthFunc(GL_LEQUAL);
		glDepthRange(0.0f, 1.0f);
		glDepthMask(GL_TRUE);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);

		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}
}

void Game::frame () {

	ImGui::Begin("Debug");
	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);

	{
		bool fullscreen = get_fullscreen();
		if (ImGui::Checkbox("fullscreen", &fullscreen)) {
			toggle_fullscreen();
		}

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

		fps_display.display_fps();
	}

	if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Chunk generation : %7.2f ms avg", world_gen.chunk_gen_time.calc_avg() * 1000);
		ImGui::Text("Chunk brightness : %7.2f ms avg", world->chunks.brightness_time.calc_avg() * 1000);
		ImGui::Text("Chunk meshing    : %7.2f ms avg", world->chunks.meshing_time.calc_avg() * 1000);

		ImGui::Text("Chunks drawn %4d / %4d", world->chunks.count() - world->chunks.count_frustrum_culled, world->chunks.count());
	}

	if (!dbg_pause) {
		input.imgui();
		graphics.imgui(world->chunks);

		{
			bool open = ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen);
		
			if (open) {
				static std::string world_seed = world->seed_str;
				ImGui::InputText("seed", &world_seed, 0, NULL, NULL);

				ImGui::SameLine();
				if (ImGui::Button("Recreate")) {
					world = std::make_unique<World>(world_seed);
				}
			}

			world->imgui(open);

			world_gen.imgui();
			world->chunks.imgui();
			block_update.imgui();
		}

		{
			bool open = ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen);
		
			if (open) ImGui::Checkbox("Toggle Flycam [P]", &activate_flycam);
			if (input.buttons[GLFW_KEY_P].went_down)
				activate_flycam = !activate_flycam;

			if (open) ImGui::DragFloat3("player_spawn_point", &player_spawn_point.x, 0.2f);
			if ((open && ImGui::Button("Respawn Player [Q]")) || input.buttons[GLFW_KEY_Q].went_down) {
				world->player.respawn();
			}

			if (open) ImGui::Separator();

			if (open) flycam.imgui("flycam");
			if (open) world->player.imgui("player");

			if (open) ImGui::Separator();
		}

		world->update(world_gen);

		if (!activate_flycam) {
			world->player.update_movement_controls(*world);
		}

		physics.update_player(*world, world->player);

		SelectedBlock selected_block;

		Camera_View player_view = world->player.update_post_physics(*world, graphics.player, !activate_flycam, &selected_block);

		Camera_View view;
		if (activate_flycam) {
			view = flycam.update();
		} else {
			view = player_view;
		}

		block_update.update_blocks(world->chunks);
		world->chunks.update_chunks_brightness();

		world->chunks.update_chunk_graphics(graphics.chunk_graphics);

		//// Draw
		graphics.draw(*world, view, player_view, activate_flycam, selected_block);
	}
	ImGui::PopItemWidth();
	ImGui::End();

	gui_console.imgui();
}
