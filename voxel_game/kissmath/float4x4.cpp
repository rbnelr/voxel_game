// file was generated by kissmath.py at <TODO: add github link>
#include "float4x4.hpp"

////// cpp definitions

#include "float2x2.hpp"
#include "float3x3.hpp"
#include "float2x3.hpp"
#include "float3x4.hpp"

namespace kissmath {
	
	//// Accessors
	
	
	//// Constructors
	
	
	// static rows() and columns() methods are preferred over constructors, to avoid confusion if column or row vectors are supplied to the constructor
	
	
	// Casting operators
	
	
	// Componentwise operators; These might be useful in some cases
	
	
	// Matrix multiplication
	
	
	// Componentwise operators; These might be useful in some cases
	
	
	
	
	
	
	// Matrix ops
	
	
	#define LETTERIFY \
	float a = mat.arr[0][0]; \
	float b = mat.arr[0][1]; \
	float c = mat.arr[0][2]; \
	float d = mat.arr[0][3]; \
	float e = mat.arr[1][0]; \
	float f = mat.arr[1][1]; \
	float g = mat.arr[1][2]; \
	float h = mat.arr[1][3]; \
	float i = mat.arr[2][0]; \
	float j = mat.arr[2][1]; \
	float k = mat.arr[2][2]; \
	float l = mat.arr[2][3]; \
	float m = mat.arr[3][0]; \
	float n = mat.arr[3][1]; \
	float o = mat.arr[3][2]; \
	float p = mat.arr[3][3];
	
	#undef LETTERIFY
	
}
