#pragma once
#include "chunks.hpp"

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
	float elev_freq = 400, elev_amp = 25;
	float rough_freq = 220;
	float detail0_freq = 70, detail0_amp = 12;
	float detail1_freq = 20, detail1_amp = 3;
	float detail2_freq = 3, detail2_amp = 0.14f;

	float noise_tree_desity_period = 200;
	float noise_tree_density_amp = 1;

	void generate_chunk (Chunk& chunk, uintptr_t world_seed) const;
};
