#include "graphics.hpp"
#include "../util/string.hpp"
#include "../util/geometry.hpp"
#include "../chunks.hpp"
#include "../world.hpp"
using namespace kiss;

#define QUAD(a,b,c,d) b,c,a, a,c,d // facing outward
#define QUAD_INWARD(a,b,c,d) a,d,b, b,d,c // facing inward

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

PlayerGraphics::PlayerGraphics () {
	std::vector<GenericVertex> verts;
	push_cube<GenericVertex>([&] (float3 pos, int face, float2 face_uv) {
		verts.push_back({ pos * float3(0.15f, 0.4f, 0.15f), srgba(255) });
	});

	fist_mesh = Mesh<GenericVertex>(verts);
}

void PlayerGraphics::draw (Player const& player) {
	if (shader) {
		shader.bind();

		float3x4 mat = player.head_to_world * translate(pos) * rot;

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
				fprintf(stderr, "Texture size does not match textures/missing.png, all textures must be of the same size!\n");
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

	BlockTileInfo add_block (block_type bt) {
		auto name = BLOCK_NAMES[bt];
		
		if (!name)
			return {0};

		auto color_filename = prints("textures/%s.png", name);
		auto alpha_filename = prints("textures/%s.alpha.png", name);
		
		Image<srgba8> color;
		Image<uint8> alpha;

		if (!Image<srgba8>::load_file(color_filename.c_str(), &color))
			return {0};

		bool has_alpha = Image<uint8>::load_file(alpha_filename.c_str(), &alpha);

		if (has_alpha && !equal(color.size, alpha.size)) {
			fprintf(stderr, "Texture size does not match between *.png and *.alpha.png, all textures must be of the same size!\n");
			assert(false);
			return {0};
		}

		if (has_alpha)
			set_alpha(color, alpha);
		
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

		tl.add_texture(Image<srgba8>("textures/missing.png"));

		for (int i=0; i<BLOCK_TYPES_COUNT; ++i) {
			block_tile_info[i] = tl.add_block((block_type)i);
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

void ChunkGraphics::draw_chunks (Chunks const& chunks) {
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
		for (Chunk const& chunk : chunks) {

			if (chunk.mesh.opaque_mesh.vertex_count != 0) {
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
		for (Chunk const& chunk : chunks) {

			if (chunk.mesh.transparent_mesh.vertex_count != 0) {
				shader.set_uniform("chunk_pos", (float3)chunk.chunk_pos_world());

				chunk.mesh.transparent_mesh.bind();
				chunk.mesh.transparent_mesh.draw();
			}
		}
	}
}

void Graphics::draw (World const& world, Camera_View const& view, bool activate_flycam, HighlightedBlock highlighted_block) {
	glViewport(0,0, input.window_size.x, input.window_size.y);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClear(GL_DEPTH_BUFFER_BIT);

	common_uniforms.set_view_uniforms(view);
	common_uniforms.set_debug_uniforms();

	gl_enable(GL_CULL_FACE, !(common_uniforms.dbg_wireframe && common_uniforms.wireframe_backfaces));

	if (activate_flycam || world.player.third_person)
		debug_graphics->push_cylinder(world.player.pos + float3(0,0, world.player.height/2), world.player.radius, world.player.height, srgba(255, 40, 255, 130), 32);

	// opaque draw
	player.draw(world.player);

	chunk_graphics.draw_chunks(world.chunks);

	skybox.draw();

	// transparent draw
	glEnable(GL_BLEND);

	if (highlighted_block) {
		block_highlight.draw((float3)highlighted_block.pos, (BlockFace)(highlighted_block.face >= 0 ? highlighted_block.face : 0));
	}

	//glCullFace(GL_FRONT);
	//chunk_graphics.draw_chunks_transparent(chunks);
	//glCullFace(GL_BACK);
	chunk_graphics.draw_chunks_transparent(world.chunks);

	glEnable(GL_CULL_FACE);
	debug_graphics->draw();

	glDisable(GL_BLEND);
}
