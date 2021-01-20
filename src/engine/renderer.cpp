#include "common.hpp"
#include "renderer.hpp"

// Define start_renderer function in dedicated cpp so that the gl and vk headers are only included where needed

#include "vulkan/vulkan_renderer.hpp"
#include "opengl/opengl_renderer.hpp"

#include "GLFW/glfw3.h" // need to include vulkan before glfw because GLFW checks for VK_VERSION_1_0

std::unique_ptr<Renderer> start_renderer (RenderBackend backend, GLFWwindow* window) {
	try {
		switch (backend) {

			case RenderBackend::OPENGL: {
				return std::make_unique<gl::OpenglRenderer>(window, APPNAME);
			} break;

			case RenderBackend::VULKAN: {
				return std::make_unique<vk::VulkanRenderer>(window, APPNAME);
			} break;
		}
	} catch (std::exception ex) {
		fprintf(stderr, "Failed to initialize renderer! Exeption: %s\n", ex.what());
	}
	return nullptr;
}
