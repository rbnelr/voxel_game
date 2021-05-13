#pragma once

static constexpr const char* APPNAME = "Voxel Game";

#include "kisslib/clean_windows_h.hpp"

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

#include "imgui/dear_imgui.hpp"

#include "Tracy.hpp"

#ifdef TRACY_ENABLE
	// avoid large functions being inlined to make profiling results more clear
	// but let compiler decide in non-profiled builds (also try to remove these after profiling section of code)
	#define PROFILE_NOINLINE __declspec(noinline)
#else
	#define PROFILE_NOINLINE
#endif

//
#include "kisslib/kissmath.hpp"
#include "kisslib/kissmath_colors.hpp"

#include "kisslib/animation.hpp"
#include "kisslib/circular_buffer.hpp"
#include "kisslib/collision.hpp"
#include "kisslib/file_io.hpp"
#include "kisslib/macros.hpp"
#include "kisslib/random.hpp"
#include "kisslib/raw_array.hpp"
#include "kisslib/image.hpp"
#include "kisslib/read_directory.hpp"
#include "kisslib/running_average.hpp"
#include "kisslib/string.hpp"
#include "kisslib/stl_extensions.hpp"
#include "kisslib/threadpool.hpp"
#include "kisslib/threadsafe_queue.hpp"
#include "kisslib/timer.hpp"
#include "kisslib/allocator.hpp"
#include "kisslib/containers.hpp"

#define SERIALIZE_LOG(type, ...) clog(type, __VA_ARGS__)
#include "kisslib/serialization.hpp"

using namespace kiss;
using namespace kissmath;

#include "audio/audio.hpp"
#include "engine/threading.hpp"
#include "engine/input_buttons.hpp"
#include "engine/input.hpp"
#include "engine/renderer.hpp"
#include "engine/debug_draw.hpp"
