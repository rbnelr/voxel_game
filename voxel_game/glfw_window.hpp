#pragma once
//#include "util/clean_windows_h.hpp"

#if _DEBUG || 1
	#define GLFW_DEBUG
#endif

#include "GLFW/glfw3.h"

// Global window
extern GLFWwindow* window;

extern int frame_counter;

bool get_vsync ();
void set_vsync (bool on);

bool get_fullscreen (bool* borderless_fullscreen=nullptr);
bool switch_fullscreen (bool fullscreen, bool borderless_fullscreen);
bool toggle_fullscreen ();
