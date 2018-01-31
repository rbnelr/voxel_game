
#define BEGIN(name)			auto _##__COUNTER__##name##_begin = glfwGetTime()
#define END(name) do {\
			f64 dt = glfwGetTime() -_##__COUNTER__##name##_begin; \
			_##name##_dt += dt; \
			++_atof_count; \
			_atof_min = min(_atof_min, dt); \
			_atof_max = max(_atof_max, dt); \
		} while (0)

#define PROFILE_ATOF 0
#define OVERRIDE_ATOF 1 // atof is EXTREMELY SLOW on some machines/compilers (2 ms!! in some cases)
#define CHECK_MY_ATOF 0 // my_str_to_f32 is 100% identical with atof for my dataset (and about 70000 times as fast as gcc's atof)

#if PROFILE_ATOF
f64 _atof_dt;
u32 _atof_count;
f64 _atof_min;
f64 _atof_max;
u64 _rdtsc;
#endif

#if CHECK_MY_ATOF
u32 atof_equal;
u32 atof_not_equal;
#endif

#if OVERRIDE_ATOF
static f32 my_str_to_f32 (cstr str) {
	#if 1
	f64 sign = *str == '-' ? -1.0 : +1.0;
	if (*str == '-' || *str == '+') ++str;
	
	u64 i = 0;
	for (;;) {
		dbg_assert(*str >= '0' && *str <= '9');
		u64 digit = *str++ -'0';
		
		i *= 10;
		i += digit;
		
		if (*str < '0' || *str > '9') break;
	}
	
	f64 f = 0;
	
	if (*str == '.') {
		++str;
		
		f64 g = 0.1;
		for (;;) {
			dbg_assert(*str >= '0' && *str <= '9');
			u64 digit = *str++ -'0';
			
			f += g * (f64)digit;
			
			if (*str < '0' || *str > '9') break;
			
			g *= 0.1;
			
		}
	}
	return (f32)(sign * ((f64)i +f));
	#else // Probably the better implementation
	f64 sign = *str == '-' ? -1.0 : +1.0;
	if (*str == '-' || *str == '+') ++str;
	
	u64 i = 0;
	for (;;) {
		dbg_assert(*str >= '0' && *str <= '9');
		u64 digit = *str++ -'0';
		
		i *= 10;
		i += digit;
		
		if (*str < '0' || *str > '9') break;
	}
	
	f64 frac_multi = 1.0;
	if (*str == '.') {
		++str;
		
		for (;;) {
			dbg_assert(*str >= '0' && *str <= '9');
			u64 digit = *str++ -'0';
			
			i *= 10;
			i += digit;
			
			frac_multi *= 0.1;
			
			if (*str < '0' || *str > '9') break;
		}
	}
	
	return (f32)(sign * (f64)i * frac_multi);
	#endif
}
#endif

namespace parse {
	struct String {
		char*	ptr;
		u32		len;
		
		operator bool () { return ptr; }
	};
	
	static bool comp (String const& l, cstr r) {
		char const* lcur = l.ptr;
		char const* lend = lcur +l.len;
		char const* rcur = r;
		while (*rcur != '\0') {
			if (*rcur++ != *lcur++) return false;
		}
		return lcur == lend;
	}
	
	static bool whitespace_c (char c) {	return c == ' ' || c == '\t'; }
	static String whitespace (char** pcur) {
		char* ret = *pcur;
		if (!whitespace_c(*ret)) return {};
		
		while (whitespace_c(**pcur)) ++(*pcur);
		
		return { ret, (u32)(*pcur -ret) };
	}
	
	static bool newline_c (char c) {	return c == '\n' || c == '\r'; }
	static String newline (char** pcur) {
		char* ret = *pcur;
		if (!newline_c(*ret)) return {};
		
		char c = **pcur;
		++(*pcur);
		
		if (newline_c(**pcur) && **pcur != c) ++(*pcur);
		
		return { ret, (u32)(*pcur -ret) };
	}
	
	static String rest_of_line (char** pcur) {
		char* ret = *pcur;
		if (newline_c(*ret) || *ret == '\0') return {};
		
		while (!newline_c(**pcur) && **pcur != '\0') ++(*pcur);
		
		newline(pcur);
		
		return { ret, (u32)(*pcur -ret) };
	}
	
	static bool identifier_c (char c) {	return (c >= 'A' && c <= 'Z')||(c >= 'a' && c <= 'z')|| c == '_'; }
	static String identifier (char** pcur) {
		char* ret = *pcur;
		if (!identifier_c(*ret)) return {};
		
		while (identifier_c(**pcur)) ++(*pcur);
		
		return { ret, (u32)(*pcur -ret) };
	}
	
	static bool token_c (char c) {	return !whitespace_c(c) && !newline_c(c) && c != '\0'; }
	static String token (char** pcur) {
		char* ret = *pcur;
		if (!token_c(*ret)) return {};
		
		while (token_c(**pcur)) ++(*pcur);
		
		return { ret, (u32)(*pcur -ret) };
	}
	
	static bool sign_c (char c) {	return c == '-' || c == '+'; }
	static bool digit_c (char c) {	return c >= '0' && c <= '9'; }
	
	static String int_ (char** pcur, u32* val) {
		char* ret = *pcur;
		if (!sign_c(*ret) && !digit_c(*ret)) return {};
		
		if (sign_c(**pcur)) ++(*pcur);
		
		while (digit_c(**pcur)) ++(*pcur);
		
		if (val) *val = (u32)atoi(ret);
		return { ret, (u32)(*pcur -ret) };
	}
	static String float_ (char** pcur, f32* val) {
		char* ret = *pcur;
		if (!sign_c(*ret) && !digit_c(*ret)) return {};
		
		if (sign_c(**pcur)) ++(*pcur);
		
		while (digit_c(**pcur)) ++(*pcur);
		
		if (**pcur == '.') ++(*pcur);
		
		while (digit_c(**pcur)) ++(*pcur);
		
		#if PROFILE_ATOF
		if (val) {
			BEGIN(atof);
			u64 _begin = __rdtsc();
			*val = (f32)atof(ret);
			u64 _end = __rdtsc();
			u64 _dt = _end -_begin;
			_rdtsc += _dt;
			END(atof);
		}
		#else
		#if OVERRIDE_ATOF
		
		if (val) *val = my_str_to_f32(ret);
		#if CHECK_MY_ATOF
		if (val) {
			f32 ref = (f32)atof(ret);
			if (*val != ref) {
				atof_not_equal += 1;
				printf(">>> =%u !=%u '%s' -> ref %f != my %f \n", atof_equal, atof_not_equal, ret, ref, *val);
			} else {
				atof_equal += 1;
			}
		}
		#endif
		
		#else
		if (val) *val = (f32)atof(ret);
		#endif
		#endif
		return { ret, (u32)(*pcur -ret) };
	}
	
}

#include <unordered_map>

namespace std {
	template<> struct hash<v2> {
		size_t operator() (v2 const& v) const {
			return	hash<f32>()(v.x) ^ hash<f32>()(v.y);
		}
	};
	template<> struct hash<v3> {
		size_t operator() (v3 const& v) const {
			return	hash<f32>()(v.x) ^ hash<f32>()(v.y) ^ hash<f32>()(v.z);
		}
	};
	template<> struct hash<v4> {
		size_t operator() (v4 const& v) const {
			return	hash<f32>()(v.x) ^ hash<f32>()(v.y) ^ hash<f32>()(v.z) ^ hash<f32>()(v.w);
		}
	};
	template<> struct hash<Mesh_Vertex> {
		size_t operator() (Mesh_Vertex const& v) const {
			// tangents are not calculated at this point (we always calculate them ourself)
			dbg_assert(all(v.tang_model == DEFAULT_TANG));
			return	hash<v3>()(v.pos_model) ^
					hash<v3>()(v.norm_model) ^
					hash<v2>()(v.uv) ^
					hash<v4>()(v.col);
		}
	};
}

bool Mesh_Vertex::operator== (Mesh_Vertex const& r) const {
	return	all(pos_model == r.pos_model) &&
			all(norm_model == r.norm_model) &&
			all(uv == r.uv) &&
			all(col == r.col);
}

static bool load_mesh (Vbo* vbo, cstr filepath, hm transform) {
	
	#if PROFILE_ATOF
	_atof_dt = 0;
	_atof_count = 0;
	_atof_min = INFd;
	_atof_max = -INFd;
	_rdtsc = 0;
	
	auto begin = glfwGetTime();
	#endif
	#if CHECK_MY_ATOF
	atof_equal = 0;
	atof_not_equal = 0;
	#endif
	
	struct Vert_Indecies {
		u32		pos;
		u32		uv;
		u32		norm;
	};
	struct Triangle {
		Vert_Indecies arr[3];
	};
	
	std::vector<v3> poss;
	std::vector<v2> uvs;
	std::vector<v3> norms;
	std::vector<Triangle> tris;
	
	poss.reserve(4*1024);
	uvs.reserve(4*1024);
	norms.reserve(4*1024);
	tris.reserve(4*1024);
	
	{ // load data from 
		str file;
		if (!read_text_file(filepath, &file)) {
			logf_warning("\"%s\" could not be loaded!", filepath);
			return false;
		}
		
		char* cur = &file[0];
		char* res;
		
		using namespace parse;
		auto ignore_line = [&] () {
			rest_of_line(&cur);
			newline(&cur);
		};
		
		auto parse_vec3 = [&] () -> v3 {
			v3 v;
			
			whitespace(&cur);
			if (!float_(&cur, &v.x)) goto error;
			
			whitespace(&cur);
			if (!float_(&cur, &v.y)) goto error;
			
			whitespace(&cur);
			if (!float_(&cur, &v.z)) goto error;
			
			if (!newline(&cur)) {
				logf_warning("load_mesh: \"%s\" Too many components in vec3 parsing, ignoring rest!");
				ignore_line();
			}
			
			return v;
			
			error: {
				logf_warning("load_mesh: \"%s\" Error in vec3 parsing, setting to NAN!");
				return QNAN;
			}
		};
		auto parse_vec2 = [&] () -> v2 {
			v2 v;
			
			whitespace(&cur);
			if (!float_(&cur, &v.x)) goto error;
			
			whitespace(&cur);
			if (!float_(&cur, &v.y)) goto error;
			
			if (!newline(&cur)) {
				logf_warning("load_mesh: \"%s\" Too many components in vec2 parsing, ignoring rest!");
				ignore_line();
			}
			
			return v;
			
			error: {
				logf_warning("load_mesh: \"%s\" Error in vec2 parsing, setting to NAN!");
				return QNAN;
			}
		};
		
		String obj_name = {};
		
		auto face = [&] () {
			Vert_Indecies vert[4];
			
			ui i = 0;
			for (;;) {
				
				whitespace(&cur);
				
				bool pos, uv=false, norm=false;
				
				pos = int_(&cur, &vert[i].pos);
				if (!pos || vert[i].pos == 0) goto error; // position missing
				
				if (*cur == '/') { ++cur;
					uv = int_(&cur, &vert[i].uv);
					if (uv && vert[i].uv == 0) goto error; // out of range index
					
					if (*cur++ != '/') goto error;
					
					norm = int_(&cur, &vert[i].norm);
					if (norm && vert[i].norm == 0) goto error; // out of range index
				}
				if (!uv)	vert[i].uv = 0;
				if (!norm)	vert[i].norm = 0;
				
				++i;
				if (newline(&cur) || *cur == '\0') break;
				if (i == 4) goto error; // only triangles and quads supported
			}
			
			if (i == 3) {
				tris.push_back({{	vert[0],
									vert[1],
									vert[2] }});
			} else /* i == 4 */ {
				tris.push_back({{	vert[1],
									vert[2],
									vert[0] }});
				tris.push_back({{	vert[0],
									vert[2],
									vert[3] }});
			}
			
			return;
			
			error: {
				logf_warning("load_mesh: \"%s\" Error in face parsing, setting to 0!", filepath);
				tris.push_back({});
			}
		};
		
		for (ui line_i=0;; ++line_i) {
			
			auto tok = token(&cur);
			//printf(">>> tok: %5d %.*s\n", line_i, tok.len,tok.ptr);
			
			if (!tok) {
				logf_warning("load_mesh: \"%s\" Missing line token, ignoring line!", filepath);
				rest_of_line(&cur); // skip line
				
			} else {
				if (		comp(tok, "v") ) {
					poss.push_back( parse_vec3() );
				}
				else if (	comp(tok, "vt") ) {
					uvs.push_back( parse_vec2() );
				}
				else if (	comp(tok, "vn") ) {
					norms.push_back( parse_vec3() );
				}
				else if (	comp(tok, "f") ) {
					face();
				}
				else if (	comp(tok, "o") ) {
					whitespace(&cur);
					
					obj_name = rest_of_line(&cur);
					newline(&cur);
				}
				else if (	comp(tok, "s") ||
							comp(tok, "mtllib") ||
							comp(tok, "usemtl") ||
							comp(tok, "#") ) {
					ignore_line();
				}
				else {
					logf_warning("load_mesh: \"%s\" Unknown line token \"%.*s\", ignoring line!", filepath, tok.len,tok.ptr);
					ignore_line();
				}
			}
			
			if (*cur == '\0') break;
		}
	}
	
	bool file_has_norm =	norms.size() != 0;
	bool file_has_uv =		uvs.size() != 0;
	bool file_has_col =		false; // .obj does not have vertex color
	
	if (!file_has_norm) {
		logf_warning("mesh_loader:: Mesh '%s' has no normal data, we do not support generating the normals ourself!");
	}
	
	{ // expand triangles from individually indexed poss/uvs/norms to non-indexed
		vbo->vertecies.reserve( tris.size() * 3 * sizeof(Mesh_Vertex) ); // vertecies are stored as a genric byte array
																		 // this is the max possible size
		vbo->indices.reserve( tris.size() * 3 );
		
		std::unordered_map<Mesh_Vertex, vert_indx_t> unique;
		
		for (auto& t : tris) {
			for (ui i=0; i<3; ++i) {
				union U {
					Mesh_Vertex v;
					byte		raw[sizeof(Mesh_Vertex)];
					
					U () {}
				} u;
				
				bool tri_has_pos =	t.arr[i].pos != 0;
				bool tri_has_norm =	t.arr[i].norm != 0;
				bool tri_has_uv =	t.arr[i].uv != 0;
				
				dbg_assert(tri_has_pos);
				dbg_assert(tri_has_norm == file_has_norm);
				dbg_assert(tri_has_uv == file_has_uv);
				
				u.v.pos_model =		tri_has_pos ?	transform * poss[t.arr[i].pos -1]	:	DEFAULT_POS;
				u.v.norm_model =	tri_has_norm ?	normalize(norms[t.arr[i].norm -1])	:	DEFAULT_NORM;
				u.v.tang_model =															DEFAULT_TANG;
				u.v.uv =			tri_has_uv ?	uvs[t.arr[i].uv -1]					:	DEFAULT_UV;
				u.v.col =																	DEFAULT_COL;
				
				auto entry = unique.find(u.v);
				bool is_unique = entry == unique.end();
				
				if (is_unique) {
					
					auto indx = (vert_indx_t)unique.size();
					unique.insert({u.v, indx});
					
					memcpy( &*vector_append(&vbo->vertecies, sizeof(u.raw)), u.raw, sizeof(u.raw) );
					
					vbo->indices.push_back(indx);
					
				} else {
					
					vbo->indices.push_back(entry->second);
					
				}
			}
		}
	}
	
	if (file_has_norm && file_has_uv) { // calc tangents
		
		// First determine which triangles are connected for each vertex
		// And calculate the tangent and bitangent of each triangle at the same time
		struct Connected_Tri {
			vert_indx_t		tri_i;
			Connected_Tri*	next;
		};
		
		auto** vert_conn_tris = (Connected_Tri**)malloc( vbo->vertecies.size() * sizeof(Connected_Tri*) );
		defer { free(vert_conn_tris); };
		memset(vert_conn_tris, 0, vbo->vertecies.size() * sizeof(Connected_Tri*));
		
		struct TB {
			v3		tang;
			v3		bitang;
			
			bool	degenerate;
			
			Connected_Tri	conn[3];
		};
		
		auto* tri_tangents = (TB*)malloc( tris.size() * sizeof(TB) );
		defer { free(tri_tangents); };
		
		auto vert = (Mesh_Vertex*)vbo->vertecies.data();
		
		for (vert_indx_t tri_i=0; tri_i<tris.size(); ++tri_i) {
			
			v3 pos[3];
			v2 uv[3];
			
			for (ui i=0; i<3; ++i) {
				
				auto indx = vbo->indices[ tri_i*3 +i ];
				
				{ // Insert our tri_i at the front of the list of connected triangles that starts at vert_conn_tris[indx] 
					auto* conn = vert_conn_tris[indx];
					
					auto* new_conn = &tri_tangents[tri_i].conn[i];
					new_conn->tri_i = tri_i;
					new_conn->next = conn;
					
					vert_conn_tris[indx] = new_conn;
				}
				
				pos[i] =	vert[indx].pos_model;
				uv[i] =		vert[indx].uv;
			}
			
			tri_tangents[tri_i].degenerate = false;
			
			// Calculate trangent and bitangent from delta uv
			v3 e0 = pos[1] -pos[0];
			v3 e1 = pos[2] -pos[0];
			
			if (all(e0 == 0) || all(e1 == 0)) {
				//logf_warning("mesh_loader:: Degenerate triangle [%llu] in mesh '%s'!", tri_i, filepath);
				tri_tangents[tri_i].degenerate = true;
			}
			
			f32 du0 = uv[1].x -uv[0].x;
			f32 dv0 = uv[1].y -uv[0].y;
			f32 du1 = uv[2].x -uv[0].x; 
			f32 dv1 = uv[2].y -uv[0].y; 
			
			f32 f_denom = (du0 * dv1) -(du1 * dv0);
			
			if (f_denom == 0) {
				//logf_warning("mesh_loader:: Degenerate uv map triangle [%llu] in mesh '%s'!", tri_i, filepath);
				tri_tangents[tri_i].degenerate = true;
			}
			
			f32 f = 1.0f / f_denom;
			
			v3 tang = v3(f) * ((v3(dv1) * e0) -(v3(dv0) * e1));
			v3 bitang = v3(f) * ((v3(du0) * e1) -(v3(du1) * e0));
			
			tang = normalize(tang);
			bitang = normalize(bitang);
			
			tri_tangents[tri_i].tang = tang;
			tri_tangents[tri_i].bitang = bitang;
		}
		
		auto calc_bitansign = [&] (v3 tang, v3 bitang, v3 norm) -> f32 {
			return dot(cross(norm, tang), bitang) < 0 ? -1.0f : +1.0f;
		};
		
		for (vert_indx_t v_i=0; v_i<vbo->vertecies.size()/sizeof(Mesh_Vertex); ++v_i) {
			
			vert_indx_t count = 0;
			v3 total_tang = 0;
			v3 total_bitang = 0;
			
			{
				dbg_assert(vert_conn_tris[v_i]);
				auto* cur = vert_conn_tris[v_i];
				
				do {
					if (!tri_tangents[cur->tri_i].degenerate) {
						v3 t = tri_tangents[cur->tri_i].tang;
						v3 b = tri_tangents[cur->tri_i].bitang;
						
						dbg_assert(all(t >= -1.01f && t <= 1.01f) && all(b >= -1.01f && b <= 1.01f));
						
						total_tang +=	t;
						total_bitang +=	b;
						
						++count;
					}
					
					cur = cur->next;
				} while (cur);
			}
			
			if (count == 0) {
				//logf_warning("mesh_loader:: Vertex was part of only degenerate triangles [%llu] in mesh '%s'!", v_i, filepath);
				//vert[v_i].col *= v4(1,1,0,1);
				
				vert[v_i].tang_model = DEFAULT_TANG;
			} else {
				// average tangent and bitangent
				v3 avg_tang = total_tang / (f32)count;
				v3 avg_bitang = total_bitang / (f32)count;
				
				if (length(avg_tang) < 0.05f || length(avg_bitang) < 0.05f) { // vectors could cancel out
					//logf_warning("mesh_loader:: tangent vectors (almost) cancel out (%g, %g) [%llu] in mesh '%s'!", length(avg_tang), length(avg_bitang), v_i, filepath);
					//vert[v_i].col *= v4(0,1,0,1);
				}
				
				avg_tang = normalize(avg_tang);
				avg_bitang = normalize(avg_bitang);
				
				v3 norm = vert[v_i].norm_model;
				
				f32 bitansign = calc_bitansign(avg_tang, avg_bitang, norm);
				
				vert[v_i].tang_model = v4(avg_tang, bitansign);
				
				if (!equal_epsilon(length(vert[v_i].tang_model.xyz()), 1, 0.01f) || abs(vert[v_i].tang_model.w) > 1) dbg_assert(false);
			}
			
		}
		
	}
	
	#if PROFILE_ATOF
	printf(">>> %s: %u tris\n", filepath, (u32)tris.size());
	
	auto dt = glfwGetTime() -begin;
	printf(">> atof: total %g per %g min %g max %g ms total time: %g ms   _rdtsc %g\n",
	_atof_dt * 1000, _atof_dt / (f64)_atof_count * 1000,
	_atof_min * 1000, _atof_max * 1000,
	dt * 1000,
	
	(f64)_rdtsc / (f64)_atof_count);
	#endif
	#if CHECK_MY_ATOF
	printf(">>> CHECK_MY_ATOF =%u !=%u\n", atof_equal, atof_not_equal);
	#endif
	
	return true;
}
