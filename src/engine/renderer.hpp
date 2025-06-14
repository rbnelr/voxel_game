#pragma once
#include "common.hpp"

enum class AttribMode {
	FLOAT,		// simply pass float to shader
	SINT,		// simply pass sint to shader
	UINT,		// simply pass uint to shader
	SINT2FLT,	// convert sint to float 
	UINT2FLT,	// convert uint to float
	SNORM,		// sint turns into [-1, 1] float (ex. from [-127, +127], note that -127 instead of -128)
	UNORM,		// uint turns into [0, 1] float (ex. from [0, 255])
};

// Base class for multiple render backends

enum class RenderBackend : int {
	OPENGL=0,
};

// Controls for in what builds to output debug info, set in this file to allow window.cpp to use these too

struct GLFWwindow;
struct Game;
struct Chunks;

class Renderer {
public:
	virtual ~Renderer () {}

	virtual void deserialize (nlohmann::ordered_json const& j) = 0;
	virtual void serialize (nlohmann::ordered_json& j) = 0;

	virtual bool get_vsync () = 0;
	virtual void set_vsync (bool state) = 0;

	virtual void frame_begin (GLFWwindow* window, Input& I, kiss::ChangedFiles& changed_files) = 0;
	virtual void render_frame (GLFWwindow* window, Input& I, Game& game) = 0;

	virtual void screenshot_imgui (Input& I) = 0;
	virtual void graphics_imgui (Input& I, Game& g) = 0;
	virtual void chunk_renderer_imgui (Chunks& chunks) = 0;
};

std::unique_ptr<Renderer> start_renderer (RenderBackend backend, GLFWwindow* window);
