#pragma once

//#include "stddef.h"
//
//void* operator new (size_t count);
//void operator delete (void* ptr) noexcept;

#include "kissmath.hpp"
#include "util/timer.hpp"
#include "util/string.hpp"
#include "util/random.hpp"

#include "dear_imgui.hpp"
#include "profiling.hpp"

#include "stdint.h"
#include "stdio.h"
#include "assert.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stack>
#include <queue>
#include <algorithm>

using namespace kiss;
using namespace kissmath;

//void* operator new (size_t count) {
//	auto ptr = malloc(count);
//	TracyAlloc(ptr, count);
//	return ptr;
//}
//
//void operator delete (void* ptr) noexcept {
//	TracyFree(ptr);
//	free(ptr);
//}
