
typedef signed char			schar;
typedef unsigned char		uchar;
typedef short				sshort;
typedef unsigned short		ushort;
typedef int					si;
typedef unsigned int		ui;
typedef long				slong;
typedef unsigned long		ulong;
typedef long long			sllong;
typedef unsigned long long	ullong;

static_assert(sizeof(schar) ==	1, "sizeof(schar) !=	1");
static_assert(sizeof(sshort) ==	2, "sizeof(sshort) !=	2");
static_assert(sizeof(si) ==		4, "sizeof(si) !=		4");
static_assert(sizeof(slong) ==	4, "sizeof(slong) !=	4");
static_assert(sizeof(sllong) ==	8, "sizeof(sllong) !=	8");

typedef schar				s8;
typedef uchar				u8;
typedef sshort				s16;
typedef ushort				u16;
typedef si					s32;
typedef ui					u32;
typedef sllong				s64;
typedef ullong				u64;

typedef u8					byte;

typedef s64					sptr;
typedef u64					uptr;

static_assert(sizeof(sptr) == sizeof(byte*), "sizeof(sptr) != sizeof(byte*)");
static_assert(sizeof(uptr) == sizeof(byte*), "sizeof(uptr) != sizeof(byte*)");

typedef float				f32;
typedef double				f64;

typedef char const*			cstr;
typedef char				utf8;
typedef char32_t			utf32;

////

#if 0
union FI32_u {
	f32		f;
	u32		i;
	constexpr FI32_u(f32 f): f{f} {}
	constexpr FI32_u(u32 i): i{i} {}
};
union FI64_u {
	f64		f;
	u64		i;
	constexpr FI64_u(f64 f): f{f} {}
	constexpr FI64_u(u64 i): i{i} {}
};

static constexpr f32 reint_int_as_flt (u32 i) {
	return FI32_u(i).f;
}
static constexpr f64 reint_int_as_flt (u64 i) {
	return FI64_u(i).f;
}
static constexpr u32 reint_flt_as_int (f32 f) {
	return FI32_u(f).i;
}
static constexpr u64 reint_flt_as_int (f64 f) {
	return FI64_u(f).i;
}
#endif
