#pragma once
#include "common.hpp"
#include "chunks.hpp"

struct WorldGenerator;
struct Graphics;

static constexpr uint64_t MESHING_BLOCK_BYTESIZE = 1 * (1024 * 1024);
static constexpr uint64_t MESHING_BLOCK_COUNT = MESHING_BLOCK_BYTESIZE / sizeof(ChunkVertex);

struct MeshingBlock {
	ChunkVertex verts[MESHING_BLOCK_COUNT];
};

// To avoid allocation and memcpy when the meshing data grows larger than predicted,
//  we output the mesh data into blocks, which can be allocated by BlockAllocator, which reuses freed blocks
struct MeshData {
	NO_MOVE_COPY_CLASS(MeshData) public:

	static constexpr int PREALLOC_BLOCKS = 0;

	ChunkVertex* next_ptr = nullptr;
	ChunkVertex* alloc_end = nullptr;

	int used_blocks = 0;
	std::vector<MeshingBlock*> blocks;

	MeshData () {
		// Preallocate before pushing job to threadpool so there is less calls of malloc in threads
		// past experience has shown rapid calls to malloc can be bottleneck (presumably because of mutex contention in malloc)
		for (int i=0; i<PREALLOC_BLOCKS; ++i)
			alloc_block();
	}
	~MeshData () {
		for (auto* b : blocks) {
			ZoneScopedNC("MeshData::free_block", tracy::Color::Crimson);
			free(b);
		}
	}

	// Free preallocated blocks when their not needed anymore (keep used blocks)
	// TODO: If malloc still shows to be a significant performance bottleneck adjust PREALLOC_BLOCKS
	//       I would like to avoid reusing blocks across frames to avoid the problem of persistent memory use while no remeshing actually happens
	void free_preallocated () {
		for (int i=used_blocks; i<(int)blocks.size(); ++i)
			free(blocks[i]);
		blocks.resize(used_blocks);
	}

	void alloc_block () {
		ZoneScopedC(tracy::Color::Crimson);
		blocks.push_back( (MeshingBlock*)malloc(MESHING_BLOCK_BYTESIZE) );
	}

	ChunkVertex* push () {
		if (next_ptr == alloc_end) {
			if (used_blocks == (int)blocks.size())
				alloc_block();

			next_ptr  = blocks.back()->verts;
			alloc_end = blocks.back()->verts + MESHING_BLOCK_COUNT;
		}
		return next_ptr++;
	}
};

struct ChunkMesh {
	MeshData opaque_vertices;
	MeshData tranparent_vertices;
};

struct RemeshChunkJob : ThreadingJob { // Chunk remesh
	// input
	Chunk* chunk;
	Chunks* chunks; // not modfied
	Graphics const* graphics;
	WorldGenerator const* wg;
	// output
	ChunkMesh mesh;

	RemeshChunkJob (Chunk* chunk, Chunks* chunks, Graphics const* graphics, WorldGenerator const* wg):
		chunk{chunk}, chunks{chunks}, graphics{graphics}, wg{wg}, mesh{} {}
	virtual ~RemeshChunkJob() = default;

	virtual void execute ();
	virtual void finalize ();
};

struct ChunkRemesher {
	size_t remesh_chunks_count;

	void queue_remeshing (Chunks& chunks, Graphics const& graphics, WorldGenerator const& wg);
	void finish_remeshing ();
};
