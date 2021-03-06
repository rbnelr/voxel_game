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

VkImageView create_image_view (VkDevice device, VkImage image, VkFormat format, int layers, VkImageAspectFlags aspect) {
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = image;
	info.viewType = layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	info.format = format;
	info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	info.subresourceRange.aspectMask = aspect;
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = 1;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = (uint32_t)layers;

	VkImageView image_view;
	VK_CHECK_RESULT(vkCreateImageView(device, &info, nullptr, &image_view));

	return image_view;
}

VkDeviceMemory StaticDataUploader::upload (VkDevice dev, VkPhysicalDevice pdev, UploadBuffer* bufs, int count) {
	// Create buffers and calculate offets into memory block
	size_t size = 0;
	uint32_t mem_req_bits = (uint32_t)-1;

	for (int i=0; i<count; ++i) {
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = bufs[i].size;
		info.usage = bufs[i].usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(dev, &info, nullptr, &bufs[i].vkbuf));

		vkGetBufferMemoryRequirements(dev, bufs[i].vkbuf, &bufs[i].mem_req);

		size = align_up(size, bufs[i].mem_req.alignment);
		bufs[i].vkoffset = size;
		size += bufs[i].mem_req.size;

		mem_req_bits &= bufs[i].mem_req.memoryTypeBits;
	}

	// alloc cpu-visible staging buffer
	auto staging_buf = allocate_buffer(dev, pdev, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// alloc gpu-resident memory
	VkDeviceMemory mem;

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = size;
	alloc_info.memoryTypeIndex = find_memory_type(pdev, mem_req_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(dev, &alloc_info, nullptr, &mem));

	// upload buffers and bind buffers to memory
	void* ptr = nullptr;
	vkMapMemory(dev, staging_buf.mem, 0, size, 0, &ptr);

	for (int i=0; i<count; ++i) {
		memcpy((char*)ptr + bufs[i].vkoffset, bufs[i].data, bufs[i].size);

		vkBindBufferMemory(dev, bufs[i].vkbuf, mem, bufs[i].vkoffset);

		// upload data by copying staging buffer to gpu
		VkBufferCopy copy_region = {};
		copy_region.srcOffset = bufs[i].vkoffset;
		copy_region.dstOffset = 0;
		copy_region.size = bufs[i].size;
		vkCmdCopyBuffer(cmds, staging_buf.buf, bufs[i].vkbuf, 1, &copy_region);
	}

	vkUnmapMemory(dev, staging_buf.mem);

	staging_allocs.push_back(staging_buf);
	return mem;
}

VkDeviceMemory StaticDataUploader::upload (VkDevice dev, VkPhysicalDevice pdev, UploadTexture* texs, int count) {
	// Create textures and calculate offets into memory block
	size_t gpu_size = 0, cpu_size = 0;
	uint32_t mem_req_bits = (uint32_t)-1;

	for (int i=0; i<count; ++i) {
		auto& tex = texs[i];

		tex.clac_mip_sizes();

		VkImageCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.imageType = VK_IMAGE_TYPE_2D;
		info.extent.width = (uint32_t)tex.size.x;
		info.extent.height = (uint32_t)tex.size.y;
		info.extent.depth = 1;
		info.mipLevels = (uint32_t)tex.mip_levels;
		info.arrayLayers = (uint32_t)tex.layers;
		info.format = tex.format;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		info.usage = tex.usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.samples = tex.samples;
		info.flags = 0;

		VK_CHECK_RESULT(vkCreateImage(dev, &info, nullptr, &tex.vkimg.img));

		vkGetImageMemoryRequirements(dev, tex.vkimg.img, &tex.mem_req);

		gpu_size = align_up(gpu_size, tex.mem_req.alignment);
		tex.vkoffset = gpu_size;
		gpu_size += tex.mem_req.size;

		cpu_size += sizeof(srgba8) * tex.size.x * tex.size.y * tex.layers;

		mem_req_bits &= tex.mem_req.memoryTypeBits;
	}

	// alloc cpu-visible staging buffer
	auto staging_buf = allocate_buffer(dev, pdev, cpu_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// alloc gpu-resident memory and create a temporary target buffer for batching the upload of all textures
	VkDeviceMemory mem;

	VkMemoryAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = gpu_size;
	alloc_info.memoryTypeIndex = find_memory_type(pdev, mem_req_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(dev, &alloc_info, nullptr, &mem));

	// upload buffers, bind buffers to memory and queue copy operations
	void* ptr = nullptr;
	vkMapMemory(dev, staging_buf.mem, 0, cpu_size, 0, &ptr);
	
	size_t cpu_offs = 0;

	for (int i=0; i<count; ++i) {
		auto& tex = texs[i];

		memcpy((char*)ptr + cpu_offs, tex.data,
			sizeof(srgba8) * tex.size.x * tex.size.y * tex.layers);

		cpu_offs += sizeof(srgba8) * tex.size.x * tex.size.y * tex.layers;

		vkBindImageMemory(dev, tex.vkimg.img, mem, tex.vkoffset);

		tex.vkimg.img_view = create_image_view(dev, tex.vkimg.img, tex.format, tex.layers, VK_IMAGE_ASPECT_COLOR_BIT);
		
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = tex.vkimg.img;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = (uint32_t)tex.mip_levels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = (uint32_t)tex.layers;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(cmds,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkBufferImageCopy region = {};
		region.bufferOffset = tex.vkoffset;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.layerCount = (uint32_t)tex.layers;
		region.imageExtent = { (uint32_t)tex.size.x, (uint32_t)tex.size.y, 1 };

		vkCmdCopyBufferToImage(cmds, staging_buf.buf, tex.vkimg.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		VkImageBlit blit = {};
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.layerCount = tex.layers;
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.layerCount = tex.layers;

		barrier.subresourceRange.levelCount = 1;

		for (int mip=1; mip<tex.mip_levels; ++mip) {

			barrier.subresourceRange.baseMipLevel = mip -1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barrier);

			blit.srcOffsets[1] = { tex.mip_sizes[mip-1].x, tex.mip_sizes[mip-1].y, 1 };
			blit.srcSubresource.mipLevel = mip -1;
			blit.dstOffsets[1] = { tex.mip_sizes[mip].x, tex.mip_sizes[mip].y, 1 };
			blit.dstSubresource.mipLevel = mip;

			vkCmdBlitImage(cmds,
				tex.vkimg.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				tex.vkimg.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = tex.layout;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		barrier.subresourceRange.baseMipLevel = (uint32_t)(tex.mip_levels -1);
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = tex.layout;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(cmds,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			tex.stage_mask,
			0, 0, nullptr, 0, nullptr, 1, &barrier);

	}

	vkUnmapMemory(dev, staging_buf.mem);

	staging_allocs.push_back(staging_buf);
	return mem;
}

// Only for debugging! Remove in real code!
void dbg_full_barrier (VkCommandBuffer cmds) {
	VkMemoryBarrier mem = {};
	mem.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	mem.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
	                    VK_ACCESS_INDEX_READ_BIT |
	                    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
	                    VK_ACCESS_UNIFORM_READ_BIT |
	                    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
	                    VK_ACCESS_SHADER_READ_BIT |
	                    VK_ACCESS_SHADER_WRITE_BIT |
	                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
	                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
	                    VK_ACCESS_TRANSFER_READ_BIT |
	                    VK_ACCESS_TRANSFER_WRITE_BIT |
	                    VK_ACCESS_HOST_READ_BIT |
	                    VK_ACCESS_HOST_WRITE_BIT;
	mem.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
	                    VK_ACCESS_INDEX_READ_BIT |
	                    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
	                    VK_ACCESS_UNIFORM_READ_BIT |
	                    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
	                    VK_ACCESS_SHADER_READ_BIT |
	                    VK_ACCESS_SHADER_WRITE_BIT |
	                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
	                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
	                    VK_ACCESS_TRANSFER_READ_BIT |
	                    VK_ACCESS_TRANSFER_WRITE_BIT |
	                    VK_ACCESS_HOST_READ_BIT |
	                    VK_ACCESS_HOST_WRITE_BIT;

	vkCmdPipelineBarrier(cmds,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0, 1, &mem, 0, nullptr, 0, nullptr);
}

} // namespace vk
