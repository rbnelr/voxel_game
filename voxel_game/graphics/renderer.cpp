#include "renderer.hpp"
#include "dear_imgui.hpp"
#include "vulkan_lib.hpp"
#include "glfw_window.hpp"

std::unique_ptr<Vulkan> vulkan = nullptr;

namespace vk {
	VKAPI_ATTR VkBool32 VKAPI_CALL Vulkan::debug_callback (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		fprintf(stderr, "[Vulkan] %s\n", pCallbackData->pMessage);
		clog(ERROR, "[Vulkan] %s\n", pCallbackData->pMessage);

		return VK_FALSE;
	}

	VkCommandBuffer Vulkan::begin_one_time_commands () {
		VkCommandBuffer buf;

		VkCommandBufferAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		info.commandPool = one_time_command_pool;
		info.commandBufferCount = 1;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &info, &buf));
		
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(buf, &begin_info);

		return buf;
	}

	void Vulkan::end_one_time_commands (VkCommandBuffer buf) {
		
		vkEndCommandBuffer(buf);

		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &buf;
		vkQueueSubmit(queues.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

		vkQueueWaitIdle(queues.graphics_queue);

		vkFreeCommandBuffers(device, one_time_command_pool, 1, &buf);
	}

	//// Swap chain creation
	void Vulkan::create_swap_chain (int2 window_size, int image_count) {

		auto support = query_swap_chain_support(physical_device, surface);

		auto format			= choose_swap_surface_format(support.formats);
		auto present_mode	= choose_swap_present_mode(support.present_modes);
		auto extent			= choose_swap_extent(support.caps, window_size);

		image_count = clamp(image_count, support.caps.minImageCount, support.caps.maxImageCount > 0 ? support.caps.maxImageCount : INT_MAX);

		VkSwapchainCreateInfoKHR info = {};
		info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		info.surface = surface;
		info.minImageCount = image_count;
		info.imageFormat = format.format;
		info.imageColorSpace = format.colorSpace;
		info.imageExtent = extent;
		info.imageArrayLayers = 1;
		info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		uint32_t q_family_indices[] = { queues.families.graphics_family, queues.families.present_family };
		if (queues.families.graphics_family != queues.families.present_family) {
			info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			info.queueFamilyIndexCount = 2;
			info.pQueueFamilyIndices = q_family_indices;
		} else {
			info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		info.preTransform = support.caps.currentTransform;
		info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		info.presentMode = present_mode;
		info.clipped = VK_TRUE;
		info.oldSwapchain = VK_NULL_HANDLE;

		VK_CHECK_RESULT(vkCreateSwapchainKHR(device, &info, nullptr, &swap_chain.swap_chain));

		auto images = get_vector<VkImage>(vkGetSwapchainImagesKHR, device, swap_chain.swap_chain);
		image_count = (int)images.size();

		swap_chain.images.resize(image_count);
		for (int i=0; i<image_count; ++i) {
			auto& img = swap_chain.images[i];

			img.image = images[i];
			img.image_view = create_image_view(device, img.image, format.format, VK_IMAGE_ASPECT_COLOR_BIT);

			VkImageView attachments[] = {
				color_buffer.image_view,
				depth_buffer.image_view,
				img.image_view,
			};

			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = render_pass;
			info.attachmentCount = ARRLEN(attachments);
			info.pAttachments = attachments;
			info.width  = extent.width ;
			info.height = extent.height;
			info.layers = 1;

			VK_CHECK_RESULT(vkCreateFramebuffer(device, &info, nullptr, &img.framebuffer));
		}

		swap_chain.format = format;
		swap_chain.extent = extent;
	}

	//// Renderpass creation
	RenderBuffer create_render_buffer (VkDevice device, VkPhysicalDevice pdev, int2 size, VkFormat format, VkImageUsageFlags usage,
			VkImageLayout initial_layout, VkMemoryPropertyFlags props, VkImageAspectFlags aspect, VkSampleCountFlagBits samples=VK_SAMPLE_COUNT_1_BIT) {
		RenderBuffer buf;
		buf.image = create_image(device, pdev, size, format, VK_IMAGE_TILING_OPTIMAL, usage, initial_layout, props, &buf.memory, 1, samples);
		buf.image_view = create_image_view(device, buf.image, format, aspect);
		return buf;
	}

	VkRenderPass create_renderpass (VkDevice device, VkFormat color_format, VkFormat depth_format, VkSampleCountFlagBits msaa) {
		VkAttachmentDescription color_attachment = {};
		color_attachment.format = color_format;
		color_attachment.samples = msaa;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depth_attachment = {};
		depth_attachment.format = depth_format;
		depth_attachment.samples = msaa;
		depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription color_attachment_resolve = {};
		color_attachment_resolve.format = color_format;
		color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference color_attachment_ref = {};
		color_attachment_ref.attachment = 0;
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_attachment_ref = {};
		depth_attachment_ref.attachment = 1;
		depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference color_attachment_resolve_ref = {};
		color_attachment_resolve_ref.attachment = 2;
		color_attachment_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment_ref;
		subpass.pResolveAttachments = &color_attachment_resolve_ref;
		subpass.pDepthStencilAttachment = &depth_attachment_ref;

		VkSubpassDependency depen = {};
		depen.srcSubpass = VK_SUBPASS_EXTERNAL;
		depen.dstSubpass = 0;
		depen.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		depen.srcAccessMask = 0;
		depen.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		depen.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription attachments[] = { color_attachment, depth_attachment, color_attachment_resolve };

		VkRenderPassCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		info.attachmentCount = ARRLEN(attachments);
		info.pAttachments = attachments;
		info.subpassCount = 1;
		info.pSubpasses = &subpass;
		info.dependencyCount = 1;
		info.pDependencies = &depen;

		VkRenderPass renderpass;

		VK_CHECK_RESULT(vkCreateRenderPass(device, &info, nullptr, &renderpass));

		return renderpass;
	}

	//// Per-Frame data creation
	std::vector<FrameData> create_frame_data (int in_flight_count, VkDevice device, uint32_t queue_family) {
		std::vector<FrameData> frame_data (in_flight_count);

		for (auto& frame : frame_data) {
			VkCommandPoolCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			info.queueFamilyIndex = queue_family;
			info.flags = 0;
			VK_CHECK_RESULT(vkCreateCommandPool(device, &info, nullptr, &frame.command_pool));

			VkCommandBufferAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			alloc_info.commandPool = frame.command_pool;
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			alloc_info.commandBufferCount  = 1;
			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &alloc_info, &frame.command_buffer));

			VkSemaphoreCreateInfo semaphore_info = {};
			semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fence_info = {};
			fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphore_info, nullptr, &frame.image_available_semaphore));
			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphore_info, nullptr, &frame.render_finished_semaphore));
			VK_CHECK_RESULT(vkCreateFence(device, &fence_info, nullptr, &frame.fence));
		}

		return frame_data;
	}

	//// Framebuffer creation
	void Vulkan::create_framebuffers (int2 size, VkFormat color_format) {
		color_buffer = create_render_buffer(device, physical_device, size, color_format,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT, max_msaa_samples);

		depth_buffer = create_render_buffer(device, physical_device, size, depth_format,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_IMAGE_ASPECT_DEPTH_BIT, max_msaa_samples);

	}
	void Vulkan::cleanup_framebuffers () {
		vkDestroyImageView(device, color_buffer.image_view, nullptr);
		vkDestroyImage(device, color_buffer.image, nullptr);
		vkFreeMemory(device, color_buffer.memory, nullptr);

		vkDestroyImageView(device, depth_buffer.image_view, nullptr);
		vkDestroyImage(device, depth_buffer.image, nullptr);
		vkFreeMemory(device, depth_buffer.memory, nullptr);
	}

	//// Descriptor pool creation
	VkDescriptorPool create_descriptor_pool (VkDevice device, int swap_chain_size) {
		VkDescriptorPool pool;

		VkDescriptorPoolSize pool_sizes[1] = {};
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = (uint32_t)swap_chain_size; // for imgui

		VkDescriptorPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		info.poolSizeCount = ARRLEN(pool_sizes);
		info.pPoolSizes = pool_sizes;
		info.maxSets = (uint32_t)swap_chain_size; // for imgui

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &info, nullptr, &pool));

		return pool;
	}

	//// 
	void Vulkan::cleanup_swap_chain () {
		if (swap_chain.swap_chain != VK_NULL_HANDLE) {
			cleanup_framebuffers();

			for (auto img : swap_chain.images) {
				vkDestroyImageView(device, img.image_view, nullptr);
				vkDestroyFramebuffer(device, img.framebuffer, nullptr);
			}

			//vkDestroyPipeline(vk_device, vk_pipeline, nullptr);
			//vkDestroyPipelineLayout(vk_device, vk_pipeline_layout, nullptr);

			vkDestroySwapchainKHR(device, swap_chain.swap_chain, nullptr);
		}
	}

	Vulkan::Vulkan (GLFWwindow* glfw_window, char const* app_name) {
		instance = create_instance(app_name, Vulkan::debug_callback, &enabled_layers);

		debug_messenger = enable_validation_layers ? create_debug_utils_messenger_ext(instance, Vulkan::debug_callback) : VK_NULL_HANDLE;
		
		auto res = glfwCreateWindowSurface(instance, glfw_window, nullptr, &surface);
		assert(res == VK_SUCCESS);

		physical_device = select_device(instance, surface, &queues.families, &color_format);
		device = create_logical_device(physical_device, enabled_layers, &queues);

		depth_format = find_depth_format(physical_device);
		max_msaa_samples = get_max_usable_multisample_count(physical_device);

		frame_data = create_frame_data(FRAMES_IN_FLIGHT, device, queues.families.graphics_family);

		render_pass = create_renderpass(device, color_format, depth_format, max_msaa_samples);
		one_time_command_pool = create_one_time_command_pool(device, queues.families.graphics_family);
		descriptor_pool = create_descriptor_pool(device, SWAP_CHAIN_SIZE);

		static_memory.mem_size = 1024 * 1024 * 128;
		static_memory.memory = alloc_memory(device, physical_device, static_memory.mem_size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		recreate_swap_chain();
	}

	Vulkan::~Vulkan () {
		vkQueueWaitIdle(queues.graphics_queue);

		vkFreeMemory(device, static_memory.memory, nullptr);

		vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
		vkDestroyRenderPass(device, render_pass, nullptr);
		vkDestroyCommandPool(device, one_time_command_pool, nullptr);

		for (auto& frame : frame_data) {
			vkDestroyCommandPool(device, frame.command_pool, nullptr);

			vkDestroySemaphore(device, frame.image_available_semaphore, nullptr);
			vkDestroySemaphore(device, frame.render_finished_semaphore, nullptr);
			vkDestroyFence(device, frame.fence, nullptr);
		}

		cleanup_swap_chain();

		vkDestroyDevice(device, nullptr);

		vkDestroySurfaceKHR(instance, surface, nullptr);

		if (enable_validation_layers)
			destroy_debug_utils_messenger_ext(instance, debug_messenger);

		vkDestroyInstance(instance, nullptr);
	}

	VkBuffer MemoryPool::alloc (VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage) {
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = size;
		info.usage = usage;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkBuffer buffer;

		VK_CHECK_RESULT(vkCreateBuffer(device, &info, nullptr, &buffer));
		
		VkMemoryRequirements mem_req;
		vkGetBufferMemoryRequirements(device, buffer, &mem_req);

		auto align = mem_req.alignment;
		cur_ptr += mem_req.alignment - (cur_ptr & (mem_req.alignment-1));

		if (cur_ptr >= mem_size)
			throw std::runtime_error("Out of memory");

		vkBindBufferMemory(device, buffer, memory, cur_ptr);

		return buffer;
	}

	void Vulkan::recreate_swap_chain () {
		vkQueueWaitIdle(queues.graphics_queue);

		glfwGetFramebufferSize(window, &cur_size.x, &cur_size.y);
		
		cleanup_swap_chain();

		//
		create_framebuffers(cur_size, color_format);
		create_swap_chain(cur_size, SWAP_CHAIN_SIZE);
	}

	bool Vulkan::frame_start () {
		auto frame = frame_data[cur_frame];

		vkWaitForFences(device, 1, &frame.fence, VK_TRUE, UINT64_MAX);

		vkResetFences(device, 1, &frame.fence);

		vkResetCommandPool(device, frame.command_pool, 0);

		// Aquire image
		auto res = vkAcquireNextImageKHR(device, swap_chain.swap_chain, UINT64_MAX, frame.image_available_semaphore, VK_NULL_HANDLE, &cur_image_index);
		if (res == VK_ERROR_OUT_OF_DATE_KHR) {
			recreate_swap_chain();
			return false;
		}
		assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);

		return true;
	}

	void Vulkan::render_begin () {
		auto buf = frame_data[cur_frame].command_buffer;
		auto framebuffer = swap_chain.images[cur_image_index].framebuffer;

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = nullptr;
		VK_CHECK_RESULT(vkBeginCommandBuffer(buf, &begin_info));

		VkClearValue clear_vales[2] = {};
		clear_vales[0].color = { .01f, .011f, .012f, 1 };
		clear_vales[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = render_pass;
		render_pass_info.framebuffer = framebuffer;
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = swap_chain.extent;
		render_pass_info.clearValueCount = ARRLEN(clear_vales);
		render_pass_info.pClearValues = clear_vales;
		vkCmdBeginRenderPass(buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

		//vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);
		//
		//VkBuffer vertex_bufs[] = { vk_vertex_buffer };
		//VkDeviceSize offsets[] = { 0 };
		//vkCmdBindVertexBuffers(buf, 0, 1, vertex_bufs, offsets);
		//
		//vkCmdBindIndexBuffer(buf, vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);
		//
		//vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout, 0, 1, &vk_descriptor_sets[cur_frame], 0, nullptr);
		//
		//vkCmdDrawIndexed(buf, (uint32_t)model.indices.size(), 1, 0, 0, 0);

	}

	void Vulkan::render_end () {

	}

	void Vulkan::frame_end () {
		auto buf = frame_data[cur_frame].command_buffer;

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), buf);

		vkCmdEndRenderPass(buf);

		VK_CHECK_RESULT(vkEndCommandBuffer(buf));

		////
		auto frame = frame_data[cur_frame];

		VkSemaphore wait_semaphores[] = { frame.image_available_semaphore };
		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSemaphore signal_semaphores[] = { frame.render_finished_semaphore };

		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &frame.command_buffer;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;
		VK_CHECK_RESULT(vkQueueSubmit(queues.graphics_queue, 1, &submit_info, frame.fence));

		// Present image
		VkSwapchainKHR swap_chains[] = { swap_chain.swap_chain };

		VkPresentInfoKHR present_info = {};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swap_chains;
		present_info.pImageIndices = &cur_image_index;
		present_info.pResults = nullptr;

		auto res = vkQueuePresentKHR(queues.present_queue, &present_info);
		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
			recreate_swap_chain();
		} else {
			VK_CHECK_RESULT(res);
		}

		cur_frame = (cur_frame + 1) % FRAMES_IN_FLIGHT;
	}
}
