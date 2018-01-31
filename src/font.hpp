
namespace font {
	
	struct Glyph_Vertex {
		v2	pos_screen;
		v2	uv;
		v4	col;
	};

	static Vertex_Layout mesh_vert_layout = {
		{ "pos_screen",	T_V2, sizeof(Glyph_Vertex), offsetof(Glyph_Vertex, pos_screen) },
		{ "uv",			T_V2, sizeof(Glyph_Vertex), offsetof(Glyph_Vertex, uv) },
		{ "col",		T_V4, sizeof(Glyph_Vertex), offsetof(Glyph_Vertex, col) },
	};
	
	struct Glyph_Range {
		cstr				fontname;
		
		stbtt_pack_range	pr;
		
		std::vector<utf32>	arr; // storage for array of glyphs
		
		Glyph_Range (cstr f, f32 s, utf32 first, utf32 last):
			fontname{f}, pr{s, (int)first, nullptr, (int)(last +1 -first), nullptr}, arr{} {}
		Glyph_Range (cstr f, f32 s, utf32 single_char): Glyph_Range{f,s,single_char,single_char} {}
		Glyph_Range (cstr f, f32 s, std::initializer_list<utf32> l): fontname{f}, arr{l} {
			pr = {s, 0, (int*)arr.data(), (int)arr.size(), nullptr};
		}
	};
	
	static iv2 tex_dim = 512; // hopefully large enough for now
	
	#define QUAD(a,b,c,d) b,c,a, a,c,d
	
	static auto QUAD_VERTS = {	QUAD(	v2(0,0),
										v2(1,0),
										v2(1,1),
										v2(0,1) )};
	
	struct Font {
		Texture2D		tex;
		
		u32				glyphs_count;
		std::vector<stbtt_packedchar>	glyphs_packed_chars;
		
		f32 border_left;
		
		f32 ascent_plus_gap;
		f32 descent_plus_gap;
		
		f32 line_height;
		
		std::vector<Glyph_Range>	ranges;
		
		Font (s32 main_sz, std::initializer_list<Glyph_Range> r) {
			ranges = r;
			
			tex.alloc_cpu_single_mip(PT_LR8, tex_dim);
			
			cstr fonts_folder = "c:/windows/fonts/";
			
			struct Loaded_Font_File {
				cstr		filename;
				Data_Block	data;
			};
			
			std::vector<Loaded_Font_File> loaded_files;
			
			stbtt_pack_context spc;
			stbtt_PackBegin(&spc, tex.mips[0].data, tex.mips[0].dim.x,tex.mips[0].dim.y, tex.dim.x*sizeof(u8), 1, nullptr);
			
			glyphs_count = 0;
			for (auto r : ranges) {
				dbg_assert(r.pr.num_chars > 0);
				glyphs_count += r.pr.num_chars;
			}
			glyphs_packed_chars.resize(glyphs_count);
			
			u32 cur = 0;
			
			for (auto r : ranges) {
				
				cstr filename = r.fontname;
				
				auto* font_file = lsearch(loaded_files, [&] (Loaded_Font_File* loaded) {
						return strcmp(loaded->filename, filename) == 0;
					} );
				
				auto filepath = prints("%s%s", fonts_folder, filename);
				
				if (!font_file) {
					loaded_files.push_back({ filename }); font_file = &loaded_files.back();
					
					read_entire_file(filepath.c_str(), &font_file->data);
					
					if (cur == 0) {
						stbtt_fontinfo info;
						dbg_assert( stbtt_InitFont(&info, font_file->data.data, 0) );
						
						f32 scale = stbtt_ScaleForPixelHeight(&info, main_sz);
						
						s32 ascent, descent, line_gap;
						stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
						
						s32 x0, x1, y0, y1;
						stbtt_GetFontBoundingBox(&info, &x0, &y0, &x1, &y1);
						
						//border_left = -x0*scale;
						border_left = 0;
						
						line_height = ceil(ascent*scale -descent*scale +line_gap*scale); // ceil, so that lines are always seperated by exactly n pixels (else lines would get rounded to a y pos, which would result in uneven spacing)
						
						f32 ceiled_line_gap = line_height -(ascent*scale -descent*scale);
						
						ascent_plus_gap = +ascent*scale +ceiled_line_gap/2;
						descent_plus_gap = -descent*scale +ceiled_line_gap/2;
						
						//printf(">>> %f %f %f %f\n", border_left, ascent_plus_gap, descent_plus_gap, line_height);
						
					}
				}
				
				r.pr.chardata_for_range = &glyphs_packed_chars[cur];
				cur += r.pr.num_chars;
				
				dbg_assert( stbtt_PackFontRanges(&spc, font_file->data.data, 0, &r.pr, 1) > 0);
				
			}
			
			stbtt_PackEnd(&spc);
			
			tex.flip_vertical();
			
			tex.upload();
		}
		
		int search_glyph (utf32 c) {
			int cur = 0;
			for (auto r : ranges) {
				if (r.pr.array_of_unicode_codepoints) {
					for (int i=0; i<r.pr.num_chars; ++i) {
						if (c == (utf32)r.pr.array_of_unicode_codepoints[i]) return cur; // found
						++cur;
					}
				} else {
					auto first = (utf32)r.pr.first_unicode_codepoint_in_range;
					if (c >= first && (c -first) < (u32)r.pr.num_chars) return cur +(c -first); // found
					cur += (u32)r.pr.num_chars;
				}
			}
			
			// This is probably a normal thing to happen, so no assert
			//dbg_assert(false, "Glyph '%c' [%x] missing in font", c, c);
			return 0; // missing glyph
		}
		
		f32 emit_glyph (std::vector<byte>* vbo_buf, f32 pos_x_px, f32 pos_y_px, utf32 c, v4 col) {
			
			stbtt_aligned_quad quad;
			
			stbtt_GetPackedQuad(glyphs_packed_chars.data(), tex.dim.x,tex.dim.y, search_glyph(c),
					&pos_x_px,&pos_y_px, &quad, 1);
			
			for (v2 quad_vert : QUAD_VERTS) {
				*(Glyph_Vertex*)&*vector_append(vbo_buf, sizeof(Glyph_Vertex)) = {
					/*pos_screen*/	lerp(v2(quad.x0,quad.y0), v2(quad.x1,quad.y1), quad_vert),
					/*uv*/			lerp(v2(quad.s0,-quad.t0), v2(quad.s1,-quad.t1), quad_vert),
					/*col*/			col };
			}
			
			return pos_x_px;
		};
		
		void draw_line (std::vector<byte>* vbo_buf, f32 pos_y_px, Shader* shad, std::basic_string<utf32> const& text, v4 col) {
			
			u32 tab_spaces = 4;
			
			u32 char_i=0;
			
			f32 pos_x_px = 0;
			
			for (utf32 c : text) {
				
				switch (c) {
					case U'\t': {
						u32 spaces_needed = tab_spaces -(char_i % tab_spaces);
						
						for (u32 j=0; j<spaces_needed; ++j) {
							pos_x_px = emit_glyph(vbo_buf, pos_x_px, pos_y_px, U' ', col);
							++char_i;
						}
						
					} break;
					
					case U'\n':
					case U'\r': {
						// Ignore, since this function only ever prints a line
					} break;
					
					default: {
						pos_x_px = emit_glyph(vbo_buf, pos_x_px, pos_y_px, c, col);
						++char_i;
					} break;
				}
			}
		}
	};
	
}
