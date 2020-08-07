#pragma once
#include "common.hpp"
#include "util/clean_windows_h.hpp"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

bool get_vsync ();
void set_vsync (bool on);

bool get_fullscreen (bool* borderless_fullscreen=nullptr);
bool switch_fullscreen (bool fullscreen, bool borderless_fullscreen);
bool toggle_fullscreen ();

#if _DEBUG || 1
#define OPENGL_DEBUG
#define GLFW_DEBUG
#endif

// Global window
extern GLFWwindow* window;

extern int frame_counter;
