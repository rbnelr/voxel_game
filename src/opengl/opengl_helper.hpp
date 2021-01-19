#pragma once
#include "common.hpp"
#include "glad/glad.h"

namespace gl {
	
enum class GlslType {
	FLOAT,	FLOAT2,	FLOAT3,	FLOAT4,
	INT,	INT2,	INT3,	INT4,
	BOOL,

	MAT2, MAT3, MAT4,

	SAMPLER_1D,		SAMPLER_2D,		SAMPLER_3D,
	ISAMPLER_1D,	ISAMPLER_2D,	ISAMPLER_3D,
	USAMPLER_1D,	USAMPLER_2D,	USAMPLER_3D,
	SAMPLER_CUBE,

	SAMPLER_1D_ARRAY,	SAMPLER_2D_ARRAY,	SAMPLER_CUBE_ARRAY,
};

constexpr inline bool is_sampler_type (GlslType t) {
	return t >= GlslType::SAMPLER_1D && t <= GlslType::SAMPLER_CUBE_ARRAY;
}

const std::unordered_map<std::string_view, GlslType> glsl_type_map = {
	{ "float",				GlslType::FLOAT					},
	{ "vec2",				GlslType::FLOAT2				},
	{ "vec3",				GlslType::FLOAT3				},
	{ "vec4",				GlslType::FLOAT4				},
	{ "int",				GlslType::INT					},
	{ "ivec2",				GlslType::INT2					},
	{ "ivec3",				GlslType::INT3					},
	{ "ivec4",				GlslType::INT4					},
	{ "bool",				GlslType::BOOL					},
	{ "mat2",				GlslType::MAT2					},
	{ "mat3",				GlslType::MAT3					},
	{ "mat4",				GlslType::MAT4					},
	{ "sampler1D",			GlslType::SAMPLER_1D			},
	{ "sampler2D",			GlslType::SAMPLER_2D			},
	{ "sampler3D",			GlslType::SAMPLER_3D			},
	{ "isampler1D",			GlslType::ISAMPLER_1D			},
	{ "isampler2D",			GlslType::ISAMPLER_2D			},
	{ "isampler3D",			GlslType::ISAMPLER_3D			},
	{ "usampler1D",			GlslType::USAMPLER_1D			},
	{ "usampler2D",			GlslType::USAMPLER_2D			},
	{ "usampler3D",			GlslType::USAMPLER_3D			},
	{ "samplerCube",		GlslType::SAMPLER_CUBE			},
	{ "sampler1DArray",		GlslType::SAMPLER_1D_ARRAY		},
	{ "sampler2DArray",		GlslType::SAMPLER_2D_ARRAY		},
	{ "samplerCubeArray",	GlslType::SAMPLER_CUBE_ARRAY	},
};

inline bool get_shader_compile_log (GLuint shad, std::string* log) {
	GLsizei log_len;
	{
		GLint temp = 0;
		glGetShaderiv(shad, GL_INFO_LOG_LENGTH, &temp);
		log_len = (GLsizei)temp;
	}

	if (log_len <= 1) {
		return false; // no log available
	} else {
		// GL_INFO_LOG_LENGTH includes the null terminator, but it is not allowed to write the null terminator in std::string, so we have to allocate one additional char and then resize it away at the end

		log->resize(log_len);

		GLsizei written_len = 0;
		glGetShaderInfoLog(shad, log_len, &written_len, &(*log)[0]);
		assert(written_len == (log_len -1));

		log->resize(written_len);

		return true;
	}
}
inline bool get_program_link_log (GLuint prog, std::string* log) {
	GLsizei log_len;
	{
		GLint temp = 0;
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &temp);
		log_len = (GLsizei)temp;
	}

	if (log_len <= 1) {
		return false; // no log available
	} else {
		// GL_INFO_LOG_LENGTH includes the null terminator, but it is not allowed to write the null terminator in std::string, so we have to allocate one additional char and then resize it away at the end

		log->resize(log_len);

		GLsizei written_len = 0;
		glGetProgramInfoLog(prog, log_len, &written_len, &(*log)[0]);
		assert(written_len == (log_len -1));

		log->resize(written_len);

		return true;
	}
}

struct OpenglState {

	GLuint shader_program = 0;
	GLuint use_shader (GLuint program) {
		GLuint prev = shader_program;
		if (prev != program) {
			shader_program = program;
			glUseProgram(program);
		}
		return prev;
	}

};

inline OpenglState glstate;

} // namespace gl
