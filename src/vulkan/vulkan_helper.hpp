#pragma once
#include "vulkan/vulkan.h"
#include "../../kisslib/macros.hpp"
#include "../../kisslib/kissmath.hpp"
#include <vector>
#include <stdexcept>

#ifndef NDEBUG
	#define VK_VALIDATION_LAYERS
#endif

namespace vk {
#ifdef VK_VALIDATION_LAYERS
	static constexpr const char* validation_layers[] = {
		"VK_LAYER_KHRONOS_validation",
	};
#endif

	static constexpr const char* device_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	static constexpr int SWAP_CHAIN_SIZE = 3;
	static constexpr int FRAMES_IN_FLIGHT = 2;

////
	#define VK_CHECK_RESULT(expr) if ((expr) != VK_SUCCESS) { \
		throw std::runtime_error("[Vulkan] Fatal error: " TO_STRING(expr)); \
	}

	inline VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		if (messageSeverity & (0
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
				//| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
			)) return VK_FALSE;

		fprintf(stderr, "[Vulkan] %s\n", pCallbackData->pMessage);
		//clog(ERROR, "[Vulkan] %s\n", pCallbackData->pMessage);

		return VK_FALSE;
	}

	template <typename T, typename FUNC, typename... ARGS>
	inline std::vector<T> get_vector (FUNC func, ARGS... args) {
		uint32_t count = 0;
		func(args..., &count, nullptr);

		std::vector<T> vec (count);
		if (count > 0)
			func(args..., &count, vec.data());

		return vec;
	}

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

	struct MemoryPool {
		VkDeviceMemory				memory;
		VkDeviceSize				mem_size;

		VkDeviceSize				cur_ptr;

		VkBuffer alloc (VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage);

		VkBuffer reset () {
			cur_ptr = 0;
		}
	};

	//// Resource creation
	inline VkSampleCountFlagBits get_max_usable_multisample_count (VkPhysicalDevice device) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(device, &props);

		VkSampleCountFlags count_flags = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;

		VkSampleCountFlagBits counts[] = {
			VK_SAMPLE_COUNT_64_BIT,
			VK_SAMPLE_COUNT_32_BIT,
			VK_SAMPLE_COUNT_16_BIT,
			VK_SAMPLE_COUNT_8_BIT , 
			VK_SAMPLE_COUNT_4_BIT , 
			VK_SAMPLE_COUNT_2_BIT ,
		};
		for (auto count : counts) {
			if (count_flags & count)
				return count;
		}

		return VK_SAMPLE_COUNT_1_BIT;
	}

	inline uint32_t find_memory_type (VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties mem_props;
		vkGetPhysicalDeviceMemoryProperties(device, &mem_props);

		for (uint32_t i=0; i<mem_props.memoryTypeCount; ++i) {
			if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("[Vulkan] Fatal error: No suitable memory type found");
	}

	inline VkFormat find_supported_format (VkPhysicalDevice device, std::initializer_list<VkFormat> const& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(device, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			} else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("[Vulkan] Fatal error: No suitable format found");
	}

	inline VkFormat find_depth_format (VkPhysicalDevice device) {
		return find_supported_format(device,
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	inline bool has_stencil_component (VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	inline VkCommandPool create_one_time_command_pool (VkDevice device, uint32_t queue_family) {
		VkCommandPool pool;

		VkCommandPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		info.queueFamilyIndex = queue_family;
		info.flags = 0;
		VK_CHECK_RESULT(vkCreateCommandPool(device, &info, nullptr, &pool));

		return pool;
	}

	inline VkDeviceMemory alloc_memory (VkDevice device, VkPhysicalDevice pdevice, VkDeviceSize size, VkMemoryPropertyFlags props) {
		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = size;
		alloc_info.memoryTypeIndex = find_memory_type(pdevice, -1, props);

		VkDeviceMemory memory;
		VK_CHECK_RESULT(vkAllocateMemory(device, &alloc_info, nullptr, &memory));

		return memory;
	}

	inline VkImage create_image (VkDevice device, VkPhysicalDevice pdev, int2 size, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageLayout initial_layout, VkMemoryPropertyFlags props,
		VkDeviceMemory* out_image_memory, int mip_levels=1, VkSampleCountFlagBits samples=VK_SAMPLE_COUNT_1_BIT) {

		VkImageCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.imageType = VK_IMAGE_TYPE_2D;
		info.extent.width = (uint32_t)size.x;
		info.extent.height = (uint32_t)size.y;
		info.extent.depth = 1;
		info.mipLevels = (uint32_t)mip_levels;
		info.arrayLayers = 1;
		info.format = format;
		info.tiling = tiling;
		info.initialLayout = initial_layout;
		info.usage = usage;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.samples = samples;
		info.flags = 0;

		VkImage image;
		VK_CHECK_RESULT(vkCreateImage(device, &info, nullptr, &image));

		VkMemoryRequirements mem_req;
		vkGetImageMemoryRequirements(device, image, &mem_req);

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_req.size;
		alloc_info.memoryTypeIndex = find_memory_type(pdev, mem_req.memoryTypeBits, props);

		VkDeviceMemory image_memory;
		VK_CHECK_RESULT(vkAllocateMemory(device, &alloc_info, nullptr, &image_memory));

		vkBindImageMemory(device, image, image_memory, 0);

		*out_image_memory = image_memory;
		return image;
	}

	inline VkImageView create_image_view (VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect) {
		VkImageViewCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.image = image;
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.format = format;
		info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		info.subresourceRange.aspectMask = aspect;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;

		VkImageView image_view;
		VK_CHECK_RESULT(vkCreateImageView(device, &info, nullptr, &image_view));

		return image_view;
	}

	//// Instance creation
	inline void set_debug_utils_messenger_create_info_ext (VkDebugUtilsMessengerCreateInfoEXT* info, PFN_vkDebugUtilsMessengerCallbackEXT callback) {
		info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		info->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		info->pfnUserCallback = callback;
		info->pUserData = nullptr;
	}

	inline VkDebugUtilsMessengerEXT create_debug_utils_messenger_ext (VkInstance instance, PFN_vkDebugUtilsMessengerCallbackEXT callback) {
		VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;

		VkDebugUtilsMessengerCreateInfoEXT info = {};
		set_debug_utils_messenger_create_info_ext(&info, callback);

		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func)
			func(instance, &info, nullptr, &debug_messenger);

		return debug_messenger;
	}
	inline void destroy_debug_utils_messenger_ext (VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func)
			func(instance, debug_messenger, nullptr);
	}

	inline VkInstance create_instance (char const* app_name, PFN_vkDebugUtilsMessengerCallbackEXT callback,
			std::vector<char const*>& request_extensions, std::vector<char const*>* out_layers) {
		std::vector<char const*> layers;

		// Check extensions
		auto avail_extensions = get_vector<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);

		// Check validation layers
	#ifdef VK_VALIDATION_LAYERS
		{
			request_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

			auto avail_layers = get_vector<VkLayerProperties>(vkEnumerateInstanceLayerProperties);

			for (auto requested : validation_layers) {
				for (auto avail : avail_layers) {
					if (strcmp(requested, avail.layerName) == 0) {
						layers.push_back(requested);
						break;
					}
				}
			}
		}
	#endif

		// Create instance
		VkApplicationInfo app_info = {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = app_name;
		app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION(1,0,0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		info.pApplicationInfo = &app_info;

		info.enabledExtensionCount	= (uint32_t)request_extensions.size();
		info.ppEnabledExtensionNames = request_extensions.data();

		info.enabledLayerCount	= (uint32_t)layers.size();
		info.ppEnabledLayerNames = layers.data();

		VkDebugUtilsMessengerCreateInfoEXT dbg_create_info = {};
	#ifdef VK_VALIDATION_LAYERS
		{
			set_debug_utils_messenger_create_info_ext(&dbg_create_info, callback);
			info.pNext = &dbg_create_info;
		}
	#endif

		VkInstance instance = VK_NULL_HANDLE;

		VK_CHECK_RESULT(vkCreateInstance(&info, nullptr, &instance));

		*out_layers = std::move(layers);
		return instance;
	}

	//// Device creation
	inline VulkanQueuesFamilies pick_queue_families (VkPhysicalDevice device, VkSurfaceKHR surface) {

		VulkanQueuesFamilies families;

		auto queue_families = get_vector<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, device);

		int i = 0;
		for (auto& fam : queue_families) {
			bool graphics = (fam.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
			bool compute  = (fam.queueFlags & VK_QUEUE_COMPUTE_BIT ) != 0;
			bool transfer = (fam.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;

			bool universal_queue = graphics && compute && transfer;

			// pick first 'universal' queue
			if (!families.has_graphics_family && universal_queue) {
				families.graphics_family = i;
				families.has_graphics_family = true;
			} 
			// pick first dedicated compute queue
			else if (!families.has_async_compute_family && (compute && !graphics)) {
				families.async_compute_family = i;
				families.has_async_compute_family = true;
			}
			// pick first dedicated transfer queue
			else if (!families.has_async_transfer_family && (transfer && !graphics && !compute)) {
				families.async_transfer_family = i;
				families.has_async_transfer_family = true;
			}

			VkBool32 present_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);

			// pick first queue with present support, but prioritize the 'universal_queue'
			if ((!families.has_present_family || universal_queue) && present_support) {
				families.present_family = i;
				families.has_present_family = true;
			}

			i++;
		}

		return families;
	}

	inline bool check_device_extensions (VkPhysicalDevice device) {

		auto avail_extensions = get_vector<VkExtensionProperties>(vkEnumerateDeviceExtensionProperties, device, nullptr);

		// Check if any of the desired extensions is not suppored
		bool all_found = true;
		for (auto ext : device_extensions) {
			bool found = false;
			for (auto avail : avail_extensions) {
				if (strcmp(ext, avail.extensionName) == 0) {
					found = true;
					break;
				}
			}
			all_found = all_found && found;
		}

		return all_found;
	}

	inline SwapChainSupport query_swap_chain_support (VkPhysicalDevice device, VkSurfaceKHR surface) {
		SwapChainSupport details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.caps);
		details.formats = get_vector<VkSurfaceFormatKHR>(vkGetPhysicalDeviceSurfaceFormatsKHR, device, surface);
		details.present_modes = get_vector<VkPresentModeKHR>(vkGetPhysicalDeviceSurfacePresentModesKHR, device, surface);

		return details;
	}

	inline VkSurfaceFormatKHR choose_swap_surface_format (std::vector<VkSurfaceFormatKHR> const& formats) {
		// My machine supports
		// VK_FORMAT_R16G16B16A16_SFLOAT  VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
		// VK_FORMAT_A2B10G10R10_UNORM_PACK32  VK_COLOR_SPACE_HDR10_ST2084_EXT
		// but VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT triggers the os to change the format for the whole system, which causes every other app and the desktop to be a washed-out greyish color
		//  -> only use in full-screen?
		// and VK_COLOR_SPACE_HDR10_ST2084_EXT completely remaps how the colors are interpreted, so there needs to be tone mapping that is aware of the hdr color space
		// also my monitor supports hdr colors, but does not really display them in any hdr kind of way (I think this is because it has laughable 8-zone local backlight dimming) so there's no reason to use hdr on my screen
		// so I can't just select the 'best' one

		for (auto& form : formats) {
			// select srgb color space, 8 bit color format
			if (form.format == VK_FORMAT_B8G8R8A8_SRGB && form.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return form;
			}
		}

		// if desired color format not found, just use the first one
		return formats[0];
	}

	inline VkPhysicalDevice select_device (VkInstance instance, VkSurfaceKHR surface, VulkanQueuesFamilies* out_queue_families, VkFormat* out_swap_chain_format) {

		auto devices = get_vector<VkPhysicalDevice>(vkEnumeratePhysicalDevices, instance);

		VkPhysicalDevice selected = VK_NULL_HANDLE;
		bool discrete_gpu;
		VulkanQueuesFamilies families;
		SwapChainSupport formats;

		for (auto& device : devices) {
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(device, &props);

			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceFeatures(device, &features);

			auto q_families = pick_queue_families(device, surface);
			if (!q_families.has_graphics_family) continue;
			if (!q_families.has_present_family) continue;

			if (!check_device_extensions(device)) continue;

			auto swap_chain = query_swap_chain_support(device, surface);
			if (swap_chain.formats.size() == 0 || swap_chain.present_modes.size() == 0) continue;

			bool is_discrete = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

			// pick first suitable discrete gpu or fallback to first suitable integrated gpu
			if (selected == VK_NULL_HANDLE || (!discrete_gpu && is_discrete)) {
				selected = device;
				families = q_families;
				formats = swap_chain;

				discrete_gpu = is_discrete;
			}
		}

		if (selected == VK_NULL_HANDLE)
			throw std::runtime_error("[Vulkan] Fatal error: No suitable physical device found");

		*out_queue_families = families;
		*out_swap_chain_format = choose_swap_surface_format(formats.formats).format;
		return selected;
	}

	inline VkDevice create_logical_device (VkPhysicalDevice physical_device, std::vector<char const*> const& enabled_layers, Queues* queues) {
		// make sure we only specifiy unique queues (We pretend that the present queue is a seperate queue, even though it is usually just the graphics queue)
		int count = queues->families.graphics_family == queues->families.present_family ? 3 : 4;

		uint32_t queue_families[] = {
			queues->families.graphics_family,
			queues->families.async_compute_family,
			queues->families.async_transfer_family,
			queues->families.present_family,
		};
		float queue_prios[] = {
			1.0f,
			0.5f, // use lower prio on the async queues
			0.5f, // use lower prio on the async queues
			1.0f,
		};

		VkDeviceQueueCreateInfo q_infos[4] = {};
		for (int i=0; i<4; ++i) {
			q_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			q_infos[i].queueFamilyIndex = queue_families[i];
			q_infos[i].queueCount = 1;
			q_infos[i].pQueuePriorities = &queue_prios[i];
		}

		VkPhysicalDeviceFeatures features = {};
		features.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		info.queueCreateInfoCount = (uint32_t)count;
		info.pQueueCreateInfos = q_infos;

		info.pEnabledFeatures = &features;

		info.enabledExtensionCount = ARRLEN(device_extensions);
		info.ppEnabledExtensionNames = device_extensions;

		info.enabledLayerCount	= (uint32_t)enabled_layers.size();
		info.ppEnabledLayerNames = enabled_layers.data();

		VkDevice device;

		VK_CHECK_RESULT(vkCreateDevice(physical_device, &info, nullptr, &device));

		vkGetDeviceQueue(device, queues->families.graphics_family,       0, &queues->graphics_queue);
		vkGetDeviceQueue(device, queues->families.async_compute_family,  0, &queues->async_compute_queue);
		vkGetDeviceQueue(device, queues->families.async_transfer_family, 0, &queues->async_transfer_queue);
		vkGetDeviceQueue(device, queues->families.present_family,        0, &queues->present_queue);

		return device;
	}

	//// Swap chain creation
	inline VkPresentModeKHR choose_swap_present_mode (std::vector<VkPresentModeKHR> const& present_modes) {
		// TODO: Test these modes
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	inline VkExtent2D choose_swap_extent (VkSurfaceCapabilitiesKHR const& caps, int2 window_size) {
		if (caps.currentExtent.width != UINT32_MAX) {
			return caps.currentExtent;
		} else {
			VkExtent2D ext;
			ext.width  = clamp(window_size.x, caps.minImageExtent.width , caps.maxImageExtent.width );
			ext.height = clamp(window_size.y, caps.minImageExtent.height, caps.maxImageExtent.height);
			return ext;
		}
	}

}
