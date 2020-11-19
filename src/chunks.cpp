#include "common.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "player.hpp"
#include "world.hpp"
#include "world_generator.hpp"
#include "voxel_light.hpp"

//// Chunk
void ChunkData::init_border () {

	for (int i=0; i<COUNT; ++i)
		id[i] = B_NULL;

	memset(block_light, 0, sizeof(block_light));
	//memset(sky_light, 0, sizeof(sky_light)); // always inited by update sky light after chunk gen
	memset(hp, 255, sizeof(hp));
}

void Chunk::init_blocks () {
	ZoneScoped;

	blocks->init_border();
}

Block Chunk::get_block (int3 pos) const {
	return blocks->get(pos);
}
void Chunk::set_block_unchecked (int3 pos, Block b) {
	blocks->set(pos, b);
}
void Chunk::_set_block_no_light_update (Chunks& chunks, int3 pos_in_chunk, Block b) {
	ZoneScoped;

	Block blk = blocks->get(pos_in_chunk);
	
	blocks->set(pos_in_chunk, b);
	flags |= REMESH;

	bool3 lo = (int3)pos_in_chunk == 0;
	bool3 hi = (int3)pos_in_chunk == (int3)CHUNK_SIZE-1;
	if (any(lo || hi)) {
		// block at border

		auto update_neighbour_block_copy = [=, &chunks] (int3 chunk_offset, int3 block) {
			auto chunk = chunks.query_chunk(pos + chunk_offset);
			if (chunk) {
				chunk->blocks->set(block, b);

				flags |= REMESH;
			}
		};

		if (lo.x) {
			update_neighbour_block_copy(int3(-1, 0, 0), int3(CHUNK_SIZE, pos_in_chunk.y, pos_in_chunk.z));
		} else if (hi.x) {
			update_neighbour_block_copy(int3(+1, 0, 0), int3(       -1, pos_in_chunk.y, pos_in_chunk.z));
		}
		if (lo.y) {
			update_neighbour_block_copy(int3(0, -1, 0), int3(pos_in_chunk.x, CHUNK_SIZE, pos_in_chunk.z));
		} else if (hi.y) {
			update_neighbour_block_copy(int3(0, +1, 0), int3(pos_in_chunk.x,        -1, pos_in_chunk.z));
		}
		if (lo.z) {
			update_neighbour_block_copy(int3(0, 0, -1), int3(pos_in_chunk.x, pos_in_chunk.y, CHUNK_SIZE));
		} else if (hi.z) {
			update_neighbour_block_copy(int3(0, 0, +1), int3(pos_in_chunk.x, pos_in_chunk.y,        -1));
		}
	}
}
void Chunk::set_block (Chunks& chunks, int3 pos_in_chunk, Block b) {
	ZoneScoped;

	Block blk = blocks->get(pos_in_chunk);

	bool only_texture_changed = blk.id == b.id && blk.block_light == b.block_light;
	if (only_texture_changed) {
		blocks->set(pos_in_chunk, b);
	} else {
		uint8_t old_block_light = blk.block_light;

		uint8_t new_block_light = calc_block_light_level(this, pos_in_chunk, b);
		int3 world_pos = pos_in_chunk + pos * CHUNK_SIZE;

		b.block_light = new_block_light;

		_set_block_no_light_update(chunks, pos_in_chunk, b);

		update_block_light(chunks, world_pos, old_block_light, new_block_light);

		update_sky_light_column(this, pos_in_chunk);
	}

	flags |= Chunk::REMESH;
}

void set_neighbour_blocks_nx (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_SIZE; ++z) {
		for (int y=0; y<CHUNK_SIZE; ++y) {
			dst.set_block_unchecked(int3(CHUNK_SIZE, y,z), src.get_block(int3(0,y,z)));
		}
	}
}
void set_neighbour_blocks_px (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_SIZE; ++z) {
		for (int y=0; y<CHUNK_SIZE; ++y) {
			dst.set_block_unchecked(int3(-1, y,z), src.get_block(int3(CHUNK_SIZE-1, y,z)));
		}
	}
}
void set_neighbour_blocks_ny (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_SIZE; ++z) {
		for (int x=0; x<CHUNK_SIZE; ++x) {
			dst.set_block_unchecked(int3(x, CHUNK_SIZE, z), src.get_block(int3(x,0,z)));
		}
	}
}
void set_neighbour_blocks_py (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_SIZE; ++z) {
		for (int x=0; x<CHUNK_SIZE; ++x) {
			dst.set_block_unchecked(int3(x, -1, z), src.get_block(int3(x, CHUNK_SIZE-1, z)));
		}
	}
}
void set_neighbour_blocks_nz (Chunk const& src, Chunk& dst) {
	for (int y=0; y<CHUNK_SIZE; ++y) {
		for (int x=0; x<CHUNK_SIZE; ++x) {
			dst.set_block_unchecked(int3(x, y, CHUNK_SIZE), src.get_block(int3(x,y,0)));
		}
	}
}
void set_neighbour_blocks_pz (Chunk const& src, Chunk& dst) {
	for (int y=0; y<CHUNK_SIZE; ++y) {
		for (int x=0; x<CHUNK_SIZE; ++x) {
			dst.set_block_unchecked(int3(x, y, -1), src.get_block(int3(x, y, CHUNK_SIZE-1)));
		}
	}
}

void Chunk::update_neighbour_blocks (Chunks& chunks) {
	ZoneScoped;

	Chunk* chunk;
	if ((chunk = chunks.query_chunk(pos + int3(-1, 0, 0))) && (chunk->flags & Chunk::LOADED)) {
		set_neighbour_blocks_nx(*this, *chunk);
		set_neighbour_blocks_px(*chunk, *this);
		chunk->flags |= REMESH;
	}
	if ((chunk = chunks.query_chunk(pos + int3(+1, 0, 0))) && (chunk->flags & Chunk::LOADED)) {
		set_neighbour_blocks_px(*this, *chunk);
		set_neighbour_blocks_nx(*chunk, *this);
		chunk->flags |= REMESH;
	}

	if ((chunk = chunks.query_chunk(pos + int3( 0,-1, 0))) && (chunk->flags & Chunk::LOADED)) {
		set_neighbour_blocks_ny(*this, *chunk);
		set_neighbour_blocks_py(*chunk, *this);
		chunk->flags |= REMESH;
	}
	if ((chunk = chunks.query_chunk(pos + int3( 0,+1, 0))) && (chunk->flags & Chunk::LOADED)) {
		set_neighbour_blocks_py(*this, *chunk);
		set_neighbour_blocks_ny(*chunk, *this);
		chunk->flags |= REMESH;
	}

	if ((chunk = chunks.query_chunk(pos + int3( 0, 0,-1))) && (chunk->flags & Chunk::LOADED)) {
		set_neighbour_blocks_nz(*this, *chunk);
		set_neighbour_blocks_pz(*chunk, *this);
		chunk->flags |= REMESH;
	}
	if ((chunk = chunks.query_chunk(pos + int3( 0, 0,+1))) && (chunk->flags & Chunk::LOADED)) {
		set_neighbour_blocks_pz(*this, *chunk);
		set_neighbour_blocks_nz(*chunk, *this);
		chunk->flags |= REMESH;
	}
}

//// Chunks
Block Chunks::query_block (int3 pos, Chunk** out_chunk, int3* out_block_pos) {
	if (out_chunk)
		*out_chunk = nullptr;

	int3 block_pos_chunk;
	int3 chunk_pos = to_chunk_pos(pos, &block_pos_chunk);
	if (out_block_pos)
		*out_block_pos = block_pos_chunk;

	Chunk* chunk = query_chunk(chunk_pos);
	if (!chunk)
		return B_NULL;

	if (out_chunk)
		*out_chunk = chunk;
	return chunk->get_block(block_pos_chunk);
}

void Chunks::set_block (int3 pos, Block& b) {
	int3 block_pos_chunk;
	int3 chunk_pos = to_chunk_pos(pos, &block_pos_chunk);
	
	Chunk* chunk = query_chunk(chunk_pos);
	assert(chunk);
	if (!chunk)
		return;

	chunk->set_block(*this, block_pos_chunk, b);
}


void Chunks::update_chunk_loading (World const& world, WorldGenerator const& wg, Player const& player) {
	ZoneScoped;

	{ // chunk unloading
		float unload_dist = generation_radius + deletion_hysteresis;
		float unload_dist_sqr = unload_dist * unload_dist;

		for (chunk_id id=0; id<max_id; ++id) {
			if ((chunks[id].flags & Chunk::LOADED) == 0) continue;
			
			float dist_sqr = chunk_dist_sq(chunks[id].pos, player.pos);
			if (dist_sqr > unload_dist_sqr)
				free_chunk(id);
		}
	}

	{ // chunk loading
		int3 start =	(int3)floor(	((float3)player.pos - generation_radius) / (float3)CHUNK_SIZE );
		int3 end =	(int3)ceil(	((float3)player.pos + generation_radius) / (float3)CHUNK_SIZE );

		// check all chunk positions within a square of chunk_generation_radius
		std::vector<int3> chunks_to_generate;

		{
			ZoneScopedN("chunks_to_generate iterate all chunks");
			ZoneValue((end.x-start.x)*(end.y-start.y)*(end.z-start.z));

			int3 cp;
			for (cp.z = start.z; cp.z<end.z; ++cp.z) {
				for (cp.y = start.y; cp.y<end.y; ++cp.y) {
					for (cp.x = start.x; cp.x<end.x; ++cp.x) {
						auto* chunk = query_chunk(cp);
						float dist_sqr = chunk_dist_sq(cp, player.pos);

						if (!chunk) {
							if (dist_sqr <= generation_radius*generation_radius) {
								// chunk is within chunk_generation_radius and not yet generated
								chunks_to_generate.push_back(cp);
							}
						}
					}
				}
			}
		}

		{
			ZoneScopedN("std::sort(chunks_to_generate)");
			ZoneValue(chunks_to_generate.size());
			// load chunks nearest to player first
			std::sort(chunks_to_generate.begin(), chunks_to_generate.end(),
				[&] (int3 l, int3 r) { return chunk_dist_sq(l, player.pos) < chunk_dist_sq(r, player.pos); }
			);
		}

		{
			ZoneScopedN("chunks_to_generate finalize jobs");

			static constexpr int LOAD_LIMIT = 64;
			std::unique_ptr<ThreadingJob> jobs[LOAD_LIMIT];

			int count = (int)background_threadpool.results.pop_n(jobs, ARRLEN(jobs));
			for (int i=0; i<count; ++i)
				jobs[i]->finalize();

			background_queued_count -= (int)count;
		}

		{
			ZoneScopedN("chunks_to_generate push jobs");

			static constexpr int QUEUE_LIMIT = 256;
			std::unique_ptr<ThreadingJob> jobs[QUEUE_LIMIT];

			int count = std::min((int)chunks_to_generate.size(), (int)ARRLEN(jobs) - background_queued_count);
			for (int i=0; i<count; ++i) {
				auto cp = chunks_to_generate[i];

				auto id = alloc_chunk(cp);
				float dist = chunk_dist_sq(cp, player.pos);

				chunks[id].blocks = std::make_unique<ChunkData>();

				jobs[i] = std::make_unique<WorldgenJob>(&chunks[id], this, &wg);
			}

			background_threadpool.jobs.push_n(jobs, count);
			background_queued_count += (int)count;

			TracyPlot("background_queued_count", (int64_t)background_queued_count);
		}
	}

}

void WorldgenJob::finalize () {
	ZoneScoped;

	chunk->flags |= Chunk::LOADED|Chunk::REMESH;
	chunk->update_neighbour_blocks(*chunks);
}
