#pragma once
#include "dear_imgui/dear_imgui.hpp"
#include "serialization.hpp"
#include <algorithm>

template <typename T, typename EQUAL>
bool contains (std::vector<T>& vec, T& r, EQUAL are_equal) {
	for (auto& i : vec) {
		if (are_equal(i, r))
			return true;
	}
	return false;
}
template <typename T, typename EQUAL>
int indexof (std::vector<T>& vec, T& r, EQUAL are_equal) {
	for (int i=0; i<(int)vec.size(); ++i) {
		if (are_equal(vec[i], r))
			return i;
	}
	return -1;
}

struct WangTiles {

	/*
		0 1 2
		3   4
		5 6 7
	*/
	static constexpr int2 neighb_pos[8] = {
		int2(-1,+1), int2( 0,+1), int2(+1,+1),
		int2(-1, 0),              int2(+1, 0),
		int2(-1,-1), int2( 0,-1), int2(+1,-1),
	};

	static constexpr int rotations[3][8] = {
		//       rot 180 deg
		{ 7, 6, 5,
		  4,    3,
		  2, 1, 0 },
		// right rot  90 deg
		{ 2, 4, 7,
		  1,    6,
		  0, 3, 5 },
		// left  rot  90 deg
		{ 5, 3, 0,
		  6,    1,
		  7, 4, 2 },
	};

	static constexpr int mirrors[2][8] = {
		// horiz mirror
		{ 2, 1, 0,
		  4,    3,
		  7, 6, 5 },
		// vert  mirror
		{ 5, 6, 7,
		  3,    4,
		  0, 1, 2 },
	};

	struct Tile {
		std::string name;
		srgb8 color;
		float prob = 100; // probability weight

		// valid neighbour tile configurations
		struct LayoutConfig {
			uint8_t rotate_sym = 0; // 0: no rot    1: all rot     2: 180 rot
			uint8_t mirror_sym = 0; // 0: no mirror 1: v/h mirror

			std::string neighb[8] = {};

			SERIALIZE(LayoutConfig, rotate_sym, mirror_sym, neighb)
		};
		std::vector<LayoutConfig> input_layouts;
		bool recompile_layout = true;

		std::vector<std::array<int, 8>> layouts;

		SERIALIZE(Tile, name, color, prob, input_layouts)
	};

	bool enable = 1;

	std::vector<Tile> tiles;

	SERIALIZE(WangTiles, tiles)

	static constexpr int SIZE = 64;
	int world[SIZE][SIZE]; // tile ids

	bool regen = 1;
	int selected_tile = -1;
	std::string painting_tile = "";

	int name_to_id (std::string const& str) {
		for (int id=0; id<(int)tiles.size(); ++id)
			if (str.compare(tiles[id].name) == 0)
				return id;
		return -1;
	}

	std::vector<std::array<int, 8>> compile_layouts (std::vector<Tile::LayoutConfig> input_layouts) {
		std::vector<std::array<int, 8>> layouts;

		for (auto& il : input_layouts) {
			std::array<int, 8> neighb;
			for (int i=0; i<8; ++i) {
				neighb[i] = name_to_id(il.neighb[i]);
			}

			layouts.push_back(neighb);

			if (il.rotate_sym != 0) {
				std::array<int, 8> neighb2;

				for (int rot=0; rot < (il.rotate_sym == 2 ? 1 : 3); ++rot) {
					for (int i=0; i<8; ++i)
						neighb2[i] = neighb[rotations[rot][i]];

					layouts.push_back(neighb2);
				}
			}

			if (il.mirror_sym != 0) {
				std::array<int, 8> neighb2;

				for (int mir=0; mir<2; ++mir) {
					for (int i=0; i<8; ++i)
						neighb2[i] = neighb[mirrors[mir][i]];

					layouts.push_back(neighb2);
				}
			}
		}

		return layouts;
	}

	int gen_per_frame = 5;
	std::vector<int2> decided_tiles;
	std::vector<int2> undecided_tiles;

	int decide_tile (int2 pos) {
		int neighb[8];
		for (int n=0; n<8; ++n) {
			int2 npos = pos + neighb_pos[n];
			if (npos.x >= 0 && npos.x < SIZE && npos.y >= 0 && npos.y < SIZE)
				neighb[n] = world[npos.y][npos.x];
			else
				neighb[n] = -1;
		}

		std::vector<float> tile_probs;
		tile_probs.resize(tiles.size(), 0);

		for (int tid=0; tid<(int)tiles.size(); ++tid) {
			auto& t = tiles[tid];

			bool layout_match;
			
			for (auto& l : t.layouts) {
				layout_match = true;
				for (int n=0; n<8; ++n) {
					bool match = neighb[n] < 0 || l[n] < 0 || neighb[n] == l[n]; // layout has -1 (which means any tile) or neighbour is undecided or neighbour matches layout
					if (!match) {
						layout_match = false;
						break;
					}
				}

				if (layout_match)
					break;
			}

			tile_probs[tid] = layout_match ? t.prob : 0;
		}

		// pick tile randomly from all tiles weighted by the calculated probability weights
		// (random tile if all equal probs, 0 prob are never picked)
		return random.weighted_choice(tile_probs);
	}

	void update () {
		if (!enable) return;

		static bool init = true;
		if (init) load("wang_tiles.json", this);
		init = false;

		for (int id=0; id<(int)tiles.size(); ++id) {
			if (tiles[id].recompile_layout) {
				tiles[id].layouts = compile_layouts(tiles[id].input_layouts);
				tiles[id].recompile_layout = false;

				regen = true;
			}
		}

		if (regen) {
			memset(world, -1, sizeof(world));
			decided_tiles.clear();
			undecided_tiles.clear();

			undecided_tiles.push_back(SIZE / 2);

			regen = false;
		}

		if (tiles.size() > 1) {
			int counter = 0;
			while (counter++ < gen_per_frame && !undecided_tiles.empty()) {
				int i = random.uniform(0, (int)undecided_tiles.size());

				int2 pos = undecided_tiles[i];

				for (int j=0; j<8; ++j) {
					int2 npos = pos + neighb_pos[j];
					if (npos.x >= 0 && npos.x < SIZE && npos.y >= 0 && npos.y < SIZE &&
						!contains(undecided_tiles, npos, [](int2 l, int2 r) { return equal(l,r); }) &&
						!contains(decided_tiles  , npos, [](int2 l, int2 r) { return equal(l,r); }))
						undecided_tiles.push_back(npos);
				}

				int tile = decide_tile(pos);

				if (tile >= 0) {
					// tile could be decided

					// remove from undecided_tiles by overwriting with last
					undecided_tiles[i] = undecided_tiles.back();
					undecided_tiles.pop_back();

					decided_tiles.push_back(pos);

					world[pos.y][pos.x] = tile;
				} else {
					// tile is undecideable

					// revert all neighbours to be undecided
					for (int j=0; j<8; ++j) {
						int2 npos = pos + neighb_pos[j];
						if (npos.x >= 0 && npos.x < SIZE && npos.y >= 0 && npos.y < SIZE) {
							if (world[npos.y][npos.x] >= 0) {
								int idx = indexof(decided_tiles, npos, [](int2 l, int2 r) { return equal(l,r); });
								assert(idx >= 0);

								// remove from decided_tiles by overwriting with last
								decided_tiles[idx] = decided_tiles.back();
								decided_tiles.pop_back();

								undecided_tiles.push_back(npos);

								world[npos.y][npos.x] = -1;
							}
						}
					}
				}
			}
		}
		
		for (int y=0; y<SIZE; ++y) {
			for (int x=0; x<SIZE; ++x) {
				int id = world[y][x];
				if (id < 0)
					continue;
				auto& t = tiles[id];

				debug_graphics->push_quad(float3((float)x, (float)y, 0), float3(1,0,0), float3(0,1,0), lrgba(to_linear(t.color), 1));
			}
		}
	}


	//// Imgui
	static constexpr int IMGUI_TILE_SZ = 28;

	bool colored_button (srgba8 col, char const* label="##") {
		auto c = (float4)col / 255.0f;
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(c.x,c.y,c.z,c.w));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(c.x,c.y,c.z,c.w));

		bool hovered = ImGui::GetHoveredID() == ImGui::GetID(label);
		if (hovered) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3);

		bool ret = ImGui::Button(label, ImVec2(IMGUI_TILE_SZ, IMGUI_TILE_SZ));

		ImGui::PopStyleColor(2);
		if (hovered) ImGui::PopStyleVar();
		return ret;
	}
	void tile_imgui (bool* selected, Tile& t) {
		using namespace ImGui;

		PushID(&t);

		imgui_ColorEdit("##color", &t.color);
		SameLine();
		Selectable(prints("%s", t.name.c_str()).c_str(), selected);

		if (*selected) {
			bool layout_changed = false;

			Begin("Tile", selected);

			InputText("name", &t.name);
			imgui_ColorEdit("color", &t.color);
			DragFloat("prob", &t.prob, 1);

			auto wndsz = GetContentRegionAvail();

			float cw = IMGUI_TILE_SZ*3 + GetStyle().CellPadding.x*2;
			int w = max(1, floori(wndsz.x / cw));
			auto size = ImVec2(0, wndsz.y * 0.45f);

			Text("layouts:");
			auto flags = ImGuiTableFlags_ScrollY|ImGuiTableFlags_Borders|ImGuiTableFlags_SizingPolicyFixedX;
			if (BeginTable("layouts", w, flags, size)) {
				for (int i=0; i<w; ++i)
					TableSetColumnWidth(i, IMGUI_TILE_SZ*3);

				for (int i=0; i<(int)t.input_layouts.size(); ++i) {
					TableNextCell();
					PushID(i);

					auto& l = t.input_layouts[i];

					if (ImGui::Button(prints("R(%d)###R", l.rotate_sym).c_str())) {
						l.rotate_sym = (l.rotate_sym + 1) % 3;
						t.recompile_layout = true;
					}
					SameLine();
					if (ImGui::Button(prints("M(%d)###M", l.mirror_sym).c_str())) {
						l.mirror_sym = (l.mirror_sym + 1) % 2;
						t.recompile_layout = true;
					}

					PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
					BeginTable("##neighbours", 3);

					for (int j=0; j<8; ++j) {
						if (j == 4) {
							TableNextCell();
							PushID(-1);
							colored_button(srgba8(t.color, 255));
							PopID();
						}
						TableNextCell();
						PushID(j);

						auto nid = name_to_id(l.neighb[j]);
						if (colored_button(nid >= 0 ? srgba8(tiles[nid].color, 255) : srgba8(0))) {
							l.neighb[j] = painting_tile;
							t.recompile_layout = true;
						}
						if (BeginPopupContextWindow("Select Tile")) {
							//// Tile select for changing layouts

							if (BeginTable("tiles", 16, ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingPolicyFixedX,
								ImVec2(16 * IMGUI_TILE_SZ, 8 * IMGUI_TILE_SZ))) {

								TableNextCell();
								PushID(-1);

								if (colored_button(srgba8(0), "NUL")) {
									painting_tile = "";
									CloseCurrentPopup();
								}
								PopID();

								for (int id=0; id<(int)tiles.size(); ++id) {
									TableNextCell();
									PushID(id);

									bool clicked = colored_button(srgba8(tiles[id].color, 255));

									if (ImGui::IsItemHovered()) {
										ImGui::BeginTooltip();
										ImGui::Text(tiles[id].name.c_str());
										ImGui::EndTooltip();
									}

									if (clicked) {
										painting_tile = tiles[id].name;
										CloseCurrentPopup();
									}

									PopID();
								}
								EndTable();
							}
							EndPopup();
						}

						PopID();
					}

					EndTable();
					PopStyleVar();
					PopID();
				}

				TableNextCell();
				if (ImGui::Button("+")) {
					t.input_layouts.emplace_back();
					t.recompile_layout = true;
				}
				if (ImGui::Button("-") && !t.input_layouts.empty()) {
					t.input_layouts.pop_back();
					t.recompile_layout = true;
				}

				EndTable();
			}

			Text("compiled_layouts:");
			if (BeginTable("compiled_layouts", w, flags, size)) {
				for (int i=0; i<w; ++i)
					TableSetColumnWidth(i, IMGUI_TILE_SZ*3);

				for (int i=0; i<(int)t.layouts.size(); ++i) {
					TableNextCell();
					PushID(i);

					auto& l = t.layouts[i];

					PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
					BeginTable("##neighbours", 3);

					for (int j=0; j<8; ++j) {
						if (j == 4) {
							TableNextCell();
							PushID(-1);
							colored_button(srgba8(t.color, 255));
							PopID();
						}
						TableNextCell();
						PushID(j);

						colored_button(l[j] >= 0 ? srgba8(tiles[l[j]].color, 255) : srgba8(0));

						PopID();
					}

					EndTable();
					PopStyleVar();
					PopID();
				}

				EndTable();
			}

			End();
		}

		PopID();
	}
	void imgui () {
		if (!imgui_push("WangTiles"))
			return;

		ImGui::Checkbox("enable", &enable);

		if (ImGui::Button("Save")) save("wang_tiles.json", *this);
		ImGui::SameLine();
		if (ImGui::Button("Load")) load("wang_tiles.json", this);

		regen = ImGui::Button("Regen");

		if (ImGui::TreeNode("tiles")) {
			for (int id=0; id<(int)tiles.size(); ++id) {
				bool was_selected = selected_tile == id;
				bool is_selected = was_selected;

				tile_imgui(&is_selected, tiles[id]);

				if (is_selected)					selected_tile = id;
				if (was_selected && !is_selected)	selected_tile = -1;
			}
			if (ImGui::Button("+"))
				tiles.emplace_back();

			ImGui::TreePop();
		}

		ImGui::DragInt("gen_per_frame", &gen_per_frame, 0.1f);

		imgui_pop();
	}

};
