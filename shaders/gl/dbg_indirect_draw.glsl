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
struct IndirectWireCubeInstace {
	vec4 pos;
	vec4 size;
	vec4 col;
};

struct IndirectLines {
	glDrawArraysIndirectCommand cmd;
	IndirectLineVertex vertices[4096];
};
struct IndirectWireCubes {
	glDrawElementsIndirectCommand cmd;
	uint _pad[3]; // padding for std430 alignment
	IndirectWireCubeInstace vertices[4096];
};

layout(std430, binding = 1) restrict buffer IndirectBuffer {
	IndirectLines     lines;
	IndirectWireCubes wire_cubes;
} _dbgdrawbuf;

//void dbgdraw_clear () {
//	_dbgdrawbuf.lines     .cmd.count = 0;
//	_dbgdrawbuf.wire_cubes.cmd.primCount = 0;
//}
void dbgdraw_vector (vec3 pos, vec3 dir, vec4 col) {
	uint idx = atomicAdd(_dbgdrawbuf.lines.cmd.count, 2u);
	if (idx < 4096) {
		_dbgdrawbuf.lines.vertices[idx++] = IndirectLineVertex( vec4(pos      , 0), col );
		_dbgdrawbuf.lines.vertices[idx++] = IndirectLineVertex( vec4(pos + dir, 0), col );
		//_dbgdrawbuf.lines.cmd.count %= 4096;
	}
}
void dbgdraw_wire_cube (vec3 pos, vec3 size, vec4 col) {
	uint idx = atomicAdd(_dbgdrawbuf.wire_cubes.cmd.primCount, 1u);
	if (idx < 4096) {
		_dbgdrawbuf.wire_cubes.vertices[idx] =
			IndirectWireCubeInstace( vec4(pos, 0), vec4(size, 0), col);
		//_dbgdrawbuf.wire_cubes.cmd.primCount %= 4096;
	}
}
