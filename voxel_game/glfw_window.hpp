#pragma once
#undef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1

#undef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define WIN32_NOMINMAX

#undef NOMINMAX
#define NOMINMAX

#include "windows.h"

#undef near
#undef far
#undef min
#undef max

#include "glad/glad.h"

#include "GLFW/glfw3.h"

// Global window
extern GLFWwindow* window;

bool get_vsync ();
void set_vsync (bool on);

bool get_fullscreen ();
void toggle_fullscreen ();
