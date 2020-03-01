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

	uniform sampler2DArray Texture;
	uniform float r = 0.0;

	out vec4 frag_col;

	void main () {
		frag_col = Frag_Color * texture(Texture, vec3(Frag_UV.st, r));
	}
$endif
