#pragma once

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

#define CONCAT(a,b) a##b

#define STATIC_ASSERT(cond) static_assert((cond), STRINGIFY(cond))

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define EVEN(i)			(((i) % 2) == 0)
#define ODD(i)			(((i) % 2) == 1)

#define BOOL_XOR(a, b)	(((a) != 0) == ((b) != 0))
