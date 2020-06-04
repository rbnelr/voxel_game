#pragma once
#include "kissmath.hpp"
#include "vulkan/vulkan.h"
#include <vector>
#include <memory>

struct GLFWwindow;

namespace vk {
	inline void _check_result (VkResult res, char const* expr) {
		if (res == VK_SUCCESS)
			return;

		throw std::runtime_error(expr);
	}

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define TO_STRING(x) #x

#define VK_CHECK_RESULT(expr) vk::_check_result(expr, "[Vulkan] Fatal error: " TO_STRING(expr))

	static constexpr int SWAP_CHAIN_SIZE = 3;
	static constexpr int FRAMES_IN_FLIGHT = 2;

	struct VulkanQueuesFamilies {
		// graphics, compute and transfer queue (if graphics are supported there has to be one 'universal' queue)
		uint32_t graphics_family = 0;
		// async compute queue (can also do transfers)
		uint32_t async_compute_family = 0;
		// async transfer queue
		uint32_t async_transfer_family = 0;
		// present queue
		uint32_t present_family = 0;

		bool has_graphics_family = false;
		bool has_async_compute_family = false;
		bool has_async_transfer_family = false;
		bool has_present_family = false;
	};
	struct Queues {
		VulkanQueuesFamilies		families;

		VkQueue						graphics_queue;
		VkQueue						async_compute_queue;
		VkQueue						async_transfer_queue;
		VkQueue						present_queue;
	};

	struct SwapChainSupport {
		VkSurfaceCapabilitiesKHR caps;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};
	struct SwapChain {
		SwapChainSupport			support;

		VkSwapchainKHR				swap_chain = VK_NULL_HANDLE;
		VkSurfaceFormatKHR			format;
		VkExtent2D					extent;

		struct ImageData {
			VkImage					image;
			VkImageView				image_view;
			VkFramebuffer			framebuffer;
		};
		std::vector<ImageData>		images;
	};

	struct RenderBuffer {
		VkImage						image;
		VkImageView					image_view;

		// Dedicated Memory allocation for now
		VkDeviceMemory				memory;
	};

	struct FrameData {
		VkCommandPool				command_pool;
		VkCommandBuffer				command_buffer;

		VkSemaphore					image_available_semaphore;
		VkSemaphore					render_finished_semaphore;
		VkFence						fence;
	};

	struct MemoryPool {
		VkDeviceMemory				memory;
		VkDeviceSize				mem_size;

		VkDeviceSize				cur_ptr;

		VkBuffer alloc (VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage);

		VkBuffer reset () {
			cur_ptr = 0;
		}
	};

	struct Vulkan {
		VkInstance					instance;
		VkSurfaceKHR				surface;
		VkPhysicalDevice			physical_device;
		VkDevice					device;
		Queues						queues;

		std::vector<char const*>	enabled_layers;
		VkFormat					color_format;
		VkFormat					depth_format;
		VkSampleCountFlagBits		max_msaa_samples;
		VkDebugUtilsMessengerEXT	debug_messenger;

		RenderBuffer				color_buffer;
		RenderBuffer				depth_buffer;

		VkRenderPass				render_pass;

		VkDescriptorPool			descriptor_pool;

		VkCommandPool				one_time_command_pool;

		SwapChain					swap_chain;
		std::vector<FrameData>		frame_data;

		MemoryPool					static_memory;

		int2						cur_size;
		uint32_t					cur_image_index;
		int							cur_frame = 0;

		//
		VkDescriptorSetLayout		descriptor_set_layout;
		VkDescriptorSet				common_ubos_descriptor_sets[FRAMES_IN_FLIGHT];
		VkBuffer					common_ubos_buffer[FRAMES_IN_FLIGHT];

		VkPipelineLayout			skybox_pipeline_layout;
		VkPipeline					skybox_pipeline;


		static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

		void create_framebuffers (int2 size, VkFormat color_format);
		void cleanup_framebuffers ();

		void create_swap_chain (int2 window_size, int image_count);
		void cleanup_swap_chain ();

		VkCommandBuffer begin_one_time_commands ();
		void end_one_time_commands (VkCommandBuffer buf);

		Vulkan (GLFWwindow* glfw_window, char const* app_name);

		void recreate_swap_chain ();

		~Vulkan ();

		void render_begin ();
		void render_end ();

		bool frame_start ();
		void frame_end ();
	};

}
using vk::Vulkan;

extern std::unique_ptr<Vulkan> vulkan;

namespace vk {

	////// Wrappers

	//// UBOs
	struct UBOLayoutCheck {
		int offset = 0;
		bool valid = true;

		static constexpr int N = 4;

		static constexpr int align (int offs, int alignment) {
			int mod = offs % alignment;
			return offs + (mod == 0 ? 0 : alignment - mod);
		}

		template <typename T>
		static constexpr int get_align ();

		template<> static constexpr int get_align<float    > () { return N; }
		template<> static constexpr int get_align<int      > () { return N; }
		//template<> static constexpr int get_align<glsl_bool> () { return N; }
		template<> static constexpr int get_align<float2   > () { return 2*N; }
		template<> static constexpr int get_align<float3   > () { return 4*N; }
		template<> static constexpr int get_align<float4   > () { return 4*N; }
		template<> static constexpr int get_align<float4x4 > () { return 4*N; }

		template<typename T>
		constexpr void member (int offs) {
			offset = align(offset, get_align<T>());
			valid = valid && offset == offs;
			offset += sizeof(T);
		}

		template <typename T>
		constexpr bool is_valid () {
			return valid && sizeof(T) == offset;
		}
	};

#if 0
	struct PipelineConfig {
		VkPipelineInputAssemblyStateCreateInfo input_assembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		VkPipelineDepthStencilStateCreateInfo depth_stencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		VkPipelineColorBlendAttachmentState color_blend_attachment = {};

		struct DepthRange {
			float min;
			float max;
		} depth_range = {};
	};

	constexpr PipelineConfig default_pipeline () {
		PipelineConfig cfg;

		cfg.input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		cfg.input_assembly.primitiveRestartEnable = VK_FALSE;

		cfg.rasterizer.depthClampEnable = VK_FALSE;
		cfg.rasterizer.rasterizerDiscardEnable = VK_FALSE;
		cfg.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		cfg.rasterizer.lineWidth = 1.0f;
		cfg.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		cfg.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		cfg.rasterizer.depthBiasEnable = VK_FALSE;
		cfg.rasterizer.depthBiasConstantFactor = 0.0f;
		cfg.rasterizer.depthBiasClamp = 0.0f;
		cfg.rasterizer.depthBiasSlopeFactor = 0.0f;

		cfg.multisampling.sampleShadingEnable = VK_FALSE;
		cfg.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // Configured dynamicly
		cfg.multisampling.minSampleShading = 1.0f;
		cfg.multisampling.pSampleMask = nullptr;
		cfg.multisampling.alphaToCoverageEnable = VK_FALSE;
		cfg.multisampling.alphaToOneEnable = VK_FALSE;

		cfg.depth_stencil.depthTestEnable = VK_TRUE;
		cfg.depth_stencil.depthWriteEnable = VK_TRUE;
		cfg.depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
		cfg.depth_stencil.depthBoundsTestEnable = VK_FALSE;
		cfg.depth_stencil.minDepthBounds = 0.0f;
		cfg.depth_stencil.maxDepthBounds = 1.0f;
		cfg.depth_stencil.stencilTestEnable = VK_FALSE;
		cfg.depth_stencil.front = {};
		cfg.depth_stencil.back = {};

		cfg.color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		cfg.color_blend_attachment.blendEnable = VK_FALSE;
		cfg.color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		cfg.color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		cfg.color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		cfg.color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		cfg.color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		cfg.color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

		cfg.depth_range = { 0.0f, 1.0f };

		return cfg;
	}

	constexpr auto PIPELINE_DEFAULT = default_pipeline();

	struct Pipeline {
		VkPipeline pipeline;

		Pipeline (char const* shader_name, PipelineLayout const& layout, PipelineConfig const& config) {

			// Load (cached?) shaders
			//auto vert_module = vk_create_shader_module("shaders/shader.vert.spv");
			//auto frag_module = vk_create_shader_module("shaders/shader.frag.spv");

			// 
			VkPipelineShaderStageCreateInfo shader_stages[2] = {};

			shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			//shader_stages[0].module = vert_module;
			shader_stages[0].pName = "main";

			shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			//shader_stages[1].module = frag_module;
			shader_stages[1].pName = "main";

			//auto input_binding = Vertex::get_input_binding();
			//auto input_attribs = Vertex::get_input_attribs();
			//
			//VkPipelineVertexInputStateCreateInfo vert_input = {};
			//vert_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			//vert_input.vertexBindingDescriptionCount = 1;
			//vert_input.pVertexBindingDescriptions = &input_binding;
			//vert_input.vertexAttributeDescriptionCount = (uint32_t)input_attribs.size();
			//vert_input.pVertexAttributeDescriptions = input_attribs.data();

			VkViewport viewport = {};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width  = (float)vulkan->cur_size.x;
			viewport.height = (float)vulkan->cur_size.y;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor = {};
			scissor.offset = { 0, 0 };
			scissor.extent = vulkan->swap_chain.extent;

			VkPipelineViewportStateCreateInfo viewport_state = {};
			viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewport_state.viewportCount = 1;
			viewport_state.pViewports = &viewport;
			viewport_state.scissorCount = 1;
			viewport_state.pScissors = &scissor;

			config.multisampling.rasterizationSamples = vulkan->max_msaa_samples;

			VkPipelineColorBlendStateCreateInfo color_blending = {};
			color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			color_blending.logicOpEnable = VK_FALSE;
			color_blending.logicOp = VK_LOGIC_OP_COPY;
			color_blending.attachmentCount = 1;
			color_blending.pAttachments = &config.color_blend_attachment;
			color_blending.blendConstants[0] = 0.0f;
			color_blending.blendConstants[1] = 0.0f;
			color_blending.blendConstants[2] = 0.0f;
			color_blending.blendConstants[3] = 0.0f;

			VkGraphicsPipelineCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			info.stageCount				= 2;
			info.pStages				= shader_stages;
			//info.pVertexInputState		= &vert_input;
			info.pInputAssemblyState	= &config.input_assembly;
			info.pViewportState			= &viewport_state;
			info.pRasterizationState	= &config.rasterizer;
			info.pMultisampleState		= &config.multisampling;
			info.pDepthStencilState		= &config.depth_stencil;
			info.pColorBlendState		= &color_blending;
			info.pDynamicState			= nullptr;
			info.layout					= pipeline_layout;
			info.renderPass				= vulkan->render_pass;
			info.subpass				= 0;
			info.basePipelineHandle		= VK_NULL_HANDLE;
			info.basePipelineIndex		= -1;

			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));

			//vkDestroyShaderModule(device, vert_module, nullptr);
			//vkDestroyShaderModule(device, frag_module, nullptr);

		}
	};
#endif
}

// dummies
struct Texture2D {

};

struct Texture2DArray {

};

struct Sampler {

	Sampler () {}
};

struct Shader {

	Shader (char const* name) {}
};

template <typename VERTEX>
struct Mesh {

};

struct Attributes {

	template <typename T>
	void add (int index, char const* name, size_t stride, size_t offset, bool normalized=false) {}

	template <typename T>
	void add_int (int index, char const* name, size_t stride, size_t offset, bool normalized=false) {}
};
