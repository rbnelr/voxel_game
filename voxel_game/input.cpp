#include "glfw_window.hpp"
#include "input.hpp"

Input input;

float2 Input::get_mouselook_delta () {
	float2 delta = 0;
	if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED || buttons[GLFW_MOUSE_BUTTON_RIGHT].is_down) {
		delta = mouse_delta;
	}
	return delta;
}
