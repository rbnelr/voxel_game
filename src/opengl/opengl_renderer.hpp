#pragma once
#include "common.hpp"
#include "engine/renderer.hpp"
#include "opengl_context.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "gl_chunk_renderer.hpp"
#include "gl_raytracer.hpp"
#include "gl_dbg_draw.hpp"
#include "bloom.hpp"
#include "engine/input.hpp"

namespace gl {

// mesh has to be bound
inline void draw_submesh (GenericSubmesh submesh) {
	glDrawElementsBaseVertex(GL_TRIANGLES,
		submesh.index_count, // vertex draw count is how many indicies there are
		GL_UNSIGNED_INT,
		(void*)(submesh.index_offs * sizeof(uint32_t)), // index offset
		(GLint)submesh.base_vertex);
}

struct ViewUniforms {
	// forward
	float4x4 world_to_clip;
	float4x4 world_to_cam;
	float4x4 cam_to_clip;
	// inverse
	float4x4 clip_to_world;
	float4x4 clip_to_cam;
	float4x4 cam_to_world;

	float    clip_near;
	float    clip_far;

	float2   _pad;

	float2   viewport_size;
	// 1 / viewport_size * 2
	// use  px_center * inv_double_viewport_size - 1.0  to get [-1,1] uvs from px coords
	float2   inv_viewport_size2;

	// for simpler RT get_ray calculation float4 for padding
	float4   frust_x; // from center of near plane to right edge
	float4   frust_y; // from center of near plane to top edge
	float4   frust_z; // from camera center to center of near plane

	float4   cam_pos; // cam position
	float4   cam_forw; // like frust_z but normalized to 1 world unit

	void set (Camera_View const& view, float2 const& render_size) {
		world_to_clip = view.cam_to_clip * (float4x4)view.world_to_cam;
		world_to_cam = (float4x4)view.world_to_cam;
		cam_to_clip = view.cam_to_clip;

		clip_to_world = (float4x4)view.cam_to_world * view.clip_to_cam;
		clip_to_cam = view.clip_to_cam;
		cam_to_world = (float4x4)view.cam_to_world;

		clip_near = view.clip_near;
		clip_far = view.clip_far;

		viewport_size = render_size;
		inv_viewport_size2 = 2.0f / render_size;

		{
			float3 cam_x   = (float3)cam_to_world.arr[0];
			float3 cam_y   = (float3)cam_to_world.arr[1];
			float3 cam_z   = (float3)cam_to_world.arr[2];
			float3 cam_pos = (float3)view.cam_to_world.arr[3];

			this->frust_x = float4(cam_x * view.frustrum_size.x / 2.0f, 0);
			this->frust_y = float4(cam_y * view.frustrum_size.y / 2.0f, 0);
			this->frust_z = float4(-cam_z * view.clip_near, 0);
			this->cam_pos = float4(cam_pos, 0);

			this->cam_forw = float4(normalize(-cam_z), 0);
		}
	}
};
struct CommonUniforms {
	ViewUniforms view;
};

struct BlockHighlight {
	Shader*					shad;
	BlockHighlightSubmeshes	block_highl;

	BlockHighlight (Shaders& shaders) {
		shad = shaders.compile("block_highlight");
	}
	void draw (OpenglRenderer& r, SelectedBlock& block);
};

struct GuiRenderer {
	Shader*			gui_shad;
	Sampler			gui_sampler = {"gui_sampler"};

	struct GUIVertex {
		float2 pos; // 2d because we display 2d gui
		float3 normal; // but still 3d normals for a bit of lighting on the pseudo 3d cubes for blocks
		float3 uvi; // z is tile texture index, -1 means gui texture
		
		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			int loc = 0;
			a.init(sizeof(GUIVertex));
			a.template add<AttribMode::FLOAT, decltype(pos   )>(loc++, "pos"   , offsetof(GUIVertex, pos   ));
			a.template add<AttribMode::FLOAT, decltype(normal)>(loc++, "normal", offsetof(GUIVertex, normal));
			a.template add<AttribMode::FLOAT, decltype(uvi   )>(loc++, "uvi"   , offsetof(GUIVertex, uvi   ));
		}
	};

	IndexedBuffer	gui_ib	= indexed_buffer<GUIVertex>("GUIRenderer.gui_vbo");

	std::vector<GUIVertex> gui_vertex_data;
	std::vector<uint16_t> gui_index_data;

	// allocate vertices for N quads (4 each) and already fill indices for them
	GUIVertex* push_quads (int count) {
		size_t base_idx = gui_vertex_data.size();
		gui_vertex_data.resize(base_idx + 4*count);
		auto* verts = &gui_vertex_data[base_idx];

		size_t idx_offs = gui_index_data.size();
		gui_index_data.resize(idx_offs + 6*count);
		auto* indices = &gui_index_data[idx_offs];

		for (int j=0; j<count; ++j) {
			for (int i=0; i<6; ++i) {
				*indices++ = (uint16_t)(base_idx + j*4 + QUAD_INDICES[i]);
			}
		}

		return verts;
	}

	struct AtlasUVs {
		float2 pos;
		float2 size;
	};
	static constexpr AtlasUVs crosshair_uv			= { float2(   0,    0)   ,    32    };
	static constexpr AtlasUVs frame_uv				= { float2(32*1,    0) +6,    16 +4 };
	static constexpr AtlasUVs frame_highl_uv		= { float2(32*2,    0) +6,    16 +4 };
	static constexpr AtlasUVs frame_grabbed_uv		= { float2(32*2, 32*1) +6,    16 +4 };
	static constexpr AtlasUVs frame_selected_uv		= { float2(32*3,    0) +4,    16 +8 };

	int gui_scale = 4;
	bool crosshair = false;

	// render quad with pixel coords
	void draw_gui_quad (float2 const& pos, float2 const& size, AtlasUVs const& uv);
	void draw_item_quad (float2 const& pos, float2 const& size, item_id item);

	GuiRenderer (Shaders& shaders) {
		gui_shad = shaders.compile("gui");

		glSamplerParameteri(gui_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glSamplerParameterf(gui_sampler, GL_TEXTURE_MAX_ANISOTROPY, 1);
	}
	void draw_gui (OpenglRenderer& r, Input& I, Game& game);

	void update_gui (Input& I, Game& game);
};

// Drawing of meshes that are displayed in first and third person for now
struct PlayerRenderer {
	Shader*			held_block_shad;
	Shader*			held_item_shad;

	PlayerRenderer (Shaders& shaders) {
		held_block_shad    = shaders.compile("held_block");
		held_item_shad     = shaders.compile("held_item");
	}
	void draw (OpenglRenderer& r, Game& game);

};

class OpenglRenderer : public Renderer {
public:
	SERIALIZE(OpenglRenderer, chunk_renderer, raytracer, debug_draw, imopen)

	struct ImguiOpen {
		SERIALIZE(ImguiOpen, framebuffer, debugdraw, gui)
		bool framebuffer=true, debugdraw=true, gui=true;
	};

	virtual void deserialize (nlohmann::ordered_json const& j) { j.get_to(*this); }
	virtual void serialize (nlohmann::ordered_json& j) { j = *this; }

	OpenglContext	ctx; // make an 'opengl context' first member so opengl init happens before any other ctors (which might make opengl calls)

	int2			render_size;

	Vao				dummy_vao = {"dummy_vao"};

	StateManager	state;
	Shaders			shaders;

	CommonUniforms	common_uniforms;

	bool			trigger_screenshot = false;
	bool			screenshot_hud = false;

	ChunkRenderer	chunk_renderer	= ChunkRenderer(shaders);
	PlayerRenderer	player_rederer	= PlayerRenderer(shaders);
	Raytracer		raytracer		= Raytracer(shaders);

	BlockHighlight	block_highl		= BlockHighlight(shaders);
	GuiRenderer		gui_renderer	= GuiRenderer(shaders);

	Ubo				common_uniforms_ubo = {"common_ubo"};

	Sampler			pixelated_sampler = {"pixelated_sampler"};
	Sampler			smooth_sampler = {"smooth_sampler"};
	Sampler			smooth_sampler_wrap = {"normal_sampler_wrap"};

	Ssbo			block_meshes_ssbo = {"block_meshes_ssbo"};
	Ssbo			block_tiles_ssbo = {"block_tiles_ssbo"};

	struct BreakingTile {
		int first;
		int count;
	};
	BreakingTile	damage_tiles = { 15*16 + 6, 10 };

	IndexedBuffer				mesh_data = indexed_buffer<GenericVertex>("mesh_data");
	std::vector<GenericSubmesh>	item_meshes;
	
	Texture2DArray	tile_textures	= {"tile_textures"};
	Texture2D		gui_atlas		= {"gui_atlas"};
	Texture2D		gradient		= {"gradient"};

	glDebugDraw		debug_draw = glDebugDraw(shaders);

	virtual bool get_vsync () {
		return ctx.vsync;
	}
	virtual void set_vsync (bool state) {
		ctx.set_vsync(state);
	}
	
	void update_view (Camera_View const& view, int2 viewport_size) {
		memset(&common_uniforms, 0, sizeof(common_uniforms)); // zero padding
		common_uniforms.view.set(view, (float2)viewport_size);
		upload_bind_ubo(common_uniforms_ubo, 0, &common_uniforms, sizeof(common_uniforms));
			
		glViewport(0,0, viewport_size.x, viewport_size.y);
		glScissor (0,0, viewport_size.x, viewport_size.y);
	}

	bool load_textures (GenericVertexData& mesh_data); // can be reloaded
	bool load_static_data ();

	OpenglRenderer (GLFWwindow* window, char const* app_name): ctx{window, app_name} {

		// I never align my pixel rows
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		load_static_data();

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
	}
	virtual ~OpenglRenderer () {}

	virtual void frame_begin (GLFWwindow* window, Input& I, kiss::ChangedFiles& changed_files);
	virtual void render_frame (GLFWwindow* window, Input& I, Game& game);

	ImguiOpen imopen;

	virtual void screenshot_imgui (Input& I) {
		trigger_screenshot = ImGui::Button("Screenshot [F8]") || I.buttons[KEY_F8].went_down;
		ImGui::SameLine();
		ImGui::Checkbox("With HUD", &screenshot_hud);
	}
	virtual void graphics_imgui (Input& I) {
		if (imgui_treenode("Debug Draw", &imopen.debugdraw)) {
			debug_draw.imgui();

			ImGui::TreePop();
		}

		ImGui::Checkbox("draw_chunks", &chunk_renderer._draw_chunks);

		if (imgui_treenode("GUI", &imopen.gui)) {
			ImGui::Checkbox("crosshair", &gui_renderer.crosshair);
			ImGui::SliderInt("gui_scale", &gui_renderer.gui_scale, 1, 16);

			ImGui::TreePop();
		}

		raytracer.imgui(I);
	}

	virtual void chunk_renderer_imgui (Chunks& chunks) {
		chunk_renderer.imgui(chunks);
	}
};

} // namespace gl
