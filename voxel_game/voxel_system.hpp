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

	float load_lod_start = 0.0f;
	float load_lod_unit = 64.0f;
	
	// artifically limit both the size of the async queue and how many results to take from the results
	int cap_chunk_load = 64;
	// artifically limit (delay) meshing of chunks to prevent complete freeze of main thread at the cost of some visual artefacts
	int cap_chunk_mesh = max(parallelism_threads*2, 4); // max is 2 meshings per cpu core per frame

	void imgui () {
		if (!imgui_push("Voxels")) return;

		ImGui::DragFloat("load_lod_start", &load_lod_start, 1, 0, 1024);
		ImGui::DragFloat("load_lod_unit", &load_lod_unit, 1, 16, 1024);

		ImGui::DragInt("cap_chunk_load", &cap_chunk_load, 0.02f);
		ImGui::DragInt("cap_chunk_mesh", &cap_chunk_mesh, 0.02f);

		svo.imgui();

		imgui_pop();
	}

	block_id query_block (int3 pos, bool phys_read=false) {
		return svo.octree_read(pos, phys_read);
	}
	void set_block (int3 pos, block_id bid) {
		using namespace svo;
		svo.octree_write(pos, 0, BLOCK_ID, (Voxel)bid);
	}
};
