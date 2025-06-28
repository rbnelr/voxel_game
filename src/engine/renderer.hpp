#pragma once
#include "kisslib/serialization.hpp"
#include "kisslib/read_directory.hpp"

enum class AttribMode {
	FLOAT,		// simply pass float to shader
	SINT,		// simply pass sint to shader
	UINT,		// simply pass uint to shader
	SINT2FLT,	// convert sint to float 
	UINT2FLT,	// convert uint to float
	SNORM,		// sint turns into [-1, 1] float (ex. from [-127, +127], note that -127 instead of -128)
	UNORM,		// uint turns into [0, 1] float (ex. from [0, 255])
};

// Base class for supporting multiple render backends, but I removed vulkan for now
// so would need to reimplement feature so actually switch renderers again

enum class RenderBackend : int {
	OPENGL=0,
};

// Controls for in what builds to output debug info, set in this file to allow window.cpp to use these too

struct GLFWwindow;
class Game;
struct Chunks;
struct Input;

class Renderer {
public:
	virtual ~Renderer () {}

	virtual void deserialize (nlohmann::ordered_json const& j) = 0;
	virtual void serialize (nlohmann::ordered_json& j) = 0;

	virtual bool get_vsync () = 0;
	virtual void set_vsync (bool state) = 0;

	virtual void render_frame (Game& game) = 0;

	virtual void screenshot_imgui () = 0;
	virtual void graphics_imgui () = 0;
	virtual void chunk_renderer_imgui (Chunks& chunks) = 0;
	
	virtual bool update_files_changed (kiss::ChangedFiles& changed_files) = 0;
};

std::unique_ptr<Renderer> start_renderer (RenderBackend backend, Game& game);
