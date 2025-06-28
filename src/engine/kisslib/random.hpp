#pragma once
#include "kissmath.hpp"
#include <random>

template <typename ENGINE>
struct _Random {
	ENGINE generator;

	_Random (); // seed with random value from global rng
	_Random (uint64_t seed): generator{(unsigned int)seed} {} // seed with value

	inline uint32_t uniform_u32 () {
		static_assert(sizeof(uint32_t) == sizeof(int), "");
		std::uniform_int_distribution<int> distribution (std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
		return (uint32_t)distribution(generator);
	}
	inline uint64_t uniform_u64 () {
		return (uint64_t)uniform_u32() << 32ull | (uint64_t)uniform_u32();
	}

	inline bool chance (float prob=0.5f) {
		std::bernoulli_distribution	distribution (prob);
		return distribution(generator);
	}

	inline int uniformi (int min, int max) {
		std::uniform_int_distribution<int> distribution (min, max -1);
		return distribution(generator);
	}

	inline float uniformf (float min=0, float max=1) {
		std::uniform_real_distribution<float>	distribution (min, max);
		return distribution(generator);
	}

	inline float normalf (float stddev, float mean=0) {
		std::normal_distribution<float> distribution (mean, stddev);
		return distribution(generator);
	}

	inline int2   uniform2i (int2   min  , int2   max   ) { return int2(  uniformi(min.x, max.x), uniformi(min.y, max.y)); }
	inline float2 uniform2f (float2 min=0, float2 max=1 ) { return float2(uniformf(min.x, max.x), uniformf(min.y, max.y)); }
	inline float2 normal2f  (float2 stddev, float2 mean=0) { return float2(normalf(stddev.x, mean.x), normalf(stddev.y, mean.y)); }

	inline int3   uniform3i (int3   min  , int3   max   ) { return int3(  uniformi(min.x, max.x), uniformi(min.y, max.y), uniformi(min.z, max.z)); }
	inline float3 uniform3f (float3 min=0, float3 max=1 ) { return float3(uniformf(min.x, max.x), uniformf(min.y, max.y), uniformf(min.z, max.z)); }
	inline float3 normal3f  (float3 stddev, float3 mean=0) { return float3(normalf(stddev.x, mean.x), normalf(stddev.y, mean.y), normalf(stddev.z, mean.z)); }

	inline int4   uniform4i (int4   min  , int4   max   ) { return int4(  uniformi(min.x, max.x), uniformi(min.y, max.y), uniformi(min.z, max.z), uniformi(min.w, max.w)    ); }
	inline float4 uniform4f (float4 min=0, float4 max=1 ) { return float4(uniformf(min.x, max.x), uniformf(min.y, max.y), uniformf(min.z, max.z), uniformf(min.w, max.w)    ); }
	inline float4 normal4f  (float4 stddev, float4 mean=0) { return float4(normalf(stddev.x, mean.x), normalf(stddev.y, mean.y), normalf(stddev.z, mean.z), normalf(stddev.w, mean.w)); }

	// TODO: replace with spherical coordinates

	// random point uniformly distributed in unit sphere
	inline float3 uniform_in_sphere () {
	#if 0
		float3 pos;
		do {
			pos = uniform3f(-1.0f, +1.0f);
		} while (length_sqr(pos) > 1.0f);
		return pos;
	#else
		std::uniform_real_distribution<float>	distribution (0.0f, 1.0f);

		float azim =      distribution(generator) * PI*2.0f;
		float elev = acos(distribution(generator) * 2.0f - 1.0f);
		float radius = cbrtf(distribution(generator)); // cbrtf = cube root

		float ac = cos(azim), as = sin(azim);
		float ec = cos(elev), es = sin(elev);

		return float3(radius * ac*es, radius * as*es, radius * ec);
	#endif
	}

	// random point uniformly distributed on surface of unit sphere
	inline float3 uniform_direction () {
	#if 0
		float3 pos;
		float len;
		do {
			pos = uniform3f(-1.0f, +1.0f);
			len = length_sqr(pos);
		} while (len > 1.0f || len == 0.0f);
		
		return pos / sqrt(len);
	#else
		std::uniform_real_distribution<float>	distribution (0.0f, 1.0f);

		float azim =      distribution(generator) * PI*2.0f;
		float elev = acos(distribution(generator) * 2.0f - 1.0f);

		float ac = cos(azim), as = sin(azim);
		float ec = cos(elev), es = sin(elev);

		return float3(ac*es, as*es, ec);
	#endif
	}

	// random uniform direction with random uniform length in some range
	// could be used as random velocity
	inline float3 uniform_vector (float min_magnitude=0, float max_magnitude=1) {
		return uniform_direction() * uniformf(min_magnitude, max_magnitude);
	}

#if 0
	// get random index of array of probability weights
	// numbers are assumed to be >= 0
	// returns -1 if probabilities are all 0s and 0s are never picked 
	// array is scanned to get total probability weight, then RNG is called to get random num in [0, total prob weight]
	// then array is scanned again to find index where random num crosses the cumulative probability
	template <typename ARRAY>
	inline int weighted_choice (ARRAY const& probabilities) {
		float total_prob = 0;
		for (auto p : probabilities) {
			total_prob += p;
		}

		if (total_prob == 0.0f)
			return -1; // all 0 prob

		float rand = uniform(0.0f, total_prob);

		// pick index by comparing cumulative weight against random num
		float accum = 0.0f;

		int count = (int)probabilities.size();
		for (int i=0;; i++) {
			accum += probabilities[i];
			if (rand < accum || i+1 == count)
				return i;
		}
	}

	template <typename FUNC>
	inline int weighted_choice (int count, FUNC get_prob, float total_prob) {
		if (total_prob == 0.0f)
			return -1; // all 0 prob

		float rand = uniform(0.0f, total_prob);

		// pick index by comparing cumulative weight against random num
		float accum = 0.0f;

		for (int i=0;; i++) {
			accum += get_prob(i);
			if (rand < accum || i+1 == count)
				return i;
		}
	}

#endif
};

typedef _Random<std::default_random_engine> Random;

uint64_t get_initial_random_seed ();

// Global random number generator
inline Random random = Random( get_initial_random_seed() );

template <typename T>
_Random<T>::_Random (): _Random(random.uniform_u32()) {} // just use 32 bits because engine only takes u32 seed anyway
