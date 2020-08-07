#pragma once
#include "common.hpp"

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

class Chunk;

struct WorldGenerator {
	std::string seed_str = "test2";
	uint64_t seed;

	float elev_freq = 400, elev_amp = 25;
	float rough_freq = 220;

	struct Detail {
		float freq, amp;
	};
	std::vector<Detail> detail = {
		{ 70, 12 },
		{ 20,  3 },
		{  3, 0.14f },
	};

	float tree_desity_period = 200;
	float tree_density_amp = 1;

	float grass_desity_period = 40;
	float grass_density_amp = .5f;
	
	WorldGenerator (): seed{get_seed(seed_str)} {
		
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

	void generate_chunk (Chunk& chunk) const;
};
