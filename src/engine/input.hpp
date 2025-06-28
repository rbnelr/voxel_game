#pragma once
#include "kisslib/kissmath.hpp"
#include "kisslib/timer.hpp"
#include "kisslib/serialization.hpp"
#include "imgui/dear_imgui.hpp"

class Engine;

struct ButtonState {
	bool is_down   : 1; // button is down
	bool went_down : 1; // button was pressed this frame
	bool went_up   : 1; // button was released this frame
};

// Redeclare GLFW keyscodes as an enum to allow game to use input without needing to include glfw everywhere
// enums are also nicer in debugger
// NOTE: mouse buttons are offset by 1 from GLFW to make space for dummy key KEY_NULL
enum Button : int {
	// Can be used in key bindings
	// -> button[KEY_NULL] is always zero
	KEY_NULL               = 0,

	// Mouse buttons
	// NOTE: offset by 1 from GLFW to make space for dummy key KEY_NULL
	MOUSE_BUTTON_LEFT      = 1,
	MOUSE_BUTTON_RIGHT     = 2,
	MOUSE_BUTTON_MIDDLE    = 3,
	MOUSE_BUTTON_1         = MOUSE_BUTTON_LEFT,
	MOUSE_BUTTON_2         = MOUSE_BUTTON_RIGHT,
	MOUSE_BUTTON_3         = MOUSE_BUTTON_MIDDLE,
	MOUSE_BUTTON_4         = 4,
	MOUSE_BUTTON_5         = 5,
	MOUSE_BUTTON_6         = 6,
	MOUSE_BUTTON_7         = 7,
	MOUSE_BUTTON_8         = 8,
	MOUSE_BUTTONS_COUNT    = MOUSE_BUTTON_8+1,

	// Printable keys
	KEY_SPACE              = ' ' , // 32
	KEY_APOSTROPHE         = '\'', // 39 
	KEY_COMMA              = ',' , // 44 
	KEY_MINUS              = '-' , // 45 
	KEY_PERIOD             = '.' , // 46 
	KEY_SLASH              = '/' , // 47 
	KEY_0                  = '0' , // 48 
	KEY_1                  = '1' , // 49 
	KEY_2                  = '2' , // 50 
	KEY_3                  = '3' , // 51 
	KEY_4                  = '4' , // 52 
	KEY_5                  = '5' , // 53 
	KEY_6                  = '6' , // 54 
	KEY_7                  = '7' , // 55 
	KEY_8                  = '8' , // 56 
	KEY_9                  = '9' , // 57 
	KEY_SEMICOLON          = ';' , // 59 
	KEY_EQUAL              = '=' , // 61 
	KEY_A                  = 'A' , // 65 
	KEY_B                  = 'B' , // 66 
	KEY_C                  = 'C' , // 67 
	KEY_D                  = 'D' , // 68 
	KEY_E                  = 'E' , // 69 
	KEY_F                  = 'F' , // 70 
	KEY_G                  = 'G' , // 71 
	KEY_H                  = 'H' , // 72 
	KEY_I                  = 'I' , // 73 
	KEY_J                  = 'J' , // 74 
	KEY_K                  = 'K' , // 75 
	KEY_L                  = 'L' , // 76 
	KEY_M                  = 'M' , // 77 
	KEY_N                  = 'N' , // 78 
	KEY_O                  = 'O' , // 79 
	KEY_P                  = 'P' , // 80 
	KEY_Q                  = 'Q' , // 81 
	KEY_R                  = 'R' , // 82 
	KEY_S                  = 'S' , // 83 
	KEY_T                  = 'T' , // 84 
	KEY_U                  = 'U' , // 85 
	KEY_V                  = 'V' , // 86 
	KEY_W                  = 'W' , // 87 
	KEY_X                  = 'X' , // 88 
	KEY_Y                  = 'Y' , // 89 
	KEY_Z                  = 'Z' , // 90 
	KEY_LEFT_BRACKET       = '[' , // 91 
	KEY_BACKSLASH          = '\\', // 92 
	KEY_RIGHT_BRACKET      = ']' , // 93 
	KEY_GRAVE_ACCENT       = '`' , // 96 
	KEY_WORLD_1            = 161 , /* non-US #1 */
	KEY_WORLD_2            = 162 , /* non-US #2 */

	// Function keys
	KEY_ESCAPE             = 256,
	KEY_ENTER              = 257,
	KEY_TAB                = 258,
	KEY_BACKSPACE          = 259,
	KEY_INSERT             = 260,
	KEY_DELETE             = 261,
	KEY_RIGHT              = 262,
	KEY_LEFT               = 263,
	KEY_DOWN               = 264,
	KEY_UP                 = 265,
	KEY_PAGE_UP            = 266,
	KEY_PAGE_DOWN          = 267,
	KEY_HOME               = 268,
	KEY_END                = 269,
	KEY_CAPS_LOCK          = 280,
	KEY_SCROLL_LOCK        = 281,
	KEY_NUM_LOCK           = 282,
	KEY_PRINT_SCREEN       = 283,
	KEY_PAUSE              = 284,
	KEY_F1                 = 290,
	KEY_F2                 = 291,
	KEY_F3                 = 292,
	KEY_F4                 = 293,
	KEY_F5                 = 294,
	KEY_F6                 = 295,
	KEY_F7                 = 296,
	KEY_F8                 = 297,
	KEY_F9                 = 298,
	KEY_F10                = 299,
	KEY_F11                = 300,
	KEY_F12                = 301,
	KEY_F13                = 302,
	KEY_F14                = 303,
	KEY_F15                = 304,
	KEY_F16                = 305,
	KEY_F17                = 306,
	KEY_F18                = 307,
	KEY_F19                = 308,
	KEY_F20                = 309,
	KEY_F21                = 310,
	KEY_F22                = 311,
	KEY_F23                = 312,
	KEY_F24                = 313,
	KEY_F25                = 314,
	// Numpad keys
	KEY_KP_0               = 320,
	KEY_KP_1               = 321,
	KEY_KP_2               = 322,
	KEY_KP_3               = 323,
	KEY_KP_4               = 324,
	KEY_KP_5               = 325,
	KEY_KP_6               = 326,
	KEY_KP_7               = 327,
	KEY_KP_8               = 328,
	KEY_KP_9               = 329,
	KEY_KP_DECIMAL         = 330,
	KEY_KP_DIVIDE          = 331,
	KEY_KP_MULTIPLY        = 332,
	KEY_KP_SUBTRACT        = 333,
	KEY_KP_ADD             = 334,
	KEY_KP_ENTER           = 335,
	KEY_KP_EQUAL           = 336,
	// Modifier keys
	KEY_LEFT_SHIFT         = 340,
	KEY_LEFT_CONTROL       = 341,
	KEY_LEFT_ALT           = 342,
	KEY_LEFT_SUPER         = 343,
	KEY_RIGHT_SHIFT        = 344,
	KEY_RIGHT_CONTROL      = 345,
	KEY_RIGHT_ALT          = 346,
	KEY_RIGHT_SUPER        = 347,
	KEY_MENU               = 348,

	BUTTONS_COUNT          = KEY_MENU+1,
};

struct Input {
	SERIALIZE(Input, max_dt, time_scale, pause_time)

	uint64_t frame_counter = 0;
	
	//// Time 
	uint64_t frame_begin_ts;

	// zero on first frame
	float real_dt; // for displaying fps etc.
	float unscaled_dt;
	float dt;

	//// Others
	bool cursor_enabled = true;

	int2 window_size;
	float2 cursor_pos; // in pixels, float because glfw retuns doubles
	
	float2 cursor_pos_bottom_up () const { // in pixels, float because glfw retuns doubles
		return float2(cursor_pos.x, (float)window_size.y - 1.0f - cursor_pos.y);
	}

	float2 mouse_delta;
	int mouse_wheel_delta; // in "clicks"

	ButtonState buttons[BUTTONS_COUNT];

	//// Input Settings
	// Max dt to prevent large physics time steps
	float max_dt = 1.0f / 20;
	// Timer scaler (does not scale unscaled_dt)
	float time_scale = 1;
	// Pause time (equivalent to time_scale=0)
	bool pause_time = false;

	// to fix glfw mouse input
	double _prev_mouse_pos_x, _prev_mouse_pos_y;
	bool _prev_cursor_enabled;
	
	float _max_fps = 0;

	void imgui () {
		if (!imgui_Header("Input")) return;

		ImGui::DragFloat("max_dt", &max_dt, 0.01f);
		ImGui::DragFloat("time_scale", &time_scale, 0.01f);
		ImGui::Checkbox("pause_time [,]", &pause_time);

		float max_fps = _max_fps;
		ImGui::SetNextItemWidth(100);
		if (ImGui::InputFloat("max_fps (eats cpu, only for dbg)", &max_fps, 0, 0, "%.0f", ImGuiInputTextFlags_EnterReturnsTrue))
			_max_fps = max(max_fps, 0.0f);

		ImGui::Text("timestep (dt): %6.3f ms", real_dt * 1000);

		ImGui::PopID();
	}

	// used by gameloop to clear frame based input like deltas and flags
	void clear_frame_input () {
		mouse_delta = 0;
		mouse_wheel_delta = 0;

		for (auto& b : buttons) {
			b.went_down = 0;
			b.went_up = 0;
		}
	}

	// inaccurate, only use for debugging?
	void attempt_sleep_for_max_fps () {
		if (_max_fps == 0) return;

		float target_sec = 1.0f / max(_max_fps, 1.0f);
		float freq = (float)kiss::timestamp_freq;

		//float remain = target_sec - (float)(kiss::get_timestamp() - frame_begin_ts) / freq;
		
		//auto sleep_msecs = (int)(remain * 1000) - 16;
		//if (sleep_msecs > 0) {
		//	// attempt to use sleep if more than 1 msec remaining
		//	kiss::sleep_msec((uint32_t)sleep_msecs);
		//}

		uint64_t target_period = (uint64_t)(freq * target_sec);

		uint64_t target = frame_begin_ts + target_period;
		while (kiss::get_timestamp() < target) {
			// busy loop
		}
	}

	void get_time (bool was_not_visible_and_paused) {
		// Calc next frame dt based on this frame duration
		uint64_t now = kiss::get_timestamp();

		real_dt = (float)(now - frame_begin_ts) / (float)kiss::timestamp_freq;
		dt = min(real_dt * (pause_time ? 0 : time_scale), max_dt);

		frame_begin_ts = now;

		if (was_not_visible_and_paused) {
			real_dt = 0; // do not count time elapsed while application was minimized etc.
		}
	}

	// used to ignore inputs that imgui has already captured
	void disable_keyboard () {
		for (int i=KEY_SPACE; i<BUTTONS_COUNT; ++i)
			buttons[i] = {};
	}
	void disable_mouse () {
		mouse_delta = 0;
		mouse_wheel_delta = 0;
		for (int i=MOUSE_BUTTON_1; i<MOUSE_BUTTON_8+1; ++i)
			buttons[i] = {};
	}

	void set_cursor_mode (Engine& eng, bool enabled);
	void toggle_cursor_mode (Engine& eng) {
		set_cursor_mode(eng, !cursor_enabled);
	}
};

////
void glfw_input_pre_gameloop (Engine& eng);
void glfw_sample_non_callback_input (Engine& eng);

void glfw_register_input_callbacks (Engine& eng);
