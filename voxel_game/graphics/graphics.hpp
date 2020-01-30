#pragma once
#include "../kissmath.hpp"
#include "glshader.hpp"
#include "globjects.hpp"
#include "debug_graphics.hpp"

struct SkyboxGraphics {

	Shader shader = Shader("skybox");

	void draw (Camera_View& view);
};

