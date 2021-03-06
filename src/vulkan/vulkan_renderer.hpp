#pragma once
#include "common.hpp"
#include "vulkan_helper.hpp"
#include "vulkan_shaders.hpp"
#include "vulkan_window.hpp"
#include "vulkan_debug_drawer.hpp"
#include "vulkan_screenshot.hpp"
#include "vk_chunk_renderer.hpp"
#include "vulkan_staging.hpp"
#include "engine/camera.hpp"

#include "TracyVulkan.hpp"

struct GLFWwindow;

namespace vk {

static constexpr int FRAMES_IN_FLIGHT = 2;

inline float4x4 clip_ogl2vk (float4x4 const& m) {
	float4x4 ret = m;
	ret.arr[1][1] = -ret.arr[1][1]; // y-down in clip space for vulkan vs y-up in opengl
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

class VulkanRenderer : public Renderer {
public:

	VulkanWindowContext			ctx;

	PipelineManager				pipelines;

	VkFormat					wnd_color_format;

	// Intermediary frame buffer formats
	VkFormat					fb_color_format; // hopefully f16
	VkFormat					fb_depth_format;
	VkFormat					fb_float_format;
	VkFormat					fb_vec2_format;
	VkFormat					fb_vec3_format;

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

	StagingBuffers				staging;

	VkDeviceMemory				ubo_memory;
	void*						ubo_mem_ptr;

	VkBuffer					block_meshes_buf;

	VkDeviceMemory				mesh_mem;
	VkDeviceMemory				tex_mem;

	VkDescriptorSetLayout		common_descriptor_layout; // set 0

	//// Renderpasses
	// Main renderpass (skybox, 3d world and 3d first person)
	// cleared with skybox, rendered at window_res * renderscale
	VkRenderPass				main_renderpass;
	RenderBuffer				main_color, main_normal, main_depth;
	VkFramebuffer				main_framebuffer;

	VkSampler					main_sampler;
	Texture						tilemap_img;

	// SSAO Renderpass
	VkRenderPass				ssao_renderpass;
	RenderBuffer				ssao_fac;
	VkFramebuffer				ssao_framebuffer;

	// UI renderpass (game ui + imgui)
	// initial image is main_color rescaled, rendered at window_res
	VkRenderPass				ui_renderpass;

	VkDescriptorSetLayout		ssao_descriptor_layout; // set 1
	VkPipelineLayout			ssao_pipeline_layout;
	Pipeline*					ssao_pipeline;

	VkDescriptorSetLayout		rescale_descriptor_layout; // set 1
	VkPipelineLayout			rescale_pipeline_layout;
	Pipeline*					rescale_pipeline;

	VkDescriptorSet				ssao_descriptor_set;
	VkDescriptorSet				rescale_descriptor_set;

	VkSampler					framebuf_sampler, framebuf_sampler_nearest;

	ChunkRenderer				chunk_renderer;
	DebugDrawer					debug_drawer;

	Screenshots					screenshot;

	float						renderscale = 1.0f;
	int2						renderscale_size = -1;
	bool						renderscale_nearest = false;
	bool						renderscale_nearest_changed = false;

	bool						wireframe = false;

	virtual void screenshot_imgui (Input& I) {
		screenshot.imgui();
	}
	virtual void graphics_imgui (Input& I) {
		if (imgui_push("Renderscale")) {
			ImGui::Text("res: %4d x %4d px (%5.2f Mpx)", renderscale_size.x, renderscale_size.y, (float)(renderscale_size.x * renderscale_size.y) / 1000 / 1000);
			ImGui::SliderFloat("renderscale", &renderscale, 0.02f, 2.0f);
			
			renderscale_nearest_changed = ImGui::Checkbox("renderscale nearest", &renderscale_nearest);

			imgui_pop();
		}

		ImGui::Checkbox("wireframe", &wireframe);

		staging.imgui();
	}

	virtual void chunk_renderer_imgui (Chunks& chunks) {
		chunk_renderer.imgui(chunks);
	}

	virtual bool get_vsync () {
		return false;
	}
	virtual void set_vsync (bool state) {
		// not implemented
	}

	VulkanRenderer (GLFWwindow* window, char const* app_name);
	virtual ~VulkanRenderer ();

	virtual void frame_begin (GLFWwindow* window, Input& I, kiss::ChangedFiles& changed_files);
	virtual void render_frame (GLFWwindow* window, Input& I, Game& game);
	
	void set_view_uniforms (Camera_View& view, int2 viewport_size);
	void submit (GLFWwindow* window, VkCommandBuffer buf);

	void upload_static_data ();
	void destroy_static_data ();

	////
	void create_descriptor_pool ();

	void create_ubo_buffers ();
	void destroy_ubo_buffers ();

	void create_common_descriptors ();

	void create_ssao_descriptors ();
	void update_ssao_img_descr ();

	void create_rescale_descriptors ();
	void update_rescale_img_descr ();

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

	VkFormat find_float_format () {
		return find_supported_format(ctx.pdev,
			{ VK_FORMAT_R16_SFLOAT, VK_FORMAT_R8_SNORM },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
		);
	}
	VkFormat find_vec2_format () {
		return find_supported_format(ctx.pdev,
			{ VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R8G8_SNORM },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
		);
	}
	VkFormat find_vec3_format () {
		return find_supported_format(ctx.pdev,
			{ VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R8G8B8A8_SNORM }, // no rgb16 format?
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
		);
	}

	RenderBuffer create_render_buffer (int2 size, VkFormat format, VkImageUsageFlags usage, VkImageLayout initial_layout, VkMemoryPropertyFlags props, VkImageAspectFlags aspect, int msaa);

	VkRenderPass create_main_renderpass (VkFormat color_format, VkFormat normal_format, VkFormat depth_format, int msaa);
	VkRenderPass create_ssao_renderpass (VkFormat color_format);
	VkRenderPass create_ui_renderpass (VkFormat color_format);

	void create_main_framebuffer (int2 size, VkFormat color_format, VkFormat normal_format, VkFormat depth_format, int msaa);
	void destroy_main_framebuffer ();
	void recreate_main_framebuffer (int2 wnd_size);
	
	void create_ssao_framebuffer (int2 size, VkFormat color_format);
	void destroy_ssao_framebuffer ();

	//// One time commands
	VkCommandBuffer begin_init_cmds ();
	void end_init_cmds (VkCommandBuffer buf);
};

} // namespace vk
