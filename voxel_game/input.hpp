#pragma once
#include "kissmath/float2.hpp"
#include "kissmath/int2.hpp"
using namespace kissmath;

class Input {

public:
	float max_dt = 0.2f;

	// zero on first frame
	// else duration of the prev frame but never larger than max_dt
	float dt;

	int2 window_size;
	float2 cursor_pos; // in pixels, float because glfw retuns doubles

	float2 mouse_delta;
	float mouse_wheel_delta; // in steps, float because glfw retuns doubles

	// used by gameloop to clear frame based input like deltas and flags
	inline void clear_frame_input () {
		mouse_delta = 0;
		mouse_wheel_delta = 0;
	}
};

// global input
extern Input input;
