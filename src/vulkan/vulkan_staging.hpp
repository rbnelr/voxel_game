#pragma once
#include "common.hpp"
#include "vulkan_helper.hpp"
#include "vulkan_window.hpp"

namespace vk {

static constexpr VkDeviceSize STAGING_BUF_SIZE = 1 * (1024ull * 1024);
static constexpr int MIN_STAGING_BUFS = 1; // keep n staging bufs alive to reduce number of allocations every frame

struct MappedAllocation {
	VkBuffer		buf;
	VkDeviceMemory	mem;

	void*			mapped_ptr;

	static MappedAllocation alloc (char const* name, VulkanWindowContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props) {
		ZoneScopedC(tracy::Color::Crimson);

		auto buf = allocate_buffer(ctx.dev, ctx.pdev, size, usage, props);
		GPU_DBG_NAME(ctx, buf.buf, name);
		GPU_DBG_NAME(ctx, buf.mem, name);

		void* ptr;
		vkMapMemory(ctx.dev, buf.mem, 0, STAGING_BUF_SIZE, 0, &ptr);

		return { buf.buf, buf.mem, ptr };
	}
	void free (VkDevice dev) {
		ZoneScopedC(tracy::Color::Crimson);
		vkUnmapMemory(dev, mem);

		vkDestroyBuffer(dev, buf, nullptr);
		vkFreeMemory(dev, mem, nullptr);
	}
};

struct StagingBuffers {
	
	struct FrameData {
		std_vector<MappedAllocation> staging_bufs;
	};

	std_vector<FrameData>	frames;

	void imgui () {
		ImGui::Text("Staging bufs [frames]:");
		for (auto& f : frames) {
			ImGui::SameLine();
			ImGui::Text(" %d", f.staging_bufs.size());
		}
	}

	void create (VulkanWindowContext& ctx, int frames_in_flight) {
		frames.resize(frames_in_flight);
	}
	void destroy (VkDevice dev) {
		for (auto& f : frames) {
			for (auto& buf : f.staging_bufs)
				buf.free(dev);
		}
	}

	void new_staging_buffer (VulkanWindowContext& ctx, int cur_frame) {
		frames[cur_frame].staging_bufs.emplace_back(
			MappedAllocation::alloc(
				prints("staging_buf[%d][%d]", cur_frame, (int)frames[cur_frame].staging_bufs.size()).c_str(),
				ctx, STAGING_BUF_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
		);
	}

	VkDeviceSize avail_size = 0; // remaining size in current staging buffer
	int used_bufs = 0; // number of staging buffers used this frame (there can be more allocated if I want to keep them alive to reduce allocs)

	void staged_copy (VulkanWindowContext& ctx, VkCommandBuffer cmds, int cur_frame,
			void* data, VkDeviceSize size, VkBuffer dst_buf, VkDeviceSize dst_offs) {
		auto& frame = frames[cur_frame];

		VkDeviceSize read_ptr = 0;
		while (read_ptr < size) {
			if (avail_size == 0) {
				if (used_bufs == frame.staging_bufs.size())
					new_staging_buffer(ctx, cur_frame);

				used_bufs++;
				avail_size = STAGING_BUF_SIZE;
			}

			VkDeviceSize copy_size = std::min(avail_size, size - read_ptr);

			auto& buf = frame.staging_bufs[used_bufs-1];

			VkDeviceSize buf_offs = STAGING_BUF_SIZE - avail_size;
			memcpy((char*)buf.mapped_ptr + buf_offs, (char*)data + read_ptr, copy_size);
			
			VkBufferCopy copy_region = {};
			copy_region.srcOffset = buf_offs;
			copy_region.dstOffset = dst_offs + read_ptr;
			copy_region.size = copy_size;
			vkCmdCopyBuffer(cmds, buf.buf, dst_buf, 1, &copy_region);

			read_ptr += copy_size;
			avail_size -= copy_size;
		}
	}

	// must call after all staged_copy calls every frame
	void update_buffer_alloc (VulkanWindowContext& ctx, int cur_frame) {
		ZoneScopedC(tracy::Color::Crimson);
		
		auto& frame = frames[cur_frame];

		while ((int)frame.staging_bufs.size() > max(used_bufs, MIN_STAGING_BUFS)) {
			frame.staging_bufs.back().free(ctx.dev);
			frame.staging_bufs.pop_back();
		}

		avail_size = 0;
		used_bufs = 0;
	}
};

} // namespace vk
