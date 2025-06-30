#include "common.hpp"
#include "opengl_renderer.hpp"
#include "gl_chunk_renderer.hpp"
#include "gl_raytracer.hpp"
#include "player_renderer.hpp"
#include "radiance_cascades.hpp"

#include "GLFW/glfw3.h" // include after glad
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

namespace gl {
	
void OpenglRenderer::deserialize (nlohmann::ordered_json const& j) {
	auto& t = *this;
	_JSON_EXPAND(_JSON_PASTE(_JSON_FROM, SERIALIZE_OpenglRenderer))

	// take vsync into account
	set_vsync(vsync);
}
void OpenglRenderer::serialize (nlohmann::ordered_json& j) {
	auto& t = *this;
	_JSON_EXPAND(_JSON_PASTE(_JSON_TO, SERIALIZE_OpenglRenderer))
}

void APIENTRY OpenglRenderer::debug_callback (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, void const* userParam) {
	//OpenglRenderer* r = (OpenglRenderer*)userParam;

	//if (source == GL_DEBUG_SOURCE_SHADER_COMPILER_ARB) return;
	if (source == GL_DEBUG_SOURCE_APPLICATION_ARB) {
		//printf("%.*s\n", length, message); // message is not null terminated, pass explicit length
		return; // OGL_TRACE is only for organizing drawcalls in nsight, not to spam the console
	}
		
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

		// Pixel-path detailed info: The current pixel-path operation converts data from 2-bit integer to 1-bit integer, and may exhibit data loss
		// when intentionally uploading uint16_t to GL_R8UI
		case 131153:
			return;

		default:
			break;
	}

	const char* src_str = "<unknown>";
	switch (source) {
		case GL_DEBUG_SOURCE_API_ARB:				src_str = "API";				break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:		src_str = "WINDOW_SYSTEM";		break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:	src_str = "SHADER_COMPILER";	break;
		case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:		src_str = "THIRD_PARTY";		break;
		case GL_DEBUG_SOURCE_APPLICATION_ARB:		src_str = "APPLICATION";		break;
		case GL_DEBUG_SOURCE_OTHER_ARB:				src_str = "OTHER";				break;
	}

	const char* type_str = "<unknown>";
	switch (source) {
		case GL_DEBUG_TYPE_ERROR_ARB:				type_str = "ERROR";					break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:	type_str = "DEPRECATED_BEHAVIOR";	break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:	type_str = "UNDEFINED_BEHAVIOR";	break;
		case GL_DEBUG_TYPE_PORTABILITY_ARB:			type_str = "PORTABILITY";			break;
		case GL_DEBUG_TYPE_PERFORMANCE_ARB:			type_str = "PERFORMANCE";			break;
		case GL_DEBUG_TYPE_OTHER_ARB:				type_str = "OTHER";					break;
	}

	const char* severity_str = "<unknown>";
	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH_ARB:			severity_str = "HIGH";		break;
		case GL_DEBUG_SEVERITY_MEDIUM_ARB:			severity_str = "MEDIUM";	break;
		case GL_DEBUG_SEVERITY_LOW_ARB:				severity_str = "LOW";		break;
	}

	log(severity == GL_DEBUG_SEVERITY_HIGH_ARB ? ERROR : WARNING,
		"[OpenGL] debug message: severity:%s  src:%s  type:%s  id:%d\n%.*s", severity_str, src_str, type_str, id, length, message); // message is not null terminated, pass explicit length

#if RENDERER_DEBUG_OUTPUT_BREAKPOINT
	if (severity == GL_DEBUG_SEVERITY_HIGH_ARB)
		__debugbreak();
#endif
}

bool OpenglRenderer::get_vsync () {
	return vsync;
}
void OpenglRenderer::set_vsync (bool state) {
	ZoneScoped;
	glfwSwapInterval(state ? _vsync_on_interval : 0);
	vsync = state;
}

OpenglContext::OpenglContext (OpenglRenderer* r, GLFWwindow* window) {
	ZoneScopedN("OpenglContext init");
	log("OpenGL init...");
	
	glfwMakeContextCurrent(window);

	{
		ZoneScopedN("gladLoadGLLoader");
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			fatal_error("gladLoadGLLoader error!");
		}
		gladLoadGL();
	}
		
	{
		ZoneScopedN("check extensions and print version");
		
	#if RENDERER_DEBUG_OUTPUT
		if (glfwExtensionSupported("GL_ARB_debug_output")) {
			glDebugMessageCallbackARB(OpenglRenderer::debug_callback, this);
		#if RENDERER_DEBUG_OUTPUT_BREAKPOINT
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB); // Call message on thread that call was made
		#endif
		}
	#endif

		if (glfwExtensionSupported("WGL_EXT_swap_control_tear"))
			r->_vsync_on_interval = -1;


		if (glfwExtensionSupported("WGL_EXT_swap_control_tear"))
			r->_vsync_on_interval = -1;
		
		//if (!glfwExtensionSupported("GL_ARB_bindless_texture")) {
		//	log(ERROR,"[OpenGL] No bindless textures supported! This is bad!");
		//}
		if (  !glfwExtensionSupported("GL_ARB_gpu_shader5") ||
				!glfwExtensionSupported("GL_ARB_gpu_shader_int64")) {
			log(ERROR,"[OpenGL] GL_ARB_gpu_shader5 or GL_ARB_gpu_shader_int64 not supported! This is bad!");
		}

		// srgb enabled by default if supported
		// TODO: should I use glfwExtensionSupported or GLAD_GL_ARB_framebuffer_sRGB? does it make a difference?
		if (glfwExtensionSupported("GL_ARB_framebuffer_sRGB"))
			glEnable(GL_FRAMEBUFFER_SRGB);
		else
			log(ERROR,"[OpenGL] No sRGB framebuffers supported! Shading will be wrong!");

	#if OGL_USE_REVERSE_DEPTH
		ogl::reverse_depth = glfwExtensionSupported("GL_ARB_clip_control");
	#endif

		//if (	!glfwExtensionSupported("GL_NV_gpu_shader5") ||
		//	!glfwExtensionSupported("GL_NV_shader_buffer_load")) {
		//	log(ERROR, "[OpenGL] GL_NV_gpu_shader5 or GL_NV_shader_buffer_load not supported!");
		//}
		
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS); // core since 3.2

		// I never align my pixel rows
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &r->max_aniso);

		auto* vend = glGetString(GL_VENDOR);
		auto* rend = glGetString(GL_RENDERER);
		auto* vers = glGetString(GL_VERSION);
		
		log( "GL_VENDOR:   %s\n"
		     "GL_RENDERER: %s\n"
		     "GL_VERSION:  %s", vend, rend, vers);
	}

	{
		ZoneScopedN("TracyGpuContext");
		TracyGpuContext;
	}
	
	r->set_vsync(r->vsync);
}

//// OpenglRenderer
OpenglRenderer::OpenglRenderer (Game& game): ctx{this, game.window} {
	ZoneScopedN("OpenglRenderer init");

	float max_aniso = 1.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_aniso);

	glSamplerParameteri(pixelated_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(pixelated_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glSamplerParameteri(pixelated_sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(pixelated_sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glSamplerParameterf(pixelated_sampler, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);

	glSamplerParameteri(smooth_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(smooth_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(smooth_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(smooth_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSamplerParameterf(smooth_sampler, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);

	glSamplerParameteri(smooth_sampler_wrap, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glSamplerParameteri(smooth_sampler_wrap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glSamplerParameteri(smooth_sampler_wrap, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glSamplerParameteri(smooth_sampler_wrap, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glSamplerParameterf(smooth_sampler_wrap, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);

	ImGui_ImplOpenGL3_Init();

	chunk_renderer = std::make_unique<ChunkRenderer>(shaders);
	raytracer      = std::make_unique<Raytracer>(shaders);
	player_rederer = std::make_unique<PlayerRenderer>(shaders);
	gui_renderer   = std::make_unique<GuiRenderer>(shaders);

	load_static_data();

	rc2D = std::make_unique<RadianceCascades2D>(*this);
	rc3D = std::make_unique<RadianceCascades3D>(*this);
}
OpenglRenderer::~OpenglRenderer () {
	ImGui_ImplOpenGL3_Shutdown();
}

void OpenglRenderer::screenshot_imgui () {
	trigger_screenshot = ImGui::Button("Screenshot [F8]") || g->input.buttons[KEY_F8].went_down;
	ImGui::SameLine();
	ImGui::Checkbox("With HUD", &screenshot_hud);
}
void OpenglRenderer::graphics_imgui () {
	ImGui::Checkbox("rc2D", &rc2D->imopen);
	ImGui::Checkbox("rc3D", &rc3D->imopen);
	rc2D->imgui();
	rc3D->imgui();

	if (ImGui::TreeNode("Debug Draw")) {
		debug_draw.imgui();

		ImGui::TreePop();
	}

	ImGui::Checkbox("draw_chunks", &chunk_renderer->_draw_chunks);

	if (ImGui::TreeNode("GUI")) {
		ImGui::Checkbox("crosshair", &gui_renderer->crosshair);
		ImGui::SliderInt("gui_scale", &gui_renderer->gui_scale, 1, 16);

		ImGui::TreePop();
	}

	fog.imgui();

	raytracer->imgui(g->input);
}

void OpenglRenderer::chunk_renderer_imgui (Chunks& chunks) {
	chunk_renderer->imgui(chunks);
}

void OpenglRenderer::render_frame (Game& game) {
	render_size = game.input.window_size;

	chunk_renderer->upload_remeshed(*game.chunks);
	raytracer->update(*this, game.input);


	glLineWidth(debug_draw.line_width);
	{
		OGL_TRACE("set state defaults");

		debug_draw.set_overrides(state);
		state.set_default();
	}

	{
		OGL_TRACE("binds");

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, debug_draw.indirect_vbo);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, block_meshes_ssbo);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, block_tiles_ssbo);

		debug_draw.update(game.input);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	{
		glClearColor(fog.fog_col.x, fog.fog_col.y, fog.fog_col.z,1);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		OGL_TRACE("3d draws");

		if (raytracer->enable)
			raytracer->draw(*this);
		else
			update_view(g->view, render_size, g->lod_center());

		//glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);

		// draw before chunks so it shows through transparent blocks
		if (g->player->selected_block)
			gui_renderer->block_highl.draw(*this, g->player->selected_block);

		debug_draw.draw_wireframe_able(state, [&] () {
			if (!raytracer->enable) {
				chunk_renderer->draw_chunks(*this);
			}
		});

		rc2D->update(*this);
		rc3D->update(*this);

		debug_draw.draw(*this);

		if (!g->activate_flycam && !g->player->third_person) {
			// clear depth buffer to draw first person items on top of everything to avoid clipping into walls
			glClear(GL_DEPTH_BUFFER_BIT); // NOTE: clobbers the depth buffer, if it's still needed for SSAO etc. we might want to use a second depth buffer instead
		}

		// draws first and third person player items
		player_rederer->draw(*this);
	}

	{
		OGL_TRACE("ui draws");

		if (trigger_screenshot && !screenshot_hud)	take_screenshot(game.input.window_size);

		if (!g->activate_flycam || g->creative_mode)
			gui_renderer->draw_gui(*this, game.input);

		game.draw_imgui();

		if (trigger_screenshot && screenshot_hud)	take_screenshot(game.input.window_size);
		trigger_screenshot = false;
	}

	TracyGpuCollect;
	
	{
		ZoneScopedN("glfwSwapBuffers");
		glfwSwapBuffers(game.window);
	}
}

//// OpenglRenderer
bool OpenglRenderer::load_textures (GenericVertexData& mesh_data) {
	log("Loading textures...");

	{
		Image<srgba8> img;
		//if (!img.load_from_file("textures/atlas_no_alpha.png", &img)) // use non alpha version in raytracer, alpha version does not filter mips correctly
		if (!img.load_from_file("textures/atlas.png", &img))
			return false;

		// place layers at y dir so ot make the memory contiguous
		Image<srgba8> img_arr (int2(16, 16 * TILEMAP_SIZE.x * TILEMAP_SIZE.y));
		// convert texture atlas/tilemap into texture array for proper sampling in shader
		for (int y=0; y<TILEMAP_SIZE.y; ++y)
		for (int x=0; x<TILEMAP_SIZE.x; ++x) {
			Image<srgba8>::blit_rect(
				img, int2(x,y)*16,
				img_arr, int2(0, ((15-y) * TILEMAP_SIZE.x + x) * 16),
				16);
		}

		{
			glBindTexture(GL_TEXTURE_2D_ARRAY, tile_textures);

			glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_SRGB8_ALPHA8, 16, 16, TILEMAP_SIZE.x * TILEMAP_SIZE.y, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, img_arr.pixels);

			glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

			glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
		}

		{
			auto get_pixel = [&] (int x, int y, int tileid) {
				return img_arr.pixels[tileid * 16*16 + y*16 + x];
			};

			GenericVertexData data;
			item_meshes = generate_item_meshes(&mesh_data, get_pixel, ITEM_COUNT, ITEM_TILES);
		}
	}

	// heat_gradient.png   rainbow_gradient.png   blue_red_gradient.png
	upload_texture(gradient, "textures/heat_gradient.png");
	upload_texture(gui_atlas, "textures/gui.png");

	upload_texture(water_displ, "textures/periodic_perlin.png");

	return true;
}

bool OpenglRenderer::load_static_data () {

	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, block_meshes_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, g->assets->block_meshes.slices.size() * sizeof(g->assets->block_meshes.slices[0]),
		                                       g->assets->block_meshes.slices.data(), GL_STREAM_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, block_meshes_ssbo);
	}
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, block_tiles_ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, g->assets->block_tiles.size() * sizeof(g->assets->block_tiles[0]),
		                                       g->assets->block_tiles.data(), GL_STREAM_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, block_tiles_ssbo);
	}

	GenericVertexData data;

	gui_renderer->block_highl.block_highl = load_block_highlight_mesh(&data);

	if (!load_textures(data))
		return false;

	upload_buffer(mesh_data, data.vertices, data.indices);

	return true;
}

} // namespace gl
