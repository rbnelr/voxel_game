//// Debug Line Rendering

uniform bool update_debugdraw = false;

struct glDrawArraysIndirectCommand {
	uint count;
	uint instanceCount;
	uint first;
	uint baseInstance;
};

struct glDrawElementsIndirectCommand {
	uint count;
	uint primCount;
	uint firstIndex;
	uint baseVertex;
	uint baseInstance;
};

struct IndirectLineVertex {
	vec4 pos;
	vec4 col;
};
struct IndirectWireInstace {
	vec4 pos;
	vec4 size;
	vec4 col;
};

const int _INDIRECT_BUFSZ = 4096;
struct IndirectLines {
	glDrawArraysIndirectCommand cmd;
	IndirectLineVertex vertices[_INDIRECT_BUFSZ];
};
struct IndirectWireInstances {
	glDrawElementsIndirectCommand cmd;
	uint _pad[3]; // padding for std430 alignment
	IndirectWireInstace vertices[_INDIRECT_BUFSZ];
};

layout(std430, binding = 1) restrict buffer IndirectBuffer {
	IndirectLines         lines;
	IndirectWireInstances wire_cubes;
	IndirectWireInstances wire_spheres;
} _dbgdrawbuf;

//void dbgdraw_clear () {
//	_dbgdrawbuf.lines       .cmd.count = 0;
//	_dbgdrawbuf.wire_cubes  .cmd.primCount = 0;
//	_dbgdrawbuf.wire_spheres.cmd.primCount = 0;
//}
void dbgdraw_vector (vec3 pos, vec3 dir, vec4 col) {
	uint idx = atomicAdd(_dbgdrawbuf.lines.cmd.count, 2u);
	if (idx >= _INDIRECT_BUFSZ) return;
	
	_dbgdrawbuf.lines.vertices[idx++] = IndirectLineVertex( vec4(pos      , 0), col );
	_dbgdrawbuf.lines.vertices[idx++] = IndirectLineVertex( vec4(pos + dir, 0), col );
}
void dbgdraw_point (vec3 pos, float r, vec4 col) {
	uint idx = atomicAdd(_dbgdrawbuf.lines.cmd.count, 6u);
	if (idx >= _INDIRECT_BUFSZ) return;
	
	_dbgdrawbuf.lines.vertices[idx++] = IndirectLineVertex( vec4(pos - vec3(r,0,0), 0), col );
	_dbgdrawbuf.lines.vertices[idx++] = IndirectLineVertex( vec4(pos + vec3(r,0,0), 0), col );
	
	_dbgdrawbuf.lines.vertices[idx++] = IndirectLineVertex( vec4(pos - vec3(0,r,0), 0), col );
	_dbgdrawbuf.lines.vertices[idx++] = IndirectLineVertex( vec4(pos + vec3(0,r,0), 0), col );
	
	_dbgdrawbuf.lines.vertices[idx++] = IndirectLineVertex( vec4(pos - vec3(0,0,r), 0), col );
	_dbgdrawbuf.lines.vertices[idx++] = IndirectLineVertex( vec4(pos + vec3(0,0,r), 0), col );
}

void dbgdraw_wire_cube (vec3 pos, vec3 size, vec4 col) { // size is edge length aka diameter
	uint idx = atomicAdd(_dbgdrawbuf.wire_cubes.cmd.primCount, 1u);
	if (idx >= _INDIRECT_BUFSZ) return;
	_dbgdrawbuf.wire_cubes.vertices[idx] = IndirectWireInstace( vec4(pos, 0), vec4(size, 0), col);
}
void dbgdraw_wire_sphere (vec3 pos, vec3 size, vec4 col) { // size is sphere diameter (not radius)
	uint idx = atomicAdd(_dbgdrawbuf.wire_spheres.cmd.primCount, 1u);
	if (idx >= _INDIRECT_BUFSZ) return;
	_dbgdrawbuf.wire_spheres.vertices[idx] = IndirectWireInstace( vec4(pos, 0), vec4(size, 0), col);
}
