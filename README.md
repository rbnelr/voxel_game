# voxel_game

A Minecraft inspired voxel engine I am developing from scratch for fun and practice.
Might be turned into game at some point if I get that far.

Features of note right now (compared to MC):
-3d chunks, to allow infinite z depth
-crude GPU raycasting based on Nvidia's ESVOs, with adhoc (very noisy) lighting
-fully dynamic octree with manual memory paging system to allow for real time edits, while keeping memory compressed and raytrace-able

Near term goals:
-LOD levels to allow for pseudo infinite view distance
-underground cave world generation with biomes
-per-voxel raytraced GI that does need to be recalculated every frame
