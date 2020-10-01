#include "common.hpp"
#include "input.hpp"
#include "window.hpp"

void Input::set_cursor_mode (Window& window, bool enabled) {
	cursor_enabled = enabled;

	if (enabled)
		glfwSetInputMode(window.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Cursor enabled, can interact with Imgui
	else
		glfwSetInputMode(window.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Cursor disabled & Imgui interaction disabled, all controls go to game
}

////
void glfw_get_non_callback_input (Window& window) {
	glfwGetFramebufferSize(window.window, &window.input.window_size.x, &window.input.window_size.y);

	double x, y;
	glfwGetCursorPos(window.window, &x, &y);

	window.input.cursor_pos = float2((float)x, (float)y);
	window.input.cursor_pos.y = window.input.window_size.y - 1 - window.input.cursor_pos.y;

	//logf("cursor_pos: %f %f\n", input.cursor_pos.x, input.cursor_pos.y);
}
void glfw_input_pre_gameloop (Window& window) {
	// Get initial mouse position
	glfwGetCursorPos(window.window, &window.input._prev_mouse_pos_x, &window.input._prev_mouse_pos_y);

	// Set initial cursor mode
	window.input.set_cursor_mode(window, window.input.cursor_enabled);
	window.input._prev_cursor_enabled = window.input.cursor_enabled;
}
void glfw_sample_non_callback_input (Window& window) {
	glfwGetFramebufferSize(window.window, &window.input.window_size.x, &window.input.window_size.y);

	double x, y;
	glfwGetCursorPos(window.window, &x, &y);

	window.input.cursor_pos = float2((float)x, (float)y);
	window.input.cursor_pos.y = window.input.window_size.y - 1 - window.input.cursor_pos.y;

	//logf("cursor_pos: %f %f\n", input.cursor_pos.x, input.cursor_pos.y);
}

//// Callbacks
void glfw_key_event (GLFWwindow* wnd, int key, int scancode, int action, int mods) {
	assert(action == GLFW_PRESS || action == GLFW_RELEASE || action == GLFW_REPEAT);
	auto& window = *(Window*)glfwGetWindowUserPointer(wnd);
	auto& input = window.input;

	bool went_down =	action == GLFW_PRESS;
	bool went_up =		action == GLFW_RELEASE;

	bool alt =			(mods & GLFW_MOD_ALT) != 0;

	// Toggle fullscreen with F11 or CTRL-ENTER
	if (key == GLFW_KEY_F11 || (alt && key == GLFW_KEY_ENTER)) {
		if (went_down) window.toggle_fullscreen();
		return;
	}

	// Toggle Imgui visibility with F1
	if (key == GLFW_KEY_F1) {
		if (went_down) imgui.enabled = !imgui.enabled;
		return;
	}

	// Toggle between Imgui interaction and game control
	if (key == GLFW_KEY_F2) {
		if (went_down) input.toggle_cursor_mode(window);
		return;
	}

	if ((went_down || went_up) && key >= GLFW_KEY_SPACE && key <= GLFW_KEY_LAST) {
		input.buttons[key].is_down = went_down;
		input.buttons[key].went_down = went_down;
		input.buttons[key].went_up = went_up;
	}
}
void glfw_char_event (GLFWwindow* wnd, unsigned int codepoint, int mods) {
	auto& input = ((Window*)glfwGetWindowUserPointer(wnd))->input;

	// for typing input
}
void glfw_mouse_button_event (GLFWwindow* wnd, int button, int action, int mods) {
	assert(action == GLFW_PRESS || action == GLFW_RELEASE);
	auto& input = ((Window*)glfwGetWindowUserPointer(wnd))->input;

	bool went_down = action == GLFW_PRESS;
	bool went_up =	 action == GLFW_RELEASE;

	if ((went_down || went_up) && button >= GLFW_MOUSE_BUTTON_1 && button <= GLFW_MOUSE_BUTTON_8) {
		input.buttons[button].is_down = went_down;
		input.buttons[button].went_down = went_down;
		input.buttons[button].went_up = went_up;
	}
}

// The initial event seems to report the same position as our initial glfwGetCursorPos, so that delta is fine
// But when toggling the cursor from disabled to visible cursor jumps back to the prev position, and an event reports this as delta so we need to discard this 
void glfw_mouse_move_event (GLFWwindow* wnd, double xpos, double ypos) {
	auto& input = ((Window*)glfwGetWindowUserPointer(wnd))->input;

	float2 delta = float2((float)(xpos - input._prev_mouse_pos_x), (float)(ypos - input._prev_mouse_pos_y));
	delta.y = -delta.y; // convert to bottom up

	input._prev_mouse_pos_x = xpos;
	input._prev_mouse_pos_y = ypos;

	bool cursor_enabled = glfwGetInputMode(wnd, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;
	bool cursor_toggled = cursor_enabled != input._prev_cursor_enabled;
	input._prev_cursor_enabled = cursor_enabled;

	bool discard_delta = cursor_toggled;

	//logf("glfw_mouse_move_event: %7d: %f %f%s\n", frame_counter, delta.x, delta.y, discard_delta ? " (discarded)":"");

	if (!discard_delta)
		input.mouse_delta += delta;
}
void glfw_mouse_scroll (GLFWwindow* wnd, double xoffset, double yoffset) {
	auto& input = ((Window*)glfwGetWindowUserPointer(wnd))->input;

	// assume int, if glfw_mouse_scroll ever gives us 0.2 for ex. this might break
	// But the gameplay code wants to assume mousewheel moves in "clicks", for item swapping
	// I've personally never seen a mousewheel that does not move in "clicks" anyway
	input.mouse_wheel_delta += (int)ceil(abs(yoffset)) * (int)normalizesafe((float)yoffset); // -1.1f => -2    0 => 0    0.3f => +1
}

void glfw_register_input_callbacks (Window& window) {
	glfwSetWindowUserPointer(window.window, &window);

	glfwSetKeyCallback(window.window,			glfw_key_event);
	glfwSetCharModsCallback(window.window,		glfw_char_event);
	glfwSetCursorPosCallback(window.window,		glfw_mouse_move_event);
	glfwSetMouseButtonCallback(window.window,	glfw_mouse_button_event);
	glfwSetScrollCallback(window.window,		glfw_mouse_scroll);
}
