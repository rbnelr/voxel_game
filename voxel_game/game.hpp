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
	float latest_min_dt, latest_max_dt, latest_std_dev_dt;

	float update_period = .5f; // sec
	float update_timer = 0;

	int histogram_height = 60;

	void display_fps () {
		dt_avg.push(input.real_dt);

		if (update_timer <= 0) {
			latest_avg_dt = dt_avg.calc_avg(&latest_min_dt, &latest_max_dt, &latest_std_dev_dt);
			update_timer += update_period;
		}
		update_timer -= input.real_dt;

		{
			float avg_fps = 1.0f / latest_avg_dt;
			ImGui::Text("avg fps: %5.1f (%6.3fms  min: %6.3f  max: %6.3f  stddev: %6.3f)",
				avg_fps, latest_avg_dt * 1000, latest_min_dt * 1000, latest_max_dt * 1000, latest_std_dev_dt * 1000);
			ImGui::Text("timestep: %6.3fms", input.dt * 1000);

			ImGui::SetNextItemWidth(-1);
			ImGui::PlotHistogram("##frametimes_histogram", dt_avg.data(), (int)dt_avg.count(), 0, "frametimes:", 0, 1.0f/20, ImVec2(0, (float)histogram_height));

			if (ImGui::BeginPopupContextItem("##frametimes_histogram popup")) {
				ImGui::SliderInt("histogram_height", &histogram_height, 20, 120);
				
				int cap = (int)dt_avg.capacity();
				if (ImGui::SliderInt("avg_count", &cap, 16, 1024)) {
					dt_avg.resize(cap);
				}

				ImGui::EndPopup();
			}
		}
	}
};

class Game {
	bool dbg_pause = false;

	FPS_Display fps_display;

	// Global world gen I can tweak (changes are only visible on world recreate)
	WorldGenerator world_gen;

	// World gets world gen copy on create 
	std::unique_ptr<World> world = std::make_unique<World>(world_gen);

	BlockUpdate block_update;

	bool activate_flycam = false;

	Flycam flycam = { float3(-5, -10, 50), float3(0, deg(-20), 0), 12 };

	bool trigger_place_block = false;

	Graphics graphics;
	
public:
	Game ();

	void frame ();

	~Game () {
		// make sure to shutdown global threadpools before optick dumps profiling data
		shutdown_threadpools();
	}
};
