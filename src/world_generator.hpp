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

		B_EARTH			,
		B_GRASS			,

		B_STONE			,
		B_HARDSTONE		,

		B_SAND			,
		B_GRAVEL		,

		B_MOSSY_ROCK	,
		B_ICE			,
		B_MAGMA			,
		B_BADROCK		,
		B_SLIME			,
		B_CRYSTAL		,
		B_URANIUM		,

		B_TREE_LOG		,
		B_LEAVES		,
		B_TALLGRASS		,
		B_TORCH			,

		B_CRYSTAL2		,
		B_CRYSTAL3		,
		B_CRYSTAL4		,
		B_CRYSTAL5		,
		B_CRYSTAL6		,

		B_COUNT			,
	};
	struct BlockIDs {
		block_id	bids[B_COUNT];

		char const*	names[B_COUNT] = {
			/*B_AIR			*/ "air",
			/*B_UNBREAKIUM	*/ "unbreakium",
			/*B_WATER		*/ "water",

			/*B_EARTH		*/ "earth",
			/*B_GRASS		*/ "grass",

			/*B_STONE		*/ "stone",
			/*B_HARDSTONE	*/ "hardstone",

			/*B_SAND		*/ "sand",
			/*B_GRAVEL		*/ "gravel",

			/*B_MOSSY_ROCK	*/ "mossy_rock",
			/*B_ICE			*/ "ice",
			/*B_MAGMA		*/ "magma",
			/*B_BADROCK		*/ "badrock",
			/*B_SLIME		*/ "slime",
			/*B_CRYSTAL		*/ "crystal",
			/*B_URANIUM		*/ "uranium",

			/*B_TREE_LOG	*/ "tree_log",
			/*B_LEAVES		*/ "leaves",
			/*B_TALLGRASS	*/ "tallgrass",
			/*B_TORCH		*/ "torch",

			/*B_CRYSTAL2	*/ "crystal2",
			/*B_CRYSTAL3	*/ "crystal3",
			/*B_CRYSTAL4	*/ "crystal4",
			/*B_CRYSTAL5	*/ "crystal5",
			/*B_CRYSTAL6	*/ "crystal6",
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
	friend void to_json(nlohmann::ordered_json& j, const WorldGenerator& t) {
		j["seed_str"] = t.seed_str;
	}
	friend void from_json(const nlohmann::ordered_json& j, WorldGenerator& t) {
		if (j.contains("seed_str")) j.at("seed_str").get_to(t.seed_str);

		t.seed = get_seed(t.seed_str); // recalc seed from loaded seed_str
	}
	
	//	max_depth, base_depth,
	//	large_noise, small_noise,
	//	ground_ang

	std_string seed_str = "test2";
	uint64_t seed = get_seed(seed_str);

	struct NoiseParam {
		SERIALIZE(NoiseParam, period, strength, cutoff, cutoff_val)

		float period = 20;
		float strength = 1;

		bool cutoff = false;
		float cutoff_val = 0.0f;
	};

	float max_depth = 40;
	float base_depth = 25;

	std::vector<NoiseParam> large_noise = {
		{ 300, -1 },
		{ 180, -1 },
		{  70, -1.5f },
	};

	std::vector<NoiseParam> small_noise = {
		{ 4, 0.3f },
		{ 20, 0.9f, true },
	};

	bool stalac = true;
	float stalac_dens = 0.001f;
	
	bool disable_grass = true;

	float ground_ang = 0.55f;
	float earth_overhang_stren = 0.55f;

	float earth_depth = 5;
	float rock_depth = 1;

	float tree_desity_period = 200;
	float tree_density_amp = 1.5f;

	float grass_desity_period = 40;
	float grass_density_amp = .5f;

	worldgen::BlockIDs	bids;
	
	WorldGenerator () {
		bids.load();
	}


	static void imgui_noise_layers (char const* name, std::vector<NoiseParam>& layers) {
		if (!imgui_push(name)) return;

		int count = (int)layers.size();
		ImGui::DragInt("count", &count, 0.01f, 0, 20);
		layers.resize(count);

		for (int i=0; i<(int)layers.size(); ++i) {
			ImGui::PushID(i);
			ImGui::DragFloat("freq", &layers[i].period, 0.05f);		ImGui::SameLine();
			ImGui::DragFloat("amp",  &layers[i].strength,  0.05f);	ImGui::SameLine();

			ImGui::Checkbox("cutoff", &layers[i].cutoff);

			if (layers[i].cutoff) {	ImGui::SameLine();
			ImGui::DragFloat("cutoff_val",  &layers[i].cutoff_val,  0.05f);
			}
			ImGui::PopID();
		}

		imgui_pop();
	}
	void imgui () {
		if (!imgui_push("WorldGenerator")) return;

		if (ImGui::InputText("seed str", &seed_str, 0, NULL, NULL))
			seed = get_seed(seed_str); // recalc seed from modified seed_str
		ImGui::Text("seed code: 0x%016p", seed);

		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.2f);

		ImGui::Separator();

		ImGui::DragFloat("max_depth", &max_depth, 0.1f);
		ImGui::DragFloat("base_depth", &base_depth, 0.1f);

		ImGui::Spacing();

		ImGui::Spacing();
		imgui_noise_layers("large_noise", large_noise);

		ImGui::Spacing();
		imgui_noise_layers("small_noise", small_noise);

		ImGui::Checkbox("stalac", &stalac);
		ImGui::DragFloat("stalac_dens", &stalac_dens, 0.1f);

		ImGui::Spacing();
		ImGui::DragFloat("ground_ang", &ground_ang, 0.01f);
		ImGui::DragFloat("earth_overhang_stren", &earth_overhang_stren, 0.1f);

		ImGui::Checkbox("disable_grass", &disable_grass);

		ImGui::PopItemWidth();
		imgui_pop();
	}
};

// TODO: get rid of open_simplex_noise and replace it with a better library that is not object oriented and supports a seed per call
// ideally also is written with simd usage in mind
#include "open_simplex_noise/open_simplex_noise.hpp"

namespace worldgen {
	struct NoisePass {
		int3					chunk_pos;
		WorldGenerator const*	wg;
		OSN::Noise<3>			noise3;

		// output
	#define LARGE_NOISE_SIZE 4
	#define LARGE_NOISE_CHUNK_SIZE (CHUNK_SIZE / LARGE_NOISE_SIZE)
	#define LARGE_NOISE_COUNT (LARGE_NOISE_CHUNK_SIZE +1)

		// NOTE: deriv is opposite of real derivative -> vector pointing to negative values, ie. air, because I prefer it this way around
		
		// float value; float3 deriv;
		float large_noise[LARGE_NOISE_COUNT][LARGE_NOISE_COUNT][LARGE_NOISE_COUNT][4];
		block_id voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];

		NoisePass (int3 chunk_pos, WorldGenerator const* wg):
			chunk_pos{chunk_pos}, wg{wg}, noise3{wg->seed} {

		}

		float noise01 (float3 const& pos, float period, float seed) {
			float3 p = pos / period; // period is inverse frequency

			return noise3.eval<float>(p.x, p.y, p.z) * 0.5f + 0.5f;
		}
		float noise (float3 const& pos, float period, float seed) {
			//pos += (1103 * float3(53, 211, 157)) * seed; // random prime directional offset to replace lack of seed in OSN noise
			float3 p = pos / period; // period is inverse frequency

			// adjust noise values because I treat them like a SDF and they seem to be off by a factor of ~4
			return noise3.eval<float>(p.x, p.y, p.z) * period * 0.25f;
		}

		float calc_large_noise (float3 const& pos);
		BlockID cave_noise (float3 const& pos, float large_noise, float3 const& normal);

		void generate ();
	};
}

struct WorldgenJob {
	worldgen::NoisePass		noise_pass;

	// unfortunately need ctor because OSN::Noise<3> does work in it's ctor, which it shouldn't
	WorldgenJob (int3 chunk_pos, WorldGenerator const* wg): noise_pass{chunk_pos, wg} {}

	void execute ();
};

inline auto background_threadpool = Threadpool<WorldgenJob>(background_threads, TPRIO_BACKGROUND, ">> background threadpool"  );

namespace worldgen {
	// Faster voxel access in 3x3x3 chunk region than hashmap-based global chunk lookup
	struct Neighbours {
		chunk_id neighbours[3][3][3];

		chunk_id get (int x, int y, int z) {
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

			chunk_id chunk = neighbours[cz+1][cy+1][cx+1];
			return chunks.read_block(bx,by,bz, chunk);
		}
	};


	void object_pass (Chunks& chunks, chunk_id cid, Neighbours& neighbours, WorldGenerator const* wg);
}
