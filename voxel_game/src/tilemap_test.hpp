#pragma once
#include "stdafx.hpp"
#include "dear_imgui/dear_imgui.hpp"
#include "serialization.hpp"
#include <algorithm>

struct TilemapTest {
	SERIALIZE(TilemapTest, enable, input_filename)
	
	bool enable = 1;

	static constexpr uint8_t NULLID = (uint8_t)-1;

	static constexpr int2 dirs[4] = {
		int2(-1, 0), int2(+1, 0), int2(0, -1), int2(0, +1),
	};

	struct Tile {
		SERIALIZE(Tile, name, color)

		std::string	name;
		srgb8		color;
		int			count = 0;
	};
	struct WFCInput {
		SERIALIZE(WFCInput, sample_filename, size, N, symmetrize, periodic_x, periodic_y, border_x, border_y, tiles)

		std::string sample_filename;

		int N = 2;

		bool symmetrize = false;
		bool periodic_x = false, periodic_y = false;
		bool border_x = false, border_y = false;
		
		Image<srgb8> sample_raw; // color image
		Image<uint8_t> sample; // palettized color image where palette is tiles

		std::vector<Tile> tiles;

		int2 size = 64; // output size
	};

	struct Pattern {
		// number of instances of this pattern in the source
		float weight;
		float weight_log;

		lrgb lcolor;

		// number of patterns that are compatible when at an offset in the four directions
		int compatible_count[4];

		std::vector<int> compatible_patterns[4];
	};

	std::string input_filename;
	WFCInput I;

	int N; // copy of N to make displaying of patterns safe
	
	int pattern_total; // number of all instances of patterns in the source

	std::vector<Pattern> patterns;
	std::vector<uint8_t> patterns_data;
	
	////
	std::vector<int> symmetry_modes[8]; // identity, rot cw 90, rot cw 180, rot ccw 90, mirror horiz, 4x mirror horiz + rotations 
	void create_symmetry_modes () {
		for (auto& v : symmetry_modes)	v.resize(N*N);

		// copy indices in src into dst, rotated by 90 deg cw
		auto rotate_cw = [&] (std::vector<int>& src, std::vector<int>& dst) {
			for (int y=0; y<N; ++y)
				for (int x=0; x<N; ++x)
					dst[(N-1-x) * N + y] = src[y * N + x];
		};
		auto flip_x = [&] (std::vector<int>& src, std::vector<int>& dst) {
			for (int y=0; y<N; ++y)
				for (int x=0; x<N; ++x)
					dst[y * N + (N-1-x)] = src[y * N + x];
		};

		for (int i=0; i<N*N; ++i) symmetry_modes[0][i] = i;

		for (int i=1; i<4; ++i)
			rotate_cw(symmetry_modes[i-1], symmetry_modes[i]);

		flip_x(symmetry_modes[0], symmetry_modes[4]);

		for (int i=5; i<8; ++i)
			rotate_cw(symmetry_modes[i-1], symmetry_modes[i]);
	}

	void gather_tiles () {
		ZoneScoped;

		for (auto& t : I.tiles) {
			t.count = 0;
		}

		I.sample = Image<uint8_t>(I.sample_raw.size);

		for (int y=0; y<I.sample.size.y; ++y) {
			for (int x=0; x<I.sample.size.x; ++x) {
				auto col = I.sample_raw.get(x,y);

				int i = indexof(I.tiles, col, [] (Tile& l, srgb8 r) { return l.color == r; });
				if (i < 0) { // new tile, create tile in list
					assert(I.tiles.size() < 254);
					I.tiles.push_back({ "", col, 1 });
				} else { // existing tile, count occurance
					I.tiles[i].count++;
				}

				I.sample.set(x,y, (uint8_t)i);
			}
		}
	}

	void gather_patterns () {
		ZoneScoped;

		N = I.N;

		create_symmetry_modes();

		patterns.clear();
		patterns_data.clear();

		pattern_total = 0;

		// sample lower border if border is activated (don't wrap to the left becaue then we would sample more often on the border, keep sampling uniform)
		int startx = I.border_x ? 1 - N : 0;
		int starty = I.border_y ? 1 - N : 0;
		// sample beyond image either the border or by wrapping around
		int endx   = I.border_x || I.periodic_x ? I.sample.size.x : I.sample.size.x - N;
		int endy   = I.border_y || I.periodic_y ? I.sample.size.y : I.sample.size.y - N;

		std::vector<uint8_t> tmp;
		tmp.resize(N*N * ARRLEN(symmetry_modes));

		for (int y=starty; y<endy; ++y) {
			for (int x=startx; x<endx; ++x) {
				ZoneScopedN("Pixel");

				// extract pattern from sample image into tmp[0]
				for (int py=0; py<N; ++py) {
					for (int px=0; px<N; ++px) {
						int sx = x + px;
						int sy = y + py;

						if (I.periodic_x) sx = wrap(sx, I.sample.size.x);
						if (I.periodic_y) sy = wrap(sy, I.sample.size.y);

						uint8_t id = NULLID;
						if (	sx >= 0 && sx < I.sample.size.x &&
								sy >= 0 && sy < I.sample.size.y ) {
							id = I.sample.get(sx,sy);
						}

						tmp[py * N + px] = id;
					}
				}

				for (int s=0; s<(I.symmetrize ? ARRLEN(symmetry_modes) : 1); ++s) {
					uint8_t* pat = &tmp[s * N*N];

					// create symmetry mode of pattern
					for (int i=0; i<N*N; ++i) {
						pat[ symmetry_modes[s][i] ] = tmp[i];
					}

					// scan patterns to see if pattern is unique
					bool unique = true;
					for (int i=0; i<(int)patterns.size(); ++i) {
						int cmp = memcmp(pat, &patterns_data[i * N*N], N*N * sizeof(uint8_t));
						if (cmp == 0) {
							unique = false;
							patterns[i].weight += 1.0f; // count pattern
							pattern_total++;
							break;
						}
					}

					if (unique) {
						// register pattern
						patterns.push_back({ 1.0f });
						pattern_total++;

						// alloc pattern_data
						size_t offs = patterns_data.size();
						patterns_data.resize( offs + N*N );
						uint8_t* data = &patterns_data[offs];

						memcpy(data, pat, N*N * sizeof(uint8_t));
					}
				}
			}
		}

		for (int i=0; i<(int)patterns.size(); ++i) {
			auto& p = patterns[i];

			float w = (float)p.weight;
			p.weight_log = w * log2f(w);
			p.lcolor = to_linear( I.tiles[patterns_data[i * N*N]].color );
		}

		for (int d=0; d<4; ++d) {
			for (int a=0; a<(int)patterns.size(); ++a) {
				
				patterns[a].compatible_count[d] = 0;
				
				for (int b=0; b<(int)patterns.size(); ++b) {
					if (pattern_compatible(a, b, dirs[d])) {
						patterns[a].compatible_count[d]++;
						patterns[a].compatible_patterns[d].push_back(b);
					}
				}
			}
		}
	}
	// Can Pattern B be placed at offs relative to pattern B so that the overlapping region has identical tiles in both?
	bool pattern_compatible (int a, int b, int2 offs) {
		// check overlapping rect of pattern A agianst pattern B
		int x0 =     max(offs.x, 0);
		int y0 =     max(offs.y, 0);
		int x1 = N + min(offs.x, 0);
		int y1 = N + min(offs.y, 0);

		for (int ya=y0; ya<y1; ++ya) {
			for (int xa=x0; xa<x1; ++xa) {
				int xb = xa - offs.x;
				int yb = ya - offs.y;
				assert(xa >= 0 && xa < N && ya >= 0 && ya < N);
				assert(xb >= 0 && xb < N && yb >= 0 && yb < N);

				uint8_t pxa = patterns_data[a * N*N + ya * N + xa];
				uint8_t pxb = patterns_data[b * N*N + yb * N + xb];
				if (pxa != pxb)
					return false;
			}
		}
		return true;
	}

	void compile () {
		ZoneScoped;

		if (!Image<srgb8>::load_from_file(prints("WFC/%s", I.sample_filename.c_str()).c_str(), &I.sample_raw)) {
			clog(WARNING, "[TilemapTest] Could not load tilemap from file!");
			I.sample_raw = Image<srgb8>();
		}

		gather_tiles();
		gather_patterns();

		show_pattern_details = false;
	}

	struct Cell {

		int wave_count; // number of still valid patterns
		float entropy; // cached entropy

		float sum_weights; // sum of weight of remaining patterns
		float sum_weights_log; // sum of weight * log2(weight) of remaining patterns

		lrgb sum_color;
	};

	// sigh if I wanted std::vector<bool> to be a bitset I would use a type called bitset and not a vector
	// what 'genius' had the idea to make a vector that does not work like a vector?
	struct fake_bool {
		bool b;
		fake_bool () {}
		fake_bool (bool b): b{b} {}
		operator bool () { return b; }
	};

	std::vector<fake_bool> waves;
	std::vector<int> compats;

	fake_bool* wave_get (size_t i) {
		return waves.data() + i * patterns.size();
	}
	fake_bool* wave_get (size_t x, size_t y) {
		return waves.data() +
			y * I.size.x*patterns.size() +
			x * patterns.size();
	}
	int* compat_get (size_t i, size_t dir) {
		return compats.data() +
			i * 4*patterns.size() +
			dir * patterns.size();
	}
	int* compat_get (size_t x, size_t y, size_t dir) {
		return compats.data() +
			y * I.size.x*4*patterns.size() +
			x * 4*patterns.size() +
			dir * patterns.size();
	}

	struct RemovedPattern {
		int2	pos;
		int		pat;
	};

	std::vector<Cell> output;

	std::vector<RemovedPattern> propagate_stack;

	bool contradiction;

	void ban_pattern (Cell& c, int2 pos, int pat) {
		auto* wave = wave_get(pos.x, pos.y);

		assert(wave[pat] == true);
		wave[pat] = false;
		c.wave_count--;

		c.sum_weights -= patterns[pat].weight;
		c.sum_weights_log -= patterns[pat].weight_log;
		c.entropy = log2f(c.sum_weights) - c.sum_weights_log / c.sum_weights;

		c.sum_color -= patterns[pat].lcolor;

		for (int d=0; d<4; ++d)
			compat_get(pos.x, pos.y, d)[pat] = 0;

		if (c.wave_count == 0) {
			contradiction = true;
			return;
		}

		propagate_stack.push_back({ pos, pat });
	}

	void collapse_wave (int2 pos) {
		ZoneScoped;

		auto& c = output[pos.y * I.size.x + pos.x];
		auto* wave = wave_get(pos.x, pos.y);

		float total_weight = 0;
		for (int p=0; p<(int)patterns.size(); ++p) {
			if (!wave[p]) continue;
			total_weight += (float)patterns[p].weight;
		}

		int choice = random.weighted_choice((int)patterns.size(), [&] (int i) {
			return wave[i] ? (float)patterns[i].weight : 0.0f;
		}, total_weight);
		assert(wave[choice] == true);

		for (int p=0; p<(int)patterns.size(); ++p) {
			if (!wave[p] || p == choice) continue;
			ban_pattern(c, pos, p);
		}
	}

	//// Init
	void clear () {
		ZoneScoped;

		float sum_weights = 0;
		float sum_weights_log = 0;

		lrgb sum_color = 0;

		for (auto pat : patterns) {
			float w = (float)pat.weight;
			sum_weights += (float)w;
			sum_weights_log += w * log2f(w);

			sum_color += pat.lcolor;
		}
		float entropy = log2f(sum_weights) - sum_weights_log / sum_weights;

		output.resize(I.size.x * I.size.y);
		waves.resize(I.size.x * I.size.y * patterns.size());
		compats.resize(I.size.x * I.size.y * 4 * patterns.size());

		for (int i=0; i<I.size.x * I.size.y; ++i) {
			output[i].wave_count = (int)patterns.size();

			output[i].entropy = entropy;
			output[i].sum_weights = sum_weights;
			output[i].sum_weights_log = sum_weights_log;
			output[i].sum_color = sum_color;

			memset(wave_get(i), 1, patterns.size() * sizeof(fake_bool));

			for (int d=0; d<4; ++d) {
				auto* comp = compat_get(i, d);
				for (int p=0; p<(int)patterns.size(); ++p) {
					comp[p] = patterns[p].compatible_count[d];
				}
			}
		}

		contradiction = false;
	}

	void generate () {
		ZoneScoped;

		if (contradiction)
			return;

		{ //// Observe
			ZoneScopedN("TilemapTest:: Observe");

			float min_entropy = INF;
			int2 min_cell;

			// find lowest entropy
			for (int y=0; y<I.size.y; ++y) {
				for (int x=0; x<I.size.x; ++x) {
					auto& c = output[y * I.size.x + x];

					//if (c.wave_count == 0) {
					//	contradiction = true;
					//	return; // contradiction
					//}
					
					float e = c.entropy;

					if (noisy_entropy)
						e += random.uniform(-0.001f, +0.001f);
					
					if (c.wave_count > 1 && e < min_entropy) {
						min_entropy = e;
						min_cell = int2(x,y);
					}
				}
			}

			if (min_entropy == INF) {
				return; // done
			}

			collapse_wave(min_cell);
		}

		//// Propagate
		{
			ZoneScopedN("TilemapTest:: Propagate");

			while (!propagate_stack.empty() && !contradiction) {
				ZoneScopedN("TilemapTest:: Propagate Iter");

				int2 pos = propagate_stack.back().pos;
				int pat = propagate_stack.back().pat;
				propagate_stack.pop_back();

				for (int d=0; d<4; ++d) {
					int2 npos = pos + dirs[d];

					if (	npos.x < 0 || npos.x >= I.size.x ||
							npos.y < 0 || npos.y >= I.size.y )
						continue;

					auto& nc = output[npos.y * I.size.x + npos.x];
					auto* comp = compat_get(npos.x, npos.y, d ^ 1); // ^1 flips the direction

					// for all patterns that are valid in direction d for the pattern pat that was banned
					for (auto p : patterns[pat].compatible_patterns[d]) {

						int& compat_count = comp[p];
						if (compat_count > 0 && --compat_count == 0)
							ban_pattern(nc, npos, p);
					}
				}
			}
		}
	}

	void render () {
		ZoneScoped;

		for (int y=0; y<I.size.y; ++y) {
			for (int x=0; x<I.size.x; ++x) {
				auto& c = output[y * I.size.x + x];

				lrgb color;
				float alpha = 1;

				if (visualize_entropy) {
					color = lerp(lrgb(1,0,0), lrgb(0,0,0), c.entropy / visualize_max_entropy);
				} else {
					if (c.wave_count == 0) {
						color = lrgb(1,1,0);
					} else {
						color = c.sum_color / (float)c.wave_count;
					}
				}

				if (entropy_alpha) {
					alpha = 1.0f - c.entropy / visualize_max_entropy;
				}

				debug_graphics->push_quad(float3((float)x, (float)y, 0), float3(1,0,0), float3(0,1,0), lrgba(color, alpha));
			}
		}
	}

	bool init = true;
	bool reload = true; // reload input config from json
	bool recompile = true; // regenerate tile lists and patterns
	bool regenerate = true; // regenerate new output

	bool manual_step = false;
	bool step = false;

	bool visualize_entropy = false;
	bool entropy_alpha = false;
	float visualize_max_entropy = 5;

	bool noisy_entropy = true;

	void update () {
		if (!enable) return;
		ZoneScoped;

		if (init) {
			load("tilemap_test.json", this);
			init = false;
		}

		if (reload) {
			if (!load(prints("WFC/%s.json", input_filename.c_str()).c_str(), &I)) {
				I = WFCInput();
				I.sample_filename = input_filename + ".png";
			}
			reload = false;
			recompile = true;
		}

		if (recompile) {
			compile();
			recompile = false;
			regenerate = true;
		}

		if (regenerate) {
			clear();
			regenerate = false;
		}

		if (!manual_step || step)
			generate();

		render();
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
	
	bool show_pattern_details;
	int selected_pattern = -1;

	static constexpr int IMGUI_TILE_SZ = 28;

	void imgui () {
		ImGui::Checkbox("TilemapTest enable", &enable);

		if (enable && ImGui::Begin("TilemapTest")) {
			if (ImGui::Button("Save Settings")) save("tilemap_test.json", *this);
			ImGui::SameLine();
			if (ImGui::Button("Load Settings")) load("tilemap_test.json", this);

			ImGui::InputText("input_filename", &input_filename);

			reload = ImGui::Button("Load") || reload;
			ImGui::SameLine();
			if (ImGui::Button("Save")) save(prints("WFC/%s.json", input_filename.c_str()).c_str(), I);

			if (ImGui::TreeNodeEx("Input", ImGuiTreeNodeFlags_DefaultOpen)) {

				ImGui::InputText("sample_filename", &I.sample_filename);
				
				ImGui::Text("Sample:");
				ImGui::Text("Size: %d x %d px", I.sample.size.x, I.sample.size.y);

				recompile = ImGui::SliderInt("pattern_N", &I.N, 2, 8) || recompile;
				if (recompile) {
					int a = 5;
				}

				recompile = ImGui::Checkbox("symmetrize", &I.symmetrize) || recompile;

				recompile = ImGui::Checkbox("border X", &I.border_x) || recompile;
				ImGui::SameLine();
				recompile = ImGui::Checkbox("border Y", &I.border_y) || recompile;

				recompile = ImGui::Checkbox("Periodic X", &I.periodic_x) || recompile;
				ImGui::SameLine();
				recompile = ImGui::Checkbox("Periodic Y", &I.periodic_y) || recompile;

				ImGui::TreePop();
			}

			ImGui::Checkbox("Use noisy entropy", &noisy_entropy);

			recompile = ImGui::Button("Recomp") || recompile;
			regenerate = ImGui::Button("Regen") || regenerate;

			ImGui::Checkbox("Manual Step", &manual_step);
			ImGui::SameLine();
			step = ImGui::Button("Step");

			ImGui::Checkbox("visualize_entropy", &visualize_entropy);
			ImGui::SameLine();
			ImGui::DragFloat("visualize_max_entropy", &visualize_max_entropy);
			ImGui::Checkbox("entropy_alpha", &entropy_alpha);
			
			if (	ImGui::TreeNodeEx("Tiles", ImGuiTreeNodeFlags_NoTreePushOnOpen) &&
					ImGui::BeginTable("tiles", 3, ImGuiTableFlags_Borders|ImGuiTableFlags_Resizable)) {
				ImGui::TableSetupColumn("Tile Color", ImGuiTableColumnFlags_WidthFixed, IMGUI_TILE_SZ);
				ImGui::TableSetupColumn("Tile Name", ImGuiTableColumnFlags_WidthFixed, 80);
				ImGui::TableSetupColumn("Count");
				for (int id=0; id<(int)I.tiles.size(); ++id) {
					ImGui::PushID(id);
					ImGui::TableNextCell();
					imgui_ColorEdit("###color", &I.tiles[id].color);
					ImGui::TableNextCell();
					ImGui::SetNextItemWidth(-1);
					ImGui::InputText("###name", &I.tiles[id].name);
					ImGui::TableNextCell();
					ImGui::Text("%5d", I.tiles[id].count);
					ImGui::PopID();
				}
				ImGui::EndTable();
			}

			ImGui::Checkbox("Show Patterns", &show_patterns);

			ImGui::End();

			if (show_patterns && ImGui::Begin("Patterns", &show_patterns)) {
				ImGui::Text("pattern_total: %6d", pattern_total);

				imgui_pattern_list();
				ImGui::End();
				
				if (show_pattern_details && ImGui::Begin(prints("Pattern Details [%d]###Pattern Details", selected_pattern).c_str(), &show_pattern_details)) {
					
					for (int d=0; d<4; ++d) {
						auto& pat = patterns[selected_pattern];

						if (ImGui::TreeNodeEx(
							prints("Compatible Patterns with offset (%+d,%+d) [%d]###compat[%d]", dirs[d].x,dirs[d].y, pat.compatible_count[d], d).c_str(),
							ImGuiTreeNodeFlags_DefaultOpen)) {

							imgui_pattern_list(false, &pat.compatible_patterns[d], (int)pat.compatible_patterns[d].size());

							ImGui::TreePop();
						}
					}
					ImGui::End();
				}
			}
		}
	}

	// draw either all patterns or vector of pattern indexes
	void imgui_pattern_list (bool show_details=true, std::vector<int>* indexes=nullptr, int count=0) {
		if (count == 0) count = (int)patterns.size();
		
		auto wndsz = ImGui::GetContentRegionAvail();

		float cw = IMGUI_TILE_SZ*N + ImGui::GetStyle().CellPadding.x*2;
		int w = max(1, min(floori(wndsz.x / cw), count));

		if (ImGui::BeginTable("patterns", w, ImGuiTableFlags_Borders|ImGuiTableFlags_SizingPolicyFixedX)) {
			for (int i=0; i<count; ++i) {
				ImGui::PushID(i);

				ImGui::TableNextCell();

				int index = indexes ? (*indexes)[i] : i;

				if (show_details) {
					if (ImGui::SmallButton("..")) {
						show_pattern_details = true;
						selected_pattern = index;
					}
					ImGui::SameLine();
				}

				if (indexes)
					ImGui::Text("[%d] %4d", index, patterns[index].weight);
				else
					ImGui::Text("%4d", patterns[index].weight);

				ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
				if (ImGui::BeginTable("patterns", N)) {

					int j=0;
					for (int py=0; py<N; ++py) {
						for (int px=0; px<N; ++px) {
							ImGui::PushID(j++);

							auto id = patterns_data[index * N*N + (N-1 -py) * N + px];
							srgba8 col = id != NULLID ? srgba8(I.tiles[id].color, 255) : srgba8(0,0,0,0);

							ImGui::TableNextCell();
							colored_button(col, IMGUI_TILE_SZ);

							if (ImGui::IsItemHovered()) {
								ImGui::BeginTooltip();
								ImGui::Text(id != NULLID ? I.tiles[id].name.c_str() : "null");
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
	}

};
