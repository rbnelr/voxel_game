#include "common.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "player.hpp"
#include "world.hpp"
#include "world_generator.hpp"
#include "voxel_light.hpp"

block_id Chunks::query_block (int3 pos, Chunk** out_chunk, int3* out_block_pos) {
	if (out_chunk)
		*out_chunk = nullptr;

	int3 block_pos_chunk;
	int3 chunk_pos = to_chunk_pos(pos, &block_pos_chunk);
	if (out_block_pos)
		*out_block_pos = block_pos_chunk;

	Chunk* chunk = query_chunk(chunk_pos);
	if (!chunk || (chunk->flags & Chunk::LOADED) == 0)
		return B_NULL;

	if (out_chunk)
		*out_chunk = chunk;
	return chunk->voxels.get_block(block_pos_chunk.x, block_pos_chunk.y, block_pos_chunk.z);
}

void Chunks::set_block (int3 pos, block_id b) {
	int3 block_pos_chunk;
	int3 chunk_pos = to_chunk_pos(pos, &block_pos_chunk);
	
	Chunk* chunk = query_chunk(chunk_pos);
	assert(chunk);
	if (!chunk)
		return;

	chunk->voxels.set_block(block_pos_chunk.x, block_pos_chunk.y, block_pos_chunk.z, b);
}

void Chunks::update_chunk_loading (World const& world, WorldGenerator const& wg, Player const& player) {
	ZoneScoped;

	{ // chunk loading/unloading
		constexpr float BUCKET_FAC = 1.0f / (CHUNK_SIZE*CHUNK_SIZE * 4);

		// check all chunk positions within a square of chunk_generation_radius
		std_vector< std_vector<int3> > chunks_to_generate;
		chunks_to_generate.resize((int)(load_radius*load_radius * BUCKET_FAC) + 1);
		
		{
			ZoneScopedN("chunks_to_generate iterate all chunks");

			{
				// queue first chunk explicitly, so that we can queue the rest via the neighbour refs
				int3 player_pos = floori(player.pos / CHUNK_SIZE);
				if (pos_to_id.find(player_pos) == pos_to_id.end()) {
					chunks_to_generate[0].push_back(player_pos);
				}
			}

			float load_dist_sqr = load_radius * load_radius;
			float unload_dist = load_radius + unload_hyster;
			float unload_dist_sqr = unload_dist * unload_dist;

			for (chunk_id id=0; id<max_id; ++id) {
				chunks[id]._validate_flags();
				if ((chunks[id].flags & Chunk::LOADED) == 0) continue;

				float dist_sqr = chunk_dist_sq(chunks[id].pos, player.pos);

				if (dist_sqr > unload_dist_sqr) {
					// unload
					free_chunk(id);
				} else {
					// check neighbour references in chunk for nulls
					// if null check if chunks is supposed to be loaded, if yes then queue
					// note that duplicates can be queued here since the neighbours can be reached via 6 directions
					// instead of doing a potentially expensive hashmap lookup or linear search
					// simply deal with the duplicates later, when we have decided which chunks we even consider to generate (these are more limited in number)
					for (int i=0; i<6; ++i) {
						if (chunks[id].neighbours[i] == U16_NULL) {
							// neighbour chunk not yet loaded
							int3 pos = chunks[id].pos + OFFSETS[i];
							float ndist_sqr = chunk_dist_sq(pos, player.pos);

							if (ndist_sqr <= load_radius*load_radius) {
								int bucketi = (int)(ndist_sqr * BUCKET_FAC);
								chunks_to_generate[bucketi].push_back(pos);
							}
						}
					}
				}
			}

		}

		{
			ZoneScopedN("chunks_to_generate finalize jobs");

			static constexpr int MAX_REMESH_PER_THREAD_FRAME = 3;
			static const int LOAD_LIMIT = parallelism_threads * MAX_REMESH_PER_THREAD_FRAME;

			std::unique_ptr<WorldgenJob> jobs[64];

			int count = (int)background_threadpool.results.pop_n(jobs, std::min((size_t)LOAD_LIMIT, ARRLEN(jobs)));
			for (int i=0; i<count; ++i) {
				auto job = std::move(jobs[i]);
				auto& chunk = *job->chunk;

				for (int i=0; i<6; ++i) {
					if (chunk.neighbours[i] != U16_NULL) {
						auto& n = chunks[chunk.neighbours[i]];
						if (n.flags & Chunk::LOADED) n.flags |= Chunk::DIRTY;
					}
				}

				chunk.flags |= Chunk::LOADED|Chunk::DIRTY;
			}

			background_queued_count -= (int)count;
		}

		{
			ZoneScopedN("chunks_to_generate push jobs");

			static constexpr int QUEUE_LIMIT = 256;
			std::unique_ptr<WorldgenJob> jobs[QUEUE_LIMIT];

			// Process bucket-sorted chunks_to_generate in order, remove duplicates
			//  and push jobs until threadpool has at max background_queued_count jobs (ignore the remaining chunks, which will get pushed as soon as jobs are completed)
			int bucket=0, j=0; // sort bucket indices

			int max_count = (int)ARRLEN(jobs) - background_queued_count;
			int count = 0;
			while (count < max_count) {

				while (bucket < chunks_to_generate.size() && j == chunks_to_generate[bucket].size()) {
					bucket++;
					j = 0;
				}
				if (bucket >= chunks_to_generate.size())
					break; // all chunks_to_generate processed

				auto pos = chunks_to_generate[bucket][j++];
				if (pos_to_id.find(pos) != pos_to_id.end())
					continue; // duplicate (chunk with pos already alloc_chunk'd), ignore

				auto id = alloc_chunk(pos);

				auto job = std::make_unique<WorldgenJob>();
				job->chunk = &chunks[id];
				job->wg = &wg;
				jobs[count++] = std::move(job);
			}

			background_threadpool.jobs.push_n(jobs, count);
			background_queued_count += (int)count;

			TracyPlot("background_queued_count", (int64_t)background_queued_count);
		}
	}

}
