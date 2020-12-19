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
inline std_vector<T> get_vector (FUNC func, ARGS... args) {
	uint32_t count = 0;
	func(args..., &count, nullptr);

	std_vector<T> vec (count);
	if (count > 0)
		func(args..., &count, vec.data());

	return vec;
}

////
struct DebugMarker {
	PFN_vkDebugMarkerSetObjectTagEXT  vkDebugMarkerSetObjectTagEXT  = nullptr;
	PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT = nullptr;
	PFN_vkCmdDebugMarkerBeginEXT      vkCmdDebugMarkerBeginEXT      = nullptr;
	PFN_vkCmdDebugMarkerEndEXT        vkCmdDebugMarkerEndEXT        = nullptr;
	PFN_vkCmdDebugMarkerInsertEXT     vkCmdDebugMarkerInsertEXT     = nullptr;

	void load (VkDevice dev) {
		vkDebugMarkerSetObjectTagEXT  = (PFN_vkDebugMarkerSetObjectTagEXT )vkGetDeviceProcAddr(dev, "vkDebugMarkerSetObjectTagEXT");
		vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(dev, "vkDebugMarkerSetObjectNameEXT");
		vkCmdDebugMarkerBeginEXT      = (PFN_vkCmdDebugMarkerBeginEXT     )vkGetDeviceProcAddr(dev, "vkCmdDebugMarkerBeginEXT");
		vkCmdDebugMarkerEndEXT        = (PFN_vkCmdDebugMarkerEndEXT       )vkGetDeviceProcAddr(dev, "vkCmdDebugMarkerEndEXT");
		vkCmdDebugMarkerInsertEXT     = (PFN_vkCmdDebugMarkerInsertEXT    )vkGetDeviceProcAddr(dev, "vkCmdDebugMarkerInsertEXT");
	}

	void set_name (VkDevice dev, VkDebugReportObjectTypeEXT type, uint64_t obj, const char* name) {
		if (!vkDebugMarkerSetObjectNameEXT) return;
		VkDebugMarkerObjectNameInfoEXT info = {};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		info.objectType = type;
		info.object = obj;
		info.pObjectName = name;
		vkDebugMarkerSetObjectNameEXT(dev, &info);
	}

	void set_name (VkDevice dev, VkDeviceMemory obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkCommandBuffer obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkBuffer obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkSampler obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkImage obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkImageView obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkFramebuffer obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkPipelineLayout obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkPipeline obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkRenderPass obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkDescriptorPool obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkDescriptorSetLayout obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkDescriptorSet obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, (uint64_t)obj, name);
	}
	void set_name (VkDevice dev, VkShaderModule obj, const char* name) {
		set_name(dev, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, (uint64_t)obj, name);
	}

	void begin_marker (VkDevice dev, VkCommandBuffer cmds, char const* name) {
		if (!vkCmdDebugMarkerBeginEXT) return;
		VkDebugMarkerMarkerInfoEXT info = {};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		//info.color[4]
		info.pMarkerName = name;
		vkCmdDebugMarkerBeginEXT(cmds, &info);
	}
	void end_marker (VkDevice dev, VkCommandBuffer cmds) {
		if (!vkCmdDebugMarkerEndEXT) return;
		vkCmdDebugMarkerEndEXT(cmds);
	}
};

////
template <AttribMode M, typename T>
inline constexpr VkFormat get_format ();
template <AttribMode M, typename T, int VECN>
inline constexpr VkFormat get_formatv ();

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

template<> inline VkFormat get_format<AttribMode::UNORM, uint8_t> () { return VK_FORMAT_R8_UNORM; }
template<> inline VkFormat get_format<AttribMode::UNORM, uint8v2> () { return VK_FORMAT_R8G8_UNORM; }
template<> inline VkFormat get_format<AttribMode::UNORM, uint8v3> () { return VK_FORMAT_R8G8B8_UNORM; }
template<> inline VkFormat get_format<AttribMode::UNORM, uint8v4> () { return VK_FORMAT_R8G8B8A8_UNORM; }

template<> inline VkFormat get_format<AttribMode::UINT, uint16_t> () { return VK_FORMAT_R16_UINT; }

template<> inline VkFormat get_format<AttribMode::UINT2FLT, uint16_t> () { return VK_FORMAT_R16_USCALED; }
template<> inline VkFormat get_formatv<AttribMode::UINT2FLT, uint16_t, 1> () { return VK_FORMAT_R16_USCALED; }
template<> inline VkFormat get_formatv<AttribMode::UINT2FLT, uint16_t, 2> () { return VK_FORMAT_R16G16_USCALED; }
template<> inline VkFormat get_formatv<AttribMode::UINT2FLT, uint16_t, 3> () { return VK_FORMAT_R16G16B16_USCALED; }
template<> inline VkFormat get_formatv<AttribMode::UINT2FLT, uint16_t, 4> () { return VK_FORMAT_R16G16B16A16_USCALED; }

template<> inline VkFormat get_format<AttribMode::SINT2FLT, int16_t> () { return VK_FORMAT_R16_SSCALED; }
template<> inline VkFormat get_formatv<AttribMode::SINT2FLT, int16_t, 1> () { return VK_FORMAT_R16_SSCALED; }
template<> inline VkFormat get_formatv<AttribMode::SINT2FLT, int16_t, 2> () { return VK_FORMAT_R16G16_SSCALED; }
template<> inline VkFormat get_formatv<AttribMode::SINT2FLT, int16_t, 3> () { return VK_FORMAT_R16G16B16_SSCALED; }
template<> inline VkFormat get_formatv<AttribMode::SINT2FLT, int16_t, 4> () { return VK_FORMAT_R16G16B16A16_SSCALED; }

struct VertexAttributes {
	VkVertexInputBindingDescription descr = {};
	std::vector<VkVertexInputAttributeDescription> attribs;

	bool empty () {
		return descr.stride == 0;
	}

	void init (size_t vertex_size, bool instanced=false) {
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

	template <AttribMode M, typename T, int VECN>
	void addv (int location, char const* name, size_t offset) {
		add(get_formatv<M,T,VECN>(), location, name, offset);
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

	void free (VkDevice dev) {
		vkDestroyBuffer(dev, buf, nullptr);
		vkFreeMemory(dev, mem, nullptr);
	}
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
	int						mip_levels = 1; // special value -1 means all mips (ie. as many size halvings until 1x1 is reached)
	VkFormat				format = VK_FORMAT_R8G8B8A8_SRGB;
	VkBufferUsageFlags		usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	VkImageLayout			layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // initial layout
	VkSampleCountFlagBits	samples = (VkSampleCountFlagBits)1;
	VkPipelineStageFlags	stage_mask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	Texture					vkimg;
	size_t					vkoffset;
	VkMemoryRequirements	mem_req;
	std_vector<int2>		mip_sizes;

	void clac_mip_sizes () {
		mip_sizes.reserve(16);

		int2 sz = size;
		int mip = 0;
		for (mip=0; mip_levels <= 0 || mip < mip_levels; ++mip) {
			mip_sizes.push_back(sz);
			if (sz.x == 1 && sz.y == 1)
				break;
			sz.x = max(sz.x / 2, 1);
			sz.y = max(sz.y / 2, 1);
		}
		mip_levels = mip+1;
	}
};

struct StaticDataUploader {
	VkCommandBuffer cmds;

	std_vector<Allocation> staging_allocs;

	VkDeviceMemory upload (VkDevice dev, VkPhysicalDevice pdev, UploadBuffer* uploads, int uploads_count);
	VkDeviceMemory upload (VkDevice dev, VkPhysicalDevice pdev, UploadTexture* uploads, int uploads_count);

	void end (VkDevice dev) {
		for (auto& a : staging_allocs) {
			vkDestroyBuffer(dev, a.buf, nullptr);
			vkFreeMemory   (dev, a.mem, nullptr);
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

// Only for debugging! Remove in real code!
void dbg_full_barrier (VkCommandBuffer cmds);

} // namespace vk
