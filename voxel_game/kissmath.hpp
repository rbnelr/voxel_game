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

using namespace kissmath;

#include "kissmath_colors.hpp"

#include <type_traits>

// Hashmap key type for vectors
template <typename VEC>
struct vector_key {
	VEC v;

	vector_key (VEC const& v): v{v} {}
	bool operator== (vector_key<VEC> const& r) const { // for hash map
		return equal(v, r.v);
	}
};

inline size_t hash (int3 v) {
	size_t h;
	h  = std::hash<int>()(v.x);
	h = 53ull * (h + 53ull);

	h += std::hash<int>()(v.y);
	h = 53ull * (h + 53ull);

	h += std::hash<int>()(v.z);
	return h;
};

namespace std {
	template <typename VEC>
	struct hash<vector_key<VEC>> { // for hash map
		size_t operator() (vector_key<VEC> const& v) const {
			return ::hash(v.v);
		}
	};
}
