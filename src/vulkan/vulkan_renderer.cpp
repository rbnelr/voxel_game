#include "common.hpp"
#include "vulkan_renderer.hpp"
#include "engine/camera.hpp"
#include "GLFW/glfw3.h"
#include "game.hpp"

namespace vk {

void VulkanRenderer::set_view_uniforms (Camera_View& view, int2 viewport_size) {
	ViewUniforms ubo;
	ubo.set(view, viewport_size);

	memcpy((char*)ubo_mem_ptr + frame_data[cur_frame].ubo_mem_offs, &ubo, sizeof(ViewUniforms));
}

void VulkanRenderer::frame_begin (GLFWwindow* window, kiss::ChangedFiles& changed_files) {
	ZoneScoped;

	pipelines.update(ctx, changed_files, wireframe);

	auto frame = frame_data[cur_frame];

	vkWaitForFences(ctx.dev, 1, &frame.fence, VK_TRUE, UINT64_MAX);
	vkResetFences(ctx.dev, 1, &frame.fence);

	vkResetCommandPool(ctx.dev, frame.command_pool, 0);

	ctx.aquire_image(frame.image_available_semaphore);

	if (ctx.swap_chain.format.format != wnd_color_format) {
		// swapchain recreation could possibly change swap chain image format
		wnd_color_format = ctx.swap_chain.format.format;
		// recreate final renderpass (pipelines can be kept because they only care about compatible renderpasses)
		vkDestroyRenderPass(ctx.dev, ui_renderpass, nullptr);
		ui_renderpass = create_ui_renderpass(wnd_color_format);
	}

	recreate_main_framebuffer(ctx.wnd_size);
}

void set_viewport (VkCommandBuffer cmds, int2 size) {
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width  = (float)size.x;
	viewport.height = (float)size.y;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = { (uint32_t)size.x, (uint32_t)size.y };

	vkCmdSetViewport(cmds, 0, 1, &viewport);
	vkCmdSetScissor(cmds, 0, 1, &scissor);
}

void VulkanRenderer::render_frame (GLFWwindow* window, Input& I, Game& game) {
	ZoneScoped;

	screenshot.begin(I);

	////
	auto cmds = frame_data[cur_frame].command_buffer;
	auto wnd_framebuffer = ctx.swap_chain.images[ctx.image_index];

	set_view_uniforms(game.view, I.window_size);

	{
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = nullptr;
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmds, &begin_info));
	}

	{
		GPU_TRACE(ctx, cmds, "upload_remeshed");
		chunk_renderer.upload_remeshed(*this, game.world->chunks, cmds, cur_frame);
	}

	{ // set 0
		// TODO: why do we need to specify a PipelineLayout here even though a the set0 is used in all pipelines?
		vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, chunk_renderer.pipeline_layout, 0, 1,
			&frame_data[cur_frame].ubo_descriptor_set, 0, nullptr);
	}

	{ // main render pass
		{
			VkClearValue clear_vales[3] = {};
			clear_vales[0].color = { .01f, .011f, .012f, 1 };
			clear_vales[1].color = { 0, 0, 0, 0 };
			clear_vales[2].depthStencil = { 0.0f, 0 }; // use reverse depth

			VkRenderPassBeginInfo render_pass_info = {};
			render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			render_pass_info.renderPass = main_renderpass;
			render_pass_info.framebuffer = main_framebuffer;
			render_pass_info.renderArea.offset = { 0, 0 };
			render_pass_info.renderArea.extent = { (uint32_t)renderscale_size.x, (uint32_t)renderscale_size.y };
			render_pass_info.clearValueCount = ARRLEN(clear_vales);
			render_pass_info.pClearValues = clear_vales;
			vkCmdBeginRenderPass(cmds, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		}

		GPU_TRACE(ctx, cmds, "main_renderpass");

		set_viewport(cmds, renderscale_size);

		{
			GPU_TRACE(ctx, cmds, "draw chunks");
			chunk_renderer.draw_chunks(ctx, cmds, game, debug_frustrum_culling, cur_frame);
		}

		// -> debug_drawer.pipeline_layout is not compatible with chunk_renderer.pipeline_layout because of differing push constants
		// It's wierd that I need to rebind set 0, but I prefer this to giving 
		vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, debug_drawer.pipeline_layout, 0, 1,
			&frame_data[cur_frame].ubo_descriptor_set, 0, nullptr);
	}
	vkCmdEndRenderPass(cmds);

	auto image_barrier = [&] (RenderBuffer& renderbuf, bool depth=false) {
		VkImageMemoryBarrier img = {};
		img.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		img.srcAccessMask = depth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		img.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		img.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		img.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		img.image = renderbuf.image;
		img.subresourceRange.aspectMask = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		img.subresourceRange.baseMipLevel = 0;
		img.subresourceRange.levelCount = 1;
		img.subresourceRange.baseArrayLayer = 0;
		img.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(cmds,
			depth ?
				VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : // TODO: correct stage for depth writes?
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &img);
	};

	{ // ssao render pass

		image_barrier(main_depth, true);
		image_barrier(main_normal);

		{
			VkRenderPassBeginInfo render_pass_info = {};
			render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			render_pass_info.renderPass = ssao_renderpass;
			render_pass_info.framebuffer = ssao_framebuffer;
			render_pass_info.renderArea.offset = { 0, 0 };
			render_pass_info.renderArea.extent = { (uint32_t)renderscale_size.x, (uint32_t)renderscale_size.y };
			vkCmdBeginRenderPass(cmds, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		}

		{
			GPU_TRACE(ctx, cmds, "ssao_renderpass");

			//set_viewport(cmds, renderscale_size);

			{
				GPU_TRACE(ctx, cmds, "ssao draw");

				// set 1
				vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, ssao_pipeline_layout, 1, 1,
					&ssao_descriptor_set, 0, nullptr);

				vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, ssao_pipeline->pipeline);

				vkCmdDraw(cmds, 3, 1, 0, 0);
			}
		}
	}
	vkCmdEndRenderPass(cmds);

	{ // ui render pass
		
		image_barrier(main_color);
		image_barrier(ssao_fac);

		{
			VkRenderPassBeginInfo render_pass_info = {};
			render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			render_pass_info.renderPass = ui_renderpass;
			render_pass_info.framebuffer = wnd_framebuffer.framebuffer;
			render_pass_info.renderArea.offset = { 0, 0 };
			render_pass_info.renderArea.extent = { (uint32_t)ctx.wnd_size.x, (uint32_t)ctx.wnd_size.y };
			vkCmdBeginRenderPass(cmds, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		}

		{
			GPU_TRACE(ctx, cmds, "ui_renderpass");

			set_viewport(cmds, ctx.wnd_size);

			{
				GPU_TRACE(ctx, cmds, "rescale draw");

				// set 1
				vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, rescale_pipeline_layout, 1, 1,
					&rescale_descriptor_set, 0, nullptr);

				vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, rescale_pipeline->pipeline);

				vkCmdDraw(cmds, 3, 1, 0, 0);
			}

			debug_drawer.draw(ctx, cmds, cur_frame);
			
			ctx.imgui_draw(cmds, screenshot.take_screenshot && !screenshot.include_ui);
		}
	}
	vkCmdEndRenderPass(cmds);

	if (screenshot.take_screenshot)
		screenshot.screenshot_swapchain_img(ctx, cmds);

	staging.update_buffer_alloc(ctx, cur_frame);

	TracyVkCollect(ctx.tracy_ctx, cmds);

	VK_CHECK_RESULT(vkEndCommandBuffer(cmds));

	submit(window, cmds);
}

void VulkanRenderer::submit (GLFWwindow* window, VkCommandBuffer cmds) {
	ZoneScoped;
	
	auto frame = frame_data[cur_frame];
	
	{
		ZoneScopedN("vkQueueSubmit");

		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// see https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples - Swapchain Image Acquire and Present
		// Any color attachment output in the cmdbuf will wait for image_available_semaphore
		// TODO: is this sync to strict? only the final renderpass actually writes to the img, but all renderpasses are affected by this
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &frame.image_available_semaphore;
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submit_info.pWaitDstStageMask = &wait_stage;

		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &frame.command_buffer;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &frame.render_finished_semaphore;
		VK_CHECK_RESULT(vkQueueSubmit(ctx.queues.graphics_queue, 1, &submit_info, frame.fence));
	}

	ctx.present_image(frame.render_finished_semaphore);

	screenshot.end(ctx);
	
	cur_frame = (cur_frame + 1) % FRAMES_IN_FLIGHT;
}

VulkanRenderer::VulkanRenderer (GLFWwindow* window, char const* app_name): ctx{window, app_name} {
	ZoneScoped;

	wnd_color_format = ctx.swap_chain.format.format;

	fb_color_format = find_color_format();
	fb_depth_format = find_depth_format();
	fb_float_format = find_float_format();
	fb_vec2_format = find_vec2_format();
	fb_vec3_format = find_vec3_format();

	max_msaa_samples = get_max_usable_multisample_count(ctx.pdev);
	msaa = 1;
	//msaa = min(msaa, max_msaa_samples);

	init_cmd_pool = create_one_time_command_pool(ctx.dev, ctx.queues.families.graphics_family);

#ifdef TRACY_ENABLE
	ctx.init_vk_tracy(init_cmd_pool);
#endif

	pipelines.init(ctx.dev);
	staging.create(ctx, FRAMES_IN_FLIGHT);

	create_frame_data();

	upload_static_data();

	create_descriptor_pool();
	create_ubo_buffers();
	
	main_renderpass = create_main_renderpass(fb_color_format, fb_vec3_format, fb_depth_format, msaa);
	GPU_DBG_NAME(ctx, main_renderpass, "main_renderpass");

	ssao_renderpass = create_ssao_renderpass(fb_float_format);
	GPU_DBG_NAME(ctx, ssao_renderpass, "ssao_renderpass");

	ui_renderpass = create_ui_renderpass(wnd_color_format);
	GPU_DBG_NAME(ctx, ui_renderpass, "ui_renderpass");

	create_common_descriptors();
	GPU_DBG_NAME(ctx, main_sampler, "main_sampler");
	for (int i=0; i<FRAMES_IN_FLIGHT; ++i)
		GPU_DBG_NAMEf(ctx, frame_data[i].ubo_descriptor_set, "ubo_descriptor_set[%d]", i);

	{
		VkSamplerCreateInfo sampler_info = {};
		sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_info.magFilter = VK_FILTER_LINEAR;
		sampler_info.minFilter = VK_FILTER_LINEAR;
		sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.anisotropyEnable = VK_FALSE;
		sampler_info.minLod = 0;
		sampler_info.maxLod = 0;
		vkCreateSampler(ctx.dev, &sampler_info, nullptr, &framebuf_sampler);
		GPU_DBG_NAME(ctx, framebuf_sampler, "framebuf_sampler");

		sampler_info.magFilter = VK_FILTER_NEAREST;
		sampler_info.minFilter = VK_FILTER_LINEAR;
		vkCreateSampler(ctx.dev, &sampler_info, nullptr, &framebuf_sampler_nearest);
		GPU_DBG_NAME(ctx, framebuf_sampler_nearest, "framebuf_sampler_nearest");
	}

	{
		create_ssao_descriptors();
		GPU_DBG_NAME(ctx, ssao_descriptor_layout, "ssao_descriptor_layout");
		GPU_DBG_NAME(ctx, ssao_descriptor_set, "ssao_descriptor_set");

		ssao_pipeline_layout = create_pipeline_layout(ctx.dev,
			{ common_descriptor_layout, ssao_descriptor_layout }, {});
		GPU_DBG_NAME(ctx, ssao_pipeline_layout, "ssao_pipeline_layout");

		PipelineOptions opt;
		opt.alpha_blend = false;
		opt.depth_test = false;
		opt.cull_mode = VK_CULL_MODE_NONE;
		auto cfg = PipelineConfig("vk/ssao", ssao_pipeline_layout, ssao_renderpass, 0, opt, {}, {});
		cfg.allow_wireframe = false; // don't do wireframe for fullscreen quad draws or we won't see anything sensible
		ssao_pipeline = pipelines.create_pipeline(ctx, "ssao_pipeline", cfg);
	}
	{
		create_rescale_descriptors();
		GPU_DBG_NAME(ctx, rescale_descriptor_layout, "rescale_descriptor_layout");
		GPU_DBG_NAME(ctx, rescale_descriptor_set, "rescale_descriptor_set");

		rescale_pipeline_layout = create_pipeline_layout(ctx.dev,
			{ common_descriptor_layout, rescale_descriptor_layout }, {});
		GPU_DBG_NAME(ctx, rescale_pipeline_layout, "rescale_pipeline_layout");

		PipelineOptions opt;
		opt.alpha_blend = false;
		opt.depth_test = false;
		opt.cull_mode = VK_CULL_MODE_NONE;
		auto cfg = PipelineConfig("vk/rescale", rescale_pipeline_layout, ui_renderpass, 0, opt, {}, {});
		cfg.allow_wireframe = false; // don't do wireframe for fullscreen quad draws or we won't see anything sensible
		rescale_pipeline = pipelines.create_pipeline(ctx, "rescale_pipeline", cfg);
	}

	chunk_renderer.create(ctx, pipelines, main_renderpass, common_descriptor_layout, FRAMES_IN_FLIGHT);
	debug_drawer.create(ctx, pipelines, ui_renderpass, common_descriptor_layout, FRAMES_IN_FLIGHT);
}
VulkanRenderer::~VulkanRenderer () {
	ZoneScoped;

	//vkQueueWaitIdle(ctx.queues.graphics_queue);
	vkDeviceWaitIdle(ctx.dev);

	chunk_renderer.destroy(ctx.dev);
	debug_drawer.destroy(ctx.dev);

	destroy_static_data();

	destroy_ubo_buffers();

	vkDestroyPipelineLayout(ctx.dev, rescale_pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(ctx.dev, rescale_descriptor_layout, nullptr);
	vkDestroySampler(ctx.dev, framebuf_sampler, nullptr);
	vkDestroySampler(ctx.dev, framebuf_sampler_nearest, nullptr);

	vkDestroySampler(ctx.dev, main_sampler, nullptr);

	destroy_main_framebuffer();

	vkDestroyRenderPass(ctx.dev, main_renderpass, nullptr);
	vkDestroyRenderPass(ctx.dev, ui_renderpass, nullptr);

	vkDestroyDescriptorPool(ctx.dev, descriptor_pool, nullptr);

	staging.destroy(ctx.dev);
	pipelines.destroy(ctx.dev);

	for (auto& frame : frame_data) {
		vkDestroyCommandPool(ctx.dev, frame.command_pool, nullptr);

		vkDestroySemaphore(ctx.dev, frame.image_available_semaphore, nullptr);
		vkDestroySemaphore(ctx.dev, frame.render_finished_semaphore, nullptr);
		vkDestroyFence(ctx.dev, frame.fence, nullptr);
	}

	vkDestroyCommandPool(ctx.dev, init_cmd_pool, nullptr);

	TracyVkDestroy(ctx.tracy_ctx);
}

void VulkanRenderer::upload_static_data () {
	ZoneScoped;

	StaticDataUploader uploader;
	auto cmds = begin_init_cmds();
	uploader.cmds = cmds;

	{
		UploadBuffer bufs[1];
		bufs[0].data = g_assets.block_meshes.slices.data();
		bufs[0].size = g_assets.block_meshes.slices.size() * sizeof(g_assets.block_meshes.slices[0]);
		bufs[0].usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		mesh_mem = uploader.upload(ctx.dev, ctx.pdev, bufs, ARRLEN(bufs));

		block_meshes_buf = bufs[0].vkbuf;
	}

	{
		Image<srgba8> img;
		img.load_from_file("textures/atlas.png", &img);
		assert(img.size == int2(16*TILEMAP_SIZE));

		// place layers at y dir so ot make the memory contiguous
		Image<srgba8> img_arr (int2(16, 16 * TILEMAP_SIZE.x * TILEMAP_SIZE.y));
		{ // convert texture atlas/tilemap into texture array for proper sampling in shader
			for (int y=0; y<TILEMAP_SIZE.y; ++y) {
				for (int x=0; x<TILEMAP_SIZE.x; ++x) {
					Image<srgba8>::blit_rect(
						img, int2(x,y)*16,
						img_arr, int2(0, (y * TILEMAP_SIZE.x + x) * 16),
						16);
				}
			}
		}

		UploadTexture texs[1];
		texs[0].data = img_arr.pixels;
		texs[0].size = int2(16, 16);
		texs[0].mip_levels = -1;
		texs[0].layers = TILEMAP_SIZE.x * TILEMAP_SIZE.y;
		texs[0].format = VK_FORMAT_R8G8B8A8_SRGB;
		tex_mem = uploader.upload(ctx.dev, ctx.pdev, texs, ARRLEN(texs));
		GPU_DBG_NAME(ctx, tex_mem, "tex_mem");

		tilemap_img = { texs[0].vkimg };

		GPU_DBG_NAME(ctx, tilemap_img.img, "tilemap_img.img");
		GPU_DBG_NAME(ctx, tilemap_img.img_view, "tilemap_img.img_view");
	}

	ctx.imgui_create(cmds, FRAMES_IN_FLIGHT);

	end_init_cmds(cmds);
	uploader.end(ctx.dev);
}
void VulkanRenderer::destroy_static_data () {
	vkDestroyBuffer(ctx.dev, block_meshes_buf, nullptr);

	vkFreeMemory(ctx.dev, mesh_mem, nullptr);

	vkDestroyImageView(ctx.dev, tilemap_img.img_view, nullptr);
	vkDestroyImage(ctx.dev, tilemap_img.img, nullptr);

	vkFreeMemory(ctx.dev, tex_mem, nullptr);

	ctx.imgui_destroy();
}

////
void VulkanRenderer::create_ubo_buffers () {
	ZoneScoped;

	// Create buffers and calculate offets into memory block
	size_t size = 0;
	uint32_t mem_req_bits = (uint32_t)-1;

	for (int i=0; i<FRAMES_IN_FLIGHT; ++i) {
		auto& f = frame_data[i];

		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = sizeof(ViewUniforms);
		info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(ctx.dev, &info, nullptr, &f.ubo_buf));
		GPU_DBG_NAMEf(ctx, f.ubo_buf, "ubo_buffer[%d]", i);

		VkMemoryRequirements mem_req;
		vkGetBufferMemoryRequirements(ctx.dev, f.ubo_buf, &mem_req);

		size = align_up(size, mem_req.alignment);
		f.ubo_mem_offs = size;
		size += mem_req.size;

		mem_req_bits &= mem_req.memoryTypeBits;
	}

	{ // alloc gpu-resident host-visible ubo memory
		VkMemoryAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		info.allocationSize = size;
		info.memoryTypeIndex = find_memory_type(ctx.pdev, mem_req_bits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(ctx.dev, &info, nullptr, &ubo_memory));
		GPU_DBG_NAME(ctx, ubo_memory, "ubo_memory");

		vkMapMemory(ctx.dev, ubo_memory, 0, size, 0, &ubo_mem_ptr);

		for (auto& f : frame_data)
			vkBindBufferMemory(ctx.dev, f.ubo_buf, ubo_memory, f.ubo_mem_offs);
	}
}
void VulkanRenderer::destroy_ubo_buffers () {
	vkUnmapMemory(ctx.dev, ubo_memory);

	for (auto& f : frame_data)
		vkDestroyBuffer(ctx.dev, f.ubo_buf, nullptr);
	vkFreeMemory(ctx.dev, ubo_memory, nullptr);
}

void VulkanRenderer::create_descriptor_pool () {
	VkDescriptorPoolSize pool_sizes[2] = {};
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[0].descriptorCount = (uint32_t)(FRAMES_IN_FLIGHT + 4); // common tex(per frame) ssao(20 + rescale(2)

	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[1].descriptorCount = (uint32_t)(FRAMES_IN_FLIGHT*2 + 1); // common ubos(per frame) + ssao

	VkDescriptorPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.poolSizeCount = ARRLEN(pool_sizes);
	info.pPoolSizes = pool_sizes;
	info.maxSets = (uint32_t)(FRAMES_IN_FLIGHT + 2); // for common(per frame) + ssao + rescale

	VK_CHECK_RESULT(vkCreateDescriptorPool(ctx.dev, &info, nullptr, &descriptor_pool));
	GPU_DBG_NAME(ctx, descriptor_pool, "descriptor_pool");
}

void VulkanRenderer::create_common_descriptors () {
	{ // create descriptor layout
		VkSamplerCreateInfo sampler_info = {};
		sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_info.magFilter = VK_FILTER_NEAREST;
		sampler_info.minFilter = VK_FILTER_LINEAR;
		sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // could in theory create multiple samplers and select them in the shader to avoid ugly wrapping for blocks like grass that show a sliver of grass at the bottom
		sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_info.anisotropyEnable = VK_FALSE;
		sampler_info.minLod = 0;
		sampler_info.maxLod = 1000;
		vkCreateSampler(ctx.dev, &sampler_info, nullptr, &main_sampler);

		int idx = 0;
		VkDescriptorSetLayoutBinding bindings[3] = {};
		bindings[idx].binding = idx;
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[idx].descriptorCount = 1;
		bindings[idx].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		idx++;

		bindings[idx].binding = idx;
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[idx].descriptorCount = 1;
		bindings[idx].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		idx++;

		bindings[idx].binding = idx;
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[idx].descriptorCount = 1;
		bindings[idx].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[idx].pImmutableSamplers = &main_sampler;
		idx++;

		VkDescriptorSetLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = ARRLEN(bindings);
		info.pBindings = bindings;

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ctx.dev, &info, nullptr, &common_descriptor_layout));
	}

	{ // create descriptor sets
		VkDescriptorSetLayout layouts[FRAMES_IN_FLIGHT];
		for (auto& l : layouts)
			l = common_descriptor_layout;

		VkDescriptorSetAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.descriptorPool = descriptor_pool;
		info.descriptorSetCount = FRAMES_IN_FLIGHT;
		info.pSetLayouts = layouts;

		VkDescriptorSet sets[FRAMES_IN_FLIGHT]; // TODO: this code would prefer array of sets (SOA) to current FrameData AOS
		VK_CHECK_RESULT(vkAllocateDescriptorSets(ctx.dev, &info, sets));

		for (int i=0; i<FRAMES_IN_FLIGHT; ++i) {
			frame_data[i].ubo_descriptor_set = sets[i];

			int idx = 0;
			VkWriteDescriptorSet writes[3] = {};
			
			VkDescriptorBufferInfo buf = {};
			buf.buffer = frame_data[i].ubo_buf;
			buf.offset = 0;
			buf.range = VK_WHOLE_SIZE;

			writes[idx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[idx].dstSet = sets[i];
			writes[idx].dstBinding = idx;
			writes[idx].dstArrayElement = 0;
			writes[idx].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[idx].descriptorCount = 1;
			writes[idx].pBufferInfo = &buf;
			idx++;

			VkDescriptorBufferInfo buf2 = {};
			buf2.buffer = block_meshes_buf;
			buf2.offset = 0;
			buf2.range = VK_WHOLE_SIZE;

			writes[idx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[idx].dstSet = sets[i];
			writes[idx].dstBinding = idx;
			writes[idx].dstArrayElement = 0;
			writes[idx].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[idx].descriptorCount = 1;
			writes[idx].pBufferInfo = &buf2;
			idx++;

			VkDescriptorImageInfo img = {};
			img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			img.imageView = tilemap_img.img_view;

			writes[idx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[idx].dstSet = sets[i];
			writes[idx].dstBinding = idx;
			writes[idx].dstArrayElement = 0;
			writes[idx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[idx].descriptorCount = 1;
			writes[idx].pImageInfo = &img;
			idx++;

			vkUpdateDescriptorSets(ctx.dev, ARRLEN(writes), writes, 0, nullptr);
		}
	}
}

void VulkanRenderer::create_ssao_descriptors () {
	{ // create descriptor layout
		uint32_t idx = 0;
		VkDescriptorSetLayoutBinding bindings[3] = {};
		bindings[idx].binding = idx;
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[idx].descriptorCount = 1;
		bindings[idx].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[idx].pImmutableSamplers = &framebuf_sampler;
		idx++;

		bindings[idx].binding = idx;
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[idx].descriptorCount = 1;
		bindings[idx].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[idx].pImmutableSamplers = &framebuf_sampler;
		idx++;

		bindings[idx].binding = idx;
		bindings[idx].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[idx].descriptorCount = 1;
		bindings[idx].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		idx++;

		VkDescriptorSetLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = ARRLEN(bindings);
		info.pBindings = bindings;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ctx.dev, &info, nullptr, &ssao_descriptor_layout));
	}

	{ // create descriptor sets
		VkDescriptorSetAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.descriptorPool = descriptor_pool;
		info.descriptorSetCount = 1;
		info.pSetLayouts = &ssao_descriptor_layout;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(ctx.dev, &info, &ssao_descriptor_set));
	}
}
void VulkanRenderer::update_ssao_img_descr () {
	VkDescriptorImageInfo depth = {};
	depth.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depth.imageView = main_depth.image_view;
	VkDescriptorImageInfo normals = {};
	normals.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normals.imageView = main_normal.image_view;

	VkWriteDescriptorSet writes[2] = {};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = ssao_descriptor_set;
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].descriptorCount = 1;
	writes[0].pImageInfo = &depth;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = ssao_descriptor_set;
	writes[1].dstBinding = 1;
	writes[1].dstArrayElement = 0;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].descriptorCount = 1;
	writes[1].pImageInfo = &normals;

	//writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	//writes[2].dstSet = ssao_descriptor_set;
	//writes[2].dstBinding = 2;
	//writes[2].dstArrayElement = 0;
	//writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	//writes[2].descriptorCount = 1;
	//writes[2].pBufferInfo = ;

	vkUpdateDescriptorSets(ctx.dev, ARRLEN(writes), writes, 0, nullptr);
}

void VulkanRenderer::create_rescale_descriptors () {
	{ // create descriptor layout
		VkDescriptorSetLayoutBinding bindings[2] = {};
		bindings[0].binding = 0;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		//bindings[0].pImmutableSamplers = &rescale_sampler; // no pImmutableSamplers to be able to switch

		bindings[1].binding = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[1].descriptorCount = 1;
		bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		//bindings[1].pImmutableSamplers = &framebuf_sampler; // no pImmutableSamplers to be able to switch

		VkDescriptorSetLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = ARRLEN(bindings);
		info.pBindings = bindings;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ctx.dev, &info, nullptr, &rescale_descriptor_layout));
	}

	{ // create descriptor sets
		VkDescriptorSetAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.descriptorPool = descriptor_pool;
		info.descriptorSetCount = 1;
		info.pSetLayouts = &rescale_descriptor_layout;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(ctx.dev, &info, &rescale_descriptor_set));
	}
}
void VulkanRenderer::update_rescale_img_descr () {
	VkDescriptorImageInfo color = {};
	color.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	color.imageView = main_color.image_view;
	color.sampler = renderscale_nearest ? framebuf_sampler_nearest : framebuf_sampler; // if not set in pImmutableSamplers

	VkDescriptorImageInfo ssao = {};
	ssao.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ssao.imageView = ssao_fac.image_view;
	ssao.sampler = renderscale_nearest ? framebuf_sampler_nearest : framebuf_sampler; // if not set in pImmutableSamplers

	VkWriteDescriptorSet writes[2] = {};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = rescale_descriptor_set;
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].descriptorCount = 1;
	writes[0].pImageInfo = &color;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = rescale_descriptor_set;
	writes[1].dstBinding = 1;
	writes[1].dstArrayElement = 0;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].descriptorCount = 1;
	writes[1].pImageInfo = &ssao;

	vkUpdateDescriptorSets(ctx.dev, ARRLEN(writes), writes, 0, nullptr);
}

//// Create per-frame data
void VulkanRenderer::create_frame_data () {
	for (int i=0; i<FRAMES_IN_FLIGHT; ++i) {
		auto& frame = frame_data[i];

		VkCommandPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		info.queueFamilyIndex = ctx.queues.families.graphics_family;
		info.flags = 0;
		VK_CHECK_RESULT(vkCreateCommandPool(ctx.dev, &info, nullptr, &frame.command_pool));

		VkCommandBufferAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = frame.command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount  = 1;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(ctx.dev, &alloc_info, &frame.command_buffer));

		GPU_DBG_NAMEf(ctx, frame.command_buffer, "cmds[%d]", i);

		VkSemaphoreCreateInfo semaphore_info = {};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fence_info = {};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		VK_CHECK_RESULT(vkCreateSemaphore(ctx.dev, &semaphore_info, nullptr, &frame.image_available_semaphore));
		VK_CHECK_RESULT(vkCreateSemaphore(ctx.dev, &semaphore_info, nullptr, &frame.render_finished_semaphore));
		VK_CHECK_RESULT(vkCreateFence(ctx.dev, &fence_info, nullptr, &frame.fence));
	}
}

//// Framebuffer creation
void VulkanRenderer::create_main_framebuffer (int2 size, VkFormat color_format, VkFormat normal_format, VkFormat depth_format, int msaa) {
	main_color = create_render_buffer(size, color_format,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT, msaa);
	GPU_DBG_NAME(ctx, main_color.image, "main_color");
	GPU_DBG_NAME(ctx, main_color.image_view, "main_color.img_view");
	GPU_DBG_NAME(ctx, main_color.memory, "main_color.mem");
	
	main_normal = create_render_buffer(size, normal_format,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT, msaa);
	GPU_DBG_NAME(ctx, main_normal.image, "main_normal");
	GPU_DBG_NAME(ctx, main_normal.image_view, "main_normal.img_view");
	GPU_DBG_NAME(ctx, main_normal.memory, "main_normal.mem");

	main_depth = create_render_buffer(size, depth_format,
		/*VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |*/ VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT, msaa);
	GPU_DBG_NAME(ctx, main_depth.image, "main_depth");
	GPU_DBG_NAME(ctx, main_depth.image_view, "main_depth.img_view");
	GPU_DBG_NAME(ctx, main_depth.memory, "main_depth.mem");

	VkImageView attachments[] = { main_color.image_view, main_normal.image_view, main_depth.image_view };

	VkFramebufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.renderPass = main_renderpass;
	info.attachmentCount = ARRLEN(attachments);
	info.pAttachments = attachments;
	info.width  = size.x;
	info.height = size.y;
	info.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(ctx.dev, &info, nullptr, &main_framebuffer));

	GPU_DBG_NAME(ctx, main_framebuffer, "main_framebuffer");
}
void VulkanRenderer::destroy_main_framebuffer () {
	vkDestroyFramebuffer(ctx.dev, main_framebuffer, nullptr);

	vkDestroyImageView(ctx.dev, main_color.image_view, nullptr);
	vkDestroyImage(ctx.dev, main_color.image, nullptr);
	vkFreeMemory(ctx.dev, main_color.memory, nullptr);

	vkDestroyImageView(ctx.dev, main_depth.image_view, nullptr);
	vkDestroyImage(ctx.dev, main_depth.image, nullptr);
	vkFreeMemory(ctx.dev, main_depth.memory, nullptr);
}

void VulkanRenderer::create_ssao_framebuffer (int2 size, VkFormat color_format) {
	ssao_fac = create_render_buffer(size, color_format,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT, 1);
	GPU_DBG_NAME(ctx, ssao_fac.image, "ssao_fac");
	GPU_DBG_NAME(ctx, ssao_fac.image_view, "ssao_fac.img_view");
	GPU_DBG_NAME(ctx, ssao_fac.memory, "ssao_fac.mem");

	VkImageView attachments[] = { ssao_fac.image_view };

	VkFramebufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.renderPass = ssao_renderpass;
	info.attachmentCount = ARRLEN(attachments);
	info.pAttachments = attachments;
	info.width  = size.x;
	info.height = size.y;
	info.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(ctx.dev, &info, nullptr, &ssao_framebuffer));

	GPU_DBG_NAME(ctx, ssao_framebuffer, "ssao_framebuffer");
}
void VulkanRenderer::destroy_ssao_framebuffer () {
	vkDestroyFramebuffer(ctx.dev, ssao_framebuffer, nullptr);

	vkDestroyImageView(ctx.dev, ssao_fac.image_view, nullptr);
	vkDestroyImage(ctx.dev, ssao_fac.image, nullptr);
	vkFreeMemory(ctx.dev, ssao_fac.memory, nullptr);
}

void VulkanRenderer::recreate_main_framebuffer (int2 wnd_size) {
	int2 cur_size = max(roundi((float2)wnd_size * renderscale), 1);
	if (cur_size != renderscale_size || renderscale_nearest_changed) {
		vkQueueWaitIdle(ctx.queues.graphics_queue);

		if (renderscale_size.x > 0) {
			destroy_main_framebuffer();
			destroy_ssao_framebuffer();
		}

		renderscale_size = cur_size;
		create_main_framebuffer(renderscale_size, fb_color_format, fb_vec3_format, fb_depth_format, msaa);
		create_ssao_framebuffer(renderscale_size, fb_float_format);

		update_ssao_img_descr();
		update_rescale_img_descr();
	}

	renderscale_nearest_changed = false;
}

//// Renderpass creation
RenderBuffer VulkanRenderer::create_render_buffer (int2 size, VkFormat format, VkImageUsageFlags usage, VkImageLayout initial_layout, VkMemoryPropertyFlags props, VkImageAspectFlags aspect, int msaa) {
	RenderBuffer buf;
	buf.image = create_image(ctx.dev, ctx.pdev, size, format, VK_IMAGE_TILING_OPTIMAL, usage, initial_layout, props, &buf.memory, 1, (VkSampleCountFlagBits)msaa);
	buf.image_view = create_image_view(ctx.dev, buf.image, format, 1, aspect);
	return buf;
}

VkRenderPass VulkanRenderer::create_main_renderpass (VkFormat color_format, VkFormat normal_format, VkFormat depth_format, int msaa) {
	VkAttachmentDescription attachments[3] = {};
	// color_attachment
	attachments[0].format = color_format;
	attachments[0].samples = (VkSampleCountFlagBits)msaa;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	// normal_attachment
	attachments[1].format = normal_format;
	attachments[1].samples = (VkSampleCountFlagBits)msaa;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	// depth_attachment
	attachments[2].format = depth_format;
	attachments[2].samples = (VkSampleCountFlagBits)msaa;
	attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // VK_ATTACHMENT_STORE_OP_DONT_CARE if not read later
	attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	//VkAttachmentDescription color_attachment_resolve = {};
	//color_attachment_resolve.format = color_format;
	//color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
	//color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	//color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	//color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	//color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_refs[2] = {};
	color_attachment_refs[0].attachment = 0;
	color_attachment_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	color_attachment_refs[1].attachment = 1;
	color_attachment_refs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 2;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = ARRLEN(color_attachment_refs);
	subpass.pColorAttachments = color_attachment_refs;
	subpass.pResolveAttachments = nullptr;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = ARRLEN(attachments);
	info.pAttachments = attachments;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;

	VkRenderPass renderpass;
	VK_CHECK_RESULT(vkCreateRenderPass(ctx.dev, &info, nullptr, &renderpass));
	return renderpass;
}
VkRenderPass VulkanRenderer::create_ssao_renderpass (VkFormat color_format) {
	VkAttachmentDescription attachments[1] = {};
	// color_attachment
	attachments[0].format = color_format;
	attachments[0].samples = (VkSampleCountFlagBits)1;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	// see https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples - Swapchain Image Acquire and Present
	VkSubpassDependency depen = {};
	depen.srcSubpass = VK_SUBPASS_EXTERNAL;
	depen.dstSubpass = 0;
	depen.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	depen.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	depen.srcAccessMask = 0;
	depen.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = ARRLEN(attachments);
	info.pAttachments = attachments;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 1;
	info.pDependencies = &depen;

	VkRenderPass renderpass;
	VK_CHECK_RESULT(vkCreateRenderPass(ctx.dev, &info, nullptr, &renderpass));
	return renderpass;
}
VkRenderPass VulkanRenderer::create_ui_renderpass (VkFormat color_format) {
	VkAttachmentDescription attachments[1] = {};
	// color_attachment
	attachments[0].format = color_format;
	attachments[0].samples = (VkSampleCountFlagBits)1;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	// see https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples - Swapchain Image Acquire and Present
	VkSubpassDependency depen = {};
	depen.srcSubpass = VK_SUBPASS_EXTERNAL;
	depen.dstSubpass = 0;
	depen.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	depen.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	depen.srcAccessMask = 0;
	depen.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = ARRLEN(attachments);
	info.pAttachments = attachments;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 1;
	info.pDependencies = &depen;

	VkRenderPass renderpass;
	VK_CHECK_RESULT(vkCreateRenderPass(ctx.dev, &info, nullptr, &renderpass));
	return renderpass;
}

//// One time init commands buffers
VkCommandBuffer VulkanRenderer::begin_init_cmds () {
	ZoneScoped;
	
	VkCommandBuffer buf;

	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	info.commandPool = init_cmd_pool;
	info.commandBufferCount = 1;
	VK_CHECK_RESULT(vkAllocateCommandBuffers(ctx.dev, &info, &buf));

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(buf, &begin_info);

	GPU_DBG_NAME(ctx, buf, "init_cmds");

	return buf;
}
void VulkanRenderer::end_init_cmds (VkCommandBuffer buf) {
	ZoneScoped;

	vkEndCommandBuffer(buf);

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &buf;
	vkQueueSubmit(ctx.queues.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

	vkQueueWaitIdle(ctx.queues.graphics_queue);

	// TODO: vkResetCommandPool instead?
	vkFreeCommandBuffers(ctx.dev, init_cmd_pool, 1, &buf);
}

} // namespace vk
