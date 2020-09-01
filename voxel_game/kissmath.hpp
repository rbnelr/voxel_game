#pragma once

#include "kissmath/bool2.hpp"
#include "kissmath/bool3.hpp"
#include "kissmath/bool4.hpp"

#include "kissmath/int2.hpp"
#include "kissmath/int3.hpp"
#include "kissmath/int4.hpp"

#include "kissmath/int64v2.hpp"
#include "kissmath/int64v3.hpp"
#include "kissmath/int64v4.hpp"

#include "kissmath/uint8v2.hpp"
#include "kissmath/uint8v3.hpp"
#include "kissmath/uint8v4.hpp"

#include "kissmath/float2.hpp"
#include "kissmath/float3.hpp"
#include "kissmath/float4.hpp"

#include "kissmath/float2x2.hpp"
#include "kissmath/float3x3.hpp"
#include "kissmath/float4x4.hpp"
#include "kissmath/float3x4.hpp"

#include "kissmath/transform2d.hpp"
#include "kissmath/transform3d.hpp"

#include "kissmath_colors.hpp"

#include <type_traits>

namespace kissmath {
	// round up x to y, assume y is power of two
	template <typename T> inline constexpr T round_up_pot (T x, T y) {
		return (x + y - 1) & ~(y - 1);
	}
	// check if power of two
	template <typename T> inline constexpr T is_pot (T x) {
		return (x & (x - 1)) == 0;
	}

	inline size_t hash (int3 v) {
		size_t h;
		h  = ::std::hash<int>()(v.x);
		h = 53ull * (h + 53ull);
		h += ::std::hash<int>()(v.y);
		h = 53ull * (h + 53ull);
		h += ::std::hash<int>()(v.z);
		return h;
	};
	inline size_t hash (int4 v) {
		size_t h;
		h  = ::std::hash<int>()(v.x);
		h = 53ull * (h + 53ull);
		h += ::std::hash<int>()(v.y);
		h = 53ull * (h + 53ull);
		h += ::std::hash<int>()(v.z);
		h = 53ull * (h + 53ull);
		h += ::std::hash<int>()(v.w);
		return h;
	};

	// Hashmap key type for vectors
	template <typename VEC>
	struct vector_key {
		VEC v;

		vector_key (VEC const& v): v{v} {}
		bool operator== (vector_key<VEC> const& r) const {
			return equal(v, r.v);
		}
	};
}

namespace std {
	template <typename VEC>
	struct hash<kissmath::vector_key<VEC>> {
		size_t operator() (kissmath::vector_key<VEC> const& v) const {
			return kissmath::hash(v.v);
		}
	};
}

using namespace kissmath;

#define ENUM_BITFLAG_OPERATORS(e, itype) \
	inline e operator| (e l, e r) { return (e)((itype)l | (itype)r); } \
	inline e operator& (e l, e r) { return (e)((itype)l & (itype)r); } \
	inline e operator^ (e l, e r) { return (e)((itype)l ^ (itype)r); } \
	inline e operator~ (e l) { return (e)(~(itype)l); } \
	inline e& operator|= (e& l, e r) { return l = (e)((itype)l | (itype)r); } \
	inline e& operator&= (e& l, e r) { return l = (e)((itype)l & (itype)r); } \
	inline e& operator^= (e& l, e r) { return l = (e)((itype)l ^ (itype)r); }
