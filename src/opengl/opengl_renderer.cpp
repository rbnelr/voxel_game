#include "common.hpp"
#include "opengl_renderer.hpp"

#include "GLFW/glfw3.h" // include after glad

#include "TracyOpenGL.hpp"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

namespace gl {


void OpenglRenderer::frame_begin (GLFWwindow* window, kiss::ChangedFiles& changed_files) {
	ImGui_ImplOpenGL3_NewFrame();
}

void OpenglRenderer::render_frame (GLFWwindow* window, Input& I, Game& game) {
	
	glViewport(0,0, I.window_size.x, I.window_size.y);
	glScissor(0,0, I.window_size.x, I.window_size.y);

	glClearColor(1,0,0,1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);



	imgui_draw();

	glfwSwapBuffers(window);
}

void OpenglRenderer::imgui_draw () {
	ZoneScoped;
	//GPU_TRACE(*this, cmds, "imgui_draw");

	ImGui::Render();
	if (g_imgui.enabled)
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void OpenglRenderer::set_vsync (bool state) {
	ZoneScoped;
	glfwSwapInterval(state ? _vsync_on_interval : 0);
}

OpenglRenderer::OpenglRenderer (GLFWwindow* window, char const* app_name) {
	ZoneScoped;

	glfwMakeContextCurrent(window);

	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	gladLoadGL();

#ifdef OGL_DEBUG_OUTPUT
	if (glfwExtensionSupported("GL_ARB_debug_output")) {
		glDebugMessageCallbackARB(debug_callback, this);
	#if OGL_DEBUG_OUTPUT_BREAKPOINT
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB); // this exists -> if ogl_debuproc needs to be thread safe
	#endif
	}
#endif

	if (glfwExtensionSupported("WGL_EXT_swap_control_tear"))
		_vsync_on_interval = -1;

	set_vsync(true);

	// srgb enabled by default if supported
	// TODO: should I use glfwExtensionSupported or GLAD_GL_ARB_framebuffer_sRGB? does it make a difference?
	if (glfwExtensionSupported("GL_ARB_framebuffer_sRGB"))
		glEnable(GL_FRAMEBUFFER_SRGB);
	else
		clog(ERROR, "[OpenGL] No sRGB supported! Output will be wrong!\n");

	if (glfwExtensionSupported("GL_ARB_clip_control"))
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
	else
		clog(ERROR, "[OpenGL] GL_ARB_clip_control not supported, depth won't be correct!\n");

	//if (	!glfwExtensionSupported("GL_NV_gpu_shader5") ||
	//	!glfwExtensionSupported("GL_NV_shader_buffer_load")) {
	//	clog(ERROR, "[OpenGL] GL_NV_gpu_shader5 or GL_NV_shader_buffer_load not supported!\n");
	//}

	// TODO: extension needed?
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	TracyGpuContext;
}

OpenglRenderer::~OpenglRenderer () {

	ImGui_ImplOpenGL3_Shutdown();
}

void APIENTRY OpenglRenderer::debug_callback (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, void const* userParam) {
	OpenglRenderer* r = (OpenglRenderer*)userParam;

	//if (source == GL_DEBUG_SOURCE_SHADER_COMPILER_ARB) return;

	// hiding irrelevant infos/warnings
	switch (id) {
		case 131185: // Buffer detailed info
		//case 1282: // using shader that was not compiled successfully
		//case 2: // API_ID_RECOMPILE_FRAGMENT_SHADER performance warning has been generated. Fragment shader recompiled due to state change.
		//case 131218: // Program/shader state performance warning: Fragment shader in program 3 is being recompiled based on GL state.

		////case 131154: // Pixel transfer sync with rendering warning
		//
		//case 1282: // Wierd error on notebook when trying to do texture streaming
		//case 131222: // warning with unused shadow samplers ? (Program undefined behavior warning: Sampler object 0 is bound to non-depth texture 0, yet it is used with a program that uses a shadow sampler . This is undefined behavior.), This might just be unused shadow samplers, which should not be a problem
		//case 131218: // performance warning, because of shader recompiling based on some 'key'
			return;

		default:
			break;
	}

	const char* src_str = "<unknown>";
	switch (source) {
		case GL_DEBUG_SOURCE_API_ARB:				src_str = "GL_DEBUG_SOURCE_API_ARB";				break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:		src_str = "GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB";		break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:	src_str = "GL_DEBUG_SOURCE_SHADER_COMPILER_ARB";	break;
		case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:		src_str = "GL_DEBUG_SOURCE_THIRD_PARTY_ARB";		break;
		case GL_DEBUG_SOURCE_APPLICATION_ARB:		src_str = "GL_DEBUG_SOURCE_APPLICATION_ARB";		break;
		case GL_DEBUG_SOURCE_OTHER_ARB:				src_str = "GL_DEBUG_SOURCE_OTHER_ARB";				break;
	}

	const char* type_str = "<unknown>";
	switch (source) {
		case GL_DEBUG_TYPE_ERROR_ARB:				type_str = "GL_DEBUG_TYPE_ERROR_ARB";				break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:	type_str = "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB";	break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:	type_str = "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB";	break;
		case GL_DEBUG_TYPE_PORTABILITY_ARB:			type_str = "GL_DEBUG_TYPE_PORTABILITY_ARB";			break;
		case GL_DEBUG_TYPE_PERFORMANCE_ARB:			type_str = "GL_DEBUG_TYPE_PERFORMANCE_ARB";			break;
		case GL_DEBUG_TYPE_OTHER_ARB:				type_str = "GL_DEBUG_TYPE_OTHER_ARB";				break;
	}

	const char* severity_str = "<unknown>";
	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH_ARB:			severity_str = "GL_DEBUG_SEVERITY_HIGH_ARB";		break;
		case GL_DEBUG_SEVERITY_MEDIUM_ARB:			severity_str = "GL_DEBUG_SEVERITY_MEDIUM_ARB";		break;
		case GL_DEBUG_SEVERITY_LOW_ARB:				severity_str = "GL_DEBUG_SEVERITY_LOW_ARB";			break;
	}

	clog(severity == GL_DEBUG_SEVERITY_HIGH_ARB ? ERROR : WARNING,
		"[OpenGL] debug message: severity: %s src: %s type: %s id: %d  %s\n", severity_str, src_str, type_str, id, message);

#if OGL_DEBUG_OUTPUT_BREAKPOINT
	if (severity == GL_DEBUG_SEVERITY_HIGH_ARB)
		__debugbreak();
#endif
}

} // namespace gl
