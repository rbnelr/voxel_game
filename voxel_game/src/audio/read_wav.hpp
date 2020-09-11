#pragma once
#include "stdafx.hpp"
#include "audio.hpp"

namespace audio {
	bool load_wav (const char* filepath, AudioData16* data);
}
