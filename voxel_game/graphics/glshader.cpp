#include "glshader.hpp"
#include "../file_io.hpp"
#include "../string.hpp"
#include "assert.h"
using namespace kiss;
using std::string;
using std::string_view;
using std::vector;

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

void get_all_uniforms (ShaderManager::_Shader* shad, const char* name) {
	for (auto& kv : shad->uniforms) {
		kv.second.loc = glGetUniformLocation(shad->shad, kv.first.str.c_str());
		if (kv.second.loc < 0) {
			fprintf(stderr, "Uniform \"%s\" in shader \"%s\" not active!\n", kv.first.str.c_str(), name);
		}
	}
}

// recursive $if not supported, but $if inside a $include'd file that is included within an if is supported (since $if state is per recursive call and each include gets a call)
struct ShaderPreprocessor {
	const char*					shader_name;
	ShaderManager::_Shader*		shader;
	string_view					type;
	
	// shader had a $compile <type> command with matching type
	bool			compile_shader = false;

	int				line_i;

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
	bool newline (const char** c) {
		if (**c == '\r' || **c == '\n') {
			char ch = **c;
			(*c)++;

			if ((**c == '\r' || **c == '\n') && ch != **c) { // "\n" "\r" "\n\r" "\r\n" are each one newline but "\n\n" "\r\r" count as two, this should handle even inconsistent newline files reasonably (empty $if is ok, but would probably not compile)
				(*c)++;
			}

			line_i++;
			return true;
		}
		return false;
	}
	bool line_comment (const char** c) {
		if ((*c)[0] == '/' && (*c)[1] == '/') {
			(*c) += 2;
			while (!newline(c))
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

	void syntax_error (char const* reason) {
		fprintf(stderr, "ShaderPreprocessor: syntax error around \"%s\":%d!\n>> %s\n", shader_name, line_i, reason);
	}

	struct TypeMap {
		const char*		glsl_type;
		gl::data_type	type;
	};
	static constexpr TypeMap _type_map[] = {
		{ "float", gl::T_FLT	},
		{ "vec2",  gl::T_V2		},
		{ "vec3",  gl::T_V3		},
		{ "vec4",  gl::T_V4		},
		{ "int",   gl::T_INT	},
		{ "ivec2", gl::T_IV2	},
		{ "ivec3", gl::T_IV3	},
		{ "ivec4", gl::T_IV4	},
		{ "mat3",  gl::T_M3		},
		{ "mat4",  gl::T_M4		},
		{ "bool",  gl::T_BOOL	},
	};
	bool get_type (string_view glsl_type, gl::data_type* type) {
		for (auto& tm : _type_map) {
			if (glsl_type.compare(tm.glsl_type) == 0) {
				*type = tm.type;
				return true;
			}
		}
		return false;
	}

	void uniform_found (string_view glsl_type, string_view name) {
		gl::data_type type;
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

	bool preprocess_shader (const char* source, string_view path, string* result) {
		bool in_if = false;
		bool skip_code = false;

		line_i = 1;

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
					if (!newline(&c)) {
						c++;
					}
				} else {
					if (newline(&c)) {
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

				while (*c != '"' && *c != '\0' && !newline(&c)) {
					++c; // skip until end of quotes
				}

				if (*c != '"') {
					syntax_error("EOF or newline in quotes");
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

			if (!(newline(&c) || line_comment(&c) || *c == '\0')) {
				syntax_error("$cmd did not end with newline or EOF");
				return false;
			}

			if (cmd.compare("if") == 0) {

				if (in_if) {
					syntax_error("recursive $if not supported (but you can use it in a $include'd file even if the include happens inside an $if)");
					return false;
				}

				in_if = true;
				skip_code = arg.compare(type) != 0;

			} else if (cmd.compare("endif") == 0) {

				if (!in_if) {
					syntax_error("$endif without $if");
					return false;
				}
				in_if = false;
				skip_code = false;

			} else if (cmd.compare("compile") == 0) {

				if (arg.compare(type) == 0)
					compile_shader = true;

			} else if (cmd.compare("include") == 0) {

				std::string incl_result;
				{
					std::string filepath = string(path).append(arg);
					std::string source;
					if (!kiss::read_text_file(filepath.c_str(), &source)) {
						fprintf(stderr, "Could not find include file \"%s\" for shader \"%s\"!\n", filepath.c_str(), shader_name);
						return false;
					}

					auto incl_path = get_path(filepath);

					if (!preprocess_shader(source.c_str(), incl_path, &incl_result)) {
						return false;
					}

					// Insert source file into sources set
					if (std::find(shader->sources.begin(), shader->sources.end(), filepath) == shader->sources.end())
						shader->sources.push_back(std::move(filepath));
				}

				result->append(incl_result);
				result->append("\n");

			} else {
				syntax_error("unknown $cmd");
				return false;
			}
		}

		if (in_if) {
			syntax_error("$if without $endif");
			return false;
		}

		return true;
	}
};

GLuint ShaderManager::load_shader_part (const char* type, GLenum gl_type, std::string const& source, std::string_view path, _Shader* shader, std::string const& name, bool* error) {
	
	std::string preprocessed;
	{
		ShaderPreprocessor pp;
		pp.type = type;
		pp.shader_name = name.c_str();
		pp.shader = shader;
		if (!pp.preprocess_shader(source.c_str(), path, &preprocessed)) {
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

		success = status == GL_TRUE;
		if (!success) {
			// compilation failed
			fprintf(stderr,"OpenGL error in shader compilation \"%s\"!\n>>>\n%s\n<<<\n", name.c_str(), log_avail ? log_str.c_str() : "<no log available>");
		} else {
			// compilation success
			if (log_avail) {
				fprintf(stderr,"OpenGL shader compilation log \"%s\":\n>>>\n%s\n<<<\n", name.c_str(), log_str.c_str());
			}
		}
	}

	return shad;
}

std::unique_ptr<ShaderManager::_Shader> ShaderManager::_load_shader (std::string const& name) {
	auto s = std::make_unique<_Shader>();
	
	// Load shader base source file
	std::string filepath = prints("%s%s.glsl", shaders_directory.c_str(), name.c_str());
	std::string source;
	if (!kiss::read_text_file(filepath.c_str(), &source)) {
		fprintf(stderr, "Could not load base source file for shader \"%s\"!\n", name.c_str());
		return s;
	}

	auto path = get_path(filepath);

	s->shad = glCreateProgram();

	// Load, proprocess and compile shader parts
	bool error = false;
	GLuint vert = load_shader_part("vertex",   GL_VERTEX_SHADER  , source, path, s.get(), name, &error);
	GLuint frag = load_shader_part("fragment", GL_FRAGMENT_SHADER, source, path, s.get(), name, &error);
	GLuint geom = load_shader_part("geometry", GL_GEOMETRY_SHADER, source, path, s.get(), name, &error);

	// Insert source file into sources set
	if (std::find(s->sources.begin(), s->sources.end(), filepath) == s->sources.end())
		s->sources.push_back(std::move(filepath));

	glLinkProgram(s->shad);

	{
		GLint status;
		glGetProgramiv(s->shad, GL_LINK_STATUS, &status);

		std::string log_str;
		bool log_avail = get_program_link_log(s->shad, &log_str);

		error = status == GL_FALSE;
		if (error) {
			// linking failed
			fprintf(stderr,"OpenGL error in shader linkage \"%s\"!\n>>>\n%s\n<<<\n", name.c_str(), log_avail ? log_str.c_str() : "<no log available>");
		} else {
			// linking success
			if (log_avail) {
				fprintf(stderr,"OpenGL shader linkage log \"%s\":\n>>>\n%s\n<<<\n", name.c_str(), log_str.c_str());
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
		fprintf(stderr, "Could not load some required source files for shader \"%s\", shader load aborted!\n", name.c_str());

		glDeleteProgram(s->shad);
		s->shad = 0;
		return s;
	}

	get_all_uniforms(s.get(), name.c_str());
	return s;
}

ShaderManager::_Shader* ShaderManager::load_shader (std::string name) {
	auto ptr = ShaderManager::_load_shader(name);
	_Shader* s = ptr.get();
	shaders.emplace(std::move(name), std::move(ptr));

	return s;
}

bool ShaderManager::check_file_changes () {
	return false;
}

bool ShaderManager::_Shader::get_uniform (std::string_view name, Uniform* u) {
	auto ret = uniforms.find(name);
	if (ret == uniforms.end())
		return false;
	*u = ret->second;
	return true;
}

std::unique_ptr<ShaderManager> shader_manager = nullptr;
