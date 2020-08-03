#include "read_wav.hpp"
#include "../util/file_io.hpp"

namespace audio {
	struct WAV_Reader {
		struct RiffHeader {
			char		id[4]; // "RIFF"
			uint32_t	file_size;
			char		file_type[4];
		};
		struct RiffChunk {
			char		id[4];
			uint32_t	chunk_size;
			// Data bytes
			// padding to pad to even bytes count
		};

		/*
			RiffBLOCK [loop]
			This block is for looped samples. Very few programs support this block,
			but if your program changes the wave file, it should preserve any unknown
			blocks.
			OFFSET              Count TYPE   Description
			0000h                   1 dword  Start of sample loop
			0004h                   1 dword  End of sample loop
		*/
		struct WaveFormat {
			// RiffChunk
			char		id[4]; // id == "fmt "
			uint32_t	chunk_size;
			//
			uint16_t format; // 1==PCM  ie. uncompressed samples
			uint16_t channels; // 1: mono  2: sterio  etc.
			uint32_t sample_rate; // usually 44100
			uint32_t bytes_per_sec;
			uint16_t reserved; // or Block alignment ???
			uint16_t bits_per_sample; // usually 16
		};
		struct WaveData {
			// RiffChunk
			char		id[4]; // id == "data"
			uint32_t	chunk_size;
		};

		uint64_t file_size;
		kiss::raw_data file_data;

		unsigned char* cur;

		template <typename T>
		T* get_riff_chunk (char const* id) {
			if (cur + sizeof(T) < file_data.get())
				return nullptr;

			auto chunk = (T*)cur;
			cur += sizeof(T);

			if (memcmp(chunk->id, id, 4) != 0)
				return nullptr;

			return chunk;
		}

		AudioData16 load_wav () {
			cur = file_data.get();

			auto* header = get_riff_chunk<RiffHeader>("RIFF");
			if (!header || memcmp(header->file_type, "WAVE", 4) != 0)
				return {};

			auto* fmt = get_riff_chunk<WaveFormat>("fmt ");
			auto* data = get_riff_chunk<WaveData>("data");

			if (!fmt || !data)
				return {};

			if (fmt->format != 1)
				return {};
			if (fmt->bits_per_sample != 16)
				return {};
			if (fmt->channels != 1 && fmt->channels != 2)
				return {};

			AudioData16 ret;

			ret.channels = fmt->channels;
			ret.sample_rate = (double)fmt->sample_rate;
			ret.count = data->chunk_size / (ret.channels * sizeof(int16_t));
			ret.samples = std::make_unique<int16_t[]>( ret.count * ret.channels );
			memcpy(ret.samples.get(), cur, ret.count * ret.channels * sizeof(int16_t));
			return ret;
		}
	};

	bool load_wav (const char* filepath, AudioData16* data) {
		WAV_Reader reader;
		reader.file_data = kiss::load_binary_file(filepath, &reader.file_size);
		if (!reader.file_data)
			return false;

		*data = reader.load_wav();
		return true;
	}
}
