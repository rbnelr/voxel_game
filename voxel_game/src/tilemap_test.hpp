#pragma once
#include "dear_imgui/dear_imgui.hpp"
#include "serialization.hpp"
#include <algorithm>

struct TilemapTest {

	bool enable = 1;

	std::string tilemap_filename;
	int pattern_N = 3;

	Image<srgb8> tilemap;

	SERIALIZE(TilemapTest, tilemap_filename, pattern_N)

	struct Tile {
		std::string	name;
		srgb8		color;
		int			count = 0;

		SERIALIZE(Tile, name, color)
	};
	std::vector<Tile> tiles;

	void gather_tiles () {
		load(prints("%s.tiles.json", tilemap_filename.c_str()).c_str(), &tiles);

		for (int y=0; y<tilemap.size.y; ++y) {
			for (int x=0; x<tilemap.size.x; ++x) {
				auto col = tilemap.get(x,y);

				int i = indexof(tiles, col, [] (Tile& l, srgb8 r) { return l.color == r; });
				if (i < 0) { // new tile, create tile in list
					tiles.push_back({ "", col, 1 });
				} else { // existing tile, count occurance
					tiles[i].count++;
				}
			}
		}
	}

	bool regen = 1;

	static constexpr int SIZE = 64;
	int world[SIZE][SIZE]; // tile ids

	void load_tilemap () {
		if (!Image<srgb8>::load_from_file(tilemap_filename.c_str(), &tilemap)) {
			clog("[TilemapTest] Could not load tilemap from file!");
			return;
		}

		gather_tiles();
	}

	bool init = true;
	void update () {
		if (!enable) return;

		if (init) {
			load("tilemap_test.json", this);
			load_tilemap();
			init = false;
		}

		if (regen) {
			memset(world, -1, sizeof(world));

			regen = false;
		}

		//for (int y=0; y<SIZE; ++y) {
		//	for (int x=0; x<SIZE; ++x) {
		//		int id = world[y][x];
		//		if (id < 0)
		//			continue;
		//		auto& t = tiles[id];
		//
		//		debug_graphics->push_quad(float3((float)x, (float)y, 0), float3(1,0,0), float3(0,1,0), lrgba(to_linear(t.color), 1));
		//	}
		//}
	}

	//// Imgui
	static constexpr int IMGUI_TILE_SZ = 28;

	bool colored_button (srgba8 col, int size, char const* label="##") {
		auto c = (float4)col / 255.0f;
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(c.x,c.y,c.z,c.w));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(c.x,c.y,c.z,c.w));

		bool hovered = ImGui::GetHoveredID() == ImGui::GetID(label);
		if (hovered) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3);

		bool ret = ImGui::Button(label, ImVec2((float)size, (float)size));

		ImGui::PopStyleColor(2);
		if (hovered) ImGui::PopStyleVar();
		return ret;
	}
	
	void imgui () {
		ImGui::Checkbox("TilemapTest enable", &enable);

		if (enable && ImGui::Begin("TilemapTest")) {
			if (ImGui::Button("Save Settings")) save("tilemap_test.json", *this);
			ImGui::SameLine();
			if (ImGui::Button("Load Settings")) load("tilemap_test.json", this);

			ImGui::InputText("tilemap_filename", &tilemap_filename);
			if (ImGui::Button("Load")) load_tilemap();
			ImGui::SameLine();
			if (ImGui::Button("Save Tile Config")) save(prints("%s.tiles.json", tilemap_filename.c_str()).c_str(), tiles);

			ImGui::Text("Tilemap:");
			ImGui::Text("Size: %d x %d px", tilemap.size.x, tilemap.size.y);

			ImGui::SliderInt("pattern_N", &pattern_N, 2, 8);

			regen = ImGui::Button("Regen");

			if (	ImGui::TreeNodeEx("Tiles", ImGuiTreeNodeFlags_NoTreePushOnOpen) &&
					ImGui::BeginTable("tiles", 3, ImGuiTableFlags_Borders|ImGuiTableFlags_Resizable)) {
				ImGui::TableSetupColumn("Tile Color", ImGuiTableColumnFlags_WidthFixed, IMGUI_TILE_SZ);
				ImGui::TableSetupColumn("Tile Name", ImGuiTableColumnFlags_WidthFixed, 80);
				ImGui::TableSetupColumn("Count");
				for (int id=0; id<(int)tiles.size(); ++id) {
					ImGui::PushID(id);
					ImGui::TableNextCell();
					imgui_ColorEdit("###color", &tiles[id].color);
					ImGui::TableNextCell();
					ImGui::InputText("###name", &tiles[id].name);
					ImGui::TableNextCell();
					ImGui::Text("%5d", tiles[id].count);
					ImGui::PopID();
				}
				ImGui::EndTable();
			}

			ImGui::End();
		}
	}

};
