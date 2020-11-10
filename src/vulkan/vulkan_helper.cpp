#include "vulkan_helper.hpp"
#include "Tracy.hpp"
#include "Tracy.hpp"

namespace vk {

//// Resource creation
VkSampleCountFlagBits get_max_usable_multisample_count (VkPhysicalDevice device) {
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(device, &props);

	VkSampleCountFlags count_flags = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;

	static constexpr VkSampleCountFlagBits counts[] = {
		VK_SAMPLE_COUNT_64_BIT,
		VK_SAMPLE_COUNT_32_BIT,
		VK_SAMPLE_COUNT_16_BIT,
		VK_SAMPLE_COUNT_8_BIT , 
		VK_SAMPLE_COUNT_4_BIT , 
		VK_SAMPLE_COUNT_2_BIT ,
	};
	for (auto count : counts) {
		if (count_flags & count)
			return count;
	}

	return VK_SAMPLE_COUNT_1_BIT;
}

uint32_t find_memory_type (VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties mem_props;
	vkGetPhysicalDeviceMemoryProperties(device, &mem_props);

	for (uint32_t i=0; i<mem_props.memoryTypeCount; ++i) {
		if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("[Vulkan] Fatal error: No suitable memory type found");
}

VkFormat find_supported_format (VkPhysicalDevice device, std::initializer_list<VkFormat> const& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("[Vulkan] Fatal error: No suitable format found");
}

VkFormat find_depth_format (VkPhysicalDevice device) {
	return find_supported_format(device,
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

VkCommandPool create_one_time_command_pool (VkDevice device, uint32_t queue_family) {
	VkCommandPool pool;

	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.queueFamilyIndex = queue_family;
	info.flags = 0;
	VK_CHECK_RESULT(vkCreateCommandPool(device, &info, nullptr, &pool));

	return pool;
}

VkDeviceMemory alloc_memory (VkDevice device, VkPhysicalDevice pdevice,
		VkDeviceSize size, VkMemoryPropertyFlags props) {

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = size;
	alloc_info.memoryTypeIndex = find_memory_type(pdevice, -1, props);

	VkDeviceMemory memory;
	VK_CHECK_RESULT(vkAllocateMemory(device, &alloc_info, nullptr, &memory));

	return memory;
}

Allocation allocate_buffer (VkDevice device, VkPhysicalDevice pdev,
		VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props) {
	ZoneScoped;

	Allocation a;

	VkBufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	info.size = size;
	info.usage = usage;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK_RESULT(vkCreateBuffer(device, &info, nullptr, &a.buf));

	VkMemoryRequirements mem_req;
	vkGetBufferMemoryRequirements(device, a.buf, &mem_req);

	{
		ZoneScopedN("vkAllocateMemory");
		
		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = find_memory_type(pdev, mem_req.memoryTypeBits, props);

		VK_CHECK_RESULT(vkAllocateMemory(device, &alloc_info, nullptr, &a.mem));
	}

	vkBindBufferMemory(device, a.buf, a.mem, 0);

	return a;
}

VkImage create_image (VkDevice device, VkPhysicalDevice pdev,
		int2 size, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
		VkImageLayout initial_layout, VkMemoryPropertyFlags props,
		VkDeviceMemory* out_image_memory, int mip_levels, VkSampleCountFlagBits samples) {

	VkImageCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.extent.width = (uint32_t)size.x;
	info.extent.height = (uint32_t)size.y;
	info.extent.depth = 1;
	info.mipLevels = (uint32_t)mip_levels;
	info.arrayLayers = 1;
	info.format = format;
	info.tiling = tiling;
	info.initialLayout = initial_layout;
	info.usage = usage;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.samples = samples;
	info.flags = 0;

	VkImage image;
	VK_CHECK_RESULT(vkCreateImage(device, &info, nullptr, &image));

	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(device, image, &mem_req);

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type(pdev, mem_req.memoryTypeBits, props);

	VkDeviceMemory image_memory;
	VK_CHECK_RESULT(vkAllocateMemory(device, &alloc_info, nullptr, &image_memory));

	vkBindImageMemory(device, image, image_memory, 0);

	*out_image_memory = image_memory;
	return image;
}

VkImageView create_image_view (VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect) {
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = image;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.format = format;
	info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.subresourceRange.aspectMask = aspect;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;

	VkImageView image_view;
	VK_CHECK_RESULT(vkCreateImageView(device, &info, nullptr, &image_view));

	return image_view;
}

} // namespace vk
