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
}

void Renderer::render_frame (GLFWwindow* window, RenderData& data) {
	ZoneScoped;
	
	chunk_renderer.queue_remeshing(*this, data);

	auto cmds = frame_data[cur_frame].command_buffer;
	auto framebuffer = ctx.swap_chain.images[ctx.image_index].framebuffer;

	set_view_uniforms(data.view, data.window_size);

	{
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = nullptr;
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmds, &begin_info));
	}

	chunk_renderer.upload_remeshed();

	{
		VkClearValue clear_vales[2] = {};
		clear_vales[0].color = { .01f, .011f, .012f, 1 };
		clear_vales[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = render_pass;
		render_pass_info.framebuffer = framebuffer;
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = ctx.swap_chain.extent;
		render_pass_info.clearValueCount = ARRLEN(clear_vales);
		render_pass_info.pClearValues = clear_vales;
		vkCmdBeginRenderPass(cmds, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	}

	{
		TracyVkZone(ctx.tracy_ctx, cmds, "render pass 1");

		{
			VkViewport viewport = {};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width  = (float)ctx.swap_chain.extent.width;
			viewport.height = (float)ctx.swap_chain.extent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor = {};
			scissor.offset = { 0, 0 };
			scissor.extent = ctx.swap_chain.extent;

			vkCmdSetViewport(cmds, 0, 1, &viewport);
			vkCmdSetScissor(cmds, 0, 1, &scissor);
		}

		{
			vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1,
				&frame_data[cur_frame].ubo_descriptor_set, 0, nullptr);
		}

		{
			TracyVkZone(ctx.tracy_ctx, cmds, "draw chunks");
		
			vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			
			chunk_renderer.draw_chunks(cmds);
		}

		ctx.imgui_draw(cmds);
	}

	TracyVkCollect(ctx.tracy_ctx, cmds);

	vkCmdEndRenderPass(cmds);

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

Renderer::Renderer (GLFWwindow* window, char const* app_name): ctx{window, app_name} {
	ZoneScoped;

	color_format = ctx.swap_chain.format.format;
	depth_format = find_depth_format(ctx.pdev);

	max_msaa_samples = get_max_usable_multisample_count(ctx.pdev);
	msaa = 1;
	//msaa = min(msaa, max_msaa_samples);

	init_cmd_pool = create_one_time_command_pool(ctx.dev, ctx.queues.families.graphics_family);

#ifdef TRACY_ENABLE
	ctx.init_vk_tracy(init_cmd_pool);
#endif

	create_frame_data();

	chunk_renderer.create(ctx.dev, ctx.pdev, FRAMES_IN_FLIGHT);
	shaders.init(ctx.dev);
	
	create_descriptor_pool();
	create_ubo_buffers();
	create_descriptors();

	render_pass = create_renderpass(color_format, depth_format, msaa);
	create_pipeline_layout();
	{
		VertexAttributes attribs;
		ChunkVertex::attributes(attribs);

		create_pipeline(msaa, shaders.get(ctx.dev, "chunks"), attribs);
	}

	upload_static_data();
}
Renderer::~Renderer () {
	ZoneScoped;

	//vkQueueWaitIdle(ctx.queues.graphics_queue);
	vkDeviceWaitIdle(ctx.dev);

	destroy_static_data();

	destroy_ubo_buffers();

	if (pipeline)
		vkDestroyPipeline(ctx.dev, pipeline, nullptr);
	vkDestroyPipelineLayout(ctx.dev, pipeline_layout, nullptr);
	vkDestroyRenderPass(ctx.dev, render_pass, nullptr);

	vkDestroyDescriptorSetLayout(ctx.dev, descriptor_layout, nullptr);
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

	//mesh_mem = uploader.upload(ctx.dev, ctx.pdev, meshes);

	ctx.imgui_create(cmds, FRAMES_IN_FLIGHT);

	end_init_cmds(cmds);

	uploader.end(ctx.dev);
}
void Renderer::destroy_static_data () {
	//for (auto& b : meshes)
	//	vkDestroyBuffer(ctx.dev, b.vkbuf, nullptr);
	//vkFreeMemory(ctx.dev, mesh_mem, nullptr);

	ctx.imgui_destroy();
}

////
void Renderer::create_descriptor_pool () {
	VkDescriptorPoolSize pool_sizes[2] = {};
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[0].descriptorCount = (uint32_t)SWAP_CHAIN_SIZE; // for imgui TODO: FRAMES_IN_FLIGHT instead, or even just 1? imgui won't change the image sampler per frame

	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[1].descriptorCount = (uint32_t)FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.poolSizeCount = ARRLEN(pool_sizes);
	info.pPoolSizes = pool_sizes;
	info.maxSets = (uint32_t)(
		SWAP_CHAIN_SIZE // for imgui
		+ FRAMES_IN_FLIGHT); // for ubo

	VK_CHECK_RESULT(vkCreateDescriptorPool(ctx.dev, &info, nullptr, &descriptor_pool));
}

void Renderer::create_descriptors () {
	{ // create descriptor layout
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		//binding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = 1;
		info.pBindings = &binding;

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ctx.dev, &info, nullptr, &descriptor_layout));
	}

	{ // create descriptor sets
		VkDescriptorSetLayout layouts[FRAMES_IN_FLIGHT];
		for (auto& l : layouts)
			l = descriptor_layout;

		VkDescriptorSetAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		info.descriptorPool = descriptor_pool;
		info.descriptorSetCount = FRAMES_IN_FLIGHT;
		info.pSetLayouts = layouts;

		VkDescriptorSet sets[FRAMES_IN_FLIGHT]; // TODO: this code would prefer array of sets (SOA) to current FrameData AOS
		VK_CHECK_RESULT(vkAllocateDescriptorSets(ctx.dev, &info, sets));

		for (int i=0; i<FRAMES_IN_FLIGHT; ++i) {
			frame_data[i].ubo_descriptor_set = sets[i];

			VkDescriptorBufferInfo buf_info = {};
			buf_info.buffer = frame_data[i].ubo_buffer;
			buf_info.offset = 0;
			buf_info.range = sizeof(ViewUniforms); // or VK_WHOLE_SIZE

			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = sets[i];
			write.dstBinding = 0;
			write.dstArrayElement = 0;
			write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write.descriptorCount = 1;
			write.pBufferInfo = &buf_info;
			//write.pImageInfo = nullptr;
			//write.pTexelBufferView = nullptr;
			vkUpdateDescriptorSets(ctx.dev, 1, &write, 0, nullptr);
		}
	}
}

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

void Renderer::create_pipeline_layout () {
	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.setLayoutCount = 1;
	info.pSetLayouts = &descriptor_layout;
	info.pushConstantRangeCount = 0;
	info.pPushConstantRanges = nullptr;

	VK_CHECK_RESULT(vkCreatePipelineLayout(ctx.dev, &info, nullptr, &pipeline_layout));
}

void Renderer::create_pipeline (int msaa, Shader* shader, VertexAttributes& attribs) {
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

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(ctx.dev, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));
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
void Renderer::create_framebuffers (int2 size, VkFormat color_format, int msaa) {
	//color_buffer = create_render_buffer(size, color_format,
	//	VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	//	VK_IMAGE_ASPECT_COLOR_BIT, msaa);

	depth_buffer = create_render_buffer(size, depth_format,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT, msaa);
}
void Renderer::destroy_framebuffers () {
	//vkDestroyImageView(ctx.dev, color_buffer.image_view, nullptr);
	//vkDestroyImage(ctx.dev, color_buffer.image, nullptr);
	//vkFreeMemory(ctx.dev, color_buffer.memory, nullptr);

	vkDestroyImageView(ctx.dev, depth_buffer.image_view, nullptr);
	vkDestroyImage(ctx.dev, depth_buffer.image, nullptr);
	vkFreeMemory(ctx.dev, depth_buffer.memory, nullptr);
}

//// Renderpass creation
RenderBuffer Renderer::create_render_buffer (int2 size, VkFormat format, VkImageUsageFlags usage, VkImageLayout initial_layout, VkMemoryPropertyFlags props, VkImageAspectFlags aspect, int msaa) {
	RenderBuffer buf;
	buf.image = create_image(ctx.dev, ctx.pdev, size, format, VK_IMAGE_TILING_OPTIMAL, usage, initial_layout, props, &buf.memory, 1, (VkSampleCountFlagBits)msaa);
	buf.image_view = create_image_view(ctx.dev, buf.image, format, aspect);
	return buf;
}

VkRenderPass Renderer::create_renderpass (VkFormat color_format, VkFormat depth_format, int msaa) {
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
	subpass.pDepthStencilAttachment = nullptr;//&depth_attachment_ref;

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
	info.attachmentCount = 1;//msaa > 1 ? 3 : 2;
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
