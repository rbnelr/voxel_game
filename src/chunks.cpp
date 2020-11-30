#include "common.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "player.hpp"
#include "world.hpp"
#include "world_generator.hpp"
#include "voxel_light.hpp"

//// Chunk
block_id Chunk::get_block (int3 pos) const {
	return blocks->ids[ChunkData::pos_to_index(pos)];
}
void Chunk::set_block (int3 pos, block_id b) {
	blocks->ids[ChunkData::pos_to_index(pos)] = b;
}

//// Chunks
block_id Chunks::query_block (int3 pos, Chunk** out_chunk, int3* out_block_pos) {
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

void Chunks::set_block (int3 pos, block_id b) {
	int3 block_pos_chunk;
	int3 chunk_pos = to_chunk_pos(pos, &block_pos_chunk);
	
	Chunk* chunk = query_chunk(chunk_pos);
	assert(chunk);
	if (!chunk)
		return;

	chunk->set_block(block_pos_chunk, b);
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

			static constexpr int LOAD_LIMIT = 32;
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

	for (auto dir : { int3(-1,0,0),int3(+1,0,0), int3(0,-1,0),int3(0,+1,0),int3(0,0,-1),int3(0,0,+1) }) {
		auto* c = chunks->query_chunk(chunk->pos + dir);
		if (c) c->flags |= Chunk::REMESH;
	}

	chunk->flags |= Chunk::LOADED|Chunk::REMESH;
}
