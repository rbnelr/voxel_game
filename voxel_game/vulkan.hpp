#pragma once
//#include "util/clean_windows_h.hpp"
#include "vulkan/vulkan.h"
#include "kissmath.hpp"
#include <vector>
#include <memory>

struct GLFWwindow;

namespace vk {
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

		int2						cur_size;
		uint32_t					cur_image_index;
		int							cur_frame = 0;

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
