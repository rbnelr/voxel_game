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
	template <typename T> inline constexpr T align_up (T x, T y) {
		return (x + y - 1) & ~(y - 1);
	}
	// check if power of two
	template <typename T> inline constexpr T is_pot (T x) {
		return (x & (x - 1)) == 0;
	}
	
	template <typename X, typename Y>
	inline size_t hash (X x, Y y) {
		size_t h;
		h  = ::std::hash<X>()(x);
		h = 53ull * (h + 53ull);
		h += ::std::hash<Y>()(y);
		return h;
	}
	template <typename X, typename Y, typename Z>
	inline size_t hash (X x, Y y, Z z) {
		size_t h;
		h  = ::std::hash<X>()(x);
		h = 53ull * (h + 53ull);
		h += ::std::hash<Y>()(y);
		h = 53ull * (h + 53ull);
		h += ::std::hash<Z>()(z);
		return h;
	}
	template <typename X, typename Y, typename Z, typename W>
	inline size_t hash (X x, Y y, Z z, W w) {
		size_t h;
		h  = ::std::hash<X>()(x);
		h = 53ull * (h + 53ull);
		h += ::std::hash<Y>()(y);
		h = 53ull * (h + 53ull);
		h += ::std::hash<Z>()(z);
		h = 53ull * (h + 53ull);
		h += ::std::hash<W>()(w);
		return h;
	}
	
	inline size_t hash (int2 v) { return hash(v.x, v.y); };
	inline size_t hash (int3 v) { return hash(v.x, v.y, v.z); };
	inline size_t hash (int4 v) { return hash(v.x, v.y, v.z, v.w); };
	
	inline size_t hash (uint8v3 v) { return hash(v.x, v.y, v.z); };
	inline size_t hash (uint8v4 v) { return hash(v.x, v.y, v.z, v.w); };
}

namespace std {
	template<>
	struct hash<kissmath::int2> {
		size_t operator() (kissmath::int2 const& x) const {
			return kissmath::hash(x);
		}
	};
	template<>
	struct hash<kissmath::int3> {
		size_t operator() (kissmath::int3 const& x) const {
			return kissmath::hash(x);
		}
	};
	template<>
	struct hash<kissmath::int4> {
		size_t operator() (kissmath::int4 const& x) const {
			return kissmath::hash(x);
		}
	};
	template<>
	struct hash<kissmath::uint8v3> {
		size_t operator() (kissmath::uint8v3 const& x) const {
			return kissmath::hash(x);
		}
	};
	template<>
	struct hash<kissmath::uint8v4> {
		size_t operator() (kissmath::uint8v4 const& x) const {
			return kissmath::hash(x);
		}
	};
}

using namespace kissmath;

