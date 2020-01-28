#include "glshader.hpp"
#include "../file_io.hpp"
#include "assert.h"

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

GLuint _load_shader (GLenum type, char const* directory, const char* ext, std::vector<Shader::ShaderPart>* sources, GLuint prog, std::string const& name) {
	std::string filename = directory + name + ext;

	std::string source;
	if (!kiss::read_text_file(filename.c_str(), &source))
		return 0;

	GLuint shad = glCreateShader(type);

	glAttachShader(prog, shad);

	{
		const char* ptr = source.c_str();
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
			fprintf(stderr,"OpenGL error in shader compilation \"%s\"!\n>>>\n%s\n<<<\n", filename.c_str(), log_avail ? log_str.c_str() : "<no log available>");
		} else {
			// compilation success
			if (log_avail) {
				fprintf(stderr,"OpenGL shader compilation log \"%s\":\n>>>\n%s\n<<<\n", filename.c_str(), log_str.c_str());
			}
		}
	}

	sources->push_back({ std::move(filename), std::move(source) });
	return shad;
}

#if 0
void preprocess (const char* filename, std::string* src_text) {

	{
		std::string filepath = prints("%s%s", SHADERS_BASE_PATH, filename.c_str());

		srcf.v.emplace_back(); // add file to list dependent files even before we know if it exist, so that we can find out when it becomes existant
		srcf.v.back().init(filepath);

		if (!read_text_file(filepath.c_str(), src_text)) {
			fprintf(stderr,"load_shader_source:: $include \"%s\" could not be loaded!", filename.c_str());
			return false;
		}
	}

	for (auto c=src_text->begin(); c!=src_text->end();) {

		if (*c == '$') {
			auto line_begin = c;
			++c;

			auto syntax_error = [&] () {

				while (*c != '\n' && *c != '\r') ++c;
				std::string line (line_begin, c);

				fprintf(stderr,"load_shader_source:: expected '$include \"filename\"' syntax but got: '%s'!", line.c_str());
			};

			while (*c == ' ' || *c == '\t') ++c;

			auto cmd = c;

			while ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || *c == '_') ++c;

			if (std::string(cmd, c).compare("include") == 0) {

				while (*c == ' ' || *c == '\t') ++c;

				if (*c != '"') {
					syntax_error();
					return false;
				}
				++c;

				auto filename_str_begin = c;

				while (*c != '"') ++c;

				std::string inc_filename(filename_str_begin, c);

				if (*c != '"') {
					syntax_error();
					return false;
				}
				++c;

				while (*c == ' ' || *c == '\t') ++c;

				if (*c != '\r' && *c != '\n') {
					syntax_error();
					return false;
				}

				auto line_end = c;

				{
					inc_filename = get_path_dir(filename).append(inc_filename);

					std::string inc_text;
					if (!load_shader_source(inc_filename, &inc_text)) return false;

					auto line_begin_i = line_begin -src_text->begin();

					src_text->erase(line_begin, line_end);
					src_text->insert(src_text->begin() +line_begin_i, inc_text.begin(), inc_text.end());

					c = src_text->begin() +line_begin_i +inc_text.length();
				}

			}
		} else {
			++c;
		}

	}

	return true;
}
#endif

std::shared_ptr<Shader> ShaderManager::load_shader (std::string name) {
	Shader s;
	s.shad = gl::ShaderProgram::alloc();
	
	GLuint vert = _load_shader(GL_VERTEX_SHADER  , shaders_directory.c_str(), ".vert.glsl", &s.sources, s.shad, name);
	GLuint frag = _load_shader(GL_FRAGMENT_SHADER, shaders_directory.c_str(), ".frag.glsl", &s.sources, s.shad, name);
	GLuint geom = _load_shader(GL_GEOMETRY_SHADER, shaders_directory.c_str(), ".geom.glsl", &s.sources, s.shad, name);

	glLinkProgram(s.shad);

	bool success;
	{
		GLint status;
		glGetProgramiv(s.shad, GL_LINK_STATUS, &status);

		std::string log_str;
		bool log_avail = get_program_link_log(s.shad, &log_str);

		success = status == GL_TRUE;
		if (!success) {
			// linking failed
			fprintf(stderr,"OpenGL error in shader linkage \"%s\"!\n>>>\n%s\n<<<\n", name.c_str(), log_avail ? log_str.c_str() : "<no log available>");
		} else {
			// linking success
			if (log_avail) {
				fprintf(stderr,"OpenGL shader linkage log \"%s\":\n>>>\n%s\n<<<\n", name.c_str(), log_str.c_str());
			}
		}
	}

	if (vert) glDetachShader(s.shad, vert);
	if (frag) glDetachShader(s.shad, frag);
	if (geom) glDetachShader(s.shad, geom);

	if (vert) glDeleteShader(vert);
	if (frag) glDeleteShader(frag);
	if (geom) glDeleteShader(geom);

	return success ? std::make_shared<Shader>(std::move(s)) : nullptr;
}

bool ShaderManager::check_file_changes () {
	return false;
}

Uniform Shader::get_uniform (char const* name, gl::data_type type) {
	// hope that type is correct
	Uniform u;
	u.name = name;
	u.type = type;
	u.loc = glGetUniformLocation(shad, u.name);
	//if (u.loc <= -1) log_warning("Uniform not valid %s!", u.name);
	return u;
}

ShaderManager shader_manager;
