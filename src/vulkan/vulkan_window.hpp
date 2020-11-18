#pragma once
#include "common.hpp"
#include "vulkan_helper.hpp"
#include "TracyVulkan.hpp"

struct GLFWwindow;

namespace vk {

static constexpr int SWAP_CHAIN_SIZE = 3;

#ifdef VK_VALIDATION_LAYERS
inline constexpr const char* VALIDATION_LAYERS[] = {
	"VK_LAYER_KHRONOS_validation",
};
#endif

inline constexpr const char* DEVICE_EXTENSIONS[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,

#ifdef TRACY_ENABLE
	"VK_EXT_calibrated_timestamps",
#endif

};

#define GPU_DEBUG_MARKERS 1

//// Vulkan Window Context to encapsulate most of the initialization
struct QueueFamilies {
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
	QueueFamilies				families;

	VkQueue						graphics_queue;
	VkQueue						async_compute_queue;
	VkQueue						async_transfer_queue;
	VkQueue						present_queue;
};

struct SwapChainSupport {
	VkSurfaceCapabilitiesKHR		caps;
	std::vector<VkSurfaceFormatKHR>	formats;
	std::vector<VkPresentModeKHR>	present_modes;
};
struct SwapChain {
	VkSwapchainKHR				swap_chain;
	VkSurfaceFormatKHR			format;
	VkPresentModeKHR			present_mode;
	VkExtent2D					extent;

	struct SwapChainImage {
		VkImage					image;
		VkImageView				image_view;
		VkFramebuffer			framebuffer;
	};
	std::vector<SwapChainImage>	images;
};

struct VulkanWindowContext {
	NO_MOVE_COPY_CLASS(VulkanWindowContext)
	public:

	VkInstance					instance;
	VkPhysicalDevice			pdev;
	VkDevice					dev;
	VkSurfaceKHR				surface;
	Queues						queues;

#ifdef VK_VALIDATION_LAYERS
	VkDebugUtilsMessengerEXT	debug_messenger;
#endif
	DebugMarker					dbgmarker;

	std::vector<char const*>	enabled_layers;
	std::vector<char const*>	enabled_extensions;

	VkRenderPass				dummy_render_pass;

	SwapChain					swap_chain;

#ifdef TRACY_ENABLE
	TracyVkCtx					tracy_ctx;
	void init_vk_tracy (VkCommandPool one_time_cmd_pool);
#endif

	VkDescriptorPool			imgui_descriptor_pool;

	GLFWwindow*					glfw_window; // keep glfw window ptr around for simplicity
	int2						wnd_size;

	uint32_t					image_count;
	uint32_t					image_index;

	VulkanWindowContext (GLFWwindow* window, char const* app_name);
	~VulkanWindowContext ();

	//// Called by user
	void aquire_image (VkSemaphore image_available_semaphore);
	void present_image (VkSemaphore render_finished_semaphore);

	//// Called by user
	void imgui_create (VkCommandBuffer one_time_cmds, int frames_count);
	void imgui_destroy ();
	void imgui_draw (VkCommandBuffer cmds);

	//// Internal
	// swap chain needs to create frambuffers, which depend on a renderpass (actually just the layout of the attachments)
	// instead of forcing the user of VulkanWindowContext to provide his final renderpass and the attachments
	// we simply create a dummy VkRenderPass which actually acts like a FramebufferLayout (which does not exist in vk)
	void create_dummy_render_pass (VkFormat color_format);

	void create_swap_chain (int swap_chain_size);
	void destroy_swap_chain ();

	void recreate_swap_chain (int swap_chain_size);
};

#if GPU_DEBUG_MARKERS
	struct _ScopedGpuTrace {
		VulkanWindowContext& ctx;
		VkCommandBuffer cmds;
		_ScopedGpuTrace (VulkanWindowContext& ctx, VkCommandBuffer cmds, char const* name): ctx{ctx}, cmds{cmds} {
			ctx.dbgmarker.begin_marker(ctx.dev, cmds, name);
		}
		~_ScopedGpuTrace () {
			ctx.dbgmarker.end_marker(ctx.dev, cmds);
		}
	};

	#define GPU_TRACE(wndctx, cmds, name) \
		_ScopedGpuTrace __scoped_##__COUNTER__ ((wndctx), (cmds), (name)); \
		TracyVkZone((wndctx).tracy_ctx, (cmds), (name))

	#define GPU_DBG_NAME(wndctx, obj, name) (wndctx).dbgmarker.set_name((wndctx).dev, (obj), (name))
	#define GPU_DBG_NAMEI(wndctx, obj, name, idx) (wndctx).dbgmarker.set_name((wndctx).dev, (obj), prints((name), (idx)).c_str())
#else
	#define GPU_TRACE(wndctx, cmds, name) TracyVkZone(wndctx.tracy_ctx, cmds, name)
	#define GPU_DBG_NAME(wndctx, obj, name)
	#define GPU_DBG_NAMEI(wndctx, obj, name, idx)
#endif

//// Instance creation
void set_debug_utils_messenger_create_info_ext (VkDebugUtilsMessengerCreateInfoEXT* info, PFN_vkDebugUtilsMessengerCallbackEXT callback);

VkDebugUtilsMessengerEXT create_debug_utils_messenger_ext (VkInstance instance, PFN_vkDebugUtilsMessengerCallbackEXT callback);
void destroy_debug_utils_messenger_ext (VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger);

VkInstance create_instance (char const* app_name, PFN_vkDebugUtilsMessengerCallbackEXT callback,
	std::vector<char const*>& extensions, std::vector<char const*>& layers);

//// Device creation
QueueFamilies pick_queue_families (VkPhysicalDevice device, VkSurfaceKHR surface);

bool check_device_extensions (VkPhysicalDevice device);

SwapChainSupport query_swap_chain_support (VkPhysicalDevice device, VkSurfaceKHR surface);

VkPhysicalDevice select_device (VkInstance instance, VkSurfaceKHR surface, QueueFamilies* out_queue_families);

VkDevice create_logical_device (
	VkPhysicalDevice physical_device, std::vector<char const*> const& enabled_layers, Queues* queues, DebugMarker* dbg_marker);

//// Swap chain creation
VkSurfaceFormatKHR choose_swap_surface_format (std::vector<VkSurfaceFormatKHR> const& formats);
VkPresentModeKHR choose_swap_present_mode (std::vector<VkPresentModeKHR> const& present_modes);
VkExtent2D choose_swap_extent (VkSurfaceCapabilitiesKHR const& caps, int2 window_size);

} // namespace vk
