#pragma once
#include "stdafx.hpp"
#include "worldgen.hpp"
#include "world.hpp"
#include "block_update.hpp"
#include "graphics/camera.hpp"
#include "graphics/graphics.hpp"
#include "util/running_average.hpp"

#include "serialization.hpp"
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

// Hot-reloadable dll for quick iteration of worldgen
// Recompiling this dll writes to a hot_reload.txt in the build folder, which gets detected by directory_watcher
// We then trigger a world recreate and reload the dll dynamically
struct DLL {
	HMODULE h = NULL;

	std::string dll_filename;
	std::string tmp_filename;

	static std::string get_exe_path () {
		char path[1024];
		auto len = GetModuleFileName(NULL, path, ARRLEN(path));

		return std::string( kiss::get_path(std::string_view(path, len), nullptr, '\\') );
	}
	DLL () {
		std::string exe_path = get_exe_path();

		dll_filename = exe_path + "worldgen.dll";
		tmp_filename = exe_path + "worldgen_tmp.dll";
	}

	void reload () {
		if (h)
			FreeLibrary(h);

		DeleteFile(tmp_filename.c_str());

		// Need to load a copy of the dll or else the compiler won't be able to overwrite the dll since we are using it
		auto ret = CopyFile(dll_filename.c_str(), tmp_filename.c_str(), false);
		if (ret == 0) {
			auto err = GetLastError();
			clog(ERROR, "Reload failed [%d]", err);
		}
		
		h = LoadLibrary(tmp_filename.c_str());
	}
	~DLL () {
		FreeLibrary(h);
	}
};

class Game {
	DirectoyChangeNotifier directory_watcher = DirectoyChangeNotifier(".", true);
	DLL worldgen_dll = DLL();

	bool dbg_pause = false;

	FPS_Display fps_display;

	// Global world gen I can tweak (changes are only visible on world recreate)
	WorldGenerator world_gen;

	// World gets world gen copy on create 
	std::unique_ptr<World> world;

	BlockUpdate block_update;

	bool activate_flycam = false;
	bool creative_mode = false;

	Flycam flycam = { float3(-5, -10, 50), float3(0, deg(-20), 0), 12 };

	bool trigger_place_block = false;

	Graphics graphics;
	
public:
	Game ();

	void frame ();
};
