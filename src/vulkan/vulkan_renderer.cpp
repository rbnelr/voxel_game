#include "common.hpp"
#include "vulkan_renderer.hpp"
#include "engine/camera.hpp"
#include "GLFW/glfw3.h"

namespace vk {

void Renderer::set_view_uniforms (Camera_View& view, int2 viewport_size) {
	ViewUniforms ubo;
	ubo.set(view, viewport_size);

	void* data;
	vkMapMemory(ctx.dev, ubo_memory, 0, sizeof(ViewUniforms), 0, &data);
	memcpy((char*)data + frame_data[cur_frame].ubo_mem_offset, &ubo, sizeof(ViewUniforms));
	vkUnmapMemory(ctx.dev, ubo_memory);
}

void Renderer::frame_begin (GLFWwindow* window) {
	ZoneScoped;
	
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

void Renderer::render_frame (GLFWwindow* window, RenderData& data) {
	ZoneScoped;
	
	////
	chunk_renderer.queue_remeshing(*this, data);

	auto cmds = frame_data[cur_frame].command_buffer;
	auto wnd_framebuffer = ctx.swap_chain.images[ctx.image_index].framebuffer;

	set_view_uniforms(data.view, data.window_size);

	{
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = nullptr;
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmds, &begin_info));
	}

	{
		TracyVkZone(ctx.tracy_ctx, cmds, "upload_remeshed");
		chunk_renderer.upload_remeshed(ctx.dev, ctx.pdev, cur_frame, cmds);
	}

	{ // main render pass
		{
			VkClearValue clear_vales[2] = {};
			clear_vales[0].color = { .01f, .011f, .012f, 1 };
			clear_vales[1].depthStencil = { 0.0f, 0 }; // use reverse depth

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

		TracyVkZone(ctx.tracy_ctx, cmds, "main_renderpass");

		set_viewport(cmds, renderscale_size);

		{
			vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, main_pipeline_layout, 0, 1,
				&frame_data[cur_frame].ubo_descriptor_set, 0, nullptr);
		}

		{
			TracyVkZone(ctx.tracy_ctx, cmds, "draw chunks");
			chunk_renderer.draw_chunks(cmds, data.chunks, main_pipeline, main_pipeline_layout);
		}
	}
	vkCmdEndRenderPass(cmds);

	{ // ui render pass
		
		// TODO: Do I need VkImageMemoryBarrier here or is the barrier itself enough if I don't need a layout transition?
		// (layout is transitioned in the renderpass)
		//VkImageMemoryBarrier ;
		vkCmdPipelineBarrier(cmds,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 0, nullptr);
		
		{
			VkRenderPassBeginInfo render_pass_info = {};
			render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			render_pass_info.renderPass = ui_renderpass;
			render_pass_info.framebuffer = wnd_framebuffer;
			render_pass_info.renderArea.offset = { 0, 0 };
			render_pass_info.renderArea.extent = { (uint32_t)ctx.wnd_size.x, (uint32_t)ctx.wnd_size.y };
			vkCmdBeginRenderPass(cmds, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		}

		{
			TracyVkZone(ctx.tracy_ctx, cmds, "ui_renderpass");

			set_viewport(cmds, ctx.wnd_size);

			{
				TracyVkZone(ctx.tracy_ctx, cmds, "rescale draw");

				{
					vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, rescale_pipeline_layout, 0, 1,
						&rescale_descriptor_set, 0, nullptr);
				}

				vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, rescale_pipeline);

				vkCmdDraw(cmds, 3, 1, 0, 0);
			}

			ctx.imgui_draw(cmds);
		}
	}
	vkCmdEndRenderPass(cmds);

	TracyVkCollect(ctx.tracy_ctx, cmds);

	VK_CHECK_RESULT(vkEndCommandBuffer(cmds));

	submit(window, cmds);
}

void Renderer::submit (GLFWwindow* window, VkCommandBuffer cmds) {
	ZoneScoped;
	
	auto frame = frame_data[cur_frame];
	
	{
		ZoneScopedN("vkQueueSubmit");
		
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &frame.image_available_semaphore;
		submit_info.pWaitDstStageMask = &wait_stage;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &frame.command_buffer;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &frame.render_finished_semaphore;
		VK_CHECK_RESULT(vkQueueSubmit(ctx.queues.graphics_queue, 1, &submit_info, frame.fence));
	}

	ctx.present_image(frame.render_finished_semaphore);
	
	cur_frame = (cur_frame + 1) % FRAMES_IN_FLIGHT;
}

Renderer::Renderer (GLFWwindow* window, char const* app_name, json const& blocks_json): ctx{window, app_name} {
	ZoneScoped;

	wnd_color_format = ctx.swap_chain.format.format;

	fb_color_format = find_color_format();
	fb_depth_format = find_depth_format();

	max_msaa_samples = get_max_usable_multisample_count(ctx.pdev);
	msaa = 1;
	//msaa = min(msaa, max_msaa_samples);

	init_cmd_pool = create_one_time_command_pool(ctx.dev, ctx.queues.families.graphics_family);

#ifdef TRACY_ENABLE
	ctx.init_vk_tracy(init_cmd_pool);
#endif

	create_frame_data();

	assets.load_block_textures(blocks_json);

	chunk_renderer.create(ctx.dev, ctx.pdev, FRAMES_IN_FLIGHT);
	shaders.init(ctx.dev);
	
	create_descriptor_pool();
	create_ubo_buffers();

	upload_static_data();
	
	{
		create_main_descriptors();

		main_renderpass = create_main_renderpass(fb_color_format, fb_depth_format, msaa);

		main_pipeline_layout = create_pipeline_layout(ctx.dev,
			{ main_descriptor_layout },
			{{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float3) }});

		main_pipeline = create_main_pipeline(shaders.get(ctx.dev, "chunks"),
			main_renderpass, main_pipeline_layout, msaa, make_attribs<BlockMeshInstance>());
	}

	{
		create_rescale_descriptors();

		ui_renderpass = create_ui_renderpass(wnd_color_format);

		rescale_pipeline_layout = create_pipeline_layout(ctx.dev,
			{ rescale_descriptor_layout }, {});
		rescale_pipeline = create_rescale_pipeline(shaders.get(ctx.dev, "rescale"),
			ui_renderpass, rescale_pipeline_layout);
	}
}
Renderer::~Renderer () {
	ZoneScoped;

	//vkQueueWaitIdle(ctx.queues.graphics_queue);
	vkDeviceWaitIdle(ctx.dev);

	destroy_static_data();

	destroy_ubo_buffers();

	if (rescale_pipeline)
		vkDestroyPipeline(ctx.dev, rescale_pipeline, nullptr);
	vkDestroyPipelineLayout(ctx.dev, rescale_pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(ctx.dev, rescale_descriptor_layout, nullptr);
	vkDestroySampler(ctx.dev, rescale_sampler, nullptr);

	if (main_pipeline)
		vkDestroyPipeline(ctx.dev, main_pipeline, nullptr);
	vkDestroyPipelineLayout(ctx.dev, main_pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(ctx.dev, main_descriptor_layout, nullptr);
	vkDestroySampler(ctx.dev, main_sampler, nullptr);

	destroy_main_framebuffer();

	vkDestroyRenderPass(ctx.dev, main_renderpass, nullptr);
	vkDestroyRenderPass(ctx.dev, ui_renderpass, nullptr);

	vkDestroyDescriptorPool(ctx.dev, descriptor_pool, nullptr);

	shaders.destroy(ctx.dev);
	chunk_renderer.destroy(ctx.dev);

	for (auto& frame : frame_data) {
		vkDestroyCommandPool(ctx.dev, frame.command_pool, nullptr);

		vkDestroySemaphore(ctx.dev, frame.image_available_semaphore, nullptr);
		vkDestroySemaphore(ctx.dev, frame.render_finished_semaphore, nullptr);
		vkDestroyFence(ctx.dev, frame.fence, nullptr);
	}

	vkDestroyCommandPool(ctx.dev, init_cmd_pool, nullptr);

	TracyVkDestroy(ctx.tracy_ctx);
}

void Renderer::upload_static_data () {
	ZoneScoped;
	
	StaticDataUploader uploader;
	auto cmds = begin_init_cmds();
	uploader.cmds = cmds;

	{
		//mesh_mem = uploader.upload(ctx.dev, ctx.pdev, meshes);
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
		texs[0].layers = TILEMAP_SIZE.x * TILEMAP_SIZE.y;
		texs[0].format = VK_FORMAT_R8G8B8A8_SRGB;
		tex_mem = uploader.upload(ctx.dev, ctx.pdev, texs, ARRLEN(texs));

		tilemap_img = { texs[0].vkimg };
	}

	ctx.imgui_create(cmds, FRAMES_IN_FLIGHT);

	end_init_cmds(cmds);
	uploader.end(ctx.dev);
}
void Renderer::destroy_static_data () {
	//for (auto& b : meshes)
	//	vkDestroyBuffer(ctx.dev, b.vkbuf, nullptr);
	//vkFreeMemory(ctx.dev, mesh_mem, nullptr);

	vkFreeMemory(ctx.dev, tex_mem, nullptr);

	vkDestroyImageView(ctx.dev, tilemap_img.img_view, nullptr);
	vkDestroyImage(ctx.dev, tilemap_img.img, nullptr);

	ctx.imgui_destroy();
}

////
void Renderer::create_ubo_buffers () {
	ZoneScoped;

	// Create buffers and calculate offets into memory block
	size_t size = 0;
	uint32_t mem_req_bits = (uint32_t)-1;

	for (auto& f : frame_data) {
		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = sizeof(ViewUniforms);
		info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK_RESULT(vkCreateBuffer(ctx.dev, &info, nullptr, &f.ubo_buffer));

		VkMemoryRequirements mem_req;
		vkGetBufferMemoryRequirements(ctx.dev, f.ubo_buffer, &mem_req);

		size = align_up(size, mem_req.alignment);
		f.ubo_mem_offset = size;
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

		for (auto& f : frame_data)
			vkBindBufferMemory(ctx.dev, f.ubo_buffer, ubo_memory, f.ubo_mem_offset);
	}
}
void Renderer::destroy_ubo_buffers () {
	for (auto& f : frame_data)
		vkDestroyBuffer(ctx.dev, f.ubo_buffer, nullptr);
	vkFreeMemory(ctx.dev, ubo_memory, nullptr);
}

void Renderer::create_descriptor_pool () {
	VkDescriptorPoolSize pool_sizes[2] = {};
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[0].descriptorCount = (uint32_t)(
		1 + // for rescale
		FRAMES_IN_FLIGHT); // for main image

	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[1].descriptorCount = (uint32_t)FRAMES_IN_FLIGHT; // for ubo

	VkDescriptorPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.poolSizeCount = ARRLEN(pool_sizes);
	info.pPoolSizes = pool_sizes;
	info.maxSets = (uint32_t)(
		FRAMES_IN_FLIGHT // for main
		+1); // for rescale

	VK_CHECK_RESULT(vkCreateDescriptorPool(ctx.dev, &info, nullptr, &descriptor_pool));
}

void Renderer::create_main_descriptors () {
	{ // create descriptor layout
		VkSamplerCreateInfo sampler_info = {};
		sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_info.magFilter = VK_FILTER_NEAREST;
		sampler_info.minFilter = VK_FILTER_LINEAR;
		sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // could in theory create multiple samplers and select them in the shader to avoid ugly wrapping for blocks like grass that show a sliver of grass at the bottom
		sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_info.anisotropyEnable = VK_FALSE;
		sampler_info.minLod = 0;
		sampler_info.maxLod = 0;
		vkCreateSampler(ctx.dev, &sampler_info, nullptr, &main_sampler);
		
		VkDescriptorSetLayoutBinding bindings[2] = {};
		bindings[0].binding = 0;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		bindings[1].binding = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[1].descriptorCount = 1;
		bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[1].pImmutableSamplers = &main_sampler;

		VkDescriptorSetLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = ARRLEN(bindings);
		info.pBindings = bindings;

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ctx.dev, &info, nullptr, &main_descriptor_layout));
	}

	{ // create descriptor sets
		VkDescriptorSetLayout layouts[FRAMES_IN_FLIGHT];
		for (auto& l : layouts)
			l = main_descriptor_layout;

		VkDescriptorSetAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.descriptorPool = descriptor_pool;
		info.descriptorSetCount = FRAMES_IN_FLIGHT;
		info.pSetLayouts = layouts;

		VkDescriptorSet sets[FRAMES_IN_FLIGHT]; // TODO: this code would prefer array of sets (SOA) to current FrameData AOS
		VK_CHECK_RESULT(vkAllocateDescriptorSets(ctx.dev, &info, sets));

		for (int i=0; i<FRAMES_IN_FLIGHT; ++i) {
			frame_data[i].ubo_descriptor_set = sets[i];

			VkWriteDescriptorSet writes[2] = {};
			
			VkDescriptorBufferInfo buf = {};
			buf.buffer = frame_data[i].ubo_buffer;
			buf.offset = 0;
			buf.range = sizeof(ViewUniforms); // or VK_WHOLE_SIZE

			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet = sets[i];
			writes[0].dstBinding = 0;
			writes[0].dstArrayElement = 0;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[0].descriptorCount = 1;
			writes[0].pBufferInfo = &buf;

			VkDescriptorImageInfo img = {};
			img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			img.imageView = tilemap_img.img_view;

			writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet = sets[i];
			writes[1].dstBinding = 1;
			writes[1].dstArrayElement = 0;
			writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[1].descriptorCount = 1;
			writes[1].pImageInfo = &img;

			vkUpdateDescriptorSets(ctx.dev, ARRLEN(writes), writes, 0, nullptr);
		}
	}
}

void Renderer::create_rescale_descriptors () {
	{ // create descriptor layout
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
		vkCreateSampler(ctx.dev, &sampler_info, nullptr, &rescale_sampler);

		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		binding.pImmutableSamplers = &rescale_sampler;

		VkDescriptorSetLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = 1;
		info.pBindings = &binding;

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
void Renderer::update_rescale_img_descr () {
	VkDescriptorImageInfo img = {};
	img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img.imageView = main_color.image_view;
	//img.sampler = nullptr; // set in pImmutableSamplers

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = rescale_descriptor_set;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &img;
	vkUpdateDescriptorSets(ctx.dev, 1, &write, 0, nullptr);
}

VkPipeline Renderer::create_main_pipeline (Shader* shader, VkRenderPass renderpass, VkPipelineLayout layout, int msaa, VertexAttributes attribs) {
	if (!shader->valid)
		return VK_NULL_HANDLE;

	VkPipelineShaderStageCreateInfo shader_stages[16] = {};

	for (int i=0; i<(int)shader->stages.size(); ++i) {
		shader_stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[i].stage = SHADERC_STAGE_BITS_MAP[ shader->stages[i].stage ];
		shader_stages[i].module = shader->stages[i].module;
		shader_stages[i].pName = "main";
	}

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
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // use reverse depth
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
	info.layout					= layout;
	info.renderPass				= renderpass;
	info.subpass				= 0;
	info.basePipelineHandle		= VK_NULL_HANDLE;
	info.basePipelineIndex		= -1;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(ctx.dev, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));
	return pipeline;
}
VkPipeline Renderer::create_rescale_pipeline (Shader* shader, VkRenderPass renderpass, VkPipelineLayout layout) {
	if (!shader->valid)
		return VK_NULL_HANDLE;

	VkPipelineShaderStageCreateInfo shader_stages[16] = {};

	for (int i=0; i<(int)shader->stages.size(); ++i) {
		shader_stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[i].stage = SHADERC_STAGE_BITS_MAP[ shader->stages[i].stage ];
		shader_stages[i].module = shader->stages[i].module;
		shader_stages[i].pName = "main";
	}

	VkPipelineVertexInputStateCreateInfo vertex_input = {};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

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
	rasterizer.cullMode = VK_CULL_MODE_NONE;
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
	depth_stencil.depthTestEnable = VK_FALSE;
	depth_stencil.depthWriteEnable = VK_FALSE;
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
	info.layout					= layout;
	info.renderPass				= renderpass;
	info.subpass				= 0;
	info.basePipelineHandle		= VK_NULL_HANDLE;
	info.basePipelineIndex		= -1;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(ctx.dev, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));
	return pipeline;
}

//// Create per-frame data
void Renderer::create_frame_data () {
	for (auto& frame : frame_data) {
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
void Renderer::create_main_framebuffer (int2 size, VkFormat color_format, VkFormat depth_format, int msaa) {
	main_color = create_render_buffer(size, color_format,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT, msaa);

	main_depth = create_render_buffer(size, depth_format,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT, msaa);

	VkImageView attachments[] = { main_color.image_view, main_depth.image_view };

	VkFramebufferCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.renderPass = main_renderpass;
	info.attachmentCount = ARRLEN(attachments);
	info.pAttachments = attachments;
	info.width  = size.x;
	info.height = size.y;
	info.layers = 1;
	VK_CHECK_RESULT(vkCreateFramebuffer(ctx.dev, &info, nullptr, &main_framebuffer));

	update_rescale_img_descr();
}
void Renderer::destroy_main_framebuffer () {
	vkDestroyFramebuffer(ctx.dev, main_framebuffer, nullptr);

	vkDestroyImageView(ctx.dev, main_color.image_view, nullptr);
	vkDestroyImage(ctx.dev, main_color.image, nullptr);
	vkFreeMemory(ctx.dev, main_color.memory, nullptr);

	vkDestroyImageView(ctx.dev, main_depth.image_view, nullptr);
	vkDestroyImage(ctx.dev, main_depth.image, nullptr);
	vkFreeMemory(ctx.dev, main_depth.memory, nullptr);
}

void Renderer::recreate_main_framebuffer (int2 wnd_size) {
	int2 cur_size = max(roundi((float2)wnd_size * renderscale), 1);
	if (cur_size == renderscale_size) return;

	vkQueueWaitIdle(ctx.queues.graphics_queue);

	if (renderscale_size.x > 0)
		destroy_main_framebuffer();

	renderscale_size = cur_size;
	create_main_framebuffer(renderscale_size, fb_color_format, fb_depth_format, msaa);
}

//// Renderpass creation
RenderBuffer Renderer::create_render_buffer (int2 size, VkFormat format, VkImageUsageFlags usage, VkImageLayout initial_layout, VkMemoryPropertyFlags props, VkImageAspectFlags aspect, int msaa) {
	RenderBuffer buf;
	buf.image = create_image(ctx.dev, ctx.pdev, size, format, VK_IMAGE_TILING_OPTIMAL, usage, initial_layout, props, &buf.memory, 1, (VkSampleCountFlagBits)msaa);
	buf.image_view = create_image_view(ctx.dev, buf.image, format, 1, aspect);
	return buf;
}

VkRenderPass Renderer::create_main_renderpass (VkFormat color_format, VkFormat depth_format, int msaa) {
	VkAttachmentDescription attachments[2] = {};
	// color_attachment
	attachments[0].format = color_format;
	attachments[0].samples = (VkSampleCountFlagBits)msaa;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	// depth_attachment
	attachments[1].format = depth_format;
	attachments[1].samples = (VkSampleCountFlagBits)msaa;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//VkAttachmentDescription color_attachment_resolve = {};
	//color_attachment_resolve.format = color_format;
	//color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
	//color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	//color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	//color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	//color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	subpass.pResolveAttachments = nullptr;
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	//VkSubpassDependency depen = {};
	//depen.srcSubpass = VK_SUBPASS_EXTERNAL;
	//depen.dstSubpass = 0;
	//depen.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	//depen.srcAccessMask = 0;
	//depen.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	//depen.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = ARRLEN(attachments);
	info.pAttachments = attachments;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 0;
	info.pDependencies = nullptr;

	VkRenderPass renderpass;
	VK_CHECK_RESULT(vkCreateRenderPass(ctx.dev, &info, nullptr, &renderpass));
	return renderpass;
}
VkRenderPass Renderer::create_ui_renderpass (VkFormat color_format) {
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

	VkSubpassDependency depen = {};
	depen.srcSubpass = VK_SUBPASS_EXTERNAL;
	depen.dstSubpass = 0;
	depen.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	depen.srcAccessMask = 0;
	depen.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
VkCommandBuffer Renderer::begin_init_cmds () {
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

	return buf;
}
void Renderer::end_init_cmds (VkCommandBuffer buf) {
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
