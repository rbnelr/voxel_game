#pragma once
#include "common.hpp"
#include "vulkan_helper.hpp"
#include "vulkan_shaders.hpp"
#include "vulkan_window.hpp"
#include "chunk_renderer.hpp"
#include "graphics.hpp"
#include "engine/camera.hpp"

#include "TracyVulkan.hpp"

namespace vk {

static constexpr int FRAMES_IN_FLIGHT = 2;

inline float4x4 clip_ogl2vk (float4x4 const& m) {
	float4x4 ret = m;
	ret.arr[1][1] = -ret.arr[1][1];
	return ret;
}

struct ViewUniforms {
	float4x4 world_to_cam;
	float4x4 cam_to_world;
	float4x4 cam_to_clip;
	float4x4 clip_to_cam;
	float4x4 world_to_clip;
	float    clip_near;
	float    clip_far;
	float2   viewport_size;

	void set (Camera_View const& view, int2 viewport_size) {
		auto cam2clip = clip_ogl2vk(view.cam_to_clip);
		auto clip2cam = clip_ogl2vk(view.clip_to_cam);

		memset(this, 0, sizeof(*this)); // zero padding

		world_to_cam = (float4x4)view.world_to_cam;
		cam_to_world = (float4x4)view.cam_to_world;
		cam_to_clip = cam2clip;
		clip_to_cam = clip2cam;
		world_to_clip = cam2clip * (float4x4)view.world_to_cam;
		clip_near = view.clip_near;
		clip_far = view.clip_far;
		this->viewport_size = (float2)viewport_size;
	}
};

struct Renderer {
	VulkanWindowContext			ctx;

	ShaderManager				shaders;

	VkFormat					wnd_color_format;

	// Intermediary frame buffer formats
	VkFormat					fb_color_format;
	VkFormat					fb_depth_format;

	int							max_msaa_samples;
	int							msaa = 1;

	int							cur_frame = 0;

	VkDescriptorPool			descriptor_pool;
	VkCommandPool				init_cmd_pool;

	struct FrameData {
		VkCommandPool				command_pool;
		VkCommandBuffer				command_buffer;

		VkSemaphore					image_available_semaphore;
		VkSemaphore					render_finished_semaphore;
		VkFence						fence;

		VkDescriptorSet				ubo_descriptor_set;
		VkBuffer					ubo_buf;
		size_t						ubo_mem_offs;
	};
	FrameData					frame_data[FRAMES_IN_FLIGHT];

	VkDeviceMemory				ubo_memory;
	void*						ubo_mem_ptr;

	VkDeviceMemory				mesh_mem;
	VkDeviceMemory				tex_mem;

	VkDescriptorSetLayout		common_descriptor_layout; // set 0

	//// Renderpasses
	// Main renderpass (skybox, 3d world and 3d first person)
	// cleared with skybox, rendered at window_res * renderscale
	VkRenderPass				main_renderpass;
	RenderBuffer				main_color;
	RenderBuffer				main_depth;
	VkFramebuffer				main_framebuffer;
	
	VkDescriptorSetLayout		main_descriptor_layout; // set 1
	VkPipelineLayout			main_pipeline_layout;
	VkPipeline					main_pipeline;

	VkSampler					main_sampler;
	Texture						tilemap_img;

	// UI renderpass (game ui + imgui)
	// initial image is main_color rescaled, rendered at window_res
	VkRenderPass				ui_renderpass;

	VkDescriptorSetLayout		rescale_descriptor_layout; // set 1
	VkPipelineLayout			rescale_pipeline_layout;
	VkPipeline					rescale_pipeline;

	VkDescriptorSet				rescale_descriptor_set;
	VkSampler					rescale_sampler;

	ChunkRenderer				chunk_renderer;

	Assets						assets;

	float						renderscale = 1.0f;
	int2						renderscale_size = -1;
	bool						renderscale_nearest = false;

	void renderscale_imgui () {
		if (!imgui_push("Renderscale")) return;

		ImGui::SliderFloat("scale", &renderscale, 0.02f, 2.0f);
		ImGui::SameLine();
		ImGui::Text("= %4d x %4d px", renderscale_size.x, renderscale_size.y);

		ImGui::Checkbox("renderscale nearest", &renderscale_nearest);

		imgui_pop();
	}

	Renderer (GLFWwindow* window, char const* app_name, json const& blocks_json);
	~Renderer ();

	void set_view_uniforms (Camera_View& view, int2 viewport_size);
	void render_frame (GLFWwindow* window, RenderData& data);

	void frame_begin (GLFWwindow* window);
	void submit (GLFWwindow* window, VkCommandBuffer buf);

	void upload_static_data ();
	void destroy_static_data ();

	////
	void create_descriptor_pool ();

	void create_ubo_buffers ();
	void destroy_ubo_buffers ();

	void create_common_descriptors ();
	void create_rescale_descriptors ();
	void update_rescale_img_descr ();

	VkPipeline create_main_pipeline (Shader* shader, VkRenderPass renderpass, VkPipelineLayout layout, int msaa, VertexAttributes attribs);
	VkPipeline create_rescale_pipeline (Shader* shader, VkRenderPass renderpass, VkPipelineLayout layout);
	
	//// Create per-frame data
	void create_frame_data ();

	//// Framebuffer creation
	VkFormat find_color_format () {
		return find_supported_format(ctx.pdev,
			{ VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R8G8B8A8_SRGB },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
		);
	}
	VkFormat find_depth_format () {
		return find_supported_format(ctx.pdev,
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	RenderBuffer create_render_buffer (int2 size, VkFormat format, VkImageUsageFlags usage, VkImageLayout initial_layout, VkMemoryPropertyFlags props, VkImageAspectFlags aspect, int msaa);

	VkRenderPass create_main_renderpass (VkFormat color_format, VkFormat depth_format, int msaa);
	VkRenderPass create_ui_renderpass (VkFormat color_format);

	void create_main_framebuffer (int2 size, VkFormat color_format, VkFormat depth_format, int msaa);
	void destroy_main_framebuffer ();
	void recreate_main_framebuffer (int2 wnd_size);

	//// One time commands
	VkCommandBuffer begin_init_cmds ();
	void end_init_cmds (VkCommandBuffer buf);
};

} // namespace vk

using vk::Renderer;
