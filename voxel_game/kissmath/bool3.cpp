// file was generated by kissmath.py at <TODO: add github link>
#include "bool3.hpp"

#include "bool4.hpp"
#include "bool3.hpp"
#include "uint8v3.hpp"
#include "int64v3.hpp"
#include "float3.hpp"
#include "bool2.hpp"
#include "int3.hpp"

namespace kissmath {
	//// forward declarations
	// typedef these because the _t suffix is kinda unwieldy when using these types often
	
	typedef uint8_t uint8;
	typedef int64_t int64;
	
	// Component indexing operator
	bool& bool3::operator[] (int i) {
		return arr[i];
	}
	
	// Component indexing operator
	bool const& bool3::operator[] (int i) const {
		return arr[i];
	}
	
	
	// uninitialized constructor
	bool3::bool3 () {
		
	}
	
	// sets all components to one value
	// implicit constructor -> float3(x,y,z) * 5 will be turned into float3(x,y,z) * float3(5) by to compiler to be able to execute operator*(float3, float3), which is desirable
	// and short initialization like float3 a = 0; works
	bool3::bool3 (bool all): x{all}, y{all}, z{all} {
		
	}
	
	// supply all components
	bool3::bool3 (bool x, bool y, bool z): x{x}, y{y}, z{z} {
		
	}
	
	// extend vector
	bool3::bool3 (bool2 xy, bool z): x{xy.x}, y{xy.y}, z{z} {
		
	}
	
	// truncate vector
	bool3::bool3 (bool4 v): x{v.x}, y{v.y}, z{v.z} {
		
	}
	
	//// Truncating cast operators
	
	
	// truncating cast operator
	bool3::operator bool2 () const {
		return bool2(x, y);
	}
	
	//// Type cast operators
	
	
	// type cast operator
	bool3::operator uint8v3 () const {
		return uint8v3((uint8)x, (uint8)y, (uint8)z);
	}
	
	// type cast operator
	bool3::operator int64v3 () const {
		return int64v3((int64)x, (int64)y, (int64)z);
	}
	
	// type cast operator
	bool3::operator float3 () const {
		return float3((float)x, (float)y, (float)z);
	}
	
	// type cast operator
	bool3::operator int3 () const {
		return int3((int)x, (int)y, (int)z);
	}
	
	//// reducing ops
	
	
	// all components are true
	bool all (bool3 v) {
		return v.x && v.y && v.z;
	}
	
	// any component is true
	bool any (bool3 v) {
		return v.x || v.y || v.z;
	}
	
	//// boolean ops
	
	
	bool3 operator! (bool3 v) {
		return bool3(!v.x, !v.y, !v.z);
	}
	
	bool3 operator&& (bool3 l, bool3 r) {
		return bool3(l.x && r.x, l.y && r.y, l.z && r.z);
	}
	
	bool3 operator|| (bool3 l, bool3 r) {
		return bool3(l.x || r.x, l.y || r.y, l.z || r.z);
	}
	
	//// comparison ops
	
	
	// componentwise comparison returns a bool vector
	bool3 operator== (bool3 l, bool3 r) {
		return bool3(l.x == r.x, l.y == r.y, l.z == r.z);
	}
	
	// componentwise comparison returns a bool vector
	bool3 operator!= (bool3 l, bool3 r) {
		return bool3(l.x != r.x, l.y != r.y, l.z != r.z);
	}
	
	// vectors are equal, equivalent to all(l == r)
	bool equal (bool3 l, bool3 r) {
		return all(l == r);
	}
	
	// componentwise ternary (c ? l : r)
	bool3 select (bool3 c, bool3 l, bool3 r) {
		return bool3(c.x ? l.x : r.x, c.y ? l.y : r.y, c.z ? l.z : r.z);
	}
}
