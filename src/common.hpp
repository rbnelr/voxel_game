#pragma once

//// Config
#define RENDERER_DEBUG_LABELS 1

#define OGL_USE_REVERSE_DEPTH 0
#define OGL_USE_DEDICATED_GPU 1
#define RENDERER_WINDOW_FBO_NO_DEPTH 0 // 1 if all 3d rendering happens in an FBO anyway (usually with HDR via float)

//
#if   BUILD_DEBUG
	#define RENDERER_PROFILING					1
	#define RENDERER_DEBUG_OUTPUT				1
	#define RENDERER_DEBUG_OUTPUT_BREAKPOINT	1
	#define OGL_STATE_ASSERT					1
#elif BUILD_VALIDATE
	#define RENDERER_PROFILING					1
	#define RENDERER_DEBUG_OUTPUT				1
	#define RENDERER_DEBUG_OUTPUT_BREAKPOINT	0
	#define OGL_STATE_ASSERT					0
#elif BUILD_TRACY
	//#define NDEBUG // no asserts
	#define RENDERER_PROFILING					1 // Could impact perf? Maybe disable this?
#elif BUILD_RELEASE
	//#define NDEBUG // no asserts
#endif

//// Includes
#include "stdint.h"
#include "assert.h"
#include "stdio.h"

#include <memory>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>

#include <variant>
#include <optional>

#include "kisslib/kissmath.hpp"
#include "kisslib/kissmath_colors.hpp"
#include "kisslib/string.hpp"
#include "kisslib/file_io.hpp"
#include "kisslib/macros.hpp"
#include "kisslib/random.hpp"
//#include "kisslib/raw_array.hpp"
#include "kisslib/image.hpp"
#include "kisslib/read_directory.hpp"
#include "kisslib/running_average.hpp"
#include "kisslib/stl_extensions.hpp"
//#include "kisslib/threadpool.hpp"
//#include "kisslib/threadsafe_queue.hpp"
#include "kisslib/timer.hpp"
//#include "kisslib/animation.hpp"
#include "kisslib/collision.hpp"
#include "kisslib/allocator.hpp"
//#include "kisslib/containers.hpp"

#include "tracy/Tracy.hpp"
#include "imgui/dear_imgui.hpp"
#include "imgui/curve_edit.hpp"
#include "engine.hpp"

#define SERIALIZE_LOG(type, ...) log(type, __VA_ARGS__)
#include "kisslib/serialization.hpp"

#include "audio.hpp"
#include "threading.hpp"
#include "input.hpp"
#include "renderer.hpp"
#include "debug_draw.hpp"

// Global game to access stuff like audio manager
class Game;
extern Game* g;

using namespace kiss;
using namespace kissmath;
