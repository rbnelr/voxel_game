#pragma once
#undef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1

#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define WIN32_NOMINMAX

#undef NOMINMAX
#define NOMINMAX

#include "windows.h"

#undef near
#undef far
#undef min
#undef max

#include <algorithm> // for min max

#include "assert.h"
#include <string>
#include <vector>
#include <unordered_map>

extern float elev_freq, elev_amp;
extern float rough_freq;
extern float detail0_freq, detail0_amp;
extern float detail1_freq, detail1_amp;
extern float detail2_freq, detail2_amp;

#include "graphics/common.hpp"
#include "blocks.hpp"
#include "input.hpp"
#include "graphics/camera.hpp"
#include "graphics/graphics.hpp"
#include "player.hpp"
#include "util/running_average.hpp"
#include "chunks.hpp"
#include "world_generator.hpp"
#include "world.hpp"

extern bool _use_potatomode;

static lrgba incandescent_gradient (float key) {
	return lrgba(gradient<float3>(key, {
		{ 0,		srgb(0)			},
		{ 0.3333f,	srgb(138,0,0)	},
		{ 0.6667f,	srgb(255,255,0)	},
		{ 1,		srgb(255)		},
		}), 1);
}
static lrgba spectrum_gradient (float key) {
	return lrgba(gradient<float3>(key, {
		{ 0,		srgb(0,0,127)	},
		{ 0.25f,	srgb(0,0,248)	},
		{ 0.5f,		srgb(0,127,0)	},
		{ 0.75f,	srgb(255,255,0)	},
		{ 1,		srgb(255,0,0)	},
		}), 1);
}

struct FPS_Display {
	RunningAverage<float> dt_avg = RunningAverage<float>(64);
	float latest_avg_dt;

	float update_period = .5f; // sec
	float update_timer = 0;

	int histogram_height = 40;

	void display_fps () {
		dt_avg.push(input.real_dt);

		if (update_timer <= 0) {
			latest_avg_dt = dt_avg.calc_avg();
			update_timer += update_period;
		}
		update_timer -= input.real_dt;

		{
			float avg_fps = 1.0f / latest_avg_dt;
			ImGui::Text("avg fps: %5.1f (%6.3f ms)  ----  timestep: %6.3f ms", avg_fps, latest_avg_dt * 1000, input.dt * 1000);

			ImGui::SetNextItemWidth(-1);
			ImGui::PlotHistogram("##frametimes_histogram", dt_avg.values.get(), dt_avg.count, 0, "frametimes:", 0, 0.033f, ImVec2(0, (float)histogram_height));

			if (ImGui::BeginPopupContextItem("##frametimes_histogram popup")) {
				ImGui::SliderInt("histogram_height", &histogram_height, 20, 120);
				
				int count = dt_avg.count;
				if (ImGui::SliderInt("avg_count", &count, 16, 1024)) {
					dt_avg.resize(count);
				}

				ImGui::EndPopup();
			}
		}
	}
};

class Game {
	FPS_Display fps_display;

	WorldGenerator world_gen;
	std::unique_ptr<World> world = std::make_unique<World>("test2");

	bool activate_flycam = false;

	Flycam flycam = { "flycam", float3(-5, -10, 50), float3(0, deg(80), 0), 12 };

	bool trigger_place_block = false;

	CommonUniforms common_uniforms;

	ChunkGraphics chunk_graphics;

	SkyboxGraphics skybox_graphics;
	BlockHighlightGraphics block_highlight_graphics;

	//float block_update_frequency = 1.0f;
	float block_update_frequency = 1.0f / 25;
	bpos_t cur_chunk_update_block_i = 0;
	
public:
	Game ();

	void frame ();

};
