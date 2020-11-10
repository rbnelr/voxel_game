#pragma once
#include "common.hpp"
#include "chunks.hpp"
#include "graphics.hpp"

struct ChunkVertex {
	float3	pos;
	float2	uv;
	int		tex_indx;

	template <typename ATTRIBS>
	static void attributes (ATTRIBS& a) {
		int loc = 0;
		a.init(sizeof(ChunkVertex));
		a.template add<AttribMode::FLOAT, decltype(pos     )>(loc++, "pos"     , offsetof(ChunkVertex, pos     )); // ugh templates
		a.template add<AttribMode::FLOAT, decltype(uv      )>(loc++, "uv"      , offsetof(ChunkVertex, uv      ));
		a.template add<AttribMode::SINT,  decltype(tex_indx)>(loc++, "tex_indx", offsetof(ChunkVertex, tex_indx));
	}
};

struct WorldGenerator;
struct Assets;

static constexpr uint64_t CHUNK_SLICE_BYTESIZE = 1 * (1024 * 1024);
static constexpr uint64_t CHUNK_SLICE_LENGTH = CHUNK_SLICE_BYTESIZE / sizeof(ChunkVertex);

struct ChunkSliceData {
	ChunkVertex verts[CHUNK_SLICE_LENGTH];
};

// To avoid allocation and memcpy when the meshing data grows larger than predicted,
//  we output the mesh data into blocks, which can be allocated by BlockAllocator, which reuses freed blocks
struct MeshData {
	NO_MOVE_COPY_CLASS(MeshData) public:

	static constexpr int PREALLOC_SLICES = 1;

	ChunkVertex* next_ptr = nullptr;
	ChunkVertex* alloc_end = nullptr;

	int used_slices = 0;
	std::vector<ChunkSliceData*> slices;

	MeshData () {
		// Preallocate before pushing job to threadpool so there is less calls of malloc in threads
		// past experience has shown rapid calls to malloc can be bottleneck (presumably because of mutex contention in malloc)
		for (int i=0; i<PREALLOC_SLICES; ++i)
			alloc_slice();
	}
	~MeshData () {
		for (auto* s : slices)
			free_slice(s);
	}
	static void free_slice (ChunkSliceData* s) {
		if (s) {
			ZoneScopedC(tracy::Color::Crimson);
			free(s);
		}
	}

	// Free preallocated blocks when their not needed anymore (keep used blocks)
	// TODO: If malloc still shows to be a significant performance bottleneck adjust PREALLOC_BLOCKS
	//       I would like to avoid reusing blocks across frames to avoid the problem of persistent memory use while no remeshing actually happens
	void free_preallocated () {
		ZoneScopedC(tracy::Color::Crimson);
		for (int i=used_slices; i<(int)slices.size(); ++i)
			free_slice(slices[i]);
		slices.resize(used_slices);
	}

	void alloc_slice () {
		ZoneScopedC(tracy::Color::Crimson);
		slices.push_back( (ChunkSliceData*)malloc(CHUNK_SLICE_BYTESIZE) );
	}

	ChunkVertex* push () {
		if (next_ptr == alloc_end) {
			if (used_slices == (int)slices.size())
				alloc_slice();
			used_slices++;

			next_ptr  = slices.back()->verts;
			alloc_end = slices.back()->verts + CHUNK_SLICE_LENGTH;
		}
		return next_ptr++;
	}

	uint32_t get_vertex_count (int slice_i) {
		if (slice_i < used_slices-1)
			return (uint32_t)CHUNK_SLICE_LENGTH;
		return (uint32_t)(next_ptr - slices[used_slices-1]->verts);
	}
};

struct ChunkMesh {
	MeshData opaque_vertices;
	MeshData tranparent_vertices;
};

void mesh_chunk (Assets const& assets, WorldGenerator const& wg, Chunk* chunk, ChunkMesh* mesh);
