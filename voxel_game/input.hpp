#pragma once
#include "common.hpp"
#include "GLFW/glfw3.h" // using glfw for it's key mapping

struct Button {
	bool is_down   : 1; // button is down
	bool went_down : 1; // button was pressed this frame
	bool went_up   : 1; // button was released this frame
};

class Input {

public:
	//// Input Data
	// zero on first frame
	// else duration of the prev frame scaled by time_scale but never larger than max_dt
	float dt;

	// zero on first frame
	// else duration of the prev frame (not scaled by time_scale) but never larger than max_dt
	// use for camera flying, potentially for player physics if slow motion is not supposed to affect player for ex.
	float unscaled_dt;

	float real_dt; // for displaying fps etc.

	int2 window_size;
	float2 cursor_pos; // in pixels, float because glfw retuns doubles

	float2 mouse_delta;
	int mouse_wheel_delta; // in "clicks"

	Button buttons[GLFW_KEY_LAST +1];

	//// Input Settings
	// Max dt to prevent large physics time steps
	float max_dt = 1.0f / 20;
	// Timer scaler (does not scale unscaled_dt)
	float time_scale = 1;
	// Pause time (equivalent to time_scale=0)
	bool pause_time = false;

	float mouselook_sensitiviy_divider = 500;

	float view_elevation_down_limit = deg(5);
	float view_elevation_up_limit = deg(5);
	

	// used by gameloop to clear frame based input like deltas and flags
	inline void clear_frame_input () {
		mouse_delta = 0;
		mouse_wheel_delta = 0;

		for (auto& b : buttons) {
			b.went_down = 0;
			b.went_up = 0;
		}
	}

	inline void disable_keyboard () {
		for (int i=GLFW_MOUSE_BUTTON_LAST+1; i<GLFW_KEY_LAST; ++i)
			buttons[i] = {};
	}
	inline void disable_mouse () {
		mouse_delta = 0;
		mouse_wheel_delta = 0;
		for (int i=GLFW_MOUSE_BUTTON_1; i<GLFW_MOUSE_BUTTON_LAST; ++i)
			buttons[i] = {};
	}

	// Not sure where to put this, return mouselook_delta
	//  if cursor is disabled (fps mode)
	//  if cursor is enabled and rmb down
	float2 get_mouselook_delta ();

	void imgui () {
		bool open = ImGui::CollapsingHeader("Input");

		if (open) ImGui::DragFloat("max_dt", &max_dt, 0.01f);
		if (open) ImGui::DragFloat("time_scale", &time_scale, 0.01f);
		if (open) ImGui::Checkbox("pause_time [;]", &pause_time);
		if (buttons[GLFW_KEY_SEMICOLON].went_down)
			pause_time = !pause_time;

		if (open) ImGui::DragFloat("mouselook_sensitiviy_divider", &mouselook_sensitiviy_divider, 0.1f);

		if (open) ImGui::SliderAngle("view_elevation_down_limit", &view_elevation_down_limit, 0, 45);
		if (open) ImGui::SliderAngle("view_elevation_up_limit", &view_elevation_up_limit, 0, 45);
	}
};

// global input
extern Input input;

void glfw_register_input_callbacks (GLFWwindow* window);

void glfw_input_pre_gameloop (GLFWwindow* window);
void glfw_sample_non_callback_input (GLFWwindow* window);
