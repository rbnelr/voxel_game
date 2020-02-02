
out		vec3	vs_barycentric;

const vec3[] BARYCENTRIC = vec3[] ( vec3(1,0,0), vec3(0,1,0), vec3(0,0,1) );

#define WIREFRAME_MACRO		vs_barycentric = BARYCENTRIC[gl_VertexID % 3]

$include "_common.glsl"
