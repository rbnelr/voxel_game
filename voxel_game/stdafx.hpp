#pragma once

//
#define WIN32_LEAN_AND_MEAN 1
#include "util/clean_windows_h.hpp"

//
#include "stdint.h"
#include "stdio.h"
#include "assert.h"
#include "stddef.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <stack>
#include <queue>
#include <algorithm>

//
#include "glad/glad.h"
#include "glad/glad_wgl.h"

#include "GLFW/glfw3.h"

#include "Tracy.hpp"
#include "TracyOpenGL.hpp"

#include "stb_rect_pack.hpp"
#include "stb_image.hpp"
#include "open_simplex_noise/open_simplex_noise.hpp"

//
#include "kissmath.hpp"

#include "util/animation.hpp"
#include "util/circular_buffer.hpp"
#include "util/collision.hpp"
#include "util/file_io.hpp"
#include "util/freelist_allocator.hpp"
#include "util/geometry.hpp"
#include "kissmath_colors.hpp"
#include "util/move_only_class.hpp"
#include "util/random.hpp"
#include "util/raw_array.hpp"
#include "util/read_directory.hpp"
#include "util/running_average.hpp"
#include "util/string.hpp"
#include "util/threadpool.hpp"
#include "util/threadsafe_queue.hpp"
#include "util/timer.hpp"
#include "util/virtual_allocator.hpp"

#include "dear_imgui.hpp"
#include "profiling.hpp"

using namespace kiss;
using namespace kissmath;

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))