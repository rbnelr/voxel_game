#pragma once

#include "kissmath/output/bool2.hpp"
#include "kissmath/output/bool3.hpp"
#include "kissmath/output/bool4.hpp"

#include "kissmath/output/int2.hpp"
#include "kissmath/output/int3.hpp"
#include "kissmath/output/int4.hpp"

#include "kissmath/output/int64v2.hpp"
#include "kissmath/output/int64v3.hpp"
#include "kissmath/output/int64v4.hpp"

#include "kissmath/output/uint8v2.hpp"
#include "kissmath/output/uint8v3.hpp"
#include "kissmath/output/uint8v4.hpp"

#include "kissmath/output/float2.hpp"
#include "kissmath/output/float3.hpp"
#include "kissmath/output/float4.hpp"

#include "kissmath/output/float2x2.hpp"
#include "kissmath/output/float3x3.hpp"
#include "kissmath/output/float4x4.hpp"
#include "kissmath/output/float3x4.hpp"

#include "kissmath/output/transform2d.hpp"
#include "kissmath/output/transform3d.hpp"

#include "kissmath_colors.hpp"

#include <type_traits>
#include <string>
#include "assert.h"

#include "smhasher/MurmurHash2.h"

namespace kissmath {

	inline float sign (float x) {
		return copysignf(1.0f, x);
	}
	inline float3 sign (float3 x) {
		return float3(
			copysignf(1.0f, x.x),
			copysignf(1.0f, x.y),
			copysignf(1.0f, x.z)
		);
	}
	
	inline constexpr float map_clamp (float x, float in_a, float in_b) {
		return clamp(map(x, in_a, in_b), 0.0f, 1.0f);
	}
	inline constexpr float map_clamp (float x, float in_a, float in_b, float out_a, float out_b) {
		return lerp(out_a, out_b, clamp(map(x, in_a, in_b), 0.0f, 1.0f));
	}
	
	// return [-180deg, 180deg]
	inline float angle2d (float2 dir) {
		return length_sqr(dir) > 0 ? atan2f(dir.y, dir.x) : 0;
	}
	
	// move from src to dst at fixed steps without overshooting
	// step usually is speed * dt
	inline float move_linear (float src, float dst, float step) {
		float diff = dst - src;
		return src + sign(diff) * min(step, abs(diff));
	}
	// move from src to dst at fixed steps without overshooting
	// step usually is speed * dt
	inline float3 move_linear (float3 src, float3 dst, float step) {
		float3 dir = normalizesafe(dst - src);
		float dist = length(dst - src);
		return src + dir * min(step, dist);
	}

	// TODO: not tested
	// lerp angle along shortest rotation angle
	// returns unmodded angle, need to modulo into your desired range again
	inline float lerp_angle (float src, float dst, float t) {
		float shortest_angle = fmodf(fmodf(dst - src, deg(360)) + deg(360 + 180), deg(360)) - deg(180);

		float dir = shortest_angle > 0.0f ? +1.0f : -1.0f;
		float mag = abs(shortest_angle);

		return lerp(src, src + shortest_angle, t);
	}
	// move from src angle to dst angle at fixed steps without overshooting along shortest rotation angle
	// angular_step usually is angular_speed * dt
	// returns unmodded angle, need to modulo into your desired range again
	inline float move_angle (float src, float dst, float angular_step) {
		// https://stackoverflow.com/questions/2708476/rotation-interpolation
		float shortest_angle = fmodf(fmodf(dst - src, deg(360)) + deg(360 + 180), deg(360)) - deg(180);

		float dir = shortest_angle > 0.0f ? +1.0f : -1.0f;
		float mag = abs(shortest_angle);

		// move towards at constant speed without overshooting
		return src + dir * min(angular_step, mag);
	}

	static constexpr uint64_t KB = 1024ull;
	static constexpr uint64_t MB = 1024ull*1024;
	static constexpr uint64_t GB = 1024ull*1024*1024;
	static constexpr uint64_t TB = 1024ull*1024*1024*1024;

	static constexpr uint64_t MSEC = 1000ull;
	static constexpr uint64_t USEC = 1000ull*1000;
	static constexpr uint64_t NSEC = 1000ull*1000*1000;

	inline std::string _format_thousands (long long i, char sep, const char* printformat) {
		std::string dst;
		dst.reserve(27);

		char src[27];

		int num, commas;

		num = snprintf(src, 27, printformat, i);

		char* cur = src;

		if (*cur == '-' || *cur == '+') {
			dst.push_back(*cur++);
			num--;
		}

		for (commas = 2 - num % 3; *cur; commas = (commas + 1) % 3) {
			dst.push_back(*cur++);
			if (commas == 1 && *cur != '\0') {
				dst.push_back(sep);
			}
		}

		return dst;
	}

	inline std::string format_thousands (int i, char sep=',') {
		return _format_thousands(i, sep, "%d");
	}
	inline std::string format_thousands (unsigned i, char sep=',') {
		return _format_thousands(i, sep, "%u");
	}
	inline std::string format_thousands (long long i, char sep=',') {
		return _format_thousands(i, sep, "%lld");
	}
	inline std::string format_thousands (unsigned long long i, char sep=',') {
		return _format_thousands(i, sep, "%llu");
	}

	struct Units {
		std::pair<float, char const*> const* units;
		size_t count;

		Units (std::initializer_list< std::pair<float, char const*> > units):
			units{units.begin()}, count{units.size()} {}
	};

	inline const Units BYTE_UNITS = Units({
		{ (float)1					, "B" },
		{ (float)1024				, "KB" },
		{ (float)1024*1024			, "MB" },
		{ (float)1024*1024*1024		, "GB" },
		{ (float)1024*1024*1024*1024, "TB" },
	});
	inline const Units THOUSANDS_UNITS = Units({
		{ (float)1/1000/1000/1000	, "n" }, // nano-
		{ (float)1/1000/1000		, "u" }, // micro- (u inplace of Mu (μ))
		{ (float)1/1000				, "m" }, // milli-
		{ (float)1					, "" },
		{ (float)1000				, "k" }, // kilo-
		{ (float)1000*1000			, "M" }, // mega-
		{ (float)1000*1000*1000		, "B" }, // giga-
	});

	template <typename T>
	inline std::string format_unit (T val, Units const& units) {
		// TODO:
	}

	//// Getting scalar type + vector dimension from type

	template <typename T> inline constexpr bool is_matrix () { return false; }

	template<> inline constexpr bool is_matrix<float2x2> () { return true; }
	template<> inline constexpr bool is_matrix<float3x3> () { return true; }
	template<> inline constexpr bool is_matrix<float4x4> () { return true; }
	template<> inline constexpr bool is_matrix<float3x4> () { return true; }

	// Include all eventualities
	enum class ScalarType {
		FLOAT,	DOUBLE,	FLOAT8, FLOAT16,
		INT,	UINT,
		INT8,	UINT8,
		INT64,	UINT64,
		INT16,	UINT16,
		BOOL
	};
	struct VectorTypeInfo {
		ScalarType type;
		int components;
	};

	template <typename T> inline constexpr VectorTypeInfo get_type ();

	template<> inline constexpr VectorTypeInfo get_type<float   > () { return { ScalarType::FLOAT, 1 }; }
	template<> inline constexpr VectorTypeInfo get_type<float2  > () { return { ScalarType::FLOAT, 2 }; }
	template<> inline constexpr VectorTypeInfo get_type<float3  > () { return { ScalarType::FLOAT, 3 }; }
	template<> inline constexpr VectorTypeInfo get_type<float4  > () { return { ScalarType::FLOAT, 4 }; }
	template<> inline constexpr VectorTypeInfo get_type<int     > () { return { ScalarType::INT, 1 }; }
	template<> inline constexpr VectorTypeInfo get_type<int2    > () { return { ScalarType::INT, 2 }; }
	template<> inline constexpr VectorTypeInfo get_type<int3    > () { return { ScalarType::INT, 3 }; }
	template<> inline constexpr VectorTypeInfo get_type<int4    > () { return { ScalarType::INT, 4 }; }
	template<> inline constexpr VectorTypeInfo get_type<int64_t > () { return { ScalarType::INT64, 1 }; }
	template<> inline constexpr VectorTypeInfo get_type<int64v2 > () { return { ScalarType::INT64, 2 }; }
	template<> inline constexpr VectorTypeInfo get_type<int64v3 > () { return { ScalarType::INT64, 3 }; }
	template<> inline constexpr VectorTypeInfo get_type<int64v4 > () { return { ScalarType::INT64, 4 }; }
	template<> inline constexpr VectorTypeInfo get_type<uint8_t > () { return { ScalarType::UINT8, 1 }; }
	template<> inline constexpr VectorTypeInfo get_type<uint8v2 > () { return { ScalarType::UINT8, 2 }; }
	template<> inline constexpr VectorTypeInfo get_type<uint8v3 > () { return { ScalarType::UINT8, 3 }; }
	template<> inline constexpr VectorTypeInfo get_type<uint8v4 > () { return { ScalarType::UINT8, 4 }; }
	template<> inline constexpr VectorTypeInfo get_type<bool    > () { return { ScalarType::BOOL, 1 }; }
	template<> inline constexpr VectorTypeInfo get_type<bool2   > () { return { ScalarType::BOOL, 2 }; }
	template<> inline constexpr VectorTypeInfo get_type<bool3   > () { return { ScalarType::BOOL, 3 }; }
	template<> inline constexpr VectorTypeInfo get_type<bool4   > () { return { ScalarType::BOOL, 4 }; }

	template<> inline constexpr VectorTypeInfo get_type<int8_t  > () { return { ScalarType::INT8, 1 }; }
	template<> inline constexpr VectorTypeInfo get_type<int16_t > () { return { ScalarType::INT16, 1 }; }
	template<> inline constexpr VectorTypeInfo get_type<uint16_t> () { return { ScalarType::UINT16, 1 }; }
	template<> inline constexpr VectorTypeInfo get_type<uint64_t> () { return { ScalarType::UINT64, 1 }; }

	// round up x to y, assume y is power of two
	template <typename T> inline constexpr T align_up (T x, T y) {
		return (x + (y - 1)) & ~(y - 1);
	}
	// check if power of two
	template <typename T> inline constexpr T is_pot (T x) {
		return (x & (x - 1)) == 0;
	}

	// check if power of two
	inline constexpr int get_const_log2 (uint32_t x) {
		for (int i=31; i>=0; i--) {
			if (x & (1u << i))
				return i;
		}
		return -1;
	}

	// round up to power of two, 0->1 1->1 2->2 3->4 4->4 5->8 etc.  x>2^31 won't work
	inline uint32_t round_up_pot (uint32_t x, uint32_t* shift=nullptr) {
		if (x <= 1) {
			if (shift) *shift = 0;
			return 1;
		}
		x--;
		assert(x <= (1u << 31)); // can't represent 2^32 so x>2^31 this is out of range

		unsigned long i;
		auto res = _BitScanReverse(&i, (unsigned long)x);
		assert(res);
		if (shift) *shift = i+1;
		return 1u << (i+1);
	}

	// round up to power of two, 0->1 1->1 2->2 3->4 4->4 5->8 etc.  x>2^63 won't work
	inline uint64_t round_up_pot (uint64_t x, uint32_t* shift=nullptr) {
		if (x <= 1) {
			if (shift) *shift = 0;
			return 1;
		}
		x--;
		assert(x <= (1ull << 63)); // can't represent 2^64 so x>2^63 this is out of range

		unsigned long i;
		auto res = _BitScanReverse64(&i, x);
		assert(res);
		if (shift) *shift = i+1;
		return 1ull << (i+1);
	}
	
#if 0
	// from https://github.com/aappleby/smhasher/blob/master/src/MurmurHash1.cpp
	inline uint32_t MurmurHash1_32 (uint32_t const* key, int len) {
		constexpr uint32_t m = 0xc6a4a793;

		//uint32_t h = seed ^ (len * m);
		uint32_t h = 0;

		for (int i=0; i<len; ++i) {
			h += key[i];
			h *= m;
			h ^= h >> 16;
		}

		h *= m;
		h ^= h >> 10;
		h *= m;
		h ^= h >> 17;

		return h;
	}
	inline uint32_t MurmurHash1_8 (uint8_t const* key, int len) {
		constexpr uint32_t m = 0xc6a4a793;

		//uint32_t h = seed ^ (len * m);
		uint32_t h = 0;

		for (int i=0; i<len; i += 4) {
			h += *(uint32_t*)(key + i);
			h *= m;
			h ^= h >> 16;
		}

		switch(len) {
			case 3:
				h += (uint32_t)key[2] << 16;
			case 2:
				h += (uint32_t)key[1] << 8;
			case 1:
				h += (uint32_t)key[0];
				h *= m;
				h ^= h >> 16;
		};

		h *= m;
		h ^= h >> 10;
		h *= m;
		h ^= h >> 17;

		return h;
	}
#endif

	// Turns out that my MurmurHash1_32 code above was inlined and optimized into efficient code
	// but this hash function retains the loops in the generated code even though it it inlined
	// to fix that I made the len a compile time constant template argument, this gets us seperate code to be generated for different lengths
	// when you want a hash of a variable (or long) length of course use the regular code
	template <int LEN>
	uint64_t MurmurHash64A_fixedlen ( const void * key, uint64_t seed )
	{
		static constexpr int len = LEN;

		static constexpr uint64_t m = 0xc6a4a7935bd1e995ull;
		static constexpr int r = 47;

		uint64_t h = seed ^ (len * m);

		const uint64_t * data = (const uint64_t *)key;
		const uint64_t * end = data + (len/8);
		
		//while(data != end)
		for (int i=0; i<len/8; ++i) // This actually makes the compiler unroll this loop unlike the original while loop
		{
			uint64_t k = *data++;

			k *= m; 
			k ^= k >> r; 
			k *= m; 

			h ^= k;
			h *= m; 
		}

		const unsigned char * data2 = (const unsigned char*)data;

	#if 0
		// This does not get optimized away even though the len is constexpr
		switch (len & 7)
		{
			case 7: h ^= uint64_t(data2[6]) << 48;
			case 6: h ^= uint64_t(data2[5]) << 40;
			case 5: h ^= uint64_t(data2[4]) << 32;
			case 4: h ^= uint64_t(data2[3]) << 24;
			case 3: h ^= uint64_t(data2[2]) << 16;
			case 2: h ^= uint64_t(data2[1]) << 8;
			case 1: h ^= uint64_t(data2[0]);
				h *= m;
		};
	#else
		int i = len & 7;
		if (i > 0) {
			while (i > 0) {
				--i;
				h ^= uint64_t(data2[i]) << (i * 8);
			}
			h *= m;
		}
	#endif

		h ^= h >> r;
		h *= m;
		h ^= h >> r;

		return h;
	}

	inline float3x3 eulerXYZ (float3 ang) {
		return rotate3_Z(ang.z) * rotate3_Y(ang.y) * rotate3_X(ang.x);
	}
	inline float3x4 transform (float3 pos, float3 euler_xyz) {
		return translate(pos) * eulerXYZ(euler_xyz);
	}
	inline float3x4 transform (float3 pos, float3 euler_xyz, float3 scale) {
		return translate(pos) * eulerXYZ(euler_xyz) * kissmath::scale(scale);
	}

	inline uint64_t hash (int         i, uint64_t seed=0) {      return MurmurHash64A_fixedlen<sizeof(i)>((uint32_t const*)&i, seed); };
	inline uint64_t hash (int2 const& v, uint64_t seed=0) {      return MurmurHash64A_fixedlen<sizeof(v)>((uint32_t const*)&v, seed); };
	inline uint64_t hash (int3 const& v, uint64_t seed=0) {      return MurmurHash64A_fixedlen<sizeof(v)>((uint32_t const*)&v, seed); };
	inline uint64_t hash (int4 const& v, uint64_t seed=0) {      return MurmurHash64A_fixedlen<sizeof(v)>((uint32_t const*)&v, seed); };

	inline uint64_t hash (float         f, uint64_t seed=0) {    return MurmurHash64A_fixedlen<sizeof(f)>((uint32_t const*)&f, seed); };
	inline uint64_t hash (float2 const& v, uint64_t seed=0) {    return MurmurHash64A_fixedlen<sizeof(v)>((uint32_t const*)&v, seed); };
	inline uint64_t hash (float3 const& v, uint64_t seed=0) {    return MurmurHash64A_fixedlen<sizeof(v)>((uint32_t const*)&v, seed); };
	inline uint64_t hash (float4 const& v, uint64_t seed=0) {    return MurmurHash64A_fixedlen<sizeof(v)>((uint32_t const*)&v, seed); };

	inline uint64_t hash (uint8v2 const& v, uint64_t seed=0) {   return MurmurHash64A_fixedlen<sizeof(v)>(&v.x, seed); };
	inline uint64_t hash (uint8v3 const& v, uint64_t seed=0) {   return MurmurHash64A_fixedlen<sizeof(v)>(&v.x, seed); };
	inline uint64_t hash (uint8v4 const& v, uint64_t seed=0) {   return MurmurHash64A_fixedlen<sizeof(v)>(&v.x, seed); };
}

namespace std {
	template<> struct hash<kissmath::int2   > { size_t operator() (kissmath::int2    const& x) const { return kissmath::hash(x); } };
	template<> struct hash<kissmath::int3   > { size_t operator() (kissmath::int3    const& x) const { return kissmath::hash(x); } };
	template<> struct hash<kissmath::int4   > { size_t operator() (kissmath::int4    const& x) const { return kissmath::hash(x); } };
	template<> struct hash<kissmath::float2 > { size_t operator() (kissmath::float2  const& x) const { return kissmath::hash(x); } };
	template<> struct hash<kissmath::float3 > { size_t operator() (kissmath::float3  const& x) const { return kissmath::hash(x); } };
	template<> struct hash<kissmath::float4 > { size_t operator() (kissmath::float4  const& x) const { return kissmath::hash(x); } };
	template<> struct hash<kissmath::uint8v2> { size_t operator() (kissmath::uint8v2 const& x) const { return kissmath::hash(x); } };
	template<> struct hash<kissmath::uint8v3> { size_t operator() (kissmath::uint8v3 const& x) const { return kissmath::hash(x); } };
	template<> struct hash<kissmath::uint8v4> { size_t operator() (kissmath::uint8v4 const& x) const { return kissmath::hash(x); } };
}

using namespace kissmath;
