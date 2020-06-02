#pragma once
#include "vulkan/vulkan.h"
#include <memory>

namespace vk {

	struct Vulkan {

	};
}

using vk::Vulkan;

extern std::unique_ptr<Vulkan> vulkan;


struct Texture2D {

};

struct Texture2DArray {

};

struct Sampler {

	Sampler () {}
};

struct Shader {

	Shader (char const* name) {}
};

template <typename VERTEX>
struct Mesh {

};

struct Attributes {

	template <typename T>
	void add (int index, char const* name, size_t stride, size_t offset, bool normalized=false) {}

	template <typename T>
	void add_int (int index, char const* name, size_t stride, size_t offset, bool normalized=false) {}
};
