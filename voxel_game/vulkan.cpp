#include "vulkan.hpp"
#include "dear_imgui.hpp"
#include "glfw_window.hpp"

std::unique_ptr<Vulkan> vulkan = nullptr;

namespace vk {
#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define TO_STRING(x) #x

	// Helper functions
	template <typename T, typename FUNC, typename... ARGS>
	std::vector<T> get_vector (FUNC func, ARGS... args) {
		uint32_t count = 0;
		func(args..., &count, nullptr);

		std::vector<T> vec (count);
		if (count > 0)
			func(args..., &count, vec.data());

		return vec;
	}

	std::vector<char const*> glfwGetRequiredInstanceExtensions_vec () {
		uint32_t count = 0;
		auto names = glfwGetRequiredInstanceExtensions(&count);
		return std::vector<char const*>(names, names + count);
	}

	// 
#if _DEBUG || 1
	static constexpr bool enable_validation_layers = true;
#else
	static constexpr bool enable_validation_layers = false;
#endif

	static constexpr const char* validation_layers[] = {
		"VK_LAYER_KHRONOS_validation",
	};
	static constexpr const char* device_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	void _check_result (VkResult res, char const* expr) {
		if (res == VK_SUCCESS)
			return;

		throw std::runtime_error(expr);
	}
#define VK_CHECK_RESULT(expr) _check_result(expr, "[Vulkan] Fatal error: " TO_STRING(expr))

	//// Resource creation
	VkSampleCountFlagBits get_max_usable_multisample_count (VkPhysicalDevice device) {
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

	uint32_t find_memory_type (VkPhysicalDevice device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties mem_props;
		vkGetPhysicalDeviceMemoryProperties(device, &mem_props);

		for (uint32_t i=0; i<mem_props.memoryTypeCount; ++i) {
			if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("[Vulkan] Fatal error: No suitable memory type found");
	}

	VkFormat find_supported_format (VkPhysicalDevice device, std::initializer_list<VkFormat> const& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
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

	VkFormat find_depth_format (VkPhysicalDevice device) {
		return find_supported_format(device,
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	bool has_stencil_component (VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	VkImage create_image (VkDevice device, VkPhysicalDevice pdev, int2 size, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageLayout initial_layout, VkMemoryPropertyFlags props,
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

	VkImageView create_image_view (VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect) {
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

	VkCommandBuffer Vulkan::begin_one_time_commands () {
		VkCommandBuffer buf;

		VkCommandBufferAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		info.commandPool = one_time_command_pool;
		info.commandBufferCount = 1;
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &info, &buf));
		
		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(buf, &begin_info);

		return buf;
	}

	void Vulkan::end_one_time_commands (VkCommandBuffer buf) {
		
		vkEndCommandBuffer(buf);

		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &buf;
		vkQueueSubmit(queues.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

		vkQueueWaitIdle(queues.graphics_queue);

		vkFreeCommandBuffers(device, one_time_command_pool, 1, &buf);
	}

	VkCommandPool create_one_time_command_pool (VkDevice device, uint32_t queue_family) {
		VkCommandPool pool;

		VkCommandPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		info.queueFamilyIndex = queue_family;
		info.flags = 0;
		VK_CHECK_RESULT(vkCreateCommandPool(device, &info, nullptr, &pool));

		return pool;
	}

	//// Instance creation
	VKAPI_ATTR VkBool32 VKAPI_CALL Vulkan::debug_callback (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		fprintf(stderr, "[Vulkan] %s\n", pCallbackData->pMessage);
		clog(ERROR, "[Vulkan] %s\n", pCallbackData->pMessage);

		return VK_FALSE;
	}

	void set_debug_utils_messenger_create_info_ext (VkDebugUtilsMessengerCreateInfoEXT* info) {
		info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		info->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		info->pfnUserCallback = Vulkan::debug_callback;
		info->pUserData = nullptr;
	}

	VkDebugUtilsMessengerEXT create_debug_utils_messenger_ext (VkInstance instance) {
		VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
		
		VkDebugUtilsMessengerCreateInfoEXT info = {};
		set_debug_utils_messenger_create_info_ext(&info);

		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func)
			func(instance, &info, nullptr, &debug_messenger);

		return debug_messenger;
	}
	void destroy_debug_utils_messenger_ext (VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func)
			func(instance, debug_messenger, nullptr);
	}

	VkInstance create_instance (char const* app_name, std::vector<char const*>* out_layers) {
		std::vector<char const*> layers;

		// Check extensions
		auto avail_extensions = get_vector<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);

		auto request_extensions = glfwGetRequiredInstanceExtensions_vec();

		// Check validation layers
		if (enable_validation_layers) {

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
		if (enable_validation_layers) {
			set_debug_utils_messenger_create_info_ext(&dbg_create_info);
			info.pNext = &dbg_create_info;
		}

		VkInstance instance = VK_NULL_HANDLE;

		VK_CHECK_RESULT(vkCreateInstance(&info, nullptr, &instance));

		*out_layers = std::move(layers);
		return instance;
	}

	//// Device creation
	VulkanQueuesFamilies pick_queue_families (VkPhysicalDevice device, VkSurfaceKHR surface) {
		
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

	bool check_device_extensions (VkPhysicalDevice device) {
		
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

	SwapChainSupport query_swap_chain_support (VkPhysicalDevice device, VkSurfaceKHR surface) {
		SwapChainSupport details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.caps);
		details.formats = get_vector<VkSurfaceFormatKHR>(vkGetPhysicalDeviceSurfaceFormatsKHR, device, surface);
		details.present_modes = get_vector<VkPresentModeKHR>(vkGetPhysicalDeviceSurfacePresentModesKHR, device, surface);

		return details;
	}

	VkSurfaceFormatKHR choose_swap_surface_format (std::vector<VkSurfaceFormatKHR> const& formats) {
		// My machine supports
		// VK_FORMAT_R16G16B16A16_SFLOAT  VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
		// VK_FORMAT_A2B10G10R10_UNORM_PACK32  VK_COLOR_SPACE_HDR10_ST2084_EXT
		// but VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT triggers the os to change the format for the whole system, which causes every other app and the desktop to be a washed-out greyish color
		//  -> only use in full-screen?
		// and VK_COLOR_SPACE_HDR10_ST2084_EXT completely remaps how the colors are interpreted, so there needs to be tone mapping that is aware of the hdr color space
		// also my monitor supports hdr colors, but does not really display them in any hdr kind of way (I think this is because it has laughable 8-zone local backlight dimming) so there's not reason to use hdr on my screen
		// so at best non-standart color buffers should be used with an option, I can't just select the 'best' one

		for (auto& form : formats) {
			// select srgb color space, 8 bit color format
			if (form.format == VK_FORMAT_B8G8R8A8_SRGB && form.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return form;
			}
		}

		// if desired color format not found, just use the first one
		return formats[0];
	}

	VkPhysicalDevice select_device (VkInstance instance, VkSurfaceKHR surface, VulkanQueuesFamilies* out_queue_families, VkFormat* out_swap_chain_format) {
		
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

	VkDevice create_logical_device (VkPhysicalDevice physical_device, std::vector<char const*> const& enabled_layers, Queues* queues) {
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
	VkPresentModeKHR choose_swap_present_mode (std::vector<VkPresentModeKHR> const& present_modes) {
		// TODO: Test these modes
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D choose_swap_extent (VkSurfaceCapabilitiesKHR const& caps, int2 window_size) {
		if (caps.currentExtent.width != UINT32_MAX) {
			return caps.currentExtent;
		} else {
			VkExtent2D ext;
			ext.width  = clamp(window_size.x, caps.minImageExtent.width , caps.maxImageExtent.width );
			ext.height = clamp(window_size.y, caps.minImageExtent.height, caps.maxImageExtent.height);
			return ext;
		}
	}
	
	void Vulkan::create_swap_chain (int2 window_size, int image_count) {

		auto support = query_swap_chain_support(physical_device, surface);

		auto format			= choose_swap_surface_format(support.formats);
		auto present_mode	= choose_swap_present_mode(support.present_modes);
		auto extent			= choose_swap_extent(support.caps, window_size);

		image_count = clamp(image_count, support.caps.minImageCount, support.caps.maxImageCount > 0 ? support.caps.maxImageCount : INT_MAX);

		VkSwapchainCreateInfoKHR info = {};
		info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		info.surface = surface;
		info.minImageCount = image_count;
		info.imageFormat = format.format;
		info.imageColorSpace = format.colorSpace;
		info.imageExtent = extent;
		info.imageArrayLayers = 1;
		info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		uint32_t q_family_indices[] = { queues.families.graphics_family, queues.families.present_family };
		if (queues.families.graphics_family != queues.families.present_family) {
			info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			info.queueFamilyIndexCount = 2;
			info.pQueueFamilyIndices = q_family_indices;
		} else {
			info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		info.preTransform = support.caps.currentTransform;
		info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		info.presentMode = present_mode;
		info.clipped = VK_TRUE;
		info.oldSwapchain = VK_NULL_HANDLE;

		VK_CHECK_RESULT(vkCreateSwapchainKHR(device, &info, nullptr, &swap_chain.swap_chain));

		auto images = get_vector<VkImage>(vkGetSwapchainImagesKHR, device, swap_chain.swap_chain);
		image_count = (int)images.size();

		swap_chain.images.resize(image_count);
		for (int i=0; i<image_count; ++i) {
			auto& img = swap_chain.images[i];

			img.image = images[i];
			img.image_view = create_image_view(device, img.image, format.format, VK_IMAGE_ASPECT_COLOR_BIT);

			VkImageView attachments[] = {
				color_buffer.image_view,
				depth_buffer.image_view,
				img.image_view,
			};

			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = render_pass;
			info.attachmentCount = ARRLEN(attachments);
			info.pAttachments = attachments;
			info.width  = extent.width ;
			info.height = extent.height;
			info.layers = 1;

			VK_CHECK_RESULT(vkCreateFramebuffer(device, &info, nullptr, &img.framebuffer));
		}

		swap_chain.format = format;
		swap_chain.extent = extent;
	}

	//// Renderpass creation
	RenderBuffer create_render_buffer (VkDevice device, VkPhysicalDevice pdev, int2 size, VkFormat format, VkImageUsageFlags usage,
			VkImageLayout initial_layout, VkMemoryPropertyFlags props, VkImageAspectFlags aspect, VkSampleCountFlagBits samples=VK_SAMPLE_COUNT_1_BIT) {
		RenderBuffer buf;
		buf.image = create_image(device, pdev, size, format, VK_IMAGE_TILING_OPTIMAL, usage, initial_layout, props, &buf.memory, 1, samples);
		buf.image_view = create_image_view(device, buf.image, format, aspect);
		return buf;
	}

	VkRenderPass create_renderpass (VkDevice device, VkFormat color_format, VkFormat depth_format, VkSampleCountFlagBits msaa) {
		VkAttachmentDescription color_attachment = {};
		color_attachment.format = color_format;
		color_attachment.samples = msaa;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depth_attachment = {};
		depth_attachment.format = depth_format;
		depth_attachment.samples = msaa;
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
		subpass.pResolveAttachments = &color_attachment_resolve_ref;
		subpass.pDepthStencilAttachment = &depth_attachment_ref;

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
		info.attachmentCount = ARRLEN(attachments);
		info.pAttachments = attachments;
		info.subpassCount = 1;
		info.pSubpasses = &subpass;
		info.dependencyCount = 1;
		info.pDependencies = &depen;

		VkRenderPass renderpass;

		VK_CHECK_RESULT(vkCreateRenderPass(device, &info, nullptr, &renderpass));

		return renderpass;
	}

	//// Per-Frame data creation
	std::vector<FrameData> create_frame_data (int in_flight_count, VkDevice device, uint32_t queue_family) {
		std::vector<FrameData> frame_data (in_flight_count);

		for (auto& frame : frame_data) {
			VkCommandPoolCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			info.queueFamilyIndex = queue_family;
			info.flags = 0;
			VK_CHECK_RESULT(vkCreateCommandPool(device, &info, nullptr, &frame.command_pool));

			VkCommandBufferAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			alloc_info.commandPool = frame.command_pool;
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			alloc_info.commandBufferCount  = 1;
			VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &alloc_info, &frame.command_buffer));

			VkSemaphoreCreateInfo semaphore_info = {};
			semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fence_info = {};
			fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphore_info, nullptr, &frame.image_available_semaphore));
			VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphore_info, nullptr, &frame.render_finished_semaphore));
			VK_CHECK_RESULT(vkCreateFence(device, &fence_info, nullptr, &frame.fence));
		}

		return frame_data;
	}

	//// Framebuffer creation
	void Vulkan::create_framebuffers (int2 size, VkFormat color_format) {
		color_buffer = create_render_buffer(device, physical_device, size, color_format,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT, max_msaa_samples);

		depth_buffer = create_render_buffer(device, physical_device, size, depth_format,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VK_IMAGE_ASPECT_DEPTH_BIT, max_msaa_samples);

	}
	void Vulkan::cleanup_framebuffers () {
		vkDestroyImageView(device, color_buffer.image_view, nullptr);
		vkDestroyImage(device, color_buffer.image, nullptr);
		vkFreeMemory(device, color_buffer.memory, nullptr);

		vkDestroyImageView(device, depth_buffer.image_view, nullptr);
		vkDestroyImage(device, depth_buffer.image, nullptr);
		vkFreeMemory(device, depth_buffer.memory, nullptr);
	}

	//// Descriptor pool creation
	VkDescriptorPool create_descriptor_pool (VkDevice device, int swap_chain_size) {
		VkDescriptorPool pool;

		VkDescriptorPoolSize pool_sizes[1] = {};
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = (uint32_t)swap_chain_size; // for imgui

		VkDescriptorPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		info.poolSizeCount = ARRLEN(pool_sizes);
		info.pPoolSizes = pool_sizes;
		info.maxSets = (uint32_t)swap_chain_size; // for imgui

		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &info, nullptr, &pool));

		return pool;
	}

	//// 
	void Vulkan::cleanup_swap_chain () {
		if (swap_chain.swap_chain != VK_NULL_HANDLE) {
			cleanup_framebuffers();

			for (auto img : swap_chain.images) {
				vkDestroyImageView(device, img.image_view, nullptr);
				vkDestroyFramebuffer(device, img.framebuffer, nullptr);
			}

			//vkDestroyPipeline(vk_device, vk_pipeline, nullptr);
			//vkDestroyPipelineLayout(vk_device, vk_pipeline_layout, nullptr);

			vkDestroySwapchainKHR(device, swap_chain.swap_chain, nullptr);
		}
	}

	Vulkan::Vulkan (GLFWwindow* glfw_window, char const* app_name) {
		instance = create_instance(app_name, &enabled_layers);

		debug_messenger = enable_validation_layers ? create_debug_utils_messenger_ext(instance) : VK_NULL_HANDLE;
		
		auto res = glfwCreateWindowSurface(instance, glfw_window, nullptr, &surface);
		assert(res == VK_SUCCESS);

		physical_device = select_device(instance, surface, &queues.families, &color_format);
		device = create_logical_device(physical_device, enabled_layers, &queues);

		depth_format = find_depth_format(physical_device);
		max_msaa_samples = get_max_usable_multisample_count(physical_device);

		frame_data = create_frame_data(FRAMES_IN_FLIGHT, device, queues.families.graphics_family);

		render_pass = create_renderpass(device, color_format, depth_format, max_msaa_samples);
		one_time_command_pool = create_one_time_command_pool(device, queues.families.graphics_family);
		descriptor_pool = create_descriptor_pool(device, SWAP_CHAIN_SIZE);

		recreate_swap_chain();
	}

	Vulkan::~Vulkan () {
		vkQueueWaitIdle(queues.graphics_queue);

		vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
		vkDestroyRenderPass(device, render_pass, nullptr);
		vkDestroyCommandPool(device, one_time_command_pool, nullptr);

		for (auto& frame : frame_data) {
			vkDestroyCommandPool(device, frame.command_pool, nullptr);

			vkDestroySemaphore(device, frame.image_available_semaphore, nullptr);
			vkDestroySemaphore(device, frame.render_finished_semaphore, nullptr);
			vkDestroyFence(device, frame.fence, nullptr);
		}

		cleanup_swap_chain();

		vkDestroyDevice(device, nullptr);

		vkDestroySurfaceKHR(instance, surface, nullptr);

		if (enable_validation_layers)
			destroy_debug_utils_messenger_ext(instance, debug_messenger);

		vkDestroyInstance(instance, nullptr);
	}

	void Vulkan::recreate_swap_chain () {
		vkQueueWaitIdle(queues.graphics_queue);

		glfwGetFramebufferSize(window, &cur_size.x, &cur_size.y);
		
		cleanup_swap_chain();

		//
		create_framebuffers(cur_size, color_format);
		create_swap_chain(cur_size, SWAP_CHAIN_SIZE);
	}

	bool Vulkan::frame_start () {
		auto frame = frame_data[cur_frame];

		vkWaitForFences(device, 1, &frame.fence, VK_TRUE, UINT64_MAX);

		vkResetFences(device, 1, &frame.fence);

		vkResetCommandPool(device, frame.command_pool, 0);

		// Aquire image
		auto res = vkAcquireNextImageKHR(device, swap_chain.swap_chain, UINT64_MAX, frame.image_available_semaphore, VK_NULL_HANDLE, &cur_image_index);
		if (res == VK_ERROR_OUT_OF_DATE_KHR) {
			recreate_swap_chain();
			return false;
		}
		assert(res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);

		return true;
	}

	void Vulkan::render_begin () {
		auto buf = frame_data[cur_frame].command_buffer;
		auto framebuffer = swap_chain.images[cur_image_index].framebuffer;

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begin_info.pInheritanceInfo = nullptr;
		VK_CHECK_RESULT(vkBeginCommandBuffer(buf, &begin_info));

		VkClearValue clear_vales[2] = {};
		clear_vales[0].color = { .01f, .011f, .012f, 1 };
		clear_vales[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_info.renderPass = render_pass;
		render_pass_info.framebuffer = framebuffer;
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = swap_chain.extent;
		render_pass_info.clearValueCount = ARRLEN(clear_vales);
		render_pass_info.pClearValues = clear_vales;
		vkCmdBeginRenderPass(buf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

		//vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);
		//
		//VkBuffer vertex_bufs[] = { vk_vertex_buffer };
		//VkDeviceSize offsets[] = { 0 };
		//vkCmdBindVertexBuffers(buf, 0, 1, vertex_bufs, offsets);
		//
		//vkCmdBindIndexBuffer(buf, vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);
		//
		//vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout, 0, 1, &vk_descriptor_sets[cur_frame], 0, nullptr);
		//
		//vkCmdDrawIndexed(buf, (uint32_t)model.indices.size(), 1, 0, 0, 0);

	}

	void Vulkan::render_end () {

	}

	void Vulkan::frame_end () {
		auto buf = frame_data[cur_frame].command_buffer;

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), buf);

		vkCmdEndRenderPass(buf);

		VK_CHECK_RESULT(vkEndCommandBuffer(buf));

		////
		auto frame = frame_data[cur_frame];

		VkSemaphore wait_semaphores[] = { frame.image_available_semaphore };
		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSemaphore signal_semaphores[] = { frame.render_finished_semaphore };

		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &frame.command_buffer;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;
		VK_CHECK_RESULT(vkQueueSubmit(queues.graphics_queue, 1, &submit_info, frame.fence));

		// Present image
		VkSwapchainKHR swap_chains[] = { swap_chain.swap_chain };

		VkPresentInfoKHR present_info = {};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swap_chains;
		present_info.pImageIndices = &cur_image_index;
		present_info.pResults = nullptr;

		auto res = vkQueuePresentKHR(queues.present_queue, &present_info);
		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
			recreate_swap_chain();
		} else {
			VK_CHECK_RESULT(res);
		}

		cur_frame = (cur_frame + 1) % FRAMES_IN_FLIGHT;
	}
}
