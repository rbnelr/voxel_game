#version 330 core

$if vertex
	in vec2 Position;
	in vec2 UV;
	in vec4 Color;

	uniform mat4 ProjMtx;

	out vec2 Frag_UV;
	out vec4 Frag_Color;

	void main () {
		Frag_UV = UV;
		Frag_Color = vec4(pow(Color.rgb, vec3(2.2)), Color.a);
		gl_Position = ProjMtx * vec4(Position.xy,0,1);
	}
$endif

$if fragment
	in vec2 Frag_UV;
	in vec4 Frag_Color;

	uniform sampler1D Texture1;
	uniform sampler2D Texture2;
	uniform sampler3D Texture3;

	uniform sampler1DArray Texture1A;
	uniform sampler2DArray Texture2A;

	uniform int type;
	uniform float z;

	out vec4 frag_col;

	void main () {
		//switch (type) {
		//	case 0:
		//		frag_col = Frag_Color * texture(Texture1, Frag_UV.x);
		//		break;
		//	case 1:
		//		frag_col = Frag_Color * texture(Texture2, Frag_UV.xy);
		//		break;
		//	case 2:
		//		frag_col = Frag_Color * texture(Texture3, vec3(Frag_UV.xy, z));
		//		break;
		//
		//	case 3:
		//		frag_col = Frag_Color * texture(Texture1A, vec2(Frag_UV.x, z));
		//		break;
		//	case 4:
				frag_col = Frag_Color * texture(Texture2A, vec3(Frag_UV.xy, z));
		//		break;
		//}
	}
$endif
