#pragma once
#include "GLFW/glfw3.h" // using glfw for it's key mapping

#include "kissmath/float2.hpp"
#include "kissmath/int2.hpp"
using namespace kissmath;

struct Button {
	bool is_down   : 1; // button is down
	bool went_down : 1; // button was pressed this frame
	bool went_up   : 1; // button was released this frame
};

class Input {

public:
	float max_dt = 0.2f;
	float mouselook_sensitiviy_divider = 400;

	// zero on first frame
	// else duration of the prev frame but never larger than max_dt
	float dt;

	int2 window_size;
	float2 cursor_pos; // in pixels, float because glfw retuns doubles

	float2 mouse_delta;
	float mouse_wheel_delta; // in steps, float because glfw retuns doubles

	Button buttons[GLFW_KEY_LAST +1];

	// used by gameloop to clear frame based input like deltas and flags
	inline void clear_frame_input () {
		mouse_delta = 0;
		mouse_wheel_delta = 0;

		for (auto& b : buttons) {
			b.went_down = 0;
			b.went_up = 0;
		}
	}

	// Not sure where to put this, return mouselook_delta
	//  if cursor is disabled (fps mode)
	//  if cursor is enabled and rmb down
	float2 get_mouselook_delta ();
};

// global input
extern Input input;
