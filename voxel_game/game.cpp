#include "glfw_window.hpp"
#include "game.hpp"
#include "graphics/gl.hpp"
#include "util/threadpool.hpp"

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
	set_thread_description(">> gameloop");
	set_high_thread_priority();
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

		if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
			fps_display.display_fps();

			ImGui::Text("Chunk generation : %7.2f us avg", world->chunks.chunk_gen_time.calc_avg() * 1000 * 1000);
			ImGui::Text("Chunk light      : %7.2f us avg", world->chunks.block_light_time.calc_avg() * 1000 * 1000);
			ImGui::Text("Chunk meshing    : %7.2f us avg", world->chunks.meshing_time.calc_avg() * 1000 * 1000);

			ImGui::Text("Chunks drawn %4d / %4d", world->chunks.chunks.count() - world->chunks.count_culled, world->chunks.chunks.count());
		}

		input.imgui();
		graphics.imgui(world->chunks);

		if (ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen)) {
		
			if (ImGui::Button("Recreate")) {
				world = std::make_unique<World>(world_gen);
			}

			world_gen.imgui();
			world->imgui();
			block_update.imgui();
			world->chunks.world_octree.imgui();
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

		world->chunks.update_chunk_loading(*world, world_gen, world->player);

		if (!activate_flycam) {
			world->player.update_movement_controls(*world);
		}

		physics.update_player(*world, world->player);

		SelectedBlock selected_block;
		Camera_View player_view = world->player.update_post_physics(*world, graphics.player, !activate_flycam, &selected_block);

		if (selected_block)
			ImGui::Text("Selected Block: (%+4d, %+4d, %+4d) %s", selected_block.pos.x, selected_block.pos.y, selected_block.pos.z, blocks.name[selected_block.block.id]);
		else
			ImGui::Text("Selected Block: None");

		Camera_View view;
		if (activate_flycam) {
			view = flycam.update();
		} else {
			view = player_view;
		}

		block_update.update_blocks(world->chunks);

		world->chunks.update_chunks(graphics, world->world_gen, world->player);

		world->chunks.world_octree.update();

		//// Draw
		graphics.draw(*world, view, player_view, activate_flycam, selected_block);
	}
	ImGui::PopItemWidth();
	ImGui::End();

	gui_console.imgui();
}
