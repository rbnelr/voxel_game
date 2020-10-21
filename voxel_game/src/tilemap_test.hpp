#pragma once
#include "dear_imgui/dear_imgui.hpp"
#include "serialization.hpp"
#include <algorithm>

struct TilemapTest {
	SERIALIZE(TilemapTest, tilemap_filename, pattern_N, patterns_inc_border)
	
	bool enable = 1;

	std::string tilemap_filename;

	Image<srgb8> tilemap_raw; // color image
	Image<uint8_t> tilemap; // paletized color image where palette is tiles

	static constexpr uint8_t NULLID = (uint8_t)-1;

	struct Tile {
		SERIALIZE(Tile, name, color)

		std::string	name;
		srgb8		color;
		int			count = 0;
	};
	std::vector<Tile> tiles;

	void gather_tiles () {
		load(prints("%s.tiles.json", tilemap_filename.c_str()).c_str(), &tiles);

		tilemap = Image<uint8_t>(tilemap_raw.size);

		for (int y=0; y<tilemap_raw.size.y; ++y) {
			for (int x=0; x<tilemap_raw.size.x; ++x) {
				auto col = tilemap_raw.get(x,y);

				int i = indexof(tiles, col, [] (Tile& l, srgb8 r) { return l.color == r; });
				if (i < 0) { // new tile, create tile in list
					assert(tiles.size() < 254);
					tiles.push_back({ "", col, 1 });
				} else { // existing tile, count occurance
					tiles[i].count++;
				}

				tilemap.set(x,y, (uint8_t)i);
			}
		}
	}

	int pattern_N = 3;
	bool patterns_inc_border = false;

	struct Pattern {
		int count = 0;
	};
	std::vector<Pattern> patterns;
	std::vector<uint8_t> patterns_data;

	void gather_patterns () {
		patterns.clear();
		patterns_data.clear();

		// either include all NxN patterns that are completely on the image or
		//  all NxN patterns that touch the image with at least one pixel overlap
		int start = patterns_inc_border ? 1 - pattern_N				: 0;
		int2 end  = patterns_inc_border ? tilemap.size : tilemap.size - pattern_N;

		for (int y=start; y<end.y; ++y) {
			for (int x=start; x<end.x; ++x) {

				// alloc pattern
				size_t offs = patterns_data.size();
				patterns_data.resize( offs + pattern_N*pattern_N );
				uint8_t* data = &patterns_data[offs];

				// extract pattern from tilemap
				for (int py=0; py<pattern_N; ++py) {
					for (int px=0; px<pattern_N; ++px) {
						uint8_t id = NULLID;
						if (	x+px >= 0 && x+px < tilemap.size.x &&
								y+py >= 0 && y+py < tilemap.size.y ) {
							id = tilemap.get(x+px, y+py);
						}
						data[py * pattern_N + px] = id;
					}
				}

				// scan patterns to see if pattern is unique
				bool unique = true;
				for (int i=0; i<(int)patterns.size(); ++i) {
					int cmp = memcmp(data, &patterns_data[i * pattern_N*pattern_N], pattern_N*pattern_N * sizeof(uint8_t));
					if (cmp == 0) {
						unique = false;
						patterns[i].count++; // count pattern
						break;
					}
				}

				if (unique) {
					// register pattern
					patterns.push_back({ 1 });
				} else {
					// delete duplicate pattern data
					patterns_data.resize(offs);
				}
			}
		}
	}

	void load_tilemap () {
		if (!Image<srgb8>::load_from_file(tilemap_filename.c_str(), &tilemap_raw)) {
			clog("[TilemapTest] Could not load tilemap from file!");
			return;
		}

		gather_tiles();
		gather_patterns();
	}

	bool regen = 1;

	static constexpr int SIZE = 64;
	int world[SIZE][SIZE]; // tile ids

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
	
	bool show_patterns = false;
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
			ImGui::Checkbox("patterns_inc_border", &patterns_inc_border);

			regen = ImGui::Button("Regen");

			static constexpr int IMGUI_TILE_SZ = 28;

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

			ImGui::Checkbox("Show Patterns", &show_patterns);
			if (show_patterns) {
				ImGui::Begin("Patterns", &show_patterns);

				auto wndsz = ImGui::GetContentRegionAvail();

				float cw = IMGUI_TILE_SZ*pattern_N + ImGui::GetStyle().CellPadding.x*2;
				int w = max(1, floori(wndsz.x / cw));

				if (ImGui::BeginTable("patterns", w, ImGuiTableFlags_Borders|ImGuiTableFlags_SizingPolicyFixedX)) {
					for (int i=0; i<(int)patterns.size(); ++i) {
						ImGui::PushID(i);
						ImGui::TableNextCell();
						ImGui::Text("%6d", patterns[i].count);

						ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
						if (ImGui::BeginTable("patterns", pattern_N)) {

							int j=0;
							for (int py=0; py<pattern_N; ++py) {
								for (int px=0; px<pattern_N; ++px) {
									ImGui::PushID(j++);

									auto id = patterns_data[i * pattern_N*pattern_N + (pattern_N-1 -py) * pattern_N + px];
									srgba8 col = id != NULLID ? srgba8(tiles[id].color, 255) : srgba8(0,0,0,0);

									ImGui::TableNextCell();
									colored_button(col, IMGUI_TILE_SZ);

									if (ImGui::IsItemHovered()) {
										ImGui::BeginTooltip();
										ImGui::Text(id != NULLID ? tiles[id].name.c_str() : "null");
										ImGui::EndTooltip();
									}

									ImGui::PopID();
								}
							}

							ImGui::EndTable();
						}
						ImGui::PopStyleVar();

						ImGui::PopID();
					}
					ImGui::EndTable();
				}

				ImGui::End();
			}

			ImGui::End();
		}
	}

};
