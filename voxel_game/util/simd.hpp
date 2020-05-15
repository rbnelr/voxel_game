#pragma once
#include "../kissmath.hpp"

#include <immintrin.h>

#define VEC __m256
#define VECi __m256i

#define UNDEF _mm256_undefined_ps()
#define UNDEFi _mm256_undefined_si256()

#define SET1(f) _mm256_set1_ps(f)
#define SET1i(i) _mm256_set1_epi32(i)

#define SET(a,b,c,d,e,f,g,h) _mm256_set_ps(h,g,f,e,d,c,b,a)
#define SETi(a,b,c,d,e,f,g,h) _mm256_set_epi32(h,g,f,e,d,c,b,a)

#define CVT_I2F(i) _mm256_cvtepi32_ps(i)

#define CAST_F2I(i) _mm256_castps_si256(i)
#define CAST_I2F(i) _mm256_castsi256_ps(i)

#define FMADD(v,m,a) _mm256_fmadd_ps(v, m, a)
#define FMSUB(v,m,s) _mm256_fmsub_ps(v, m, s)

#define ANDi(l, r) _mm256_and_si256(l, r)
#define ORi(l, r) _mm256_or_si256(l, r)
#define XORi(l, r) _mm256_xor_si256(l, r)
#define SLLIi(l, r) _mm256_slli_epi32(l, r)
#define SRLIi(l, r) _mm256_srli_epi32(l, r)
#define SRAIi(l, r) _mm256_srai_epi32(l, r)

#define NEQ(l, r) _mm256_cmp_ps(l, r, _CMP_NEQ_OS)
#define LT(l, r) _mm256_cmp_ps(l, r, _CMP_LT_OS)
#define GT(l, r) _mm256_cmp_ps(l, r, _CMP_GT_OS)
#define GE(l, r) _mm256_cmp_ps(l, r, _CMP_GE_OS)
#define GTi(l, r) _mm256_cmpgt_epi32(l, r)
#define EQi(l, r) _mm256_cmpeq_epi32(l, r)

#define AND(l, r) _mm256_and_ps(l, r)
#define ANDNOT(l, r) _mm256_andnot_ps(l, r)
#define OR(l, r) _mm256_or_ps(l, r)
#define XOR(l, r) _mm256_xor_ps(l, r)

#define ADDi(l, r) _mm256_add_epi32(l, r)
#define SUBi(l, r) _mm256_sub_epi32(l, r)
#define MULi(l, r) _mm256_mul_epi32(l, r)

#define ADD(l, r) _mm256_add_ps(l, r)
#define SUB(l, r) _mm256_sub_ps(l, r)
#define MUL(l, r) _mm256_mul_ps(l, r)
#define DIV(l, r) _mm256_div_ps(l, r)

#define MIN(l, r) _mm256_min_ps(l, r)
#define MAX(l, r) _mm256_max_ps(l, r)

#define BLENDV(a, b, mask) _mm256_blendv_ps(a, b, mask)

// https://stackoverflow.com/questions/20083997/how-to-negate-change-sign-of-the-floating-point-elements-in-a-m128-type-vari
#define NEG(v) _mm256_sub_ps(_mm256_set1_ps(0.0f), v)

#define SQRT(v) _mm256_sqrt_ps(v)

#define GATHERi(arr, indices, scale) _mm256_i32gather_epi32(arr, indices, scale)
#define MASK_GATHERi(src, arr, indices, scale, mask) _mm256_mask_i32gather_epi32(src, arr, indices, mask, scale)

#define FALSE SET1(0.0f)
#define TRUE _mm256_castsi256_ps(SET1i(0xffffffffu))

struct alignas(32) sse_float2 {
	VEC x;
	VEC y;

	inline sse_float2 (): x{ UNDEF }, y{ UNDEF } {}
	inline sse_float2 (VEC x, VEC y): x{x}, y{y} {}
	inline sse_float2 (float2 all): x{ SET1(all.x) }, y{ SET1(all.y) } {}

	inline explicit operator float2 () {
		return { x.m256_f32[0], y.m256_f32[0] };
	}
};

struct sse_int;

struct alignas(32) sse_bool {
	VEC v;

	inline sse_bool (): v{ UNDEF } {}
	inline sse_bool (VEC v): v{v} {}
	inline sse_bool (bool all): v{ all ? TRUE : FALSE } {}

	inline operator VEC () {
		return v;
	}
	inline explicit operator sse_int ();
};

struct alignas(32) sse_int {
	VECi v;

	inline sse_int (): v{ UNDEFi } {}
	inline sse_int (VECi v): v{v} {}
	inline sse_int (int all): v{ SET1i(all) } {}

	inline explicit operator int () {
		return { v.m256i_i32[0] };
	}
	inline explicit operator sse_bool () {
		return { CAST_I2F(v) };
	}
};


inline sse_bool::operator sse_int () {
	return { CAST_F2I(v) };
}

struct alignas(32) sse_float {
	VEC v;

	inline sse_float (): v{ UNDEF } {}
	inline sse_float (VEC v): v{v} {}
	inline sse_float (float all): v{ SET1(all) } {}

	inline explicit operator float () {
		return { v.m256_f32[0] };
	}
};

struct alignas(32) sse_int2 {
	VECi x;
	VECi y;

	inline sse_int2 (): x{ UNDEFi }, y{ UNDEFi } {}
	inline sse_int2 (VECi x, VECi y): x{x}, y{y} {}
	inline sse_int2 (int2 all): x{ SET1i(all.x) }, y{ SET1i(all.y) } {}

	inline explicit operator int2 () {
		return { x.m256i_i32[0], y.m256i_i32[0] };
	}

	inline explicit operator sse_float2 () {
		return { CVT_I2F(x), CVT_I2F(y) };
	}
};

union alignas(32) sse_bool3 {
	struct {
		VEC x;
		VEC y;
		VEC z;
	};
	VEC arr[3];

	inline sse_bool3 (): x{ UNDEF }, y{ UNDEF }, z{ UNDEF } {}
	inline sse_bool3 (VEC x, VEC y, VEC z): x{x}, y{y}, z{z} {}
	inline sse_bool3 (sse_bool v): x{ v.v }, y{ v.v }, z{ v.v } {}
	inline sse_bool3 (bool3 v): x{ SET1(v.x) }, y{ SET1(v.y) }, z{ SET1(v.z) } {}
	inline sse_bool3 (bool all): x{ SET1(all) }, y{ SET1(all) }, z{ SET1(all) } {}

	inline sse_bool3 (sse_int x, sse_int y, sse_int z):
		x{ CAST_I2F(x.v) }, y{ CAST_I2F(y.v) }, z{ CAST_I2F(z.v) } {}

	inline VEC& operator[] (int indx) {
		return arr[indx];
	}
};

union alignas(32) sse_float3 {
	struct {
		sse_float x;
		sse_float y;
		sse_float z;
	};
	sse_float arr[3];

	inline sse_float3 (): x{ UNDEF }, y{ UNDEF }, z{ UNDEF } {}
	inline sse_float3 (VEC x, VEC y, VEC z): x{x}, y{y}, z{z} {}
	inline sse_float3 (float f): x{ SET1(f) }, y{ SET1(f) }, z{ SET1(f) } {}
	inline sse_float3 (sse_float v): x{ v.v }, y{ v.v }, z{ v.v } {}
	inline sse_float3 (sse_float x, sse_float y, sse_float z): x{ x }, y{ y }, z{ z } {}
	inline sse_float3 (float3 v): x{ SET1(v.x) }, y{ SET1(v.y) }, z{ SET1(v.z) } {}

	inline sse_float const& operator[] (int indx) const {
		return arr[indx];
	}
	inline sse_float& operator[] (int indx) {
		return arr[indx];
	}
};
struct alignas(32) sse_float4 {
	VEC x;
	VEC y;
	VEC z;
	VEC w;

	inline sse_float4 (): x{ UNDEF }, y{ UNDEF }, z{ UNDEF }, w{ UNDEF } {}
	inline sse_float4 (VEC x, VEC y, VEC z, VEC w): x{x}, y{y}, z{z}, w{w} {}
	inline sse_float4 (float4 v): x{ SET1(v.x) }, y{ SET1(v.y) }, z{ SET1(v.z) }, w{ SET1(v.w) } {}
	inline sse_float4 (sse_float2 v, VEC z, VEC w): x{v.x}, y{v.y}, z{z}, w{w} {}
	inline sse_float4 (sse_float3 v, sse_float w): x{v.x.v}, y{v.y.v}, z{v.z.v}, w{w.v} {}

	inline float4 operator[] (int indx) {
		return float4(x.m256_f32[indx], y.m256_f32[indx], z.m256_f32[indx], w.m256_f32[indx]);
	}
};

template <typename T>
inline sse_int gather (T* base_addr, sse_int vindex) {
	return GATHERi((int*)base_addr, vindex.v, (int)sizeof(T));
}
template <typename T>
inline sse_int gather (T* base_addr, sse_int vindex, sse_bool mask, sse_int def) {
	return MASK_GATHERi(def.v, (int*)base_addr, vindex.v, (int)sizeof(T), CAST_F2I(mask.v));
}

inline sse_float fmadd (sse_float v, sse_float m, sse_float a) {
	return { FMADD(v.v, m.v, a.v) };
}
inline sse_float fmsub (sse_float v, sse_float m, sse_float s) {
	return { FMSUB(v.v, m.v, s.v) };
}
inline sse_float2 fmadd (sse_float2 v, sse_float2 m, sse_float2 a) {
	return {
		FMADD(v.x, m.x, a.x),
		FMADD(v.y, m.y, a.y),
	};
}
inline sse_float3 fmadd (sse_float3 v, sse_float3 m, sse_float3 a) {
	return {
		FMADD(v.x.v, m.x.v, a.x.v),
		FMADD(v.y.v, m.y.v, a.y.v),
		FMADD(v.z.v, m.z.v, a.z.v),
	};
}

inline sse_int2 operator+ (sse_int2 l, sse_int2 r) {
	return {
		ADDi(l.x, r.x),
		ADDi(l.y, r.y),
	};
}
inline sse_int2 operator- (sse_int2 l, sse_int2 r) {
	return {
		SUBi(l.x, r.x),
		SUBi(l.y, r.y),
	};
}
inline sse_int2 operator* (sse_int2 l, sse_int2 r) {
	return {
		MULi(l.x, r.x),
		MULi(l.y, r.y),
	};
}
inline sse_int operator* (sse_int l, sse_int r) {
	return { MULi(l.v, r.v) };
}

inline sse_float operator* (sse_float l, sse_float r) {
	return { MUL(l.v, r.v) };
}
inline sse_float operator- (sse_float l, sse_float r) {
	return { SUB(l.v, r.v) };
}

inline sse_float operator- (sse_float f) {
	return { NEG(f.v) };
}

inline sse_int operator& (sse_int l, sse_int r) {
	return { ANDi(l.v, r.v) };
}
inline sse_bool operator& (sse_bool l, sse_bool r) {
	return { AND(l.v, r.v) };
}
inline sse_bool3 operator& (sse_bool3 l, sse_bool3 r) {
	return { AND(l.x, r.x), AND(l.y, r.y), AND(l.z, r.z) };
}
inline sse_int operator| (sse_int l, sse_int r) {
	return { ORi(l.v, r.v) };
}
inline sse_bool operator| (sse_bool l, sse_bool r) {
	return { OR(l.v, r.v) };
}
inline sse_bool operator! (sse_bool v) {
	return { _mm256_andnot_ps(v.v, CAST_I2F(_mm256_set1_epi32(-1))) }; // how to !x ?
}
inline sse_int operator^ (sse_int l, sse_int r) {
	return { XORi(l.v, r.v) };
}
inline sse_bool3 operator^ (sse_bool3 l, sse_bool3 r) {
	return { XOR(l.x, r.x), XOR(l.y, r.y),  XOR(l.z, r.z) };
}
inline sse_bool operator< (sse_float l, sse_float r) {
	return { LT(l.v, r.v) };
}
inline sse_bool3 operator> (sse_float3 l, sse_float3 r) {
	return { GT(l.x.v, r.x.v), GT(l.y.v, r.y.v), GT(l.z.v, r.z.v) };
}
inline sse_bool operator> (sse_float l, sse_float r) {
	return { GT(l.v, r.v) };
}
inline sse_bool3 operator< (sse_float3 l, sse_float3 r) {
	return { LT(l.x.v, r.x.v), LT(l.y.v, r.y.v), LT(l.z.v, r.z.v) };
}
inline sse_bool3 operator!= (sse_float3 l, sse_float3 r) {
	return { NEQ(l.x.v, r.x.v), NEQ(l.y.v, r.y.v), NEQ(l.z.v, r.z.v) };
}
inline sse_bool operator>= (sse_float l, sse_float r) {
	return { GE(l.v, r.v) };
}

inline sse_bool operator== (sse_int l, sse_int r) {
	return { CAST_I2F( EQi(l.v, r.v) ) };
}
inline sse_bool operator> (sse_int l, sse_int r) {
	return { CAST_I2F( GTi(l.v, r.v) ) };
}
inline sse_bool operator< (sse_int l, sse_int r) {
	return r > l;
}

inline bool any (sse_bool b) {
	return _mm256_testz_si256(CAST_F2I(b.v), CAST_F2I(b.v)) == 0;
}
inline bool all (sse_bool b) {
	return _mm256_test_all_ones(CAST_F2I(b.v)) != 0;
}

inline sse_int operator<< (sse_int l, int r) {
	return { SLLIi(l.v, r) };
}
inline sse_int operator>> (sse_int l, int r) {
	return { SRAIi(l.v, r) };
}
inline sse_int unsigned_shift_right (sse_int l, int r) {
	return { SRLIi(l.v, r) };
}

inline sse_int operator+ (sse_int l, sse_int r) {
	return { ADDi(l.v, r.v) };
}
inline sse_int operator- (sse_int l, sse_int r) {
	return { SUBi(l.v, r.v) };
}

inline VEC _select (VEC c, VEC l, VEC r) {
	//auto a = AND(c, l);
	//auto b = ANDNOT(c, r);
	//auto res = OR(a, b);
	return BLENDV(r, l, c);
}

// only works when VECi looks like 0xffff0000ffff0000 etc. but not with 0x80000000800000000
// since _mm256_blendv_epi32 does not exist
inline VECi _select (VEC c, VECi l, VECi r) {
	return _mm256_blendv_epi8(r, l, CAST_F2I(c));
}

inline sse_float select (sse_bool c, sse_float l, sse_float r) {
	return { _select(c.v, l.v, r.v) };
}
inline sse_float3 select (sse_bool3 c, sse_float3 l, sse_float3 r) {
	return { select(c.x, l.x, r.x), select(c.y, l.y, r.y), select(c.z, l.z, r.z) };
}
inline sse_float4 select (sse_bool c, sse_float4 l, sse_float4 r) {
	return { _select(c.v, l.x, r.x), _select(c.v, l.y, r.y), _select(c.v, l.z, r.z), _select(c.v, l.w, r.w) };
}
inline sse_bool select (sse_bool c, sse_bool l, sse_bool r) {
	return { _select(c.v, l.v, r.v) };
}
inline sse_int select (sse_bool c, sse_int l, sse_int r) {
	return { _select(c.v, l.v, r.v) };
}

inline sse_float2 operator+ (sse_float2 l, sse_float2 r) {
	return {
		ADD(l.x, r.x),
		ADD(l.y, r.y),
	};
}
inline sse_float2 operator- (sse_float2 l, sse_float2 r) {
	return {
		SUB(l.x, r.x),
		SUB(l.y, r.y),
	};
}
inline sse_float2 operator* (sse_float2 l, sse_float2 r) {
	return {
		MUL(l.x, r.x),
		MUL(l.y, r.y),
	};
}
inline sse_float2 operator/ (sse_float2 l, sse_float2 r) {
	return {
		DIV(l.x, r.x),
		DIV(l.y, r.y),
	};
}

inline sse_float3 operator+ (sse_float3 l, sse_float3 r) {
	return {
		ADD(l.x.v, r.x.v),
		ADD(l.y.v, r.y.v),
		ADD(l.z.v, r.z.v),
	};
}
inline sse_float3 operator- (sse_float3 l, sse_float3 r) {
	return {
		SUB(l.x.v, r.x.v),
		SUB(l.y.v, r.y.v),
		SUB(l.z.v, r.z.v),
	};
}
inline sse_float3 operator* (sse_float3 l, sse_float3 r) {
	return {
		MUL(l.x.v, r.x.v),
		MUL(l.y.v, r.y.v),
		MUL(l.z.v, r.z.v),
	};
}
inline sse_float3 operator/ (sse_float3 l, sse_float3 r) {
	return {
		DIV(l.x.v, r.x.v),
		DIV(l.y.v, r.y.v),
		DIV(l.z.v, r.z.v),
	};
}

inline sse_float3 normalize (sse_float3 v) {
	auto dot = FMADD(v.x.v, v.x.v, FMADD(v.y.v, v.y.v, MUL(v.z.v, v.z.v)));
	auto len = SQRT(dot);
	auto inv_len = DIV(SET1(1.0f), len);
	return { MUL(v.x.v, inv_len), MUL(v.y.v, inv_len), MUL(v.z.v, inv_len) };
}

inline sse_float min_component (sse_float3 v) {
	return MIN(MIN(v.x.v, v.y.v), v.z.v);
}
inline sse_float max_component (sse_float3 v) {
	return MAX(MAX(v.x.v, v.y.v), v.z.v);
}

sse_int min_component_indx (sse_float3 v) {
	sse_bool xy = v.x < v.y;
	sse_bool xz = v.x < v.z;
	sse_bool yz = v.y < v.z;

	sse_int ret = select(xy & xz, sse_int(0), select(yz, sse_int(1), sse_int(2)));
	return ret;
}
sse_int max_component_indx (sse_float3 v) {
	sse_bool xy = v.x > v.y;
	sse_bool xz = v.x > v.z;
	sse_bool yz = v.y > v.z;

	sse_int ret = select(xy & xz, sse_int(0), select(yz, sse_int(1), sse_int(2)));
	return ret;
}

sse_float3 matmul (float4x4 const& l, sse_float4 r) {
	sse_float3 ret;
	ret.x = fmadd(l.arr[0].x, r.x, fmadd(l.arr[1].x, r.y, fmadd(l.arr[2].x, r.z, l.arr[3].x * r.w)));
	ret.y = fmadd(l.arr[0].y, r.x, fmadd(l.arr[1].y, r.y, fmadd(l.arr[2].y, r.z, l.arr[3].y * r.w)));
	ret.z = fmadd(l.arr[0].z, r.x, fmadd(l.arr[1].z, r.y, fmadd(l.arr[2].z, r.z, l.arr[3].z * r.w)));
	return ret;
}
sse_float3 matmul_transl (float4x4 const& l, sse_float3 r) {
	sse_float3 ret;
	ret.x = fmadd(l.arr[0].x, r.x.v, fmadd(l.arr[1].x, r.y.v, fmadd(l.arr[2].x, r.z.v, l.arr[3].x)));
	ret.y = fmadd(l.arr[0].y, r.x.v, fmadd(l.arr[1].y, r.y.v, fmadd(l.arr[2].y, r.z.v, l.arr[3].y)));
	ret.z = fmadd(l.arr[0].z, r.x.v, fmadd(l.arr[1].z, r.y.v, fmadd(l.arr[2].z, r.z.v, l.arr[3].z)));
	return ret;
}
sse_float3 matmul (float4x4 const& l, sse_float3 r) {
	sse_float3 ret;
	ret.x = fmadd(l.arr[0].x, r.x.v, fmadd(l.arr[1].x, r.y.v, l.arr[2].x * r.z.v));
	ret.y = fmadd(l.arr[0].y, r.x.v, fmadd(l.arr[1].y, r.y.v, l.arr[2].y * r.z.v));
	ret.z = fmadd(l.arr[0].z, r.x.v, fmadd(l.arr[1].z, r.y.v, l.arr[2].z * r.z.v));
	return ret;
}

sse_float3 matmul_transl (float3x4 const& l, sse_float3 r) {
	sse_float3 ret;
	ret.x = fmadd(l.arr[0].x, r.x.v, fmadd(l.arr[1].x, r.y.v, fmadd(l.arr[2].x, r.z.v, l.arr[3].x)));
	ret.y = fmadd(l.arr[0].y, r.x.v, fmadd(l.arr[1].y, r.y.v, fmadd(l.arr[2].y, r.z.v, l.arr[3].y)));
	ret.z = fmadd(l.arr[0].z, r.x.v, fmadd(l.arr[1].z, r.y.v, fmadd(l.arr[2].z, r.z.v, l.arr[3].z)));
	return ret;
}
sse_float3 matmul (float3x4 const& l, sse_float3 r) {
	sse_float3 ret;
	ret.x = fmadd(l.arr[0].x, r.x.v, fmadd(l.arr[1].x, r.y.v, l.arr[2].x * r.z.v));
	ret.y = fmadd(l.arr[0].y, r.x.v, fmadd(l.arr[1].y, r.y.v, l.arr[2].y * r.z.v));
	ret.z = fmadd(l.arr[0].z, r.x.v, fmadd(l.arr[1].z, r.y.v, l.arr[2].z * r.z.v));
	return ret;
}
