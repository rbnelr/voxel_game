
#define INL FORCEINLINE

//
#define T	bool
#define V2	bv2
#define V3	bv3
#define V4	bv4
#define BOOLVEC	1
	
	#include "vector_tv2.hpp"
	#include "vector_tv3.hpp"
	#include "vector_tv4.hpp"
	
#undef T
#undef V2
#undef V3
#undef V4
#undef BOOLVEC

#define BOOLVEC	0
#define BV2	bv2
#define BV3	bv3
#define BV4	bv4

//
#define T	f32
#define V2	fv2
#define V3	fv3
#define V4	fv4
	
	#include "vector_tv2.hpp"
	#include "vector_tv3.hpp"
	#include "vector_tv4.hpp"
	
#undef T
#undef V2
#undef V3
#undef V4

//
#define T	f64
#define V2	dv2
#define V3	dv3
#define V4	dv4
	
	#include "vector_tv2.hpp"
	#include "vector_tv3.hpp"
	#include "vector_tv4.hpp"
	
#undef T
#undef V2
#undef V3
#undef V4

//
#define T	s32
#define V2	s32v2
#define V3	s32v3
#define V4	s32v4
#define I_TO_F_CONV	1
	
	#include "vector_tv2.hpp"
	#include "vector_tv3.hpp"
	#include "vector_tv4.hpp"
	
#undef T
#undef V2
#undef V3
#undef V4

#define T	u32
#define V2	u32v2
#define V3	u32v3
#define V4	u32v4
#define I_TO_F_CONV	1
	
	#include "vector_tv2.hpp"
	#include "vector_tv3.hpp"
	#include "vector_tv4.hpp"
	
#undef T
#undef V2
#undef V3
#undef V4

#undef I_TO_F_CONV

//
#define T	f32
#define V2	fv2
#define V3	fv3
#define V4	fv4
#define M2	fm2
#define M3	fm3
#define M4	fm4
#define HM	fhm

#undef BOOLVEC
#undef BV2
#undef BV3
#undef BV4

struct M2 {
	V2 arr[2];
	
	INL explicit M2 () {}
private: // don't allow a contructor with column mayor order because it could be confusing, use static functions row and column instead, still need a contructor though, to implement the functions below
	INL explicit constexpr M2 (V2 a, V2 b): arr{a,b} {}
public:
	
	static constexpr M2 row (		V2 a, V2 b ) {				return M2{V2(a.x,b.x),V2(b.y,b.y)}; }
	static constexpr M2 column (	V2 a, V2 b ) {				return M2{a,b}; }
	static constexpr M2 row (		T a, T b,
									T e, T f ) {				return M2{V2(a,e),V2(b,f)}; }
	static constexpr M2 ident () {								return row(1,0, 0,1); }
	
	M2& operator*= (M2 r);
};
struct M3 {
	V3 arr[3];
	
	INL explicit M3 () {}
private: //
	INL explicit constexpr M3 (V3 a, V3 b, V3 c): arr{a,b,c} {}
public:
	
	static constexpr M3 row (		V3 a, V3 b, V3 c ) {		return M3{V3(a.x,b.x,c.x),V3(a.y,b.y,c.y),V3(a.z,b.z,c.z)}; }
	static constexpr M3 column (	V3 a, V3 b, V3 c ) {		return M3{a,b,c}; }
	static constexpr M3 row (		T a, T b, T c,
									T e, T f, T g,
									T i, T j, T k ) {			return M3{V3(a,e,i),V3(b,f,j),V3(c,g,k)}; }
	static constexpr M3 ident () {								return row(1,0,0, 0,1,0, 0,0,1); }
	constexpr M3 (M2 m): arr{V3(m.arr[0], 0), V3(m.arr[1], 0), V3(0,0,1)} {}
	
	M2 m2 () const {											return M2::column( arr[0].xy(), arr[1].xy() ); }
	
	M3& operator*= (M3 r);
};
struct M4 {
	V4 arr[4];
	
	INL explicit M4 () {}
private: //
	INL explicit constexpr M4 (V4 a, V4 b, V4 c, V4 d): arr{a,b,c,d} {}
public:

	static constexpr M4 row (		V4 a, V4 b, V4 c, V4 d ) {	return M4{V4(a.x,b.x,c.x,d.x),V4(a.y,b.y,c.y,d.y),V4(a.z,b.z,c.z,d.z),V4(a.w,b.w,c.w,d.w)}; }
	static constexpr M4 column (	V4 a, V4 b, V4 c, V4 d ) {	return M4{a,b,c,d}; }
	static constexpr M4 row (		T a, T b, T c, T d,
									T e, T f, T g, T h,
									T i, T j, T k, T l,
									T m, T n, T o, T p ) {		return M4{V4(a,e,i,m),V4(b,f,j,n),V4(c,g,k,o),V4(d,h,l,p)}; }
	static constexpr M4 ident () {								return row(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1); }
	constexpr M4 (M2 m): arr{V4(m.arr[0], 0,0), V4(m.arr[1], 0,0), V4(0,0,1,0), V4(0,0,0,1)} {}
	constexpr M4 (M3 m): arr{V4(m.arr[0], 0), V4(m.arr[1], 0), V4(m.arr[2], 0), V4(0,0,0,1)} {}
	
	M2 m2 () const {											return M2::column( arr[0].xy(), arr[1].xy() ); }
	M3 m3 () const {											return M3::column( arr[0].xyz(), arr[1].xyz(), arr[2].xyz() ); }
	
	M4& operator*= (M4 r);
};
struct HM { // last row implicit 0,0,0,1
	V3 arr[4];
	
	INL explicit HM () {}
private: //
	INL explicit constexpr HM (V3 a, V3 b, V3 c, V3 d): arr{a,b,c,d} {}
public:

	static constexpr HM row (		V4 a, V4 b, V4 c ) {		return HM{V3(a.x,b.x,c.x),V3(a.y,b.y,c.y),V3(a.z,b.z,c.z),V3(a.w,b.w,c.w)}; }
	static constexpr HM column (	V3 a, V3 b, V3 c, V3 d ) {	return HM{a,b,c,d}; }
	static constexpr HM row (		T a, T b, T c, T d,
									T e, T f, T g, T h,
									T i, T j, T k, T l ) {		return HM{V3(a,e,i),V3(b,f,j),V3(c,g,k),V3(d,h,l)}; }
	static constexpr HM ident () {								return row(1,0,0,0, 0,1,0,0, 0,0,1,0); }
	constexpr HM (M2 m): arr{V3(m.arr[0], 0), V3(m.arr[1], 0), V3(0,0,1), V3(0,0,0)} {}
	constexpr HM (M3 m): arr{m.arr[0], m.arr[1], m.arr[2], V3(0,0,0)} {}
	
	M2 m2 () const {											return M2::column( arr[0].xy(), arr[1].xy() ); }
	M3 m3 () const {											return M3::column( arr[0], arr[1], arr[2] ); }
	M4 m4 () const {											return M4::column( V4(arr[0],0), V4(arr[1],0), V4(arr[2],0), V4(arr[3],1) ); }
	
	HM& operator*= (HM r);
};

static V2 operator* (M2 m, V2 v) {
	V2 ret;
	ret.x = m.arr[0].x * v.x  +m.arr[1].x * v.y;
	ret.y = m.arr[0].y * v.x  +m.arr[1].y * v.y;
	return ret;
}
static M2 operator* (M2 l, M2 r) {
	M2 ret;
	ret.arr[0] = l * r.arr[0];
	ret.arr[1] = l * r.arr[1];
	return ret;
}

static V3 operator* (M3 m, V3 v) {
	V3 ret;
	ret.x = m.arr[0].x * v.x  +m.arr[1].x * v.y  +m.arr[2].x * v.z;
	ret.y = m.arr[0].y * v.x  +m.arr[1].y * v.y  +m.arr[2].y * v.z;
	ret.z = m.arr[0].z * v.x  +m.arr[1].z * v.y  +m.arr[2].z * v.z;
	return ret;
}
static M3 operator* (M3 l, M3 r) {
	M3 ret;
	ret.arr[0] = l * r.arr[0];
	ret.arr[1] = l * r.arr[1];
	ret.arr[2] = l * r.arr[2];
	return ret;
}

static V4 operator* (M4 m, V4 v) {
	V4 ret;
	ret.x = m.arr[0].x * v.x  +m.arr[1].x * v.y  +m.arr[2].x * v.z  +m.arr[3].x * v.w;
	ret.y = m.arr[0].y * v.x  +m.arr[1].y * v.y  +m.arr[2].y * v.z  +m.arr[3].y * v.w;
	ret.z = m.arr[0].z * v.x  +m.arr[1].z * v.y  +m.arr[2].z * v.z  +m.arr[3].z * v.w;
	ret.w = m.arr[0].w * v.x  +m.arr[1].w * v.y  +m.arr[2].w * v.z  +m.arr[3].w * v.w;
	return ret;
}
static M4 operator* (M4 l, M4 r) {
	M4 ret;
	ret.arr[0] = l * r.arr[0];
	ret.arr[1] = l * r.arr[1];
	ret.arr[2] = l * r.arr[2];
	ret.arr[3] = l * r.arr[3];
	return ret;
}

static V3 operator* (HM m, V3 v) { // the common case of wanting to translate/rotate/scale some v3 -> if you just want to rotate/scale instead of doing this "M4*v4(v3,0)" -> just do "M4.m3() * V3"
	// implicit v.w = 1
	V3 ret;
	ret.x = m.arr[0].x * v.x  +m.arr[1].x * v.y  +m.arr[2].x * v.z  +m.arr[3].x;
	ret.y = m.arr[0].y * v.x  +m.arr[1].y * v.y  +m.arr[2].y * v.z  +m.arr[3].y;
	ret.z = m.arr[0].z * v.x  +m.arr[1].z * v.y  +m.arr[2].z * v.z  +m.arr[3].z;
	return ret;
}
static HM operator* (HM l, HM r) {
	HM ret;
	#if 0
	ret.arr[0] = l.m3() * r.arr[0];	// implicit r.arr[0].w = 0
	ret.arr[1] = l.m3() * r.arr[1];	// implicit r.arr[1].w = 0
	ret.arr[2] = l.m3() * r.arr[2];	// implicit r.arr[2].w = 0
	ret.arr[3] = l * r.arr[3];		// implicit r.arr[3].w = 1
	#else
	ret.arr[0].x = l.arr[0].x * r.arr[0].x  +l.arr[1].x * r.arr[0].y  +l.arr[2].x * r.arr[0].z;
	ret.arr[0].y = l.arr[0].y * r.arr[0].x  +l.arr[1].y * r.arr[0].y  +l.arr[2].y * r.arr[0].z;
	ret.arr[0].z = l.arr[0].z * r.arr[0].x  +l.arr[1].z * r.arr[0].y  +l.arr[2].z * r.arr[0].z;
	
	ret.arr[1].x = l.arr[0].x * r.arr[1].x  +l.arr[1].x * r.arr[1].y  +l.arr[2].x * r.arr[1].z;
	ret.arr[1].y = l.arr[0].y * r.arr[1].x  +l.arr[1].y * r.arr[1].y  +l.arr[2].y * r.arr[1].z;
	ret.arr[1].z = l.arr[0].z * r.arr[1].x  +l.arr[1].z * r.arr[1].y  +l.arr[2].z * r.arr[1].z;
	
	ret.arr[2].x = l.arr[0].x * r.arr[2].x  +l.arr[1].x * r.arr[2].y  +l.arr[2].x * r.arr[2].z;
	ret.arr[2].y = l.arr[0].y * r.arr[2].x  +l.arr[1].y * r.arr[2].y  +l.arr[2].y * r.arr[2].z;
	ret.arr[2].z = l.arr[0].z * r.arr[2].x  +l.arr[1].z * r.arr[2].y  +l.arr[2].z * r.arr[2].z;
	
	ret.arr[3].x = l.arr[0].x * r.arr[3].x  +l.arr[1].x * r.arr[3].y  +l.arr[2].x * r.arr[3].z  +l.arr[3].x;
	ret.arr[3].y = l.arr[0].y * r.arr[3].x  +l.arr[1].y * r.arr[3].y  +l.arr[2].y * r.arr[3].z  +l.arr[3].y;
	ret.arr[3].z = l.arr[0].z * r.arr[3].x  +l.arr[1].z * r.arr[3].y  +l.arr[2].z * r.arr[3].z  +l.arr[3].z;
	#endif
	return ret;
}

M2& M2::operator*= (M2 r) {
	return *this = *this * r;
}
M3& M3::operator*= (M3 r) {
	return *this = *this * r;
}
M4& M4::operator*= (M4 r) {
	return *this = *this * r;
}
HM& HM::operator*= (HM r) {
	return *this = *this * r;
}

static M2 inverse (M2 m) {
	T inv_det = T(1) / ( (m.arr[0].x * m.arr[1].y) -(m.arr[1].x * m.arr[0].y) );
	
	M2 ret;
	ret.arr[0].x = m.arr[1].y * +inv_det;
	ret.arr[0].y = m.arr[0].y * -inv_det;
	ret.arr[1].x = m.arr[1].x * -inv_det;
	ret.arr[1].y = m.arr[0].x * +inv_det;
	return ret;
}

#if 0
template <typename T, precision P>
GLM_FUNC_QUALIFIER tmat2x2<T, P> compute_inverse(tmat2x2<T, P> const & m)
{
	T OneOverDeterminant = static_cast<T>(1) / (
		+ m[0][0] * m[1][1]
		- m[1][0] * m[0][1]);

	tmat2x2<T, P> Inverse(
		+ m[1][1] * OneOverDeterminant,
		- m[0][1] * OneOverDeterminant,
		- m[1][0] * OneOverDeterminant,
		+ m[0][0] * OneOverDeterminant);

	return Inverse;
}
template <typename T, precision P>
GLM_FUNC_QUALIFIER tmat3x3<T, P> compute_inverse(tmat3x3<T, P> const & m)
{
	T OneOverDeterminant = static_cast<T>(1) / (
		+ m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2])
		- m[1][0] * (m[0][1] * m[2][2] - m[2][1] * m[0][2])
		+ m[2][0] * (m[0][1] * m[1][2] - m[1][1] * m[0][2]));

	tmat3x3<T, P> Inverse(uninitialize);
	Inverse[0][0] = + (m[1][1] * m[2][2] - m[2][1] * m[1][2]) * OneOverDeterminant;
	Inverse[1][0] = - (m[1][0] * m[2][2] - m[2][0] * m[1][2]) * OneOverDeterminant;
	Inverse[2][0] = + (m[1][0] * m[2][1] - m[2][0] * m[1][1]) * OneOverDeterminant;
	Inverse[0][1] = - (m[0][1] * m[2][2] - m[2][1] * m[0][2]) * OneOverDeterminant;
	Inverse[1][1] = + (m[0][0] * m[2][2] - m[2][0] * m[0][2]) * OneOverDeterminant;
	Inverse[2][1] = - (m[0][0] * m[2][1] - m[2][0] * m[0][1]) * OneOverDeterminant;
	Inverse[0][2] = + (m[0][1] * m[1][2] - m[1][1] * m[0][2]) * OneOverDeterminant;
	Inverse[1][2] = - (m[0][0] * m[1][2] - m[1][0] * m[0][2]) * OneOverDeterminant;
	Inverse[2][2] = + (m[0][0] * m[1][1] - m[1][0] * m[0][1]) * OneOverDeterminant;

	return Inverse;
}
template <typename T, precision P>
GLM_FUNC_QUALIFIER tmat4x4<T, P> compute_inverse(tmat4x4<T, P> const & m)
{
	T Coef00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
	T Coef02 = m[1][2] * m[3][3] - m[3][2] * m[1][3];
	T Coef03 = m[1][2] * m[2][3] - m[2][2] * m[1][3];

	T Coef04 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
	T Coef06 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
	T Coef07 = m[1][1] * m[2][3] - m[2][1] * m[1][3];

	T Coef08 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
	T Coef10 = m[1][1] * m[3][2] - m[3][1] * m[1][2];
	T Coef11 = m[1][1] * m[2][2] - m[2][1] * m[1][2];

	T Coef12 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
	T Coef14 = m[1][0] * m[3][3] - m[3][0] * m[1][3];
	T Coef15 = m[1][0] * m[2][3] - m[2][0] * m[1][3];

	T Coef16 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
	T Coef18 = m[1][0] * m[3][2] - m[3][0] * m[1][2];
	T Coef19 = m[1][0] * m[2][2] - m[2][0] * m[1][2];

	T Coef20 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
	T Coef22 = m[1][0] * m[3][1] - m[3][0] * m[1][1];
	T Coef23 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

	tvec4<T, P> Fac0(Coef00, Coef00, Coef02, Coef03);
	tvec4<T, P> Fac1(Coef04, Coef04, Coef06, Coef07);
	tvec4<T, P> Fac2(Coef08, Coef08, Coef10, Coef11);
	tvec4<T, P> Fac3(Coef12, Coef12, Coef14, Coef15);
	tvec4<T, P> Fac4(Coef16, Coef16, Coef18, Coef19);
	tvec4<T, P> Fac5(Coef20, Coef20, Coef22, Coef23);

	tvec4<T, P> Vec0(m[1][0], m[0][0], m[0][0], m[0][0]);
	tvec4<T, P> Vec1(m[1][1], m[0][1], m[0][1], m[0][1]);
	tvec4<T, P> Vec2(m[1][2], m[0][2], m[0][2], m[0][2]);
	tvec4<T, P> Vec3(m[1][3], m[0][3], m[0][3], m[0][3]);

	tvec4<T, P> Inv0(Vec1 * Fac0 - Vec2 * Fac1 + Vec3 * Fac2);
	tvec4<T, P> Inv1(Vec0 * Fac0 - Vec2 * Fac3 + Vec3 * Fac4);
	tvec4<T, P> Inv2(Vec0 * Fac1 - Vec1 * Fac3 + Vec3 * Fac5);
	tvec4<T, P> Inv3(Vec0 * Fac2 - Vec1 * Fac4 + Vec2 * Fac5);

	tvec4<T, P> SignA(+1, -1, +1, -1);
	tvec4<T, P> SignB(-1, +1, -1, +1);
	tmat4x4<T, P> Inverse(Inv0 * SignA, Inv1 * SignB, Inv2 * SignA, Inv3 * SignB);

	tvec4<T, P> Row0(Inverse[0][0], Inverse[1][0], Inverse[2][0], Inverse[3][0]);

	tvec4<T, P> Dot0(m[0] * Row0);
	T Dot1 = (Dot0.x + Dot0.y) + (Dot0.z + Dot0.w);

	T OneOverDeterminant = static_cast<T>(1) / Dot1;

	return Inverse * OneOverDeterminant;
}
#endif

static M2 scale2 (V2 v) {
	return M2::column(	V2(v.x,0),
						V2(0,v.y) );
}
static M2 rotate2 (T ang) {
	auto sc = sin_cos(ang);
	return M2::row(	+sc.c,	-sc.s,
					+sc.s,	+sc.c );
}

static M3 scale3 (V3 v) {
	return M3::column(	V3(v.x,0,0),
						V3(0,v.y,0),
						V3(0,0,v.z) );
}
static M3 rotate3_X (T ang) {
	auto sc = sin_cos(ang);
	return M3::row(	1,		0,		0,
					0,		+sc.c,	-sc.s,
					0,		+sc.s,	+sc.c);
}
static M3 rotate3_Y (T ang) {
	auto sc = sin_cos(ang);
	return M3::row(	+sc.c,	0,		+sc.s,
					0,		1,		0,
					-sc.s,	0,		+sc.c);
}
static M3 rotate3_Z (T ang) {
	auto sc = sin_cos(ang);
	return M3::row(	+sc.c,	-sc.s,	0,
					+sc.s,	+sc.c,	0,
					0,		0,		1);
}

static M4 translate4 (V3 v) {
	return M4::column(	V4(1,0,0,0),
						V4(0,1,0,0),
						V4(0,0,1,0),
						V4(v,1) );
}
static M4 scale4 (V3 v) {
	return M4::column(	V4(v.x,0,0,0),
						V4(0,v.y,0,0),
						V4(0,0,v.z,0),
						V4(0,0,0,1) );
}
static M4 rotate4_X (T ang) {
	auto sc = sin_cos(ang);
	return M4::row(	1,		0,		0,		0,
					0,		+sc.c,	-sc.s,	0,
					0,		+sc.s,	+sc.c,	0,
					0,		0,		0,		1 );
}
static M4 rotate4_Y (T ang) {
	auto sc = sin_cos(ang);
	return M4::row(	+sc.c,	0,		+sc.s,	0,
					0,		1,		0,		0,
					-sc.s,	0,		+sc.c,	0,
					0,		0,		0,		1 );
}
static M4 rotate4_Z (T ang) {
	auto sc = sin_cos(ang);
	return M4::row(	+sc.c,	-sc.s,	0,		0,
					+sc.s,	+sc.c,	0,		0,
					0,		0,		1,		0,
					0,		0,		0,		1 );
}

static HM translateH (V3 v) {
	return HM::column(	V3(1,0,0),
						V3(0,1,0),
						V3(0,0,1),
						v );
}
static HM scaleH (V3 v) {
	return HM::column(	V3(v.x,0,0),
						V3(0,v.y,0),
						V3(0,0,v.z),
						V3(0,0,0) );
}
static HM rotateH_X (T ang) {
	auto sc = sin_cos(ang);
	return HM::row(	1,		0,		0,		0,
					0,		+sc.c,	-sc.s,	0,
					0,		+sc.s,	+sc.c,	0 );
}
static HM rotateH_Y (T ang) {
	auto sc = sin_cos(ang);
	return HM::row(	+sc.c,	0,		+sc.s,	0,
					0,		1,		0,		0,
					-sc.s,	0,		+sc.c,	0 );
}
static HM rotateH_Z (T ang) {
	auto sc = sin_cos(ang);
	return HM::row(	+sc.c,	-sc.s,	0,		0,
					+sc.s,	+sc.c,	0,		0,
					0,		0,		1,		0 );
}

static HM transl_rot_scale (V3 t, M3 r, V3 s) {
	return translateH(t) * HM(r);// * scaleH(s);
}

#undef T
#undef V2
#undef V3
#undef V4
#undef M2
#undef M3
#undef M4

//
#define vround(T, val)	_vround<T>(val)

template <typename T> static T _vround (fv2 v);
template<> s32v2 _vround<s32v2> (fv2 v) {
	return s32v2((s32)round(v.x), (s32)round(v.y));
}

//
template <typename T>	static T to_linear (T srgb) {
	if (srgb <= T(0.0404482362771082)) {
		return srgb * T(1/12.92);
	} else {
		return pow( (srgb +T(0.055)) * T(1/1.055), T(2.4) );
	}
}
template <typename T>	static T to_srgb (T linear) {
	if (linear <= T(0.00313066844250063)) {
		return linear * T(12.92);
	} else {
		return ( T(1.055) * pow(linear, T(1/2.4)) ) -T(0.055);
	}
}
static fv3 to_linear (fv3 srgb) {
	return fv3( to_linear(srgb.x), to_linear(srgb.y), to_linear(srgb.z) );
}
static fv3 to_srgb (fv3 linear) {
	return fv3( to_srgb(linear.x), to_srgb(linear.y), to_srgb(linear.z) );
}

static fv3 srgb (f32 x, f32 y, f32 z) {	return to_linear(fv3(x,y,z) * fv3(1.0f/255.0f)); }
static fv3 srgb (f32 all) {				return srgb(all,all,all); }

static fv3 hsl_to_rgb (fv3 hsl) {
	#if 0
	// modified from http://www.easyrgb.com/en/math.php
	f32 H = hsl.x;
	f32 S = hsl.y;
	f32 L = hsl.z;
	
	auto Hue_2_RGB = [] (f32 a, f32 b, f32 vH) {
		if (vH < 0) vH += 1;
		if (vH > 1) vH -= 1;
		if ((6 * vH) < 1) return a +(b -a) * 6 * vH;
		if ((2 * vH) < 1) return b;
		if ((3 * vH) < 2) return a +(b -a) * ((2.0f/3) -vH) * 6;
		return a;
	};
	
	fv3 rgb;
	if (S == 0) {
		rgb = fv3(L);
	} else {
		f32 a, b;
		
		if (L < 0.5f)	b = L * (1 +S);
		else			b = (L +S) -(S * L);
		
		a = 2 * L -b;
		
		rgb = fv3(	Hue_2_RGB(a, b, H +(1.0f / 3)),
					Hue_2_RGB(a, b, H),
					Hue_2_RGB(a, b, H -(1.0f / 3)) );
	}
	
	return to_linear(rgb);
	#else
	f32 hue = hsl.x;
	f32 sat = hsl.y;
	f32 lht = hsl.z;
	
	f32 hue6 = hue*6.0f;
	
	f32 c = sat*(1.0f -abs(2.0f*lht -1.0f));
	f32 x = c * (1.0f -abs(mod(hue6, 2.0f) -1.0f));
	f32 m = lht -(c/2.0f);
	
	fv3 rgb;
	if (		hue6 < 1.0f )	rgb = fv3(c,x,0);
	else if (	hue6 < 2.0f )	rgb = fv3(x,c,0);
	else if (	hue6 < 3.0f )	rgb = fv3(0,c,x);
	else if (	hue6 < 4.0f )	rgb = fv3(0,x,c);
	else if (	hue6 < 5.0f )	rgb = fv3(x,0,c);
	else						rgb = fv3(c,0,x);
	rgb += m;
	
	return to_linear(rgb);
	#endif
}

#undef INL