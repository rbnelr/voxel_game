#pragma once
#include "vulkan_window.hpp"

namespace vk {

struct Screenshots {
	bool take_screenshot = false;
	bool include_ui = false;

	int2 img_size;
	VkImage readback_img;
	VkDeviceMemory readback_img_mem;

	void imgui () {
		take_screenshot = ImGui::Button("Screenshot [F3]") || take_screenshot;
		ImGui::SameLine();
		ImGui::Checkbox("With UI", &include_ui);
	}

	// call at frame start
	void begin (Input& I) {
		take_screenshot = I.buttons[KEY_F3].went_down || take_screenshot;
	}

	void screenshot_swapchain_img (VulkanWindowContext& ctx, VkCommandBuffer cmds) {
		assert(take_screenshot);

		VkImage swapc_image = ctx.swap_chain.images[ctx.image_index].image;
		int2 img_size = ctx.wnd_size;

		bool supportsBlit = true;

		// Check blit support for source and destination
		VkFormatProperties format_props;
		
		// Check if the device supports blitting from optimal images (the swapchain images are in optimal format)
		vkGetPhysicalDeviceFormatProperties(ctx.pdev, ctx.swap_chain.format.format, &format_props);
		if (!(format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT)) {
			//clog(ERROR, "Device does not support blitting from optimal tiled images, using copy instead of blit!");
			supportsBlit = false;
		}

		// Check if the device supports blitting to linear images
		vkGetPhysicalDeviceFormatProperties(ctx.pdev, VK_FORMAT_R8G8B8A8_SRGB, &format_props);
		if (!(format_props.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
			//clog(ERROR, "Device does not support blitting to linear tiled images, using copy instead of blit!");
			supportsBlit = false;
		}

		VkMemoryPropertyFlags mem_props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		readback_img = create_image(ctx.dev, ctx.pdev, img_size, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_LINEAR,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_UNDEFINED, mem_props, &readback_img_mem, 1, (VkSampleCountFlagBits)1);
		this->img_size = img_size;

		{
			VkImageMemoryBarrier img = {};
			img.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			img.srcAccessMask = 0;
			img.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			img.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			img.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			img.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			img.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			img.image = readback_img;
			img.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			img.subresourceRange.baseMipLevel = 0;
			img.subresourceRange.levelCount = 1;
			img.subresourceRange.baseArrayLayer = 0;
			img.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &img);
		}
		{
			VkImageMemoryBarrier img = {};
			img.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			img.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			img.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			img.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			img.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			img.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			img.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			img.image = swapc_image;
			img.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			img.subresourceRange.baseMipLevel = 0;
			img.subresourceRange.levelCount = 1;
			img.subresourceRange.baseArrayLayer = 0;
			img.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &img);
		}

		if (supportsBlit) {
			VkImageBlit region = {};
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.srcOffsets[1] = { img_size.x, img_size.y, 1 };
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.dstSubresource.layerCount = 1;
			region.dstOffsets[1] = { img_size.x, img_size.y, 1 };

			// Issue the blit command
			vkCmdBlitImage(cmds,
				swapc_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				readback_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &region, VK_FILTER_NEAREST);

		} else {
			// Otherwise use image copy (requires us to manually flip components)
			VkImageCopy region = {};
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.layerCount = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.dstSubresource.layerCount = 1;
			region.extent.width = (uint32_t)img_size.x;
			region.extent.height = (uint32_t)img_size.y;
			region.extent.depth = 1;
		
			// Issue the copy command
			vkCmdCopyImage(cmds,
				swapc_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				readback_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &region);
		}

		{
			VkImageMemoryBarrier img = {};
			img.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			img.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			img.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT; // safe?
			img.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			img.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			img.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			img.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			img.image = swapc_image;
			img.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			img.subresourceRange.baseMipLevel = 0;
			img.subresourceRange.levelCount = 1;
			img.subresourceRange.baseArrayLayer = 0;
			img.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // safe?
				0, 0, nullptr, 0, nullptr, 1, &img);
		}
		{
			VkImageMemoryBarrier img = {};
			img.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			img.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			img.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			img.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			img.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			img.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			img.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			img.image = readback_img;
			img.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			img.subresourceRange.baseMipLevel = 0;
			img.subresourceRange.levelCount = 1;
			img.subresourceRange.baseArrayLayer = 0;
			img.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // safe?
				0, 0, nullptr, 0, nullptr, 1, &img);
		}
	}

	// call after cmd buf submit
	void end (VulkanWindowContext& ctx) {
		if (take_screenshot) {
			vkQueueWaitIdle(ctx.queues.graphics_queue);

			// Get layout of the image (including row pitch)
			VkImageSubresource subresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
			VkSubresourceLayout subresource_layout;
			vkGetImageSubresourceLayout(ctx.dev, readback_img, &subresource, &subresource_layout);

			void* raw_data;
			vkMapMemory(ctx.dev, readback_img_mem, 0, VK_WHOLE_SIZE, 0, &raw_data);
			char const* data = (char const*)raw_data + subresource_layout.offset;
			char* data_copy = nullptr;
			size_t pitch = subresource_layout.rowPitch;

			if (ctx.swap_chain.format.format == VK_FORMAT_B8G8R8A8_SRGB) {
				data_copy = (char*)malloc(4ull * img_size.x * img_size.y);

				for (int y=0; y<img_size.y; ++y) {
					for (int x=0; x<img_size.x; ++x) {
						size_t a = y * pitch + x * 4;
						size_t b = y * subresource_layout.rowPitch + x * 4;
						data_copy[a + 0] = data[b + 2];
						data_copy[a + 1] = data[b + 1];
						data_copy[a + 2] = data[b + 0];
						data_copy[a + 3] = data[b + 3];
					}
				}

				data = data_copy; // let stbi_write_png read swizzled image data
				pitch = 4ull * img_size.x;
			}

			if (stbi_write_png("../screenshot.png", img_size.x, img_size.y, 4, data, (int)subresource_layout.rowPitch)) {
				clog(INFO, "Screenshot taken!");
			} else {
				clog(WARNING, "Screenshot error!");
			}

			free(data_copy);

			vkUnmapMemory(ctx.dev, readback_img_mem);

			vkFreeMemory(ctx.dev, readback_img_mem, nullptr);
			vkDestroyImage(ctx.dev, readback_img, nullptr);
		}
		take_screenshot = false;
	}
};
} // namspace vk
