#pragma once
#include "stdafx.hpp"
#include "svo.hpp"
#include "voxel_mesher.hpp"
#include "threading.hpp"

#define CHUNK_SIZE 64
#define CHUNK_SCALE 6

// Backend system resposible for handling storage, changes to, and displaying of the voxel world
class Voxels {
public:
	VoxelMesher mesher;
	SVO svo;

	float load_radius =	20;//2000.0f;
	float unload_hyster = 0;

	float load_lod_start = 200.0f;
	float load_lod_unit = 200.0f;
	
	// artifically limit both the size of the async queue and how many results to take from the results
	int cap_chunk_load = 64;
	// artifically limit (delay) meshing of chunks to prevent complete freeze of main thread at the cost of some visual artefacts
	int cap_chunk_mesh = max(parallelism_threads*2, 4); // max is 2 meshings per cpu core per frame

	void imgui () {
		if (!imgui_push("Voxels")) return;

		ImGui::SliderFloat("load_radius", &load_radius, 0, 2000, "%.0f", 2);
		ImGui::DragFloat("unload_hyster", &unload_hyster, 1);

		ImGui::DragFloat("load_lod_start", &load_lod_start, 1);
		ImGui::DragFloat("load_lod_unit", &load_lod_unit, 1);

		ImGui::DragInt("cap_chunk_load", &cap_chunk_load, 0.02f);
		ImGui::DragInt("cap_chunk_mesh", &cap_chunk_mesh, 0.02f);

		svo.imgui();

		imgui_pop();
	}

	block_id query_block (int3 pos) {
		return svo.octree_read(pos);
	}
	void set_block (int3 pos, block_id bid) {
		svo.octree_write(pos, 0, bid);
	}
};
