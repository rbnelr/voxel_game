#include "read_wav.hpp"
#include "../util/file_io.hpp"

namespace audio {
	struct RiffHeader {
		char		id[4];
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
		RiffBLOCK [fmt ]
		This block contains the data necessary for playback of the sound
		files. Note the blank after fmt !
		OFFSET              Count TYPE   Description
		0000h                   1 word   Format tag
										   1 = PCM (raw sample data)
										   2 etc. for APCDM, a-Law, u-Law ...
		0002h                   1 word   Channels (1=mono,2=stereo,...)
		0004h                   1 dword  Sampling rate
		0008h                   1 dword  Average bytes per second (=sampling rate*channels)
		000Ch                   1 word   Block alignment / reserved ??
		000Eh                   1 word   Bits per sample (8/12/16-bit samples)

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
		char		id[4]; // id == "fmt "
		uint32_t	chunk_size;
	};

	struct FileReader {
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
	};

	AudioData16 load_wav (const char* filepath) {
		FileReader reader;
		reader.file_data = kiss::load_binary_file(filepath, &reader.file_size);
		if (!reader.file_data)
			return {};

		reader.cur = reader.file_data.get();

		auto* header = reader.get_riff_chunk<RiffHeader>("RIFF");
		if (!header || memcmp(header->file_type, "WAVE", 4) != 0)
			return {};

		auto* fmt = reader.get_riff_chunk<WaveFormat>("fmt ");
		auto* data = reader.get_riff_chunk<WaveData>("data");

		if (!fmt || !data)
			return {};



		return {};
	}
}
