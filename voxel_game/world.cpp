#include "world.hpp"

bool World::raycast_solid_blocks (Ray ray, float max_dist, BlockHit* out_hit) {
	return raycast_voxels(ray, max_dist, [=] (bpos bp, int face, float dist) {
		Block* b = chunks.query_block(bp);
		if (block_props[b->type].collision == CM_SOLID) {
			out_hit->dist = dist;
			out_hit->pos_world = ray.pos + ray.dir * dist;
			out_hit->block = bp;
			out_hit->face = face;
			return true;
		}
		return false;
	});
}

void World::update (WorldGenerator const& world_gen) {

	chunks.update_chunks_load(*this, world_gen, player);

}
