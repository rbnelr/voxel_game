// file was generated by kissmath.py at <TODO: add github link>
#pragma once

////// Forward declarations

#include <cmath>
#include <cstdint>

namespace kissmath {
	typedef int64_t int64; // typedef this because the _t suffix is kinda unwieldy when using these types often
	
	// Use std math functions for these
	using std::abs;
	using std::floor;
	using std::ceil;
	using std::pow;
	using std::round;
	
	// wrap x into range [0,range)
	// negative x wrap back to +range unlike c++ % operator
	// negative range supported
	inline int64 wrap (int64 x, int64 range);
	
	// wrap x into [a,b) range
	inline int64 wrap (int64 x, int64 a, int64 b);
	
	// returns the greater value of a and b
	inline constexpr int64 min (int64 l, int64 r);
	
	// returns the smaller value of a and b
	inline constexpr int64 max (int64 l, int64 r);
	
	// equivalent to ternary c ? l : r
	// for conformity with vectors
	inline constexpr int64 select (bool c, int64 l, int64 r);
	
	// clamp x into range [a, b]
	// equivalent to min(max(x,a), b)
	inline constexpr int64 clamp (int64 x, int64 a, int64 b);
	
	// clamp x into range [0, 1]
	// also known as saturate in hlsl
	inline constexpr int64 clamp (int64 x);
	
	
	// length(scalar) = abs(scalar)
	// for conformity with vectors
	inline int64 length (int64 x);
	
	// length_sqr(scalar) = abs(scalar)^2
	// for conformity with vectors (for vectors this func is preferred over length to avoid the sqrt)
	inline int64 length_sqr (int64 x);
	
	// scalar normalize for conformity with vectors
	// normalize(-6.2f) = -1f, normalize(7) = 1, normalize(0) = <div 0>
	// can be useful in some cases
	inline int64 normalize (int64 x);
	
	// normalize(x) for length(x) != 0 else 0
	inline int64 normalizesafe (int64 x);
	
	
}


#include "int64.inl"