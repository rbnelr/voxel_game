#pragma once
#include "vulkan_helper.hpp"
#include "vulkan_shaders.hpp"
#include "GLFW/glfw3.h"
#include "assert.h"
#include "imgui/dear_imgui.hpp"

namespace vk {
	inline std::vector<char const*> glfwGetRequiredInstanceExtensions_vec () {
		uint32_t count = 0;
		auto names = glfwGetRequiredInstanceExtensions(&count);
		return std::vector<char const*>(names, names + count);
	}

	struct Renderer {
		VkInstance					instance;
		VkSurfaceKHR				surface;
		VkPhysicalDevice			physical_device;
		VkDevice					device;
		Queues						queues;

		ShaderManager				shaders;

		std::vector<char const*>	enabled_layers;
		VkFormat					color_format;
		VkFormat					depth_format;

		int							max_msaa_samples;
		int							msaa = 1;

	#ifdef VK_VALIDATION_LAYERS
		VkDebugUtilsMessengerEXT	debug_messenger;
	#endif

		int2						cur_size;
		uint32_t					cur_image_index;
		int							cur_frame = 0;

		//MemoryPool					static_memory;
		VkDescriptorPool			descriptor_pool;
		VkCommandPool				one_time_command_pool;

		SwapChain					swap_chain;

		struct FrameData {
			VkCommandPool				command_pool;
			VkCommandBuffer				command_buffer;

			VkSemaphore					image_available_semaphore;
			VkSemaphore					render_finished_semaphore;
			VkFence						fence;

			//VkDescriptorSet				common_ubos_descriptor_set;
			//VkBuffer					common_ubos_buffer;
		};
		FrameData					frame_data[FRAMES_IN_FLIGHT];

		// Passes
		VkRenderPass				render_pass;
		//RenderBuffer				color_buffer;
		RenderBuffer				depth_buffer;

		VkPipelineLayout			pipeline_layout;
		VkPipeline					pipeline;

		struct Vertex {
			float2	pos;
			lrgb	col;
		
			static void attributes (VertexAttributes& a) {
				int loc = 0;
				a.init(sizeof(Vertex));
				a.add(loc++, "pos", AttribFormat::FLOAT2, offsetof(Vertex, pos));
				a.add(loc++, "col", AttribFormat::FLOAT3, offsetof(Vertex, col));
			}
		};
		Vertex gradient_vertices[4] = {
			{ float2(-0.8f, -0.8f), lrgb(0,0,0) },
			{ float2(+0.8f, -0.8f), lrgb(1,1,1) },
			{ float2(+0.8f, +0.8f), lrgb(1,1,1) },
			{ float2(-0.8f, +0.8f), lrgb(0,0,0) },
		};
		uint16_t gradient_indices[6] = {
			1,0,2, 2,0,3,
		};

		struct UploadBuffer {
			void* data;
			size_t size;
			VkBufferUsageFlags usage;

			VkBuffer vkbuf;
			VkMemoryRequirements mem_req;
			size_t vkoffset;
		};
		UploadBuffer bufs[2] = {
			{ gradient_vertices, sizeof(gradient_vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT },
			{ gradient_indices , sizeof(gradient_indices) , VK_BUFFER_USAGE_INDEX_BUFFER_BIT },
		};

		VkDeviceMemory mesh_mem;

		bool frame_begin (GLFWwindow* window) {
			auto frame = frame_data[cur_frame];

			vkWaitForFences(device, 1, &frame.fence, VK_TRUE, UINT64_MAX);

			vkResetFences(device, 1, &frame.fence);

			vkResetCommandPool(device, frame.command_pool, 0);

			// Aquire image
			auto res = vkAcquireNextImageKHR(device, swap_chain.swap_chain, UINT64_MAX, frame.image_available_semaphore, VK_NULL_HANDLE, &cur_image_index);
			if (res == VK_ERROR_OUT_OF_DATE_KHR) {
				recreate_swap_chain(window, msaa);
				return false;
			}
			assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);

			return true;
		}

		void render_frame (GLFWwindow* window, DearImgui& imgui) {

			if (!frame_begin(window))
				return;

			auto buf = frame_data[cur_frame].command_buffer;
			auto framebuffer = swap_chain.images[cur_image_index].framebuffer;

			{
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
			}

			{
				VkViewport viewport = {};
				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width  = (float)cur_size.x;
				viewport.height = (float)cur_size.y;
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;
			
				VkRect2D scissor = {};
				scissor.offset = { 0, 0 };
				scissor.extent = swap_chain.extent;
			
				vkCmdSetViewport(buf, 0, 1, &viewport);
				vkCmdSetScissor(buf, 0, 1, &scissor);
			}
			
			if (pipeline) {
				vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			
				VkBuffer vertex_bufs[] = { bufs[0].vkbuf };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(buf, 0, 1, vertex_bufs, offsets);

				vkCmdBindIndexBuffer(buf, bufs[1].vkbuf, 0, VK_INDEX_TYPE_UINT16);

				//vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout, 0, 1, &vk_descriptor_sets[cur_frame], 0, nullptr);

				vkCmdDrawIndexed(buf, (uint32_t)ARRLEN(gradient_indices), 1, 0, 0, 0);
			}

			{
				imgui.draw(buf);
			}

			{
				vkCmdEndRenderPass(buf);

				VK_CHECK_RESULT(vkEndCommandBuffer(buf));

				submit(window, buf);
			}
		}

		void submit (GLFWwindow* window, VkCommandBuffer buf) {
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
			VkPresentInfoKHR present_info = {};
			present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			present_info.waitSemaphoreCount = 1;
			present_info.pWaitSemaphores = signal_semaphores;
			present_info.swapchainCount = 1;
			present_info.pSwapchains = &swap_chain.swap_chain;
			present_info.pImageIndices = &cur_image_index;
			present_info.pResults = nullptr;

			auto res = vkQueuePresentKHR(queues.present_queue, &present_info);
			if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
				recreate_swap_chain(window, msaa);
			} else {
				VK_CHECK_RESULT(res);
			}

			cur_frame = (cur_frame + 1) % FRAMES_IN_FLIGHT;
		}

		Renderer (char const* app_name, GLFWwindow* window) {
			auto request_extensions = glfwGetRequiredInstanceExtensions_vec();

			instance = create_instance(app_name, debug_callback, request_extensions, &enabled_layers);

		#ifdef VK_VALIDATION_LAYERS
			debug_messenger = create_debug_utils_messenger_ext(instance, debug_callback);
		#endif

			auto res = glfwCreateWindowSurface(instance, window, nullptr, &surface);
			assert(res == VK_SUCCESS);

			physical_device = select_device(instance, surface, &queues.families, &color_format);
			device = create_logical_device(physical_device, enabled_layers, &queues);

			depth_format = find_depth_format(physical_device);
			max_msaa_samples = get_max_usable_multisample_count(physical_device);
			msaa = min(msaa, max_msaa_samples);

			create_frame_data();

			//static_memory.mem_size = 128 * 1024*1024;
			//static_memory.memory = alloc_memory(device, physical_device, static_memory.mem_size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			
			one_time_command_pool = create_one_time_command_pool(device, queues.families.graphics_family);
			descriptor_pool = create_descriptor_pool(device, SWAP_CHAIN_SIZE);

			render_pass = create_renderpass(device, color_format, depth_format, msaa);
			shaders.init(device);
			create_pipeline_layout();
			create_pipeline(msaa, shaders.get(device, "test"));

			upload_meshes();

			recreate_swap_chain(window, msaa);
		}
		~Renderer () {
			vkQueueWaitIdle(queues.graphics_queue);

			//vkFreeMemory(device, static_memory.memory, nullptr);

			destroy_meshes();

			destroy_pipeline();
			destroy_pipeline_layout();
			shaders.destroy(device);

			vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
			vkDestroyRenderPass(device, render_pass, nullptr);
			vkDestroyCommandPool(device, one_time_command_pool, nullptr);

			for (auto& frame : frame_data) {
				vkDestroyCommandPool(device, frame.command_pool, nullptr);

				vkDestroySemaphore(device, frame.image_available_semaphore, nullptr);
				vkDestroySemaphore(device, frame.render_finished_semaphore, nullptr);
				vkDestroyFence(device, frame.fence, nullptr);
			}

			destroy_swap_chain();

			vkDestroyDevice(device, nullptr);

			vkDestroySurfaceKHR(instance, surface, nullptr);

		#ifdef VK_VALIDATION_LAYERS
			destroy_debug_utils_messenger_ext(instance, debug_messenger);
		#endif

			vkDestroyInstance(instance, nullptr);
		}

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

		//// Create swapchain
		void create_swap_chain (int2 window_size, int image_count, int msaa) {

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

				std::vector<VkImageView> attachments;
				if (msaa > 1) {
					//attachments = {
					//	color_buffer.image_view,
					//	depth_buffer.image_view,
					//	img.image_view,
					//};
				} else {
					attachments = {
						img.image_view,
						depth_buffer.image_view,
					};
				}

				VkFramebufferCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				info.renderPass = render_pass;
				info.attachmentCount = (uint32_t)attachments.size();
				info.pAttachments = attachments.data();
				info.width  = extent.width ;
				info.height = extent.height;
				info.layers = 1;

				VK_CHECK_RESULT(vkCreateFramebuffer(device, &info, nullptr, &img.framebuffer));
			}

			swap_chain.format = format;
			swap_chain.extent = extent;
		}

		void recreate_swap_chain (GLFWwindow* window, int msaa) {
			vkQueueWaitIdle(queues.graphics_queue);

			glfwGetFramebufferSize(window, &cur_size.x, &cur_size.y);

			destroy_swap_chain();

			//
			create_framebuffers(cur_size, color_format, msaa);
			create_swap_chain(cur_size, SWAP_CHAIN_SIZE, msaa);
		}
		void destroy_swap_chain () {
			if (swap_chain.swap_chain != VK_NULL_HANDLE) {
				destroy_framebuffers();

				for (auto img : swap_chain.images) {
					vkDestroyImageView(device, img.image_view, nullptr);
					vkDestroyFramebuffer(device, img.framebuffer, nullptr);
				}

				//vkDestroyPipeline(vk_device, vk_pipeline, nullptr);
				//vkDestroyPipelineLayout(vk_device, vk_pipeline_layout, nullptr);

				vkDestroySwapchainKHR(device, swap_chain.swap_chain, nullptr);
			}
		}

		//// Create per-frame data
		void create_frame_data () {
			for (auto& frame : frame_data) {
				VkCommandPoolCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				info.queueFamilyIndex = queues.families.graphics_family;
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
		}

		//// Framebuffer creation
		void create_framebuffers (int2 size, VkFormat color_format, int msaa) {
			//color_buffer = create_render_buffer(size, color_format,
			//	VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			//	VK_IMAGE_ASPECT_COLOR_BIT, msaa);

			depth_buffer = create_render_buffer(size, depth_format,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				VK_IMAGE_ASPECT_DEPTH_BIT, msaa);
		}
		void destroy_framebuffers () {
			//vkDestroyImageView(device, color_buffer.image_view, nullptr);
			//vkDestroyImage(device, color_buffer.image, nullptr);
			//vkFreeMemory(device, color_buffer.memory, nullptr);

			vkDestroyImageView(device, depth_buffer.image_view, nullptr);
			vkDestroyImage(device, depth_buffer.image, nullptr);
			vkFreeMemory(device, depth_buffer.memory, nullptr);
		}

		//// Renderpass creation
		RenderBuffer create_render_buffer (int2 size, VkFormat format, VkImageUsageFlags usage, VkImageLayout initial_layout, VkMemoryPropertyFlags props, VkImageAspectFlags aspect, int msaa) {
			RenderBuffer buf;
			buf.image = create_image(device, physical_device, size, format, VK_IMAGE_TILING_OPTIMAL, usage, initial_layout, props, &buf.memory, 1, (VkSampleCountFlagBits)msaa);
			buf.image_view = create_image_view(device, buf.image, format, aspect);
			return buf;
		}

		VkRenderPass create_renderpass (VkDevice device, VkFormat color_format, VkFormat depth_format, int msaa) {
			VkAttachmentDescription color_attachment = {};
			color_attachment.format = color_format;
			color_attachment.samples = (VkSampleCountFlagBits)msaa;
			color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			color_attachment.finalLayout = msaa > 1 ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentDescription depth_attachment = {};
			depth_attachment.format = depth_format;
			depth_attachment.samples = (VkSampleCountFlagBits)msaa;
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
			subpass.pResolveAttachments = msaa > 1 ? &color_attachment_resolve_ref : nullptr;
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
			info.attachmentCount = msaa > 1 ? 3 : 2;
			info.pAttachments = attachments;
			info.subpassCount = 1;
			info.pSubpasses = &subpass;
			info.dependencyCount = 1;
			info.pDependencies = &depen;

			VkRenderPass renderpass;

			VK_CHECK_RESULT(vkCreateRenderPass(device, &info, nullptr, &renderpass));

			return renderpass;
		}

		void create_pipeline_layout () {
			VkPipelineLayoutCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			info.setLayoutCount = 0;
			info.pSetLayouts = nullptr;
			info.pushConstantRangeCount = 0;
			info.pPushConstantRanges = nullptr;

			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &info, nullptr, &pipeline_layout));
		}
		void destroy_pipeline_layout () {
			vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
		}

		void upload_meshes () {
			// cmdbuf needed to upload anything
			auto cmdbuf = begin_one_time_commands();

			// Create buffers and calculate offets into memory block
			size_t size = 0;
			uint32_t mem_req_bits = 0;

			for (auto& b : bufs) {
				VkBufferCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				info.size = b.size;
				info.usage = b.usage;
				info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				VK_CHECK_RESULT(vkCreateBuffer(device, &info, nullptr, &b.vkbuf));

				vkGetBufferMemoryRequirements(device, b.vkbuf, &b.mem_req);

				size = align_up(size, b.mem_req.alignment);
				b.vkoffset = size;
				size += b.mem_req.size;

				mem_req_bits |= b.mem_req.memoryTypeBits;
			}

			// alloc cpu-visible staging buffer
			VkDeviceMemory staging_buf_mem;
			VkBuffer staging_buf = create_buffer(device, physical_device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buf_mem);

			// alloc gpu-resident memory and create a temporary target buffer for batching the upload of all buffers (vkCmdCopyBuffer cannot deal with VkDeviceMemory)
			VkBuffer staging_target_buf;
			VkDeviceMemory mem;
			
			VkBufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			info.size = size;
			info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK_RESULT(vkCreateBuffer(device, &info, nullptr, &staging_target_buf));

			VkMemoryAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.allocationSize = size;
			alloc_info.memoryTypeIndex = find_memory_type(physical_device, mem_req_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &alloc_info, nullptr, &mem));

			vkBindBufferMemory(device, staging_target_buf, mem, 0);

			// upload buffers and bind buffers to memory
			void* ptr = nullptr;
			vkMapMemory(device, staging_buf_mem, 0, size, 0, &ptr);
			
			for (auto& b : bufs) {
				memcpy((char*)ptr + b.vkoffset, b.data, b.size);

				vkBindBufferMemory(device, b.vkbuf, mem, b.vkoffset);
			}

			vkUnmapMemory(device, staging_buf_mem);

			// upload data by copying staging buffer to gpu
			VkBufferCopy copy_region = {};
			copy_region.srcOffset = 0;
			copy_region.dstOffset = 0;
			copy_region.size = size;
			vkCmdCopyBuffer(cmdbuf, staging_buf, staging_target_buf, 1, &copy_region);

			end_one_time_commands(cmdbuf);

			vkDestroyBuffer(device, staging_buf, nullptr);
			vkDestroyBuffer(device, staging_target_buf, nullptr);
			vkFreeMemory(device, staging_buf_mem, nullptr);

			mesh_mem = mem;
		}
		void destroy_meshes () {
			for (auto& b : bufs)
				vkDestroyBuffer(device, b.vkbuf, nullptr);
			vkFreeMemory(device, mesh_mem, nullptr);
		}

		void create_pipeline (int msaa, Shader* shader) {
			pipeline = VK_NULL_HANDLE;

			if (!shader->valid)
				return;

			VkPipelineShaderStageCreateInfo shader_stages[16] = {};

			for (int i=0; i<(int)shader->stages.size(); ++i) {
				shader_stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				shader_stages[i].stage = SHADERC_STAGE_BITS_MAP[ shader->stages[i].stage ];
				shader_stages[i].module = shader->stages[i].module;
				shader_stages[i].pName = "main";
			}

			VertexAttributes attribs;
			Vertex::attributes(attribs);

			VkPipelineVertexInputStateCreateInfo vertex_input = {};
			vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertex_input.vertexBindingDescriptionCount = 1;
			vertex_input.pVertexBindingDescriptions = &attribs.descr;
			vertex_input.vertexAttributeDescriptionCount = (uint32_t)attribs.attribs.size();
			vertex_input.pVertexAttributeDescriptions = attribs.attribs.data();

			VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
			input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			input_assembly.primitiveRestartEnable = VK_FALSE;

			// fake viewport and scissor, actually set dynamically
			VkViewport viewport = {};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width  = (float)1920;//cur_size.x;
			viewport.height = (float)1080;//cur_size.y;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			
			VkRect2D scissor = {};
			scissor.offset = { 0, 0 };
			scissor.extent = { 1920, 1080 };//swap_chain.extent;

			VkPipelineViewportStateCreateInfo viewport_state = {};
			viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewport_state.viewportCount = 1;
			viewport_state.pViewports = &viewport;
			viewport_state.scissorCount = 1;
			viewport_state.pScissors = &scissor;

			VkPipelineRasterizationStateCreateInfo rasterizer = {};
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.depthClampEnable = VK_FALSE;
			rasterizer.rasterizerDiscardEnable = VK_FALSE;
			rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizer.lineWidth = 1.0f;
			rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizer.depthBiasEnable = VK_FALSE;
			rasterizer.depthBiasConstantFactor = 0.0f;
			rasterizer.depthBiasClamp = 0.0f;
			rasterizer.depthBiasSlopeFactor = 0.0f;

			VkPipelineMultisampleStateCreateInfo multisampling = {};
			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.sampleShadingEnable = VK_FALSE;
			multisampling.rasterizationSamples = (VkSampleCountFlagBits)msaa;
			multisampling.minSampleShading = 1.0f;
			multisampling.pSampleMask = nullptr;
			multisampling.alphaToCoverageEnable = VK_FALSE;
			multisampling.alphaToOneEnable = VK_FALSE;

			VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
			depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depth_stencil.depthTestEnable = 0;//VK_TRUE;
			depth_stencil.depthWriteEnable = 0;//VK_TRUE;
			depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
			depth_stencil.depthBoundsTestEnable = VK_FALSE;
			depth_stencil.minDepthBounds = 0.0f;
			depth_stencil.maxDepthBounds = 1.0f;
			depth_stencil.stencilTestEnable = VK_FALSE;
			depth_stencil.front = {};
			depth_stencil.back = {};

			VkPipelineColorBlendAttachmentState color_blend_attachment = {};
			color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			color_blend_attachment.blendEnable = VK_FALSE;
			color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
			color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

			VkPipelineColorBlendStateCreateInfo color_blending = {};
			color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			color_blending.logicOpEnable = VK_FALSE;
			color_blending.logicOp = VK_LOGIC_OP_COPY;
			color_blending.attachmentCount = 1;
			color_blending.pAttachments = &color_blend_attachment;
			color_blending.blendConstants[0] = 0.0f;
			color_blending.blendConstants[1] = 0.0f;
			color_blending.blendConstants[2] = 0.0f;
			color_blending.blendConstants[3] = 0.0f;

			VkDynamicState dynamic_states[] = {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR,
			};

			VkPipelineDynamicStateCreateInfo dynamic_state = {};
			dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamic_state.dynamicStateCount = ARRLEN(dynamic_states);
			dynamic_state.pDynamicStates = dynamic_states;

			VkGraphicsPipelineCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			info.stageCount				= (uint32_t)shader->stages.size();
			info.pStages				= shader_stages;
			info.pVertexInputState		= &vertex_input;
			info.pInputAssemblyState	= &input_assembly;
			info.pViewportState			= &viewport_state;
			info.pRasterizationState	= &rasterizer;
			info.pMultisampleState		= &multisampling;
			info.pDepthStencilState		= &depth_stencil;
			info.pColorBlendState		= &color_blending;
			info.pDynamicState			= &dynamic_state;
			info.layout					= pipeline_layout;
			info.renderPass				= render_pass;
			info.subpass				= 0;
			info.basePipelineHandle		= VK_NULL_HANDLE;
			info.basePipelineIndex		= -1;

			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));
		}
		void destroy_pipeline () {
			if (pipeline)
				vkDestroyPipeline(device, pipeline, nullptr);
		}

		//// One time commands
		VkCommandBuffer begin_one_time_commands () {
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
		void end_one_time_commands (VkCommandBuffer buf) {

			vkEndCommandBuffer(buf);

			VkSubmitInfo submit_info = {};
			submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &buf;
			vkQueueSubmit(queues.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

			vkQueueWaitIdle(queues.graphics_queue);

			vkFreeCommandBuffers(device, one_time_command_pool, 1, &buf);
		}
	};
}
using namespace vk;
