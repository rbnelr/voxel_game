#pragma once
#include "util/clean_windows_h.hpp"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

// Global window
extern GLFWwindow* window;

extern int frame_counter;

bool get_vsync ();
void set_vsync (bool on);

bool get_fullscreen ();
void toggle_fullscreen ();
