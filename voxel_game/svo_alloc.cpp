#include "stdafx.hpp"
#include "svo_alloc.hpp"
#include "svo.hpp"

namespace svo {

	glSparseBuffer::glSparseBuffer () {
		if (!glfwExtensionSupported("GL_ARB_sparse_buffer")) {
			clog(ERROR, "GL_ARB_sparse_buffer not supported!");
			return;
		}

		glGetIntegerv(GL_SPARSE_BUFFER_PAGE_SIZE_ARB, &gpu_page_size);
		assert(is_pot(gpu_page_size));

		glGenBuffers(1, &ssbo);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

		GLsizeiptr reserve_size = (uintptr_t)MAX_CHUNKS * MAX_NODES * sizeof(Node);
		glBufferStorage(GL_SHADER_STORAGE_BUFFER, reserve_size, nullptr, /*GL_SPARSE_STORAGE_BIT_ARB |*/ GL_DYNAMIC_STORAGE_BIT);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void glSparseBuffer::upload_changes (SVO& svo) {
		TracyGpuZone("gpu SVO upload");

		if (ssbo == 0) return; // not supported

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

		auto update_chunk = [&] (Chunk* chunk) {
			if ((chunk->flags & GPU_DIRTY) == 0) return;

			GLintptr baseoffs = (uintptr_t)svo.allocator.indexof(chunk) * MAX_NODES * sizeof(Node);

			int new_commit_ptr = round_up_pot(chunk->commit_ptr, gpu_page_size / (int)sizeof(Node));

			if (chunk->gpu_commit_ptr != new_commit_ptr) {

				//if (new_commit_ptr > chunk->gpu_commit_ptr) {
				//	GLintptr offs = (uintptr_t)chunk->gpu_commit_ptr * sizeof(Node);
				//	GLsizeiptr size = ((uintptr_t)new_commit_ptr - chunk->gpu_commit_ptr) * sizeof(Node);
				//	glBufferPageCommitmentARB(GL_SHADER_STORAGE_BUFFER, baseoffs + offs, size, true); // commit
				//} else {
				//	GLintptr offs = (uintptr_t)new_commit_ptr * sizeof(Node);
				//	GLsizeiptr size = ((uintptr_t)chunk->gpu_commit_ptr - new_commit_ptr) * sizeof(Node);
				//	glBufferPageCommitmentARB(GL_SHADER_STORAGE_BUFFER, baseoffs + offs, size, false); // decommit
				//}

				chunk->gpu_commit_ptr = new_commit_ptr;
			}

			GLsizeiptr size = chunk->commit_ptr * sizeof(Node);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, baseoffs, size, chunk->nodes);

			chunk->flags &= ~GPU_DIRTY;
		};

		update_chunk(svo.root);
		for (Chunk* chunk : svo.chunks) {
			update_chunk(chunk);
		}

		//Node tmp[8][1024];
		//
		//for (int i=0; i<8; ++i) {
		//	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, (uintptr_t)i * MAX_NODES * sizeof(Node), 1024 * sizeof(Node), tmp[i]);
		//}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}
}
