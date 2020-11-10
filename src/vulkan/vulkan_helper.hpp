#pragma once
#include "vulkan/vulkan.h"
#include "shaderc/shaderc.h"
#include "kisslib/macros.hpp"
#include "kisslib/kissmath.hpp"
#include <vector>
#include <stdexcept>

#ifndef NDEBUG
	#define VK_VALIDATION_LAYERS
#endif

namespace vk {

////
#define VK_CHECK_RESULT(expr) if ((expr) != VK_SUCCESS) { \
	throw std::runtime_error("[Vulkan] Fatal error: \"" TO_STRING(expr) "\""); \
	__debugbreak(); \
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback (
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

// Return results of vulkan fuction that return arrays as std::vector by calling once for the length and then again to fill the array
template <typename T, typename FUNC, typename... ARGS>
inline std::vector<T> get_vector (FUNC func, ARGS... args) {
	uint32_t count = 0;
	func(args..., &count, nullptr);

	std::vector<T> vec (count);
	if (count > 0)
		func(args..., &count, vec.data());

	return vec;
}

////
enum class AttribFormat : int {
	FLOAT1		= VK_FORMAT_R32_SFLOAT,
	FLOAT2		= VK_FORMAT_R32G32_SFLOAT,
	FLOAT3		= VK_FORMAT_R32G32B32_SFLOAT,
	FLOAT4		= VK_FORMAT_R32G32B32A32_SFLOAT,
};

struct VertexAttributes {
	VkVertexInputBindingDescription descr;
	std::vector<VkVertexInputAttributeDescription> attribs;

	void init (size_t vertex_size) {
		descr = {};
		descr.binding = 0;
		descr.stride = (uint32_t)vertex_size;
		descr.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	}

	void add (int location, char const* name, AttribFormat format, size_t offset) {
		VkVertexInputAttributeDescription a = {};
		a.binding = 0;
		a.location = (uint32_t)location;
		a.format = VK_FORMAT_R32G32B32_SFLOAT;
		a.offset = (uint32_t)offset;
		attribs.push_back(a);
	}
};

struct RenderBuffer {
	VkImage						image;
	VkImageView					image_view;

	// Dedicated Memory allocation for now
	VkDeviceMemory				memory;
};

struct Allocation {
	VkDeviceMemory	mem;
	VkBuffer		buf;
};

//// Resource creation
VkSampleCountFlagBits get_max_usable_multisample_count (VkPhysicalDevice device);

uint32_t find_memory_type (VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties);

VkFormat find_supported_format (VkPhysicalDevice device, std::initializer_list<VkFormat> const& candidates,
	VkImageTiling tiling, VkFormatFeatureFlags features);

VkFormat find_depth_format (VkPhysicalDevice device);

inline bool has_stencil_component (VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkCommandPool create_one_time_command_pool (VkDevice device, uint32_t queue_family);

VkDeviceMemory alloc_memory (VkDevice device, VkPhysicalDevice pdevice, VkDeviceSize size, VkMemoryPropertyFlags props);

Allocation allocate_buffer (VkDevice device, VkPhysicalDevice pdev,
	VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props);

VkImage create_image (VkDevice device, VkPhysicalDevice pdev,
	int2 size, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
	VkImageLayout initial_layout, VkMemoryPropertyFlags props,
	VkDeviceMemory* out_image_memory, int mip_levels = 1, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

VkImageView create_image_view (VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect);

} // namespace vk
