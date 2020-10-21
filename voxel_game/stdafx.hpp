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

#include "stb/stb_rect_pack.hpp"
#include "stb/stb_image.hpp"
#include "stb/stb_image_write.hpp"

#include "dear_imgui/dear_imgui.hpp"

//
#include "kissmath.hpp"

#include "util/animation.hpp"
#include "util/circular_buffer.hpp"
#include "util/collision.hpp"
#include "util/file_io.hpp"
#include "util/freelist_allocator.hpp"
#include "util/geometry.hpp"
#include "kissmath_colors.hpp"
#include "util/macros.hpp"
#include "util/random.hpp"
#include "util/raw_array.hpp"
#include "util/read_directory.hpp"
#include "util/running_average.hpp"
#include "util/string.hpp"
#include "util/threadpool.hpp"
#include "util/threadsafe_queue.hpp"
#include "util/timer.hpp"
#include "util/allocator.hpp"
#include "util/stl_extensions.hpp"

using namespace kiss;
using namespace kissmath;

#include "profiling.hpp"
#include "serialization.hpp"
