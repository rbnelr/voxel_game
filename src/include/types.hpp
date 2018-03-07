#pragma once

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

#include "stdint.h"

typedef int8_t				s8;
typedef uint8_t				u8;
typedef int16_t				s16;
typedef uint16_t			u16;
typedef int32_t				s32;
typedef uint32_t			u32;
typedef int64_t				s64;
typedef uint64_t			u64;

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
