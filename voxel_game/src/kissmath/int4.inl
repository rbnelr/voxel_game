// file was generated by kissmath.py at <TODO: add github link>

////// Inline definitions

#include "bool4.hpp"
#include "float4.hpp"
#include "int3.hpp"
#include "int2.hpp"
#include "uint8v4.hpp"
#include "int64v4.hpp"

namespace kissmath {
	//// forward declarations
	// typedef these because the _t suffix is kinda unwieldy when using these types often
	
	typedef uint8_t uint8;
	typedef int64_t int64;
	
	// Component indexing operator
	inline constexpr int& int4::operator[] (int i) {
		return arr[i];
	}
	
	// Component indexing operator
	inline constexpr int const& int4::operator[] (int i) const {
		return arr[i];
	}
	
	
	// sets all components to one value
	// implicit constructor -> float3(x,y,z) * 5 will be turned into float3(x,y,z) * float3(5) by to compiler to be able to execute operator*(float3, float3), which is desirable
	// and short initialization like float3 a = 0; works
	inline constexpr int4::int4 (int all): x{all}, y{all}, z{all}, w{all} {
		
	}
	
	// supply all components
	inline constexpr int4::int4 (int x, int y, int z, int w): x{x}, y{y}, z{z}, w{w} {
		
	}
	
	// extend vector
	inline constexpr int4::int4 (int2 xy, int z, int w): x{xy.x}, y{xy.y}, z{z}, w{w} {
		
	}
	
	// extend vector
	inline constexpr int4::int4 (int3 xyz, int w): x{xyz.x}, y{xyz.y}, z{xyz.z}, w{w} {
		
	}
	
	//// Truncating cast operators
	
	
	// truncating cast operator
	inline constexpr int4::operator int2 () const {
		return int2(x, y);
	}
	
	// truncating cast operator
	inline constexpr int4::operator int3 () const {
		return int3(x, y, z);
	}
	
	//// Type cast operators
	
	
	// type cast operator
	inline constexpr int4::operator bool4 () const {
		return bool4((bool)x, (bool)y, (bool)z, (bool)w);
	}
	
	// type cast operator
	inline constexpr int4::operator float4 () const {
		return float4((float)x, (float)y, (float)z, (float)w);
	}
	
	// type cast operator
	inline constexpr int4::operator int64v4 () const {
		return int64v4((int64)x, (int64)y, (int64)z, (int64)w);
	}
	
	// type cast operator
	inline constexpr int4::operator uint8v4 () const {
		return uint8v4((uint8)x, (uint8)y, (uint8)z, (uint8)w);
	}
	
	
	// componentwise arithmetic operator
	inline int4 int4::operator+= (int4 r) {
		x += r.x;
		y += r.y;
		z += r.z;
		w += r.w;
		return *this;
	}
	
	// componentwise arithmetic operator
	inline int4 int4::operator-= (int4 r) {
		x -= r.x;
		y -= r.y;
		z -= r.z;
		w -= r.w;
		return *this;
	}
	
	// componentwise arithmetic operator
	inline int4 int4::operator*= (int4 r) {
		x *= r.x;
		y *= r.y;
		z *= r.z;
		w *= r.w;
		return *this;
	}
	
	// componentwise arithmetic operator
	inline int4 int4::operator/= (int4 r) {
		x /= r.x;
		y /= r.y;
		z /= r.z;
		w /= r.w;
		return *this;
	}
	
	//// arthmethic ops
	
	
	inline constexpr int4 operator+ (int4 v) {
		return int4(+v.x, +v.y, +v.z, +v.w);
	}
	
	inline constexpr int4 operator- (int4 v) {
		return int4(-v.x, -v.y, -v.z, -v.w);
	}
	
	inline constexpr int4 operator+ (int4 l, int4 r) {
		return int4(l.x + r.x, l.y + r.y, l.z + r.z, l.w + r.w);
	}
	
	inline constexpr int4 operator- (int4 l, int4 r) {
		return int4(l.x - r.x, l.y - r.y, l.z - r.z, l.w - r.w);
	}
	
	inline constexpr int4 operator* (int4 l, int4 r) {
		return int4(l.x * r.x, l.y * r.y, l.z * r.z, l.w * r.w);
	}
	
	inline constexpr int4 operator/ (int4 l, int4 r) {
		return int4(l.x / r.x, l.y / r.y, l.z / r.z, l.w / r.w);
	}
	
	//// bitwise ops
	
	
	inline constexpr int4 operator~ (int4 v) {
		return int4(~v.x, ~v.y, ~v.z, ~v.w);
	}
	
	inline constexpr int4 operator& (int4 l, int4 r) {
		return int4(l.x & r.x, l.y & r.y, l.z & r.z, l.w & r.w);
	}
	
	inline constexpr int4 operator| (int4 l, int4 r) {
		return int4(l.x | r.x, l.y | r.y, l.z | r.z, l.w | r.w);
	}
	
	inline constexpr int4 operator^ (int4 l, int4 r) {
		return int4(l.x ^ r.x, l.y ^ r.y, l.z ^ r.z, l.w ^ r.w);
	}
	
	inline constexpr int4 operator<< (int4 l, int4 r) {
		return int4(l.x << r.x, l.y << r.y, l.z << r.z, l.w << r.w);
	}
	
	inline constexpr int4 operator>> (int4 l, int4 r) {
		return int4(l.x >> r.x, l.y >> r.y, l.z >> r.z, l.w >> r.w);
	}
	
	//// comparison ops
	
	
	// componentwise comparison returns a bool vector
	inline constexpr bool4 operator< (int4 l, int4 r) {
		return bool4(l.x < r.x, l.y < r.y, l.z < r.z, l.w < r.w);
	}
	
	// componentwise comparison returns a bool vector
	inline constexpr bool4 operator<= (int4 l, int4 r) {
		return bool4(l.x <= r.x, l.y <= r.y, l.z <= r.z, l.w <= r.w);
	}
	
	// componentwise comparison returns a bool vector
	inline constexpr bool4 operator> (int4 l, int4 r) {
		return bool4(l.x > r.x, l.y > r.y, l.z > r.z, l.w > r.w);
	}
	
	// componentwise comparison returns a bool vector
	inline constexpr bool4 operator>= (int4 l, int4 r) {
		return bool4(l.x >= r.x, l.y >= r.y, l.z >= r.z, l.w >= r.w);
	}
	
	// componentwise comparison returns a bool vector
	inline constexpr bool4 operator== (int4 l, int4 r) {
		return bool4(l.x == r.x, l.y == r.y, l.z == r.z, l.w == r.w);
	}
	
	// componentwise comparison returns a bool vector
	inline constexpr bool4 operator!= (int4 l, int4 r) {
		return bool4(l.x != r.x, l.y != r.y, l.z != r.z, l.w != r.w);
	}
	
	// vectors are equal, equivalent to all(l == r)
	inline constexpr bool equal (int4 l, int4 r) {
		return all(l == r);
	}
	
	// componentwise ternary (c ? l : r)
	inline constexpr int4 select (bool4 c, int4 l, int4 r) {
		return int4(c.x ? l.x : r.x, c.y ? l.y : r.y, c.z ? l.z : r.z, c.w ? l.w : r.w);
	}
	
	//// misc ops
	
	// componentwise absolute
	inline int4 abs (int4 v) {
		return int4(abs(v.x), abs(v.y), abs(v.z), abs(v.w));
	}
	
	// componentwise minimum
	inline constexpr int4 min (int4 l, int4 r) {
		return int4(min(l.x,r.x), min(l.y,r.y), min(l.z,r.z), min(l.w,r.w));
	}
	
	// componentwise maximum
	inline constexpr int4 max (int4 l, int4 r) {
		return int4(max(l.x,r.x), max(l.y,r.y), max(l.z,r.z), max(l.w,r.w));
	}
	
	// componentwise clamp into range [a,b]
	inline constexpr int4 clamp (int4 x, int4 a, int4 b) {
		return min(max(x,a), b);
	}
	
	// componentwise clamp into range [0,1] also known as saturate in hlsl
	inline constexpr int4 clamp (int4 x) {
		return min(max(x, int(0)), int(1));
	}
	
	// get minimum component of vector, optionally get component index via min_index
	inline int min_component (int4 v, int* min_index) {
		int index = 0;
		int min_val = v.x;	
		for (int i=1; i<4; ++i) {
			if (v.arr[i] <= min_val) {
				index = i;
				min_val = v.arr[i];
			}
		}
		if (min_index) *min_index = index;
		return min_val;
	}
	
	// get maximum component of vector, optionally get component index via max_index
	inline int max_component (int4 v, int* max_index) {
		int index = 0;
		int max_val = v.x;	
		for (int i=1; i<4; ++i) {
			if (v.arr[i] >= max_val) {
				index = i;
				max_val = v.arr[i];
			}
		}
		if (max_index) *max_index = index;
		return max_val;
	}
	
	
	// componentwise wrap
	inline int4 wrap (int4 v, int4 range) {
		return int4(wrap(v.x,range.x), wrap(v.y,range.y), wrap(v.z,range.z), wrap(v.w,range.w));
	}
	
	// componentwise wrap
	inline int4 wrap (int4 v, int4 a, int4 b) {
		return int4(wrap(v.x,a.x,b.x), wrap(v.y,a.y,b.y), wrap(v.z,a.z,b.z), wrap(v.w,a.w,b.w));
	}
	
	
	//// Vector math
	
	
	// magnitude of vector
	inline float length (int4 v) {
		return sqrt((float)(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w));
	}
	
	// squared magnitude of vector, cheaper than length() because it avoids the sqrt(), some algorithms only need the squared magnitude
	inline constexpr int length_sqr (int4 v) {
		return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
	}
	
	// distance between points, equivalent to length(a - b)
	inline float distance (int4 a, int4 b) {
		return length(a - b);
	}
	
	// normalize vector so that it has length() = 1, undefined for zero vector
	inline float4 normalize (int4 v) {
		return float4(v) / length(v);
	}
	
	// normalize vector so that it has length() = 1, returns zero vector if vector was zero vector
	inline float4 normalizesafe (int4 v) {
		float len = length(v);
		if (len == float(0)) {
			return float(0);
		}
		return float4(v) / float4(len);
	}
	
	// dot product
	inline constexpr int dot (int4 l, int4 r) {
		return l.x * r.x + l.y * r.y + l.z * r.z + l.w * r.w;
	}
}
