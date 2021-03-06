#include "vulkan_window.hpp"

#include "GLFW/glfw3.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include <regex>

namespace vk {

std_vector<char const*> glfwGetRequiredInstanceExtensions_vec () {
	uint32_t count = 0;
	auto names = glfwGetRequiredInstanceExtensions(&count);
	return std_vector<char const*>(names, names + count);
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback (
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {

	if (messageSeverity & (0
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
		//| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
		)) return VK_FALSE;

	// Try to get message to be more readable instead of a 500 char line
	std::string msg = pCallbackData->pMessage;
	msg = std::regex_replace(msg, std::regex("Object \\d*"), "\n$&");
	msg = std::regex_replace(msg, std::regex("\\| MessageID .*\\| "), "\n$&\n\n");
	msg = std::regex_replace(msg, std::regex(" - "), "\n$&");
	msg = std::regex_replace(msg, std::regex("The Vulkan spec states: "), "\n\n$&\n");
	msg = std::regex_replace(msg, std::regex("\\(https:.*\\)"), "\n$&");

	fprintf(stderr, "[Vulkan] %s\n", msg.c_str());
	//clog(ERROR, "[Vulkan] %s\n", msg.c_str());

#if RENDERER_DEBUG_OUTPUT_BREAKPOINT
	if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		__debugbreak();
#endif
	return VK_FALSE;
}

//// Vulkan Window Context

VulkanWindowContext::VulkanWindowContext (GLFWwindow* window, char const* app_name) {
	ZoneScoped;

	glfw_window = window;

	{
		ZoneScopedN("glfwGetRequiredInstanceExtensions_vec");
		enabled_extensions = glfwGetRequiredInstanceExtensions_vec();
	}

	instance = create_instance(app_name, debug_callback, enabled_extensions, enabled_layers);

#if VK_VALIDATION_LAYERS || RENDERER_DEBUG_LABELS
	dbg_utils.load(instance);
#endif
#if VK_VALIDATION_LAYERS
	debug_messenger = create_debug_utils_messenger_ext(dbg_utils, instance, debug_callback);
#endif

	VK_CHECK_RESULT(glfwCreateWindowSurface(instance, window, nullptr, &surface));

	pdev = select_device(instance, surface, &queues.families);
	dev = create_logical_device(pdev, enabled_layers, &queues);

	create_swap_chain(SWAP_CHAIN_SIZE);
}
VulkanWindowContext::~VulkanWindowContext () {
	ZoneScoped;

	destroy_swap_chain();
	vkDestroyDevice(dev, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
#if VK_VALIDATION_LAYERS
	destroy_debug_utils_messenger_ext(dbg_utils, instance, debug_messenger);
#endif
	vkDestroyInstance(instance, nullptr);
}

#ifdef TRACY_ENABLE
void VulkanWindowContext::init_vk_tracy (VkCommandPool one_time_cmd_pool) {
	ZoneScoped;

	auto func1 = (PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");
	auto func2 = (PFN_vkGetCalibratedTimestampsEXT)vkGetInstanceProcAddr(instance, "vkGetCalibratedTimestampsEXT");
	
	VkCommandBuffer buf;

	VkCommandBufferAllocateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	info.commandPool = one_time_cmd_pool;
	info.commandBufferCount = 1;
	VK_CHECK_RESULT(vkAllocateCommandBuffers(dev, &info, &buf));

	//tracy_ctx = TracyVkContext(pdev, dev, queues.graphics_queue, buf);
	tracy_ctx = TracyVkContextCalibrated(pdev, dev, queues.graphics_queue, buf, func1, func2);

	// TODO: vkResetCommandPool instead?
	vkFreeCommandBuffers(dev, one_time_cmd_pool, 1, &buf);
}
#endif

void VulkanWindowContext::aquire_image (VkSemaphore image_available_semaphore) {
	ZoneScoped;
	
	{ // recreate swap chain when rendering after window was resized
		int2 cur_size;
		glfwGetFramebufferSize(glfw_window, &cur_size.x, &cur_size.y);

		if (cur_size != wnd_size)
			recreate_swap_chain(image_count);
	}
	
	auto res = vkAcquireNextImageKHR(dev, swap_chain.swap_chain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &image_index);
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		// Swap chain out of date (probably won't happen if we recreate it anyway on window resize)
		recreate_swap_chain(image_count);

		// acuire new swap chain image; should work this time
		res = vkAcquireNextImageKHR(dev, swap_chain.swap_chain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &image_index);
		assert(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR);

	} else {
		VK_CHECK_RESULT(res);
		return;
	}

	imgui_begin();
}
void VulkanWindowContext::present_image (VkSemaphore render_finished_semaphore) {
	ZoneScoped;
	
	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &render_finished_semaphore;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swap_chain.swap_chain;
	present_info.pImageIndices = &image_index;
	present_info.pResults = nullptr;

	auto res = vkQueuePresentKHR(queues.present_queue, &present_info);
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		// Swap chain out of date (window was resized while we were processing this frame?)
		recreate_swap_chain(image_count);
	} else {
		VK_CHECK_RESULT(res);
	}
}

//// Instance creation
void set_debug_utils_messenger_create_info_ext (VkDebugUtilsMessengerCreateInfoEXT* info, PFN_vkDebugUtilsMessengerCallbackEXT callback) {
	info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	info->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info->pfnUserCallback = callback;
	info->pUserData = nullptr;
}

VkDebugUtilsMessengerEXT create_debug_utils_messenger_ext (DebugUtils& dbg_utils, VkInstance instance, PFN_vkDebugUtilsMessengerCallbackEXT callback) {
	VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;

	VkDebugUtilsMessengerCreateInfoEXT info = {};
	set_debug_utils_messenger_create_info_ext(&info, callback);

	dbg_utils.vkCreateDebugUtilsMessengerEXT(instance, &info, nullptr, &debug_messenger);
	return debug_messenger;
}
void destroy_debug_utils_messenger_ext (DebugUtils& dbg_utils, VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger) {
	dbg_utils.vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
}

VkInstance create_instance (char const* app_name, PFN_vkDebugUtilsMessengerCallbackEXT callback,
		std_vector<char const*>& extensions, std_vector<char const*>& layers) {
	ZoneScoped;

	// Check extensions
	auto avail_extensions = get_vector<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);

#if VK_VALIDATION_LAYERS || RENDERER_DEBUG_LABELS
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	// Check validation layers
#if VK_VALIDATION_LAYERS
	{
		auto avail_layers = get_vector<VkLayerProperties>(vkEnumerateInstanceLayerProperties);

		for (auto avail : avail_layers) {
			for (auto requested : VALIDATION_LAYERS) {
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

	info.enabledExtensionCount	= (uint32_t)extensions.size();
	info.ppEnabledExtensionNames = extensions.data();

	info.enabledLayerCount	= (uint32_t)layers.size();
	info.ppEnabledLayerNames = layers.data();

#if VK_VALIDATION_LAYERS
	VkDebugUtilsMessengerCreateInfoEXT dbg_create_info = {};
	{
		set_debug_utils_messenger_create_info_ext(&dbg_create_info, callback);
		info.pNext = &dbg_create_info;
	}
#endif

	VkInstance instance = VK_NULL_HANDLE;

	VK_CHECK_RESULT(vkCreateInstance(&info, nullptr, &instance));

	return instance;
}

//// Device creation
QueueFamilies pick_queue_families (VkPhysicalDevice device, VkSurfaceKHR surface) {

	QueueFamilies families;

	auto queue_families = get_vector<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, device);

	int i = 0;
	for (auto& fam : queue_families) {
		bool graphics = (fam.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
		bool compute  = (fam.queueFlags & VK_QUEUE_COMPUTE_BIT ) != 0;
		bool transfer = (fam.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;

		bool universal_queue = graphics && compute && transfer;

		// pick first universal queue
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

		// pick first queue with present support, but prioritize the universal_queue
		if ((!families.has_present_family || universal_queue) && present_support) {
			families.present_family = i;
			families.has_present_family = true;
		}

		i++;
	}

	return families;
}

bool contains_ext (std_vector<VkExtensionProperties> const& avail_extensions, char const* ext) {
	for (auto& avail : avail_extensions)
		if (strcmp(ext, avail.extensionName) == 0)
			return true;
	return false;
}

bool check_device_extensions (VkPhysicalDevice device) {
	auto avail_extensions = get_vector<VkExtensionProperties>(vkEnumerateDeviceExtensionProperties, device, nullptr);
	// Check if any of the desired extensions is not suppored
	for (auto ext : DEVICE_EXTENSIONS)
		if (!contains_ext(avail_extensions, ext))
			return false;
	return true;
}

SwapChainSupport query_swap_chain_support (VkPhysicalDevice device, VkSurfaceKHR surface) {
	ZoneScoped;
	
	SwapChainSupport details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.caps);
	details.formats = get_vector<VkSurfaceFormatKHR>(vkGetPhysicalDeviceSurfaceFormatsKHR, device, surface);
	details.present_modes = get_vector<VkPresentModeKHR>(vkGetPhysicalDeviceSurfacePresentModesKHR, device, surface);

	return details;
}

VkPhysicalDevice select_device (VkInstance instance, VkSurfaceKHR surface, QueueFamilies* out_queue_families) {
	ZoneScoped;

	auto devices = get_vector<VkPhysicalDevice>(vkEnumeratePhysicalDevices, instance);

	VkPhysicalDevice selected = VK_NULL_HANDLE;
	bool sel_discrete;
	QueueFamilies sel_families;
	SwapChainSupport sel_formats;

	for (auto& device : devices) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(device, &props);

		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(device, &features);

		auto q_families = pick_queue_families(device, surface);
		if (!q_families.has_graphics_family) continue;
		if (!q_families.has_present_family) continue;

		if (!check_device_extensions(device)) continue;

		auto formats = query_swap_chain_support(device, surface);
		if (formats.formats.size() == 0 || formats.present_modes.size() == 0) continue;

		bool discrete = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

		// pick first suitable discrete gpu else fallback to first suitable integrated gpu
		if (selected == VK_NULL_HANDLE || (!sel_discrete && discrete)) {
			selected = device;
			sel_families = q_families;
			sel_discrete = discrete;
		}
	}

	if (selected == VK_NULL_HANDLE)
		throw std::runtime_error("[Vulkan] Fatal error: No suitable physical device found");

	*out_queue_families = sel_families;
	return selected;
}

VkDevice create_logical_device (VkPhysicalDevice pdev, std_vector<char const*> const& enabled_layers, Queues* queues) {
	ZoneScoped;
	
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

	auto avail_extensions = get_vector<VkExtensionProperties>(vkEnumerateDeviceExtensionProperties, pdev, nullptr);

	std_vector<const char*> enabled_extensions;
	for (auto e : DEVICE_EXTENSIONS)
		enabled_extensions.push_back(e);

	VkPhysicalDeviceFeatures features = {};
	// Just fail if these are not supported
	features.samplerAnisotropy = VK_TRUE; // 99.3% support  https://vulkan.gpuinfo.org/listdevicescoverage.php?feature=samplerAnisotropy&platform=windows&option=not
	features.multiDrawIndirect = VK_TRUE; // 99.5% support  https://vulkan.gpuinfo.org/listdevicescoverage.php?feature=multiDrawIndirect&platform=windows&option=not

	features.fillModeNonSolid = VK_TRUE;  // 100% support   https://vulkan.gpuinfo.org/listdevicescoverage.php?feature=fillModeNonSolid&platform=windows
	
	VkDeviceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	info.queueCreateInfoCount = (uint32_t)count;
	info.pQueueCreateInfos = q_infos;
	info.pEnabledFeatures = &features;
	info.enabledExtensionCount = (uint32_t)enabled_extensions.size();
	info.ppEnabledExtensionNames = enabled_extensions.data();
	info.enabledLayerCount	= (uint32_t)enabled_layers.size();
	info.ppEnabledLayerNames = enabled_layers.data();

	VkDevice device;

	{
		ZoneScopedN("vkCreateDevice");
		VK_CHECK_RESULT(vkCreateDevice(pdev, &info, nullptr, &device));
	}

	{
		ZoneScopedN("vkGetDeviceQueues");
		vkGetDeviceQueue(device, queues->families.graphics_family,       0, &queues->graphics_queue);
		vkGetDeviceQueue(device, queues->families.async_compute_family,  0, &queues->async_compute_queue);
		vkGetDeviceQueue(device, queues->families.async_transfer_family, 0, &queues->async_transfer_queue);
		vkGetDeviceQueue(device, queues->families.present_family,        0, &queues->present_queue);
	}

	return device;
}

//// Swap chain creation
VkSurfaceFormatKHR choose_swap_surface_format (std_vector<VkSurfaceFormatKHR> const& formats) {
	// My machine supports
	// VK_FORMAT_R16G16B16A16_SFLOAT  VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
	// VK_FORMAT_A2B10G10R10_UNORM_PACK32  VK_COLOR_SPACE_HDR10_ST2084_EXT
	// but VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT triggers the os to change the format for the whole system even in windowed mode,
	//  which causes every other app and the desktop to be a washed-out greyish color
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

VkPresentModeKHR choose_swap_present_mode (std_vector<VkPresentModeKHR> const& present_modes) {
	// TODO: Test these modes
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swap_extent (VkSurfaceCapabilitiesKHR const& caps, int2 window_size) {
	if (caps.currentExtent.width != UINT32_MAX) {
		// OS is only able to display at the size of the surface
		return caps.currentExtent;
	} else {
		// OS is able to rescale image to fit the surface; pick size of surface anyway
		VkExtent2D ext;
		ext.width  = clamp(window_size.x, caps.minImageExtent.width , caps.maxImageExtent.width );
		ext.height = clamp(window_size.y, caps.minImageExtent.height, caps.maxImageExtent.height);
		return ext;
	}
}

void VulkanWindowContext::create_dummy_render_pass (VkFormat color_format) {
	ZoneScoped;

	VkAttachmentDescription color_attachment = {};
	color_attachment.format = color_format;
	color_attachment.samples = (VkSampleCountFlagBits)1;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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
	info.attachmentCount = 1;
	info.pAttachments = &color_attachment;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 1;
	info.pDependencies = &depen;

	VK_CHECK_RESULT(vkCreateRenderPass(dev, &info, nullptr, &dummy_render_pass));
}

void VulkanWindowContext::create_swap_chain (int swap_chain_size) {
	ZoneScoped;
	
	image_count = (uint32_t)swap_chain_size;

	glfwGetFramebufferSize(glfw_window, &wnd_size.x, &wnd_size.y);

	// After VK_ERROR_OUT_OF_DATE_KHR we apparently need to re-query this info
	auto support			= query_swap_chain_support(pdev, surface);

	swap_chain.format		= choose_swap_surface_format(support.formats);
	swap_chain.present_mode	= choose_swap_present_mode(support.present_modes);
	swap_chain.extent		= choose_swap_extent(support.caps, wnd_size);

	image_count = std::max(image_count, support.caps.minImageCount);
	image_count = std::min(image_count, support.caps.maxImageCount > 0 ? support.caps.maxImageCount : UINT_MAX);

	VkSwapchainCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = surface;
	info.minImageCount = image_count;
	info.imageFormat = swap_chain.format.format;
	info.imageColorSpace = swap_chain.format.colorSpace;
	info.imageExtent = swap_chain.extent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // allow blit for screenshots

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
	info.presentMode = swap_chain.present_mode;
	info.clipped = VK_TRUE;
	info.oldSwapchain = VK_NULL_HANDLE;

	{
		ZoneScopedN("vkCreateSwapchainKHR");
		VK_CHECK_RESULT(vkCreateSwapchainKHR(dev, &info, nullptr, &swap_chain.swap_chain));
	}

	create_dummy_render_pass(swap_chain.format.format);

	auto images = get_vector<VkImage>(vkGetSwapchainImagesKHR, dev, swap_chain.swap_chain);
	image_count = (uint32_t)images.size();

	swap_chain.images.resize(image_count);
	for (uint32_t i=0; i<image_count; ++i) {
		auto& img = swap_chain.images[i];

		img.image = images[i];
		img.image_view = create_image_view(dev, img.image, swap_chain.format.format, VK_IMAGE_ASPECT_COLOR_BIT);

		{
			ZoneScopedN("vkCreateFramebuffer");

			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = dummy_render_pass;
			info.attachmentCount = 1;
			info.pAttachments = &img.image_view;
			info.width  = swap_chain.extent.width ;
			info.height = swap_chain.extent.height;
			info.layers = 1;

			VK_CHECK_RESULT(vkCreateFramebuffer(dev, &info, nullptr, &img.framebuffer));
		}
	}
}
void VulkanWindowContext::destroy_swap_chain () {
	ZoneScoped;
	
	for (auto img : swap_chain.images) {
		vkDestroyFramebuffer(dev, img.framebuffer, nullptr);
		vkDestroyImageView(dev, img.image_view, nullptr);
	}

	vkDestroySwapchainKHR(dev, swap_chain.swap_chain, nullptr);
}

void VulkanWindowContext::recreate_swap_chain (int image_count) {
	ZoneScoped;
	
	// TODO: We wait until rendering is done, then delete the swap chain an create the new one.
	//       It is possible to instead keep the old one and pass it into oldSwapchain to somehow avoid vkQueueWaitIdle
	//vkQueueWaitIdle(queues.graphics_queue);
	vkDeviceWaitIdle(dev);

	destroy_swap_chain();

	create_swap_chain(image_count);
}

//// Imgui
void VulkanWindowContext::imgui_create (VkCommandBuffer one_time_cmds, int frames_count) {
	ZoneScoped;

	{ // Seperate descriptor pool for imgui to avoid having to include imgui in calculation of descriptor counts
		VkDescriptorPoolSize pool_size = {};
		pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_size.descriptorCount = (uint32_t)frames_count; // for imgui TODO: FRAMES_IN_FLIGHT instead, or even just 1? imgui won't change the image sampler per frame

		VkDescriptorPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		info.poolSizeCount = 1;
		info.pPoolSizes = &pool_size;
		info.maxSets = (uint32_t)frames_count; // for ubo

		VK_CHECK_RESULT(vkCreateDescriptorPool(dev, &info, nullptr, &imgui_descriptor_pool));
	}

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForVulkan(glfw_window, true);

	ImGui_ImplVulkan_InitInfo info = {};
	info.Instance			= instance;
	info.PhysicalDevice		= pdev;
	info.Device				= dev;
	info.QueueFamily		= queues.families.graphics_family;
	info.Queue				= queues.graphics_queue;
	info.PipelineCache		= VK_NULL_HANDLE;
	info.DescriptorPool		= imgui_descriptor_pool;
	info.MinImageCount		= frames_count;
	info.ImageCount			= frames_count;
	info.MSAASamples		= (VkSampleCountFlagBits)1;
	info.Allocator			= nullptr;
	info.CheckVkResultFn	= nullptr;
	ImGui_ImplVulkan_Init(&info, dummy_render_pass); // Dummy render pass should be okay here

	ImGui_ImplVulkan_CreateFontsTexture(one_time_cmds);
}
void VulkanWindowContext::imgui_destroy () {
	ZoneScoped;

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	vkDestroyDescriptorPool(dev, imgui_descriptor_pool, nullptr);
}

void VulkanWindowContext::imgui_begin () {
	ImGui_ImplVulkan_NewFrame();
}
void VulkanWindowContext::imgui_draw (VkCommandBuffer cmds, bool hide_gui) {
	ZoneScoped;
	VK_TRACE(*this, cmds, "imgui_draw");

	ImGui::Render();
	if (g_imgui.enabled && !hide_gui)
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmds);
}

} // namespace vk
