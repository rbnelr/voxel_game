#include "common.hpp"
#include "game.hpp"
#include "engine/window.hpp"
#include "kisslib/threadpool.hpp"

Game::Game () {
	ZoneScoped;
	load("debug.json", this); 
	
	//set_process_high_priority();
	set_thread_priority(TPRIO_MAIN);
	set_thread_preferred_core(0);
	set_thread_description(">> gameloop");
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

		if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
			fps_display.display_fps(window.input.real_dt, window.input.dt);

			//ImGui::Text("Chunks drawn %4d / %4d", world->chunks.chunks.count() - world->chunks.count_culled, world->chunks.chunks.count());
		}

		window.input.imgui();
		
		if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::Combo("render_backend", (int*)&g_window.render_backend, "OPENGL\0VULKAN"))
				g_window.switch_render_backend = true;

			if (renderer)
				renderer->graphics_imgui(I);
		}

		if (ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen)) {

			if (ImGui::Button("Recreate")) {
				chunks.destroy();
				_threads_world_gen = world_gen;
			}

			world_gen.imgui();
		}

		if (ImGui::CollapsingHeader("Chunks", ImGuiTreeNodeFlags_DefaultOpen)) {
			chunks.imgui(renderer);
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

	if (!activate_flycam) {
		player.update_movement_controls(I, chunks);
	}

	physics.update_player(I.dt, chunks, player);

	player_view = player.update_post_physics(I, chunks);

	auto& sel = player.selected_block;
	if (sel)
		ImGui::Text("Selected Block: (%+4d, %+4d, %+4d) %s", sel.hit.pos.x, sel.hit.pos.y, sel.hit.pos.z, g_assets.block_types[sel.hit.bid].name.c_str());
	else
		ImGui::Text("Selected Block: None");

	if (activate_flycam) {
		view = flycam.update(I);
	} else {
		view = player_view;
	}

	update_block_edits(I, *this, player, view);

	block_update.update_blocks(I, chunks);

	chunks.update_chunk_loading(*this);
	chunks.update_chunk_meshing(*this);

	if (activate_flycam)
		g_debugdraw.cylinder(player.pos, player.radius, player.height, lrgba(1,0,1,0.5f));

	ImGui::End();
}

//
void Game::raycast_breakable_blocks (SelectedBlock& block, Ray const& ray, float max_dist, bool hit_at_max_dist) {
	ZoneScoped;

	block.is_selected = false;

	VoxelHit last_hit;

	auto hit_block = [&] (int3 pos, int face, float dist) -> bool {
		//g_debugdraw.wire_cube((float3)pos+0.5f, 1, lrgba(1,0,0,1));

		block_id bid = chunks.read_block(pos.x, pos.y, pos.z);

		last_hit.bid = bid;
		last_hit.pos = pos;
		last_hit.face = (BlockFace)face;

		if ((g_assets.block_types.block_breakable(bid))) {
			//hit.pos_world = ray.pos + ray.dir * dist;
			block.is_selected = true;
			block.hit = last_hit;
			return true;
		}
		return false;
	};

	raycast_voxels(chunks, ray, max_dist, hit_block);

	if (!block.is_selected && hit_at_max_dist) {
		block.is_selected = true;
		block.hit = last_hit;
		block.hit.face = (BlockFace)-1; // select block itself instead of face
	}
}

void Game::apply_damage (SelectedBlock& block, Item& item, bool creative_mode) {
	assert(block);
	auto tool_props = item.get_props();

	auto hardness = g_assets.block_types[block.hit.bid].hardness;

	if (!g_assets.block_types.block_breakable(block.hit.bid))
		return;

	float dmg = 0;
	if (hardness == 0) {
		dmg = 1.0f;
	} else if (hardness == 255) {
		dmg = 0.0f;
	} else {
		float damage_multiplier = (float)tool_props.hardness / (float)hardness;
		if (tool_props.tool == g_assets.block_types[block.hit.bid].tool)
			damage_multiplier *= TOOL_MATCH_BONUS_DAMAGE;

		dmg = ((float)tool_props.damage / 255.0f) * damage_multiplier;

		if (creative_mode)
			dmg = 1.0f;
	}
	block.damage += dmg;

	if (block.damage >= 1) {
		break_sound.play();

		chunks.write_block(block.hit.pos.x, block.hit.pos.y, block.hit.pos.z, g_assets.block_types.air_id);
	}
}

bool Game::try_place_block (int3 pos, block_id id) {
	auto oldb = chunks.read_block(pos.x, pos.y, pos.z);

	if (!g_assets.block_types.block_breakable(oldb)) { // non-breakable blocks are solids and gasses
		chunks.write_block(pos.x, pos.y, pos.z, id);
		return true;
	}
	return false;
}
