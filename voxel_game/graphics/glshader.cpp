#include "glshader.hpp"
#include "common.hpp"
#include "../util/file_io.hpp"
#include "../util/string.hpp"
#include "assert.h"
#include <regex>
using namespace kiss;
using std::string;
using std::string_view;
using std::vector;

namespace gl {
	bool get_shader_compile_log (GLuint shad, std::string* log) {
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
	bool get_program_link_log (GLuint prog, std::string* log) {
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

	void get_all_uniforms (Shader* shad, const char* name) {
		for (auto& kv : shad->uniforms) {
			kv.second.loc = glGetUniformLocation(shad->shad, kv.first.str.c_str());
			if (kv.second.loc < 0) {
				clog(ERROR, "Uniform \"%s\" in shader \"%s\" not active!\n", kv.first.str.c_str(), name);
			}
		}
	}

	void bind_uniform_blocks (Shader* shad) {
		for (auto& u : COMMON_UNIFORMS) {
			GLuint index = glGetUniformBlockIndex(shad->shad, u.name);   
			if (index != GL_INVALID_INDEX) {
				glUniformBlockBinding(shad->shad, index, u.binding_point);
			}
		}
	}

	bool gl::Shader::get_uniform (std::string_view name, Uniform* u) {
		auto ret = uniforms.find(name);
		if (ret == uniforms.end())
			return false;
		*u = ret->second;
		return true;
	}

	void gl::Shader::bind_uniform_block (SharedUniformsInfo const& u) {
		GLuint index = glGetUniformBlockIndex(shad, u.name);   
		if (index != GL_INVALID_INDEX) {
			glUniformBlockBinding(shad, index, u.binding_point);
		}
	}

	kiss::raw_data gl::Shader::get_program_binary (uint64_t* size) {
		int _size;
		glGetProgramiv(shad, GL_PROGRAM_BINARY_LENGTH, &_size);

		auto data = std::make_unique<unsigned char[]>(_size);

		GLenum format;
		glGetProgramBinary(shad, _size, &_size, &format, data.get());

		*size = _size;
		return std::move(data);
	}

	bool is_ident_start_c (char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
	}
	bool is_ident_c (char c) {
		return is_ident_start_c(c) || (c >= '0' && c <= '9');
	}
	void whitespace (const char** c) {
		while (**c == ' ' || **c == '\t')
			(*c)++;
	}
	bool newline (const char** c, int* line_no) {
		if (**c == '\r' || **c == '\n') {
			char ch = **c;
			(*c)++;

			if ((**c == '\r' || **c == '\n') && ch != **c) { // "\n" "\r" "\n\r" "\r\n" are each one newline but "\n\n" "\r\r" count as two, this should handle even inconsistent newline files reasonably (empty $if is ok, but would probably not compile)
				(*c)++;
			}

			(*line_no)++;
			return true;
		}
		return false;
	}
	bool line_comment (const char** c, int* line_no) {
		if ((*c)[0] == '/' && (*c)[1] == '/') {
			(*c) += 2;
			while (!newline(c, line_no))
				(*c)++;
			return true;
		}
		return false;
	}
	bool identifier (const char** c, string_view* ident) {
		if (is_ident_start_c(**c)) {
			auto begin = *c;

			while (is_ident_c(**c)) (*c)++; // find end of identifier

			*ident = string_view(begin, *c - begin);
			return true;
		}
		return false;
	}
	bool integer (const char** c, int* i) {
		const char* cur = *c;

		bool neg = false;
		if (*cur == '-') {
			neg = true;
			cur++;
		} else if (*cur == '+') {
			cur++;
		}

		if (*cur < '0' || *cur > '9')
			return false;

		int out = 0;
		while (*cur >= '0' && *cur <= '9') {
			out *= 10;
			out += *cur++ - '0';
		}

		*i = neg ? -out : out;
		*c = cur;
		return true;
	}
	bool quoted_string (const char** c, string_view* str) {
		if (**c == '"') {
			(*c)++; // skip '"'
			auto begin = *c;

			while (**c != '"') (*c)++;

			*str = string_view(begin, *c - begin);

			(*c)++; // skip '"'

			return true;
		}
		return false;
	}

	// recursive $if not supported, but $if inside a $include'd file that is included within an if is supported (since $if state is per recursive call and each include gets a call)
	struct ShaderPreprocessor {
		const char*		shader_name;
		Shader*			shader;
		string_view		type;

		// shader had a $compile <type> command with matching type
		bool			compile_shader = false;

		void syntax_error (char const* reason, string_view filename, int line_no) {
			clog(ERROR, "ShaderPreprocessor: syntax error around \"%s\":%d!\n>> %s\n", filename.data(), line_no, reason);
		}

		struct TypeMap {
			const char*		glsl_type;
			gl::type		type;
		};
		static constexpr TypeMap _type_map[] = {
			{ "float", gl::FLOAT	},
			{ "vec2",  gl::FLOAT2	},
			{ "vec3",  gl::FLOAT3	},
			{ "vec4",  gl::FLOAT4	},
			{ "int",   gl::INT		},
			{ "ivec2", gl::INT2		},
			{ "ivec3", gl::INT3		},
			{ "ivec4", gl::INT4		},
			{ "mat3",  gl::MAT3		},
			{ "mat4",  gl::MAT4		},
			{ "bool",  gl::BOOL		},

			{ "sampler1D",			gl::SAMPLER_1D			},
			{ "sampler2D",			gl::SAMPLER_2D			},
			{ "sampler3D",			gl::SAMPLER_3D			},
			{ "isampler1D",			gl::ISAMPLER_1D			},
			{ "isampler2D",			gl::ISAMPLER_2D			},
			{ "isampler3D",			gl::ISAMPLER_3D			},
			{ "usampler1D",			gl::USAMPLER_1D			},
			{ "usampler2D",			gl::USAMPLER_2D			},
			{ "usampler3D",			gl::USAMPLER_3D			},
			{ "samplerCube",		gl::SAMPLER_Cube		},
			{ "sampler1DArray",		gl::SAMPLER_1D_Array	},
			{ "sampler2DArray",		gl::SAMPLER_2D_Array	},
			{ "samplerCubeArray",	gl::SAMPLER_Cube_Array	},
		};
		bool get_type (string_view glsl_type, gl::type* type) {
			for (auto& tm : _type_map) {
				if (glsl_type.compare(tm.glsl_type) == 0) {
					*type = tm.type;
					return true;
				}
			}
			return false;
		}

		void uniform_found (string_view glsl_type, string_view name) {
			gl::type type;
			if (!get_type(glsl_type, &type))
				return;

			if (shader->uniforms.find(name) == shader->uniforms.end())
				shader->uniforms.emplace(string(name), type);
		}

		void ident_found (string_view ident, const char* c) {
			if (ident.compare("uniform") == 0) {
				string_view type, name;

				whitespace(&c);

				if (!identifier(&c, &type))
					return;

				whitespace(&c);

				if (!identifier(&c, &name))
					return;

				uniform_found(type, name);
			}
		}

		bool preprocess_shader (const char* source, string_view filename, string* result) {
			bool in_if = false;
			bool skip_code = false;

			int line_no = 1;
			prints(result, "//$lineno 1 \"%s\"\n", filename.data());

			const char* ident_begin = nullptr;
			const char* ident = nullptr;

			const char* c = source;
			while (*c != '\0') {

				if (is_ident_start_c(*c)) {
					if (!ident_begin)
						ident_begin = c;
				} else {
					if (ident_begin) {
						if (!skip_code) {
							ident_found(string_view(ident_begin, c - ident_begin), c);
						}
						ident_begin = nullptr;
					}
				}

				if (*c != '$') { // copy normal char
					if (skip_code) {
						if (!newline(&c, &line_no)) {
							c++;
						}
					} else {
						if (newline(&c, &line_no)) {
							result->push_back('\n'); // convert newlines to '\n'
						} else {
							result->push_back(*c++);
						}
					}
					continue;
				}

				auto dollar = c++;

				string_view cmd, arg;

				whitespace(&c);

				identifier(&c, &cmd);

				whitespace(&c);

				if (*c == '"') {
					c++;

					auto arg_begin = c;

					while (*c != '"' && *c != '\0' && !newline(&c, &line_no)) {
						++c; // skip until end of quotes
					}

					if (*c != '"') {
						syntax_error("EOF or newline in quotes", filename,line_no);
						return false;
					}

					arg = string_view(arg_begin, c - arg_begin);

					c++; // skip '"'

				} else if (is_ident_start_c(*c)) {

					identifier(&c, &arg);

				} else {
					// no argument
				}

				whitespace(&c);

				if (!(newline(&c, &line_no) || line_comment(&c, &line_no) || *c == '\0')) {
					syntax_error("$cmd did not end with newline or EOF", filename,line_no);
					return false;
				}

				if (cmd.compare("if") == 0) {

					if (in_if) {
						syntax_error("recursive $if not supported (but you can use it in a $include'd file even if the include happens inside an $if)", filename,line_no);
						return false;
					}

					in_if = true;
					skip_code = arg.compare(type) != 0;

				} else if (cmd.compare("endif") == 0) {

					if (!in_if) {
						syntax_error("$endif without $if", filename,line_no);
						return false;
					}
					in_if = false;
					skip_code = false;

				} else if (cmd.compare("compile") == 0) {

					if (arg.compare(type) == 0)
						compile_shader = true;

				} else if (cmd.compare("include") == 0) {

					std::string incl_result;

					auto path = get_path(filename);

					std::string inc_filename = string(path).append(arg);
					std::string inc_source;
					if (!kiss::load_text_file(inc_filename.c_str(), &inc_source)) {
						clog(ERROR, "Could not find include file \"%s\" for shader \"%s\"!\n", inc_filename.c_str(), shader_name);
						return false;
					}

					if (!preprocess_shader(inc_source.c_str(), inc_filename, &incl_result)) {
						return false;
					}

					prints(result, "%s\n", incl_result.c_str());

					// Insert source file into sources set
					if (std::find(shader->sources.begin(), shader->sources.end(), inc_filename) == shader->sources.end())
						shader->sources.push_back(std::move(inc_filename));

				} else {
					syntax_error("unknown $cmd", filename,line_no);
					return false;
				}

				if (!skip_code)
					prints(result, "//$lineno %d \"%s\"\n", line_no, filename.data());
			}

			if (in_if) {
				syntax_error("$if without $endif", filename,line_no);
				return false;
			}

			return true;
		}
	};

	// map line_no in the preprocessed text to the source file + line_no
	//  based on the //$lineno lines the ShaderPreprocessor emits in the preprocessed output
	int map_line_no (string_view preprocessed, int pp_line_no, string_view* out_filename) {
		char const* cur = preprocessed.data();

		int pp_line = 1;

		string_view filename;
		int src_line;

		while (*cur) {
			if (cur[0] == '/' && cur[1] == '/' && cur[2] == '$') {
				cur += 3; // skip //$

				string_view cmd;
				whitespace(&cur);
				identifier(&cur, &cmd); // $lineno
				whitespace(&cur);
				integer(&cur, &src_line);
				whitespace(&cur);
				quoted_string(&cur, &filename);

				pp_line++;

			} else if (*cur == '\n') {
				pp_line++;
				src_line++;
			}

			cur++;

			if (pp_line == pp_line_no) {
				break;
			}
		}

		if (pp_line != pp_line_no) {
			return -1; // invalid pp_line_no passed in or bug
		}

		*out_filename = filename;
		return src_line;
	}

	// map line numbers in shader log to real file line numbers
	std::string map_shader_log (std::string const& shader_log, string_view preprocessed) {
		// line number syntax on my NV GTX 1080 seems to be like "0(<line_no>): warning"

		std::regex re("0\\(([0-9]+)\\)\\s:");
		auto begin = std::sregex_iterator(shader_log.begin(), shader_log.end(), re);
		auto end = std::sregex_iterator();

		std::string str;

		size_t latest_match_end = 0;

		for (auto it=begin; it!=end; ++it) {
			auto outter = (*it)[0];
			auto line_no_match = (*it)[1];

			size_t a = outter.first - shader_log.begin();
			size_t b = outter.second - shader_log.begin();

			if (latest_match_end < a) {
				str += shader_log.substr(latest_match_end, a - latest_match_end);
			}

			std::string line_no_pp_str = std::string(line_no_match);

			int line_no_pp;
			char const* cur = line_no_pp_str.c_str();
			if (!integer(&cur, &line_no_pp))
				return "<error>";

			string_view filename;
			int line_no = map_line_no(preprocessed, line_no_pp, &filename);

			str += prints("%s:%d :", std::string(filename).c_str(), line_no);

			latest_match_end = b;
		}

		if (latest_match_end < shader_log.size()) {
			str += shader_log.substr(latest_match_end, shader_log.size() - latest_match_end);
		}

		return str;
	}

	GLuint load_shader_part (const char* type, GLenum gl_type, std::string const& source, std::string_view filename, Shader* shader, std::string const& name, bool* error, std::vector<std::string>* preprocessed_sources) {

		std::string preprocessed = "";
		{
			ShaderPreprocessor pp;
			pp.type = type;
			pp.shader_name = name.c_str();
			pp.shader = shader;
			if (!pp.preprocess_shader(source.c_str(), filename, &preprocessed)) {
				*error = true;
				return 0;
			}
			if (!(pp.compile_shader || strcmp(type, "vertex") == 0 || strcmp(type, "fragment") == 0)) // only compiler shader part if it had a $compile <type> command but always compile vertex and fragment shaders
				return 0; // don't create shader of this type
		}

		GLuint shad = glCreateShader(gl_type);

		glAttachShader(shader->shad, shad);

		{
			const char* ptr = preprocessed.c_str();
			glShaderSource(shad, 1, &ptr, NULL);
		}

		glCompileShader(shad);

		bool success;
		{
			GLint status;
			glGetShaderiv(shad, GL_COMPILE_STATUS, &status);

			std::string log_str;
			bool log_avail = get_shader_compile_log(shad, &log_str);
			if (log_avail) log_str = map_shader_log(log_str, preprocessed);

			success = status == GL_TRUE;
			if (!success) {
				// compilation failed
				clog(ERROR,"OpenGL error in shader compilation \"%s\"!\n>>>\n%s\n<<<\n", name.c_str(), log_avail ? log_str.c_str() : "<no log available>");
			} else {
				// compilation success
				if (log_avail) {
					clog(ERROR,"OpenGL shader compilation log \"%s\":\n>>>\n%s\n<<<\n", name.c_str(), log_str.c_str());
				}
			}
		}

		preprocessed_sources->push_back(std::move(preprocessed));

		return shad;
	}

	std::unique_ptr<Shader> load_shader (string const& name, const char* shaders_directory) {
		auto s = std::make_unique<Shader>();

		// Load shader base source file
		string filename = prints("%s%s.glsl", shaders_directory, name.c_str());
		string source;
		if (!kiss::load_text_file(filename.c_str(), &source)) {
			clog(ERROR, "Could not load base source file for shader \"%s\"!\n", name.c_str());
			return s;
		}

		s->shad = glCreateProgram();

		// Load, proprocess and compile shader parts
		bool error = false;
		GLuint vert = load_shader_part("vertex",   GL_VERTEX_SHADER  , source, filename, s.get(), name, &error, &s->preprocessed_sources);
		GLuint frag = load_shader_part("fragment", GL_FRAGMENT_SHADER, source, filename, s.get(), name, &error, &s->preprocessed_sources);
		GLuint geom = load_shader_part("geometry", GL_GEOMETRY_SHADER, source, filename, s.get(), name, &error, &s->preprocessed_sources);

		// Insert source file into sources set
		if (std::find(s->sources.begin(), s->sources.end(), filename) == s->sources.end())
			s->sources.push_back(std::move(filename));

		glLinkProgram(s->shad);

		{
			GLint status;
			glGetProgramiv(s->shad, GL_LINK_STATUS, &status);

			std::string log_str;
			bool log_avail = get_program_link_log(s->shad, &log_str);

			error = status == GL_FALSE;
			if (error) {
				// linking failed
				clog(ERROR,"OpenGL error in shader linkage \"%s\"!\n>>>\n%s\n<<<\n", name.c_str(), log_avail ? log_str.c_str() : "<no log available>");
			} else {
				// linking success
				if (log_avail) {
					clog(ERROR,"OpenGL shader linkage log \"%s\":\n>>>\n%s\n<<<\n", name.c_str(), log_str.c_str());
				}
			}
		}

		if (vert) glDetachShader(s->shad, vert);
		if (frag) glDetachShader(s->shad, frag);
		if (geom) glDetachShader(s->shad, geom);

		if (vert) glDeleteShader(vert);
		if (frag) glDeleteShader(frag);
		if (geom) glDeleteShader(geom);

		if (error) {
			clog(ERROR, "Could not load some required source files for shader \"%s\", shader load aborted!\n", name.c_str());

			glDeleteProgram(s->shad);
			s->shad = 0;
			return s;
		}

		get_all_uniforms(s.get(), name.c_str());
		bind_uniform_blocks(s.get());
		return s;
	}
}
