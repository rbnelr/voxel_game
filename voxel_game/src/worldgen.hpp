#pragma once
#include "stdafx.hpp"
#include "threading.hpp"
#include "worldgen_dll.hpp"
#include "svo.hpp"

namespace worldgen {
	inline uint64_t get_seed (std::string_view str) {
		str = kiss::trim(str);

		if (str.size() == 0) // "" -> random seed
			return std::hash<uint64_t>()(random.uniform_u64());

		return std::hash<std::string_view>()(str);
	}

	struct WorldGenerator {
		std::string seed_str = "test6";
		uint64_t seed;

		generate_chunk_dll_fp generate_chunk_dll = nullptr;

		WorldGenerator (): seed{get_seed(seed_str)} {
		
		}

		void imgui () {
			if (!imgui_push("WorldGenerator")) return;

			ImGui::InputText("seed str", &seed_str, 0, NULL, NULL);
			seed = get_seed(seed_str);
			ImGui::Text("seed code: 0x%016p", seed);

			imgui_pop();
		}

		void generate_chunk (svo::Chunk* chunk, svo::SVO& svo) const {
			ZoneScoped;

			block_id* blocks;
			{
				ZoneScopedN("alloc buffer");
				blocks = (block_id*)calloc(1, sizeof(block_id) * CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE);
			}

			auto chunk_seed = seed ^ hash(chunk->pos);
			//auto rand = Random(chunk_seed);

			if (generate_chunk_dll)
				generate_chunk_dll(blocks, chunk->pos, chunk->scale - CHUNK_SCALE, chunk_seed);

			svo.chunk_to_octree(chunk, blocks);

			{
				ZoneScopedN("free buffer");
				free(blocks);
			}
		}
	};
}
using worldgen::WorldGenerator;
