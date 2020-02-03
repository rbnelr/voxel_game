#pragma once
#include "../kissmath.hpp"
#include <random>

template <typename ENGINE>
struct _Random {
	ENGINE generator;

	_Random (); // seed with random value from global rng
	_Random (uint64_t seed): generator{(unsigned int)seed} {} // seed with value

	template <typename DISTRIBUTION>
	inline auto generate (DISTRIBUTION distr) {
		return distr(generator);
	}

	inline uint32_t uniform_u32 () {
		static_assert(sizeof(uint32_t) == sizeof(int), "");
		std::uniform_int_distribution<int> distribution (std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
		return (uint32_t)generate(distribution);
	}
	inline uint64_t uniform_u64 () {
		return (uint64_t)uniform_u32() << 32ull | (uint64_t)uniform_u32();
	}

	inline bool chance (float prob=0.5f) {
		std::bernoulli_distribution	distribution (prob);
		return generate(distribution);
	}

	inline int uniform (int min, int max) {
		std::uniform_int_distribution<int> distribution (min, max -1);
		return generate(distribution);
	}

	inline float uniform (float min=0, float max=1) {
		std::uniform_real_distribution<float>	distribution (min, max);
		return generate(distribution);
	}

	inline float normal (float stddev, float mean=0) {
		std::normal_distribution<float> distribution (mean, stddev);
		return generate(distribution);
	}

	inline bool2  chance2  (float2 prob=0.5f            ) { return bool2( uniform(prob)        , uniform(prob)        ); }
	inline int2   uniform2 (int2   min  ,  int2   max   ) { return int2(  uniform(min, max)    , uniform(min, max)    ); }
	inline float  uniform2 (int2   min=0,  float2 max=1 ) { return int2(  uniform(min, max)    , uniform(min, max)    ); }
	inline float2 normal2  (float2 stddev, float2 mean=0) { return float2(uniform(stddev, mean), uniform(stddev, mean)); }

	inline bool3  chance3  (float3 prob=0.5f            ) { return bool3( uniform(prob)        , uniform(prob)        , uniform(prob)        ); }
	inline int3   uniform3 (int3   min  ,  int3   max   ) { return int3(  uniform(min, max)    , uniform(min, max)    , uniform(min, max)    ); }
	inline float  uniform3 (int3   min=0,  float3 max=1 ) { return int3(  uniform(min, max)    , uniform(min, max)    , uniform(min, max)    ); }
	inline float3 normal3  (float3 stddev, float3 mean=0) { return float3(uniform(stddev, mean), uniform(stddev, mean), uniform(stddev, mean)); }

	inline bool4  chance4  (float4 prob=0.5f            ) { return bool4( uniform(prob)        , uniform(prob)        , uniform(prob)        , uniform(prob)        ); }
	inline int4   uniform4 (int4   min  ,  int4   max   ) { return int4(  uniform(min, max)    , uniform(min, max)    , uniform(min, max)    , uniform(min, max)    ); }
	inline float  uniform4 (int4   min=0,  float4 max=1 ) { return int4(  uniform(min, max)    , uniform(min, max)    , uniform(min, max)    , uniform(min, max)    ); }
	inline float4 normal4  (float4 stddev, float4 mean=0) { return float4(uniform(stddev, mean), uniform(stddev, mean), uniform(stddev, mean), uniform(stddev, mean)); }

	inline float3 uniform_in_sphere (float radius) {
		float3 pos;
		do {
			pos = uniform3(-radius, +radius);
		} while (length_sqr(pos) > radius*radius);
		return pos;
	}
	inline float3 uniform_on_sphere (float radius) {
		float3 pos;
		float len;
		do {
			pos = uniform3(-radius, +radius);
			len = length_sqr(pos);
		} while (len > 1 || len == 0.0f);
		
		return pos / sqrt(len) * radius;
	}

	inline float3 uniform_direction () {
		return uniform_on_sphere(1);
	}
	inline float3 uniform_vector (float min_magnitude=0, float max_magnitude=1) {
		return uniform_direction() * uniform(min_magnitude, max_magnitude);
	}
};

typedef _Random<std::default_random_engine> Random;

// Global random number generator
extern Random random;

template <typename T>
_Random<T>::_Random (): _Random(random.uniform_u32()) {} // just use 32 bits because engine only takes u32 seed anyway

#if 0
struct Fast_Rand {
	int seed;
	static constexpr int rand_max = 0x7FFF;

	Fast_Rand (int seed): seed{seed} {}

	inline int fastrand() { 
		// https://stackoverflow.com/questions/1640258/need-a-fast-random-generator-for-c
		seed = (214013*seed+2531011); 
		return (seed>>16)&0x7FFF; 
	} 

	inline float uniform (float min, float max) {
		int i = fastrand();
		float f = (float)i / (float)rand_max;
		return lerp(min, max, f);
	}
};
#endif
