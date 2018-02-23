
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
