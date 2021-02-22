#pragma once
#include "common.hpp"
#include "blocks.hpp"
#include "chunks.hpp"

inline uint64_t get_seed (std::string_view str) {
	str = kiss::trim(str);

	if (str.size() == 0) // "" -> random seed
		return std::hash<uint64_t>()(random.uniform_u64());

	return std::hash<std::string_view>()(str);
}
template<typename T>
struct Gradient_KV {
	float	key;
	T		val;
};

template<typename T>
inline T gradient (float key, Gradient_KV<T> const* kvs, size_t kvs_count) {
	if (kvs_count == 0) return T(0);

	size_t i=0;
	for (; i<kvs_count; ++i) {
		if (key < kvs[i].key) break;
	}

	if (i == 0) { // val is lower than the entire range
		return kvs[0].val;
	} else if (i == kvs_count) { // val is higher than the entire range
		return kvs[i -1].val;
	} else {
		assert(kvs_count >= 2 && i < kvs_count);

		auto& a = kvs[i -1];
		auto& b = kvs[i];
		return map(key, a.key, b.key, a.val, b.val);
	}
}
template<typename T>
inline T gradient (float key, std::initializer_list<Gradient_KV<T>> const& kvs) {
	return gradient<T>(key, &*kvs.begin(), kvs.size());
}

namespace worldgen {
	// I don't have any way of supplying block names to the world generation
	// so I hardcode block types for now and map them to the ids from blocks.json
	// Really seems like this is pointless, though

	enum BlockID : block_id {
		B_AIR			=0,
		B_UNBREAKIUM	,
		B_WATER			,
		B_STONE			,
		B_HARDSTONE		,
		B_EARTH			,
		B_GRASS			,
		B_TREE_LOG		,
		B_LEAVES		,
		B_TALLGRASS		,
		B_TORCH			,
		B_COUNT			,
	};
	struct BlockIDs {
		block_id	bids[B_COUNT];

		char const*	names[B_COUNT] = {
			/*B_AIR			*/ "air",
			/*B_UNBREAKIUM	*/ "unbreakium",
			/*B_WATER		*/ "water",
			/*B_STONE		*/ "stone",
			/*B_HARDSTONE	*/ "hardstone",
			/*B_EARTH		*/ "earth",
			/*B_GRASS		*/ "grass",
			/*B_TREE_LOG	*/ "tree_log",
			/*B_LEAVES		*/ "leaves",
			/*B_TALLGRASS	*/ "tallgrass",
			/*B_TORCH		*/ "torch",
		};

		void load () {
			for (int i=0; i<B_COUNT; ++i) {
				bids[i] = g_assets.block_types.map_id(names[i]);
			}
		}

		block_id operator[] (BlockID bid) const {
			return bids[bid];
		}
	};
}

struct WorldGenerator {
	std_string seed_str = "test2";
	uint64_t seed;

	float elev_freq = 400, elev_amp = 25;
	float rough_freq = 220;

	struct Detail {
		float freq, amp;
	};
	std_vector<Detail> detail = {
		{ 70, 12 },
		{ 20,  3 },
		{  3, 0.14f },
	};

	float tree_desity_period = 200;
	float tree_density_amp = 1;

	float grass_desity_period = 40;
	float grass_density_amp = .5f;

	worldgen::BlockIDs	bids;
	
	WorldGenerator (): seed{get_seed(seed_str)} {
		bids.load();
	}

	void imgui () {
		if (!imgui_push("WorldGenerator")) return;

		ImGui::InputText("seed str", &seed_str, 0, NULL, NULL);
		seed = get_seed(seed_str);
		ImGui::Text("seed code: 0x%016p", seed);

		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.2f);

		ImGui::DragFloat("elev_freq", &elev_freq, 0.05f);
		ImGui::SameLine();
		ImGui::DragFloat("amp##elev", &elev_amp, 0.05f);

		ImGui::DragFloat("rough_freq", &rough_freq, 0.05f);

		for (int i=0; i<(int)detail.size(); ++i) {
			ImGui::PushID(i);
				ImGui::DragFloat("freq", &detail[i].freq, 0.05f);
				ImGui::SameLine();
				ImGui::DragFloat("amp",  &detail[i].amp,  0.05f);
			ImGui::PopID();
		}

		ImGui::DragFloat("tree_des_per", &tree_desity_period, 0.05f);
		ImGui::SameLine();
		ImGui::DragFloat("amp##tree_dens",  &tree_density_amp,  0.05f);

		ImGui::DragFloat("grass_des_per", &grass_desity_period, 0.05f);
		ImGui::SameLine();
		ImGui::DragFloat("amp##grass_dens",  &grass_density_amp,  0.05f);

		ImGui::PopItemWidth();
		imgui_pop();
	}
};

struct WorldgenJob {
	int3					chunk_pos;
	WorldGenerator const*	wg;

	int						phase;
	block_id*				voxel_output;

	void execute ();

	~WorldgenJob () { // handle cases where we destroy the threadpool without its results being read first
		if (voxel_output)
			free(voxel_output);
	}
};

inline auto background_threadpool = Threadpool<WorldgenJob>(background_threads, TPRIO_BACKGROUND, ">> background threadpool"  );

namespace worldgen {
	// Faster voxel access in 3x3x3 chunk region than hashmap-based global chunk lookup
	struct Neighbours {
		Chunk* neighbours[3][3][3];

		Chunk* get (int x, int y, int z) {
			return neighbours[z+1][y+1][x+1];
		}

		// read block with coord relative to center chunk
		chunk_id read_block (Chunks& chunks, int x, int y, int z) const {
			assert(x >= -CHUNK_SIZE && x < CHUNK_SIZE*2 &&
			       y >= -CHUNK_SIZE && y < CHUNK_SIZE*2 &&
			       z >= -CHUNK_SIZE && z < CHUNK_SIZE*2);
			int bx, by, bz;
			int cx, cy, cz;
			CHUNK_BLOCK_POS(x,y,z, cx,cy,cz, bx,by,bz);

			Chunk const* chunk = neighbours[cz+1][cy+1][cx+1];
			return chunks.read_block(bx,by,bz, chunk);
		}
	};


	void object_pass (Chunks& chunks, Chunk& chunk, Neighbours& neighbours, WorldGenerator const* wg);
}
