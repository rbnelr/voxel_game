#include "graphics.hpp"
#include "../util/string.hpp"
#include "../util/geometry.hpp"
#include "../chunks.hpp"
#include "../world.hpp"
using namespace kiss;

#define QUAD(a,b,c,d) b,c,a, a,c,d // facing outward
#define QUAD_INWARD(a,b,c,d) a,d,b, b,d,c // facing inward

template <typename T>
void push_quad (std::vector<T>* vec, T a, T b, T c, T d) {
	vec->resize( vec->size() + 6 );
	T* out = &(*vec)[vec->size() - 6];

	*out++ = b;
	*out++ = c;
	*out++ = a;
	*out++ = a;
	*out++ = c;
	*out++ = d;
}

SkyboxGraphics::SkyboxGraphics () {
	static constexpr Vertex LLL = { float3(-1,-1,-1) };
	static constexpr Vertex HLL = { float3(+1,-1,-1) };
	static constexpr Vertex LHL = { float3(-1,+1,-1) };
	static constexpr Vertex HHL = { float3(+1,+1,-1) };
	static constexpr Vertex LLH = { float3(-1,-1,+1) };
	static constexpr Vertex HLH = { float3(+1,-1,+1) };
	static constexpr Vertex LHH = { float3(-1,+1,+1) };
	static constexpr Vertex HHH = { float3(+1,+1,+1) };

	static constexpr Vertex arr[6*6] = {
		QUAD_INWARD(	LHL,
						LLL,
						LLH,
						LHH ),

		QUAD_INWARD(	HLL,
						HHL,
						HHH,
						HLH ),

		QUAD_INWARD(	LLL,
						HLL,
						HLH,
						LLH ),

		QUAD_INWARD(	HHL,
						LHL,
						LHH,
						HHH ),

		QUAD_INWARD(	HLL,
						LLL,
						LHL,
						HHL ),

		QUAD_INWARD(	LLH,
						HLH,
						HHH,
						LHH )
	};

	mesh.upload(arr, 6*6);
}

void SkyboxGraphics::draw () {
	if (shader) {
		shader.bind();

		glEnable(GL_DEPTH_CLAMP); // prevent skybox clipping with near plane
		glDepthRange(1, 1); // Draw skybox behind everything, even though it's actually a box of size 1 placed on the camera

		mesh.bind();
		mesh.draw();

		glDepthRange(0, 1);
		glDisable(GL_DEPTH_CLAMP);
	}
}

BlockHighlightGraphics::BlockHighlightGraphics () {

	lrgba col = srgba(40,40,40,240);
	lrgba dot_col = srgba(40,40,40,240);

	float r = 0.504f;
	float inset = 1.0f / 100;

	float side_r = r * 0.04f;
	
	std::vector<Vertex> vertices;

	for (int face=0; face<6; ++face) {

		auto vert = [&] (float3 v, lrgba col) {
			vertices.push_back(Vertex{ face_rotation[face] * v, col });
		};
		auto quad = [&] (float3 a, float3 b, float3 c, float3 d, lrgba col) {
			vert(b, col);	vert(c, col);	vert(a, col);
			vert(a, col);	vert(c, col);	vert(d, col);
		};

		quad(	float3(-r,-r,+r),
				float3(+r,-r,+r),
				float3(+r,-r,+r) + float3(-inset,+inset,0),
				float3(-r,-r,+r) + float3(+inset,+inset,0),
				col);

		quad(	float3(+r,-r,+r),
				float3(+r,+r,+r),
				float3(+r,+r,+r) + float3(-inset,-inset,0),
				float3(+r,-r,+r) + float3(-inset,+inset,0),
				col);

		quad(	float3(+r,+r,+r),
				float3(-r,+r,+r),
				float3(-r,+r,+r) + float3(+inset,-inset,0),
				float3(+r,+r,+r) + float3(-inset,-inset,0),
				col);

		quad(	float3(-r,+r,+r),
				float3(-r,-r,+r),
				float3(-r,-r,+r) + float3(+inset,+inset,0),
				float3(-r,+r,+r) + float3(+inset,-inset,0),
				col);

		if (face == BF_POS_Z) {
			// face highlight
			quad(	float3(-side_r,-side_r,+r),
					float3(+side_r,-side_r,+r),
					float3(+side_r,+side_r,+r),
					float3(-side_r,+side_r,+r),
					dot_col );
		}
	}

	mesh.upload(vertices);
}

void BlockHighlightGraphics::draw (float3 pos, BlockFace face) {
	if (shader) {
		shader.bind();

		shader.set_uniform("block_pos", pos);
		shader.set_uniform("face_rotation", face_rotation[face]);

		mesh.bind();
		mesh.draw();
	}
}

float2 GuiGraphics::get_quickbar_slot_center (int slot_index) {
	float offset_bottom = 2;

	float2 pos = float2(
		(float)input.window_size.x / gui_scale / 2 - (float)quickbar.size_px.x * 10.0f / 2,
		offset_bottom);
	pos += float2(slot_index + 0.5f, 0.5f) * (float2)quickbar.size_px;
	return pos;
}

void GuiGraphics::draw_texture (AtlasedTexture const& tex, float2 pos_px, float2 size_px, lrgba col) {
	float2 a = pos_px;
	float2 b = pos_px + size_px;
	push_quad(&vertices,
		{ float2(a.x, a.y) * gui_scale, tex.atlas_pos + float2(0,0) * tex.atlas_size, col },
		{ float2(b.x, a.y) * gui_scale, tex.atlas_pos + float2(1,0) * tex.atlas_size, col },
		{ float2(b.x, b.y) * gui_scale, tex.atlas_pos + float2(1,1) * tex.atlas_size, col },
		{ float2(a.x, b.y) * gui_scale, tex.atlas_pos + float2(0,1) * tex.atlas_size, col }
	);
}
void GuiGraphics::draw_color_quad (float2 pos_px, float2 size_px, lrgba col) {
	float2 a = pos_px;
	float2 b = pos_px + size_px;
	push_quad(&vertices,
		{ float2(a.x, a.y) * gui_scale, float2(-1), col },
		{ float2(b.x, a.y) * gui_scale, float2(-1), col },
		{ float2(b.x, b.y) * gui_scale, float2(-1), col },
		{ float2(a.x, b.y) * gui_scale, float2(-1), col }
	);
}


void GuiGraphics::draw_crosshair () {
	float2 size = (float2)crosshair.size_px * gui_scale * crosshair_scale;
	float2 pos = (float2)(input.window_size / 2) - size / 2; // center crosshair on screen, if resoultion is odd number will be off by 1/2 pixel

	draw_texture(crosshair, pos, size);
}
void GuiGraphics::draw_quickbar_slot (AtlasedTexture tex, int index) {
	float2 size = (float2)tex.size_px;
	float2 pos = get_quickbar_slot_center(index) - size/2;

	draw_texture(tex, pos, size);
}
void GuiGraphics::draw_quickbar_item (item_id id, int index, TileTextures const& tile_textures) {
	float size = 16 * gui_scale;
	float3 pos = float3(get_quickbar_slot_center(index) * gui_scale, 0);
	
	if (id < MAX_BLOCK_ID) {
		// draw block
		auto tile = tile_textures.block_tile_info[id];

		for (int face=0; face<6; ++face) {
			float tex_index = (float)tile.calc_texture_index((BlockFace)face);

			for (int i=0; i<6; ++i) {
				auto& b = gui_block_mesh[face*6+i];
				items_vertices.push_back(ItemsVertex{ b.pos * size + pos, b.normal, b.uv, tex_index });
			}
		}
	} else {

	}
}
void GuiGraphics::draw_quickbar (Player const& player, TileTextures const& tile_textures) {
	{ // border
		float offset_bottom = 2;

		float2 pos = float2((float)input.window_size.x / gui_scale / 2 - (float)quickbar.size_px.x * 10.0f / 2, offset_bottom);
		float2 size = (float2)quickbar.size_px * float2(10,1);

		draw_color_quad(float2(pos.x,			pos.y - 1     ), float2(size.x, 1), lrgba(0,0,0,1));
		draw_color_quad(float2(pos.x,			pos.y + size.y), float2(size.x, 1), lrgba(0,0,0,1));
		draw_color_quad(float2(pos.x - 1,		pos.y         ), float2(1, size.y), lrgba(0,0,0,1));
		draw_color_quad(float2(pos.x + size.x,  pos.y         ), float2(1, size.y), lrgba(0,0,0,1));
	}
	for (int i=0; i<10; ++i) {
		draw_quickbar_slot(quickbar, i);
	}
	draw_quickbar_slot(quickbar_selected, player.inventory.quickbar.selected);

	for (int i=0; i<10; ++i) {
		auto& slot = player.inventory.quickbar.slots[i];
		if (slot.stack_size > 0)
			draw_quickbar_item(slot.item.id, i, tile_textures);
	}
}

void GuiGraphics::draw (Player const& player, TileTextures const& tile_textures) {
	glActiveTexture(GL_TEXTURE0 + 0);
	sampler.bind(0);

	draw_crosshair();
	draw_quickbar(player, tile_textures);

	if (shader && vertices.size() > 0) {
		shader.bind();

		glUniform1i(glGetUniformLocation(shader.shader->shad, "tex"), 0);
		gui_atlas.bind();

		mesh.upload(vertices);
		vertices.clear();

		mesh.bind();
		mesh.draw();
	}

	if (shader_items && items_vertices.size() > 0) {
		shader_items.bind();

		glUniform1i(glGetUniformLocation(shader_items.shader->shad, "tile_textures"), 0);
		tile_textures.tile_textures.bind();

		items_mesh.upload(items_vertices);
		items_vertices.clear();

		items_mesh.bind();
		items_mesh.draw();
	}
}

PlayerGraphics::PlayerGraphics () {
	std::vector<GenericVertex> verts;
	push_cube<GenericVertex>([&] (float3 pos, int face, float2 face_uv) {
		verts.push_back({ pos * arm_size, srgba(255) });
	});

	fist_mesh = Mesh<GenericVertex>(verts);
}

void PlayerGraphics::draw (Player const& player) {
	if (shader) {
		shader.bind();
		
		float anim_t = player.break_block.anim_t != 0 ? player.break_block.anim_t : player.block_place.anim_t;

		auto a = animation.calc(anim_t);

		float3x4 mat = player.head_to_world * translate(a.pos) * a.rot;

		shader.set_uniform("model_to_world", (float4x4)mat);

		fist_mesh.bind();
		fist_mesh.draw();
	}
}

template <typename T>
inline Image<T> get_sub_tile (Image<T> const& c, int2 pos, int2 size) {
	auto i = Image<T>(size);
	for (int y=0; y<size.y; ++y) {
		for (int x=0; x<size.x; ++x) {
			auto C = c.get(x + pos.x * size.x, y + pos.y * size.y);
			i.set(x,y, C);
		}
	}
	return i;
}

struct TileLoader {
	std::vector< Image<srgba8> > images;
	int2 size;

	int add_texture (Image<srgba8>&& img) {
		if (images.size() == 0) {
			size = img.size;
		} else {
			if (!equal(size, img.size)) {
				logf(ERROR, "Texture size does not match textures/null.png, all textures must be of the same size!");
				assert(false);
				return 0;
			}
		}

		int index = (int)images.size();
		images.emplace_back(std::move(img));
		return index;
	}

	void set_alpha (Image<srgba8>& c, Image<uint8> const& a) {
		for (int y=0; y<c.size.y; ++y) {
			for (int x=0; x<c.size.x; ++x) {
				auto C = c.get(x,y);
				auto A = a.get(x,y);
				C.w = A;
				c.set(x,y, C);
			}
		}
	}

	BlockTileInfo add_block (block_id id) {
		auto name = BLOCK_NAMES[id];

		if (!name)
			return {0};

		auto color_filename = prints("textures/%s.png", name);
		auto alpha_filename = prints("textures/%s.alpha.png", name);
		
		Image<srgba8> color;
		Image<uint8> alpha;

		if (!Image<srgba8>::load_from_file(color_filename.c_str(), &color))
			return {0};

		bool has_alpha = Image<uint8>::load_from_file(alpha_filename.c_str(), &alpha);

		if (has_alpha && !equal(color.size, alpha.size)) {
			logf(ERROR, "Texture size does not match between *.png and *.alpha.png, all textures must be of the same size!");
			assert(false);
			return {0};
		}

		if (has_alpha)
			set_alpha(color, alpha);
		
		if (id == B_NULL)
			size = color.size;

		BlockTileInfo info;
		info.base_index = (int)images.size();

		if (color.size.y == size.x) {

			add_texture(std::move(color));

		} else if (color.size.y == size.x*2) {

			info.top = 1;
			info.bottom = 1;
			add_texture(get_sub_tile(color, int2(0,0), size));
			add_texture(get_sub_tile(color, int2(0,1), size));

		} else if (color.size.y == size.x*3) {

			info.top = 1;
			info.bottom = 2;
			add_texture(get_sub_tile(color, int2(0,1), size));
			add_texture(get_sub_tile(color, int2(0,2), size));
			add_texture(get_sub_tile(color, int2(0,0), size));

		}
		
		return info;
	}
};

TileTextures::TileTextures () {
	{
		TileLoader tl;
		for (int i=0; i<BLOCK_IDS_COUNT; ++i) {
			block_tile_info[i] = tl.add_block((block_id)i);
		}

		tile_size = tl.size;
		tile_textures.upload<srgba8, true>(tl.images);
	}

	{
		auto breaking = Image<uint8>("textures/breaking.png");

		breaking_frames_count = breaking.size.y / tile_size.x;

		std::vector< Image<uint8> > imgs;

		for (int i=0; i<breaking_frames_count; ++i) {
			imgs.push_back(get_sub_tile(breaking, int2(0,i), tile_size));
		}

		breaking_textures.upload<uint8, false>(imgs);
	}
}

void ChunkGraphics::imgui (Chunks& chunks) {
	sampler.imgui("sampler");

	tile_textures.imgui("tile_textures");

	if (ImGui::Checkbox("alpha_test", &alpha_test)) {
		chunks.remesh_all();
	}
}

void ChunkGraphics::draw_chunks (Chunks const& chunks, bool debug_frustrum_culling, bool debug_lod) {
	glActiveTexture(GL_TEXTURE0 + 0);
	tile_textures.tile_textures.bind();
	sampler.bind(0);

	glActiveTexture(GL_TEXTURE0 + 1);
	tile_textures.breaking_textures.bind();
	sampler.bind(1);

	if (shader) {
		shader.bind();

		shader.set_uniform("breaking_frames_count", (float)tile_textures.breaking_frames_count);
		shader.set_uniform("breaking_mutliplier", (float)tile_textures.breaking_mutliplier);

		shader.set_uniform("alpha_test", alpha_test);

		glUniform1i(glGetUniformLocation(shader.shader->shad, "tile_textures"), 0);
		glUniform1i(glGetUniformLocation(shader.shader->shad, "breaking_textures"), 1);

		// Draw all opaque chunk meshes
		for (Chunk const& chunk : chunks.chunks) {

			if (debug_frustrum_culling)
				debug_graphics->push_wire_cube((float3)chunk.chunk_pos_world() + (float3)CHUNK_DIM/2, (float3)CHUNK_DIM - 0.5f, chunk.culled ? srgba(255,50,50) : srgba(50,255,50));

			static lrgba cols[] = {
				srgba(255),
				srgba(255, 0, 0),
				srgba(0, 255, 0),
				srgba(0, 0, 255),
			};

			if (debug_lod)
				debug_graphics->push_wire_cube((float3)chunk.chunk_pos_world() + (float3)CHUNK_DIM/2, (float3)CHUNK_DIM - 0.5f, cols[chunk.lod]);

			if (!chunk.culled && chunk.mesh.opaque_mesh.vertex_count != 0) {
				shader.set_uniform("chunk_pos", (float3)chunk.chunk_pos_world());

				chunk.mesh.opaque_mesh.bind();
				chunk.mesh.opaque_mesh.draw();
			}
		}
	}
}

void ChunkGraphics::draw_chunks_transparent (Chunks const& chunks) {
	glActiveTexture(GL_TEXTURE0 + 0);
	tile_textures.tile_textures.bind();
	sampler.bind(0);

	glActiveTexture(GL_TEXTURE0 + 1);
	tile_textures.breaking_textures.bind();
	sampler.bind(1);

	if (shader) {
		shader.bind();

		shader.set_uniform("breaking_frames_count", (float)tile_textures.breaking_frames_count);
		shader.set_uniform("breaking_mutliplier", (float)tile_textures.breaking_mutliplier);

		shader.set_uniform("alpha_test", false);

		glUniform1i(glGetUniformLocation(shader.shader->shad, "tile_textures"), 0);
		glUniform1i(glGetUniformLocation(shader.shader->shad, "breaking_textures"), 1);

		// Draw all transparent chunk meshes
		for (Chunk const& chunk : chunks.chunks) {

			if (!chunk.culled && chunk.mesh.transparent_mesh.vertex_count != 0) {
				shader.set_uniform("chunk_pos", (float3)chunk.chunk_pos_world());

				chunk.mesh.transparent_mesh.bind();
				chunk.mesh.transparent_mesh.draw();
			}
		}
	}
}

void Graphics::frustrum_cull_chunks (Chunks& chunks, Camera_View const& view) {
	int count = 0;
	for (Chunk& chunk : chunks.chunks) {
		AABB aabb;
		aabb.lo = (float3)chunk.chunk_pos_world();
		aabb.hi = aabb.lo + (float3)CHUNK_DIM;

		chunk.culled = frustrum_cull_aabb(view.frustrum, aabb);
		if (chunk.culled)
			count++;
	}

	chunks.count_culled = count;
}

void Graphics::draw (World& world, Camera_View const& view, Camera_View const& player_view, bool activate_flycam, SelectedBlock selected_block) {
	fog.set(world.chunks.generation_radius);
	
	frustrum_cull_chunks(world.chunks, debug_frustrum_culling ? player_view : view);
	
	if (activate_flycam && debug_frustrum_culling) {

		debug_graphics->push_wire_frustrum(player_view, srgba(20, 20, 255));
		//for (int i=0; i<6; ++i)
		//	debug_graphics->push_arrow(player_view.frustrum.planes[i].pos, player_view.frustrum.planes[i].normal * 5, cols[i]);

	}
	if (activate_flycam || world.player.third_person) {
		debug_graphics->push_cylinder(world.player.pos + float3(0,0, world.player.height/2), world.player.radius, world.player.height, srgba(255, 40, 255, 130), 32);
	}

	//// OpenGL drawcalls
	common_uniforms.set_view_uniforms(view);
	common_uniforms.set_debug_uniforms();

	glViewport(0,0, input.window_size.x, input.window_size.y);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClear(GL_DEPTH_BUFFER_BIT);

	gl_enable(GL_CULL_FACE, !(common_uniforms.dbg_wireframe && common_uniforms.wireframe_backfaces));

	{ //// Opaque pass
		player.draw(world.player);

		chunk_graphics.draw_chunks(world.chunks, debug_frustrum_culling, debug_lod);

		skybox.draw();
	}

	glEnable(GL_BLEND);

	{ //// Transparent pass

		if (selected_block) {
			block_highlight.draw((float3)selected_block.pos, (BlockFace)(selected_block.face >= 0 ? selected_block.face : 0));
		}

		//glCullFace(GL_FRONT);
		//chunk_graphics.draw_chunks_transparent(chunks);
		//glCullFace(GL_BACK);
		chunk_graphics.draw_chunks_transparent(world.chunks);

		glEnable(GL_CULL_FACE);
		debug_graphics->draw();
	}

	{ //// Overlay pass
		glDisable(GL_DEPTH_TEST);

		if (!activate_flycam)
			gui.draw(world.player, chunk_graphics.tile_textures);

		glEnable(GL_DEPTH_TEST);
	}

	glDisable(GL_BLEND);
}
