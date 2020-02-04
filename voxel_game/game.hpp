#pragma once
#include "world_generator.hpp"
#include "world.hpp"
#include "block_update.hpp"
#include "graphics/camera.hpp"
#include "graphics/graphics.hpp"
#include "util/running_average.hpp"

extern bool _use_potatomode;

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

	BlockUpdate block_update;

	bool activate_flycam = false;

	Flycam flycam = { float3(-5, -10, 50), float3(0, deg(-20), 0), 12 };

	bool trigger_place_block = false;

	Graphics graphics;
	
public:
	Game ();

	void frame ();

};
