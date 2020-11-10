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

struct UploadData {
	void* data;
	size_t size;
	VkBufferUsageFlags usage;

	VkBuffer vkbuf;
	VkMemoryRequirements mem_req;
	size_t vkoffset;
};

struct StaticDataUploader {

	struct StagingAllocation {
		Allocation staging_buf;
		VkBuffer staging_target_buf;
	};

	VkCommandBuffer cmds;
	std::vector<StagingAllocation> allocs;
	
	VkDeviceMemory upload (VkDevice dev, VkPhysicalDevice pdev, std::vector<UploadData>& data) {
		// Create buffers and calculate offets into memory block
		size_t size = 0;
		uint32_t mem_req_bits = (uint32_t)-1;

		for (auto& b : data) {
			VkBufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			info.size = b.size;
			info.usage = b.usage;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VK_CHECK_RESULT(vkCreateBuffer(dev, &info, nullptr, &b.vkbuf));

			vkGetBufferMemoryRequirements(dev, b.vkbuf, &b.mem_req);

			size = align_up(size, b.mem_req.alignment);
			b.vkoffset = size;
			size += b.mem_req.size;

			mem_req_bits &= b.mem_req.memoryTypeBits;
		}

		// alloc cpu-visible staging buffer
		auto staging_buf = allocate_buffer(dev, pdev, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		// alloc gpu-resident memory and create a temporary target buffer for batching the upload of all buffers (vkCmdCopyBuffer cannot deal with VkDeviceMemory)
		VkBuffer staging_target_buf;
		VkDeviceMemory mem;

		VkBufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = size;
		info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VK_CHECK_RESULT(vkCreateBuffer(dev, &info, nullptr, &staging_target_buf));

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = size;
		alloc_info.memoryTypeIndex = find_memory_type(pdev, mem_req_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(dev, &alloc_info, nullptr, &mem));

		vkBindBufferMemory(dev, staging_target_buf, mem, 0);

		// upload buffers and bind buffers to memory
		void* ptr = nullptr;
		vkMapMemory(dev, staging_buf.mem, 0, size, 0, &ptr);

		for (auto& b : data) {
			memcpy((char*)ptr + b.vkoffset, b.data, b.size);

			vkBindBufferMemory(dev, b.vkbuf, mem, b.vkoffset);
		}

		vkUnmapMemory(dev, staging_buf.mem);

		// upload data by copying staging buffer to gpu
		VkBufferCopy copy_region = {};
		copy_region.srcOffset = 0;
		copy_region.dstOffset = 0;
		copy_region.size = size;
		vkCmdCopyBuffer(cmds, staging_buf.buf, staging_target_buf, 1, &copy_region);

		return mem;
	}

	void end (VkDevice dev) {
		for (auto& a : allocs) {
			vkDestroyBuffer(dev, a.staging_buf.buf   , nullptr);
			vkDestroyBuffer(dev, a.staging_target_buf, nullptr);
			vkFreeMemory   (dev, a.staging_buf.mem   , nullptr);
		}
	}
};

struct Renderer {
	VulkanWindowContext			ctx;

	ShaderManager				shaders;

	// Intermediary frame buffer formats
	VkFormat					color_format;
	VkFormat					depth_format;

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
		VkBuffer					ubo_buffer;
		size_t						ubo_mem_offset;
	};
	FrameData					frame_data[FRAMES_IN_FLIGHT];

	VkDeviceMemory				ubo_memory;
	VkDeviceMemory				mesh_mem;

	// Passes
	VkRenderPass				render_pass;
	//RenderBuffer				color_buffer;
	RenderBuffer				depth_buffer;

	VkDescriptorSetLayout		descriptor_layout;
	VkPipelineLayout			pipeline_layout;
	VkPipeline					pipeline;

	ChunkRenderer				chunk_renderer;

	Renderer (GLFWwindow* window, char const* app_name);
	~Renderer ();

	void set_view_uniforms (Camera_View& view, int2 viewport_size);
	void render_frame (GLFWwindow* window, RenderData& data);

	void frame_begin (GLFWwindow* window);
	void submit (GLFWwindow* window, VkCommandBuffer buf);

	void upload_static_data ();
	void destroy_static_data ();

	////
	void create_descriptor_pool ();

	void create_descriptors ();

	void create_ubo_buffers ();
	void destroy_ubo_buffers ();

	void create_pipeline_layout ();

	void create_pipeline (int msaa, Shader* shader, VertexAttributes& attribs);
	
	//// Create per-frame data
	void create_frame_data ();

	//// Framebuffer creation
	void create_framebuffers (int2 size, VkFormat color_format, int msaa);
	void destroy_framebuffers ();

	//// Renderpass creation
	RenderBuffer create_render_buffer (int2 size, VkFormat format, VkImageUsageFlags usage, VkImageLayout initial_layout, VkMemoryPropertyFlags props, VkImageAspectFlags aspect, int msaa);

	VkRenderPass create_renderpass (VkFormat color_format, VkFormat depth_format, int msaa);

	//// One time commands
	VkCommandBuffer begin_init_cmds ();
	void end_init_cmds (VkCommandBuffer buf);
};

} // namespace vk

using vk::Renderer;
