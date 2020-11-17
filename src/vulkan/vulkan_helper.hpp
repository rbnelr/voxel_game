#pragma once
#include "vulkan/vulkan.h"
#include "shaderc/shaderc.h"
#include "kisslib/macros.hpp"
#include "kisslib/kissmath.hpp"
#include "graphics.hpp" // for AttribMode
#include <vector>
#include <stdexcept>

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
template <AttribMode M, typename T>
inline constexpr VkFormat get_format ();

template<> inline VkFormat get_format<AttribMode::FLOAT, float > () { return VK_FORMAT_R32_SFLOAT; }
template<> inline VkFormat get_format<AttribMode::FLOAT, float2> () { return VK_FORMAT_R32G32_SFLOAT; }
template<> inline VkFormat get_format<AttribMode::FLOAT, float3> () { return VK_FORMAT_R32G32B32_SFLOAT; }
template<> inline VkFormat get_format<AttribMode::FLOAT, float4> () { return VK_FORMAT_R32G32B32A32_SFLOAT; }

template<> inline VkFormat get_format<AttribMode::SINT, int > () { return VK_FORMAT_R32_SINT; }
template<> inline VkFormat get_format<AttribMode::SINT, int2> () { return VK_FORMAT_R32G32_SINT; }
template<> inline VkFormat get_format<AttribMode::SINT, int3> () { return VK_FORMAT_R32G32B32_SINT; }
template<> inline VkFormat get_format<AttribMode::SINT, int4> () { return VK_FORMAT_R32G32B32A32_SINT; }

template<> inline VkFormat get_format<AttribMode::UINT, uint8_t> () { return VK_FORMAT_R8_UINT; }
template<> inline VkFormat get_format<AttribMode::UINT, uint8v2> () { return VK_FORMAT_R8G8_UINT; }
template<> inline VkFormat get_format<AttribMode::UINT, uint8v3> () { return VK_FORMAT_R8G8B8_UINT; }
template<> inline VkFormat get_format<AttribMode::UINT, uint8v4> () { return VK_FORMAT_R8G8B8A8_UINT; }

template<> inline VkFormat get_format<AttribMode::UINT2FLT, uint8_t> () { return VK_FORMAT_R8_USCALED; }
template<> inline VkFormat get_format<AttribMode::UINT2FLT, uint8v2> () { return VK_FORMAT_R8G8_USCALED; }
template<> inline VkFormat get_format<AttribMode::UINT2FLT, uint8v3> () { return VK_FORMAT_R8G8B8_USCALED; }
template<> inline VkFormat get_format<AttribMode::UINT2FLT, uint8v4> () { return VK_FORMAT_R8G8B8A8_USCALED; }

template<> inline VkFormat get_format<AttribMode::UINT2FLT, uint16_t> () { return VK_FORMAT_R16_USCALED; }

template<> inline VkFormat get_format<AttribMode::UNORM, uint8_t> () { return VK_FORMAT_R8_UNORM; }
template<> inline VkFormat get_format<AttribMode::UNORM, uint8v2> () { return VK_FORMAT_R8G8_UNORM; }
template<> inline VkFormat get_format<AttribMode::UNORM, uint8v3> () { return VK_FORMAT_R8G8B8_UNORM; }
template<> inline VkFormat get_format<AttribMode::UNORM, uint8v4> () { return VK_FORMAT_R8G8B8A8_UNORM; }

struct VertexAttributes {
	VkVertexInputBindingDescription descr;
	std::vector<VkVertexInputAttributeDescription> attribs;

	void init (size_t vertex_size, bool instanced=false) {
		descr = {};
		descr.binding = 0;
		descr.stride = (uint32_t)vertex_size;
		descr.inputRate = instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
	}

	void add (VkFormat format, int location, char const* name, size_t offset) {
		VkVertexInputAttributeDescription a = {};
		a.binding = 0;
		a.location = (uint32_t)location;
		a.format = format;
		a.offset = (uint32_t)offset;
		attribs.push_back(a);
	}

	template <AttribMode M, typename T>
	void add (int location, char const* name, size_t offset) {
		add(get_format<M,T>(), location, name, offset);
	}
};

template <typename T>
inline VertexAttributes make_attribs () {
	VertexAttributes attribs;
	T::attributes(attribs);
	return attribs;
}

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

VkImageView create_image_view (VkDevice device, VkImage image, VkFormat format, int layers=1, VkImageAspectFlags aspect=VK_IMAGE_ASPECT_COLOR_BIT);

////
struct UploadBuffer {
	void*					data = nullptr;
	size_t					size;
	VkBufferUsageFlags		usage;

	VkBuffer				vkbuf;
	size_t					vkoffset;
	VkMemoryRequirements	mem_req;
};

struct Texture {
	VkImage		img;
	VkImageView	img_view;
};

struct UploadTexture {
	void*					data = nullptr;
	int2					size;
	int						layers = 1;
	int						mip_levels = 1;
	VkFormat				format = VK_FORMAT_R8G8B8A8_SRGB;
	VkBufferUsageFlags		usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	VkImageLayout			layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // initial layout
	VkSampleCountFlagBits	samples = (VkSampleCountFlagBits)1;

	Texture					vkimg;
	size_t					vkoffset;
	VkMemoryRequirements	mem_req;
};

struct StaticDataUploader {
	VkCommandBuffer cmds;

	struct StagingAllocation {
		Allocation staging_buf;
		VkBuffer staging_target_buf = VK_NULL_HANDLE;
	};
	std::vector<StagingAllocation> staging_allocs;

	VkDeviceMemory upload (VkDevice dev, VkPhysicalDevice pdev, UploadBuffer* uploads, int uploads_count);
	VkDeviceMemory upload (VkDevice dev, VkPhysicalDevice pdev, UploadTexture* uploads, int uploads_count);

	void end (VkDevice dev) {
		for (auto& a : staging_allocs) {
			vkDestroyBuffer(dev, a.staging_buf.buf   , nullptr);
			if (a.staging_target_buf)
				vkDestroyBuffer(dev, a.staging_target_buf, nullptr);
			vkFreeMemory   (dev, a.staging_buf.mem   , nullptr);
		}
	}
};

inline VkPipelineLayout create_pipeline_layout (VkDevice dev,
		std::initializer_list<VkDescriptorSetLayout> descriptor_sets,
		std::initializer_list<VkPushConstantRange> push_constants) {

	VkPipelineLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.setLayoutCount = (uint32_t)descriptor_sets.size();
	info.pSetLayouts = descriptor_sets.begin();
	info.pushConstantRangeCount = (uint32_t)push_constants.size();
	info.pPushConstantRanges = push_constants.begin();

	VkPipelineLayout layout;
	VK_CHECK_RESULT(vkCreatePipelineLayout(dev, &info, nullptr, &layout));
	return layout;
}

} // namespace vk
