#pragma once
#include "stdafx.hpp"
#include "svo.hpp"
#include "voxel_mesher.hpp"
#include "threading.hpp"

#define CHUNK_SIZE 32
#define CHUNK_SCALE 5

// Backend system resposible for handling storage, changes to, and displaying of the voxel world
class Voxels {
public:
	SVO	svo;
	VoxelMesher mesher;

	float load_radius =	200.0f;
	float unload_hyster = 0;
	
	int cap_chunk_load = 64; // limit both queueing and finalizing, since (at least for now) the queuing takes too long (causing all chunks to be generated in the first frame, not like I imagined...)
	// artifically limit (delay) meshing of chunks to prevent complete freeze of main thread at the cost of some visual artefacts
	int cap_chunk_mesh = max(parallelism_threads*2, 4); // max is 2 meshings per cpu core per frame

	void imgui () {
		if (!imgui_push("Voxels")) return;

		ImGui::SliderFloat("load_radius", &load_radius, 0, 2000, "%.0f", 2);
		ImGui::DragFloat("unload_hyster", &unload_hyster, 1);

		ImGui::DragInt("cap_chunk_load", &cap_chunk_load, 0.02f);
		ImGui::DragInt("cap_chunk_mesh", &cap_chunk_mesh, 0.02f);

		svo.imgui();

		imgui_pop();
	}

	block_id query_block (int3 pos) {
		return svo.octree_read(pos);
	}
};
