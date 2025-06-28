import srcgen
from collections import namedtuple
from itertools import chain

########## Data type handling for generators
# A data type that the math library provided, can be a scalar, vector, quaternion or matrix type
class type_:
	def __init__(self):
		self.name = None # name ie. c++ type name of the generated struct / the basic scalar type (int, float etc.)
		self.scalar_type = None # the scalar type for vectors and matricies
		self.size = None # the vector size / length
		# conversion types for ex. int -> float   and int3 -> float3
		self.bool_type = None
		self.int_cast_type = None
		self.float_cast_type = None
		self.stdint_type = None
	def __repr__(self): return f'type_<{self.name}>'
	def __str__(self): return self.name
	
	def kind(self):
		if self.size == 1:						return 'scalar'
		elif not isinstance(self.size, tuple):	return 'vector'
		else:									return 'matrix'
	def get_deps(self):
		k = self.kind()
		if k == 'scalar':
			return []
		elif k == 'vector':
			return [self.scalar_type]
		elif k == 'matrix':
			return list(set([self.scalar_type, get_type(self.scalar_type, self.size[0]), get_type(self.scalar_type, self.size[1])]))

#Note: assuming int is 32 bit for simplicity

_types = {}
_types_by_name = {}
def get_type(scalar_type, size=1):
	if not scalar_type: return None

	if not isinstance(scalar_type, type_) and size != 1:
		scalar_type = _types[(str(scalar_type), 1)]

	# return existing instance in dict if exists to break infinite recursion
	id = str(scalar_type), size
	if id in _types:
		return _types[id]

	# float to int cast
	f2i = {
			'float'        : 'int',
			'double'       : 'int64_t',
			# int to int (does nothing)
			'int8_t'       : 'int8_t',
			'int16_t'      : 'int16_t',
			'int'          : 'int',
			'int64_t'      : 'int64_t',
			'uint8_t'      : 'uint8_t',
			'uint16_t'     : 'uint16_t',
			'unsigned int' : 'unsigned int',
			'uint64_t'     : 'uint64_t',
			'bool'         : 'uint8_t', # true == 1  false == 0
		}
	# int to float cast
	i2f = {
			# float to float (does nothing)
			'float'        : 'float',
			'double'       : 'double',
			# int to float (all ints <= 32 bit become float else double
			'int8_t'       : 'float',
			'int16_t'      : 'float',
			'int'          : 'float',
			'int64_t'      : 'double',
			'uint8_t'      : 'float',
			'uint16_t'     : 'float',
			'unsigend int' : 'float',
			'uint64_t'     : 'double',
			'bool'         : 'float', # true == 1  false == 0
		}
		
	# stdint.h names to less verbose version for vector versions
	shorthand_map = {
		'int8_t'        : 'int8',
		'int16_t'       : 'int16',
		'int64_t'       : 'int64',
		'uint8_t'       : 'uint8',
		'uint16_t'      : 'uint16',
		'unsigned int'  : 'uint',
		'uint64_t'      : 'uint64',
	}
	
	if str(scalar_type) in shorthand_map:
		shorthand = shorthand_map[str(scalar_type)]
	else:
		shorthand = str(scalar_type)
	
	if size == 1:
		# just a scalar type
		name = str(scalar_type)
	elif not isinstance(size, tuple):
		# vector type (for ex. int3, uint64_3)
		if shorthand[-1].isdigit():
			name = f'{shorthand}v{size}'
		else:
			name = shorthand + str(size)
		
		shorthand = name
	else:
		# matrix type
		if not shorthand in [str(f) for f in floats]:
			return None # only float matricies matricies

		name = f'{shorthand}{size[0]}x{size[1]}'
		
		shorthand = name
		
	t = type_()
	_types[id] = t
	_types_by_name[name] = t
	
	t.name = name
	t.size = size
	t.shorthand			= shorthand
	t.scalar_type		= get_type(scalar_type, 1)
	t.bool_type			= get_type('bool', size)
	t.int_cast_type		= get_type(f2i[str(scalar_type)], size)
	t.float_cast_type	= get_type(i2f[str(scalar_type)], size)

	return t

def type_exist(scalar_type, size):
	return (str(scalar_type), size) in _types

namespace = "kissmath"

########## File generation procedures

myfloatsuff = {'float':'', 'double':'d'} # my postfix of Math constants
cfloatsuff = {'float':'f', 'double':''} # postfix of Math constants

def generic_math(T, f):
	ST = T.scalar_type
	
	if ST in floats:
		f += '\n//// Angle conversion\n\n'
		
		csuff = cfloatsuff[str(T.scalar_type)]
		mysuff = myfloatsuff[str(T.scalar_type)]

		f.function(f'{T}', 'to_radians',	f'{T} deg', f'return deg * DEG_TO_RAD{mysuff};', comment='converts degrees to radiants')
		f.function(f'{T}', 'to_degrees',	f'{T} rad', f'return rad * RAD_TO_DEG{mysuff};', comment='converts radiants to degrees')
		
		f.function(f'{T}', 'deg',			f'{T} deg', f'return deg * DEG_TO_RAD{mysuff};', comment='converts degrees to radiants\nshortform to make degree literals more readable')
		if str(T) == 'float':
			f.function(f'{T}', 'deg',		f'int deg', f'return (float)deg * DEG_TO_RAD{mysuff};', comment='converts degrees to radiants\nspcial shortform to make degree literals more readable and allow to use integer literals (deg(5) would throw error)')
	
		f.header += '//// Linear interpolation\n\n'

		f.function(f'{T}', 'lerp',	f'{T} a, {T} b, {T} t',
				  #f'return a * (1.0{csuff} - t) + b * t;',
				  f'return t * (b - a) + a;',
				  comment='linear interpolation\nlike getting the output of a linear function\nex. t=0 -> a ; t=1 -> b ; t=0.5 -> (a+b)/2')
		f.function(f'{T}', 'map',	f'{T} x, {T} in_a, {T} in_b',
				  f'return (x - in_a) / (in_b - in_a);',
				  comment='linear mapping\nsometimes called inverse linear interpolation\nlike getting the x for a y on a linear function\nex. map(70, 0,100) -> 0.7 ; map(0.5, -1,+1) -> 0.75')
		f.function(f'{T}', 'map',	f'{T} x, {T} in_a, {T} in_b, {T} out_a, {T} out_b',
				  f'return lerp(out_a, out_b, map(x, in_a, in_b));',
				  comment='linear remapping\nequivalent of lerp(out_a, out_b, map(x, in_a, in_b))')

		f += '\n//// Various interpolation\n\n'
		
		f.function(f'{T}', 'smoothstep', f'{T} x', f'''
			{T} t = clamp(x);
			return t * t * (3.0{csuff} - 2.0{csuff} * t);
		''', comment="standard smoothstep interpolation")
		
		f.function(f'{T}', 'bezier', f'{T} a, {T} b, {T} c, {ST} t', f'''
			{T} d = lerp(a, b, t);
			{T} e = lerp(b, c, t);
			{T} f = lerp(d, e, t);
			return f;
		''', comment="3 point bezier interpolation")
		f.function(f'{T}', 'bezier', f'{T} a, {T} b, {T} c, {T} d, {ST} t', '''
			return bezier(
							lerp(a, b, t),
							lerp(b, c, t),
							lerp(c, d, t),
							t
						);
		''', comment="4 point bezier interpolation")
		f.function(f'{T}', 'bezier', f'{T} a, {T} b, {T} c, {T} d, {T} e, {ST} t', '''
			return bezier(
							lerp(a, b, t),
							lerp(b, c, t),
							lerp(c, d, t),
							lerp(d, e, t),
							t
						);
		''', comment="5 point bezier interpolation")

def scalar_math(T, f):
	BT = T.bool_type
	IT = T.int_cast_type # int scalar for roundi funcs
	
	f.header += '#include <cmath>\n'
	if T in floats:
		f.header += '#include <limits> // for std::numeric_limits<float>::quiet_NaN()\n'
	f.header += '#include <cstdint>\n\n'

	f += f'namespace {namespace} {{\n'
	
	if str(T) != 'bool':
			
		f.header += '''
			// Use std math functions for these
			using std::abs;
			using std::floor;
			using std::ceil;
			using std::pow;
			using std::round;
		\n'''

		f.function(f'{T}', 'wrap', f'{T} x, {T} range', f'''
			{T} modded = '''+ ('std::fmod(x, range)' if T in floats else 'x % range') +''';
			if (range > 0) {
				if (modded < 0) modded += range;
			} else {
				if (modded > 0) modded += range;
			}
			return modded;
		''', comment='wrap x into range [0,range)\nnegative x wrap back to +range unlike c++ % operator\nnegative range supported')

		f.function(f'{T}', 'wrap', f'{T} x, {T} a, {T} b', f'''
			x -= a;
			{T} range = b -a;
			
			{T} modulo = wrap(x, range);
			
			return modulo + a;
		''', comment='wrap x into [a,b) range')
		
		#min_func = 'std::fmin(l,r)' if T in floats else f'l <= r ? l : r'
		#max_func = 'std::fmax(l,r)' if T in floats else f'l >= r ? l : r'
		min_func = f'l <= r ? l : r'
		max_func = f'l >= r ? l : r'
	
		f.function(f'{T}', 'min',		f'{T} l, {T} r',			f'return {min_func};',
				comment='returns the greater value of a and b')
		f.function(f'{T}', 'max',		f'{T} l, {T} r',			f'return {max_func};',
				comment='returns the smaller value of a and b')

		f.function(f'{T}', 'select',	f'{BT} c, {T} l, {T} r',	f'return c ? l : r;',
				comment='equivalent to ternary c ? l : r\nfor conformity with vectors')
		
		f.function(f'{T}', 'clamp',		f'{T} x, {T} a, {T} b',		f'return min(max(x, a), b);',
				comment='clamp x into range [a, b]\nequivalent to min(max(x,a), b)')
		f.function(f'{T}', 'clamp',		f'{T} x',					f'return min(max(x, {T}(0)), {T}(1));',
				comment='clamp x into range [0, 1]\nalso known as saturate in hlsl')

		if T in floats:
		
			csuff = cfloatsuff[str(T.scalar_type)]
			mysuff = myfloatsuff[str(T.scalar_type)]

			f.header += f'''
				//// Math constants

				constexpr {T} PI{mysuff}          = 3.1415926535897932384626433832795{csuff};
				constexpr {T} TAU{mysuff}         = 6.283185307179586476925286766559{csuff};
				constexpr {T} SQRT_2{mysuff}      = 1.4142135623730950488016887242097{csuff};
				constexpr {T} EULER{mysuff}       = 2.7182818284590452353602874713527{csuff};
				constexpr {T} DEG_TO_RAD{mysuff}  = 0.01745329251994329576923690768489{csuff}; // 180/PI
				constexpr {T} RAD_TO_DEG{mysuff}  = 57.295779513082320876798154814105{csuff};  // PI/180
			'''
			
			f.header += f'''
				#if _MSC_VER && !__INTELRZ_COMPILER && !__clan_
					constexpr {T} INF         = (float)(1e+300 * 1e+300);
					constexpr {T} QNAN        = std::numeric_limits<float>::quiet_NaN();
				#elif __GNUC__ || __clan_
					constexpr {T} INF         = __builtin_inff();
					constexpr {T} QNAN        = (float)__builtin_nan("0");
				#endif
			\n\n''' if str(T) == 'float' else f'''
				#if _MSC_VER && !__INTELRZ_COMPILER && !__clan_
					constexpr {T} INFd         = 1e+300 * 1e+300;
					constexpr {T} QNANd        = std::numeric_limits<double>::quiet_NaN();
				#elif __GNUC__ || __clan_
					constexpr {T} INFd         = __builtin_inf();
					constexpr {T} QNANd        = __builtin_nan("0");
				#endif
			\n\n'''

			f.function(f'{T}', 'wrap', f'{T} x, {T} a, {T} b, {IT}* quotient', f'''
				x -= a;
				{T} range = b -a;
			
				{T} modulo = wrap(x, range);
				*quotient = floori(x / range);
			
				return modulo + a;
			''')
			f += '\n'
			
			f.function(f'{IT}', 'floori',	f'{T} x', f'return ({IT})floor(x);', comment='floor and convert to int')
			f.function(f'{IT}', 'ceili',	f'{T} x', f'return ({IT})ceil(x);', comment='ceil and convert to int')
			f.function(f'{IT}', 'roundi',	f'{T} x', f'return '+ {'float':'std::lround(x)', 'double':'std::llround(x)'}[str(T)] +';',
				 comment='round and convert to int')
			
			f += '\n'

		
		generic_math(T, f)

		f += '\n'

		abs_func = f'std::fabs(x)' if T in floats else f'std::abs(x)'
		
		if T not in uints: # no abs for unsigned types
			f.function(f'{T}', 'length',		f'{T} x',	f'return {abs_func};',
				 comment="length(scalar) = abs(scalar)\nfor conformity with vectors")
			f.function(f'{T}', 'length_sqr',	f'{T} x',	f'x = {abs_func};\nreturn x*x;',
				 comment="length_sqr(scalar) = abs(scalar)^2\nfor conformity with vectors (for vectors this func is preferred over length to avoid the sqrt)")
			
			f.function(f'{T}', 'normalize',		f'{T} x',	f'return x / length(x);',
				 comment="scalar normalize for conformity with vectors\nnormalize(-6.2f) = -1f, normalize(7) = 1, normalize(0) = <div 0>\ncan be useful in some cases")
		
			f.function(f'{T}', 'normalizesafe', f'{T} x', f'''
				{T} len = length(x);
				if (len == {T}(0)) {{
					return {T}(0);
				}}
				return x /= len;
			''', comment='normalize(x) for length(x) != 0 else 0')

			f += '\n'

	f += '}\n'

def gen_vector(V, f):
	size = V.size

	all_dims = ('x', 'y', 'z', 'w')
	dims = all_dims[:size]
	
	T = V.scalar_type
	FT = T.float_cast_type
	FV = V.float_cast_type
	IV = V.int_cast_type
	BV = V.bool_type
	
	def unary_op(op, comment=''):
		tmp = ', '.join(f'{op}v.{d}' for d in dims)
		f.function(f'{V}', f'operator{op}', f'{V} v', f'return {V}({tmp});', comment=comment)
	def binary_op(op, comment=''):
		tmp = ', '.join(f'l.{d} {op} r.{d}' for d in dims)
		f.function(f'{V}', f'operator{op}', f'{V} l, {V} r', f'return {V}({tmp});', comment=comment)
	def comparison_op(op, comment=''):
		tmp = ', '.join(f'l.{d} {op} r.{d}' for d in dims)
		f.function(f'{BV}', f'operator{op}', f'{V} l, {V} r', f'return {BV}({tmp});', comment=comment)
	def comparison_func(func, op, comment=''):
		tmp = ', '.join(f'l.{d} {op} r.{d}' for d in dims)
		f.function(f'{BV}', func, f'{V} l, {V} r', f'return {BV}({tmp});', comment=comment)
	
	def unary_func(func, arg='v', ret=None, comment=''):
		ret = ret or V
		f.function(f'{ret}', func, f'{V} {arg}', f'return {ret}(%s);' % ', '.join(f'{func}({arg}.{d})' for d in dims), comment=comment)
	def nary_func(func, args, comment=''):
		f.function(f'{V}', func, ', '.join(f'{V} {a}' for a in args),
			f'return {V}(%s);' % ', '.join(f'{func}(%s)' % ','.join(f'{a}.{d}' for a in args) for d in dims), comment=comment)

	def compound_binary_op(op, comment=''):
		if False: # compact
			tmp = ', '.join(f'{d} {op} r.{d}' for d in dims)
			body = f'return *this = {V}({tmp});'
		else:
			body = ''.join(f'{d} {op}= r.{d};\n' for d in dims) + 'return *this;'

		f.method(f'{V}', f'{V}', f'operator{op}=', f'{V} r', body, comment=comment)

	def casting_op(to_type, comment=''):
		tt = to_type.scalar_type

		tmp = ', '.join(f'({tt}){d}' for d in dims)
		body = f'return {to_type}({tmp});'
		
		f.method(f'{V}', '', f'operator {to_type}', '', body, const=True, explicit=True, comment=comment)

	# types references by this type
	other_size_vecs = [v for v in vectors if v.size != size and v.scalar_type == T and v in vectors]
	other_type_vecs = [v for v in vectors if v.size == size and v.scalar_type != T and v in vectors]

	forward_decl_vecs = set(other_size_vecs +([BV] if BV in vectors else [])+ other_type_vecs)
	
	if str(T.scalar_type) != 'bool':
		f.header += f'#include "{T.scalar_type.shorthand}.hpp"\n\n'
	f.header += f'namespace {namespace} {{\n'
		
	f.header += '//// forward declarations\n\n'

	for v in forward_decl_vecs:
		f.header += f'struct {v};\n'
	
	#
	f.inlined += ''.join(f'#include "{n.shorthand}.hpp"\n' for n in forward_decl_vecs)

	f.inlined += f'\nnamespace {namespace} {{\n'
	
	f.inlined += '//// forward declarations\n// typedef these because the _t suffix is kinda unwieldy when using these types often\n\n'

	for v in forward_decl_vecs:
		if v.scalar_type.stdint_type:
			f.inlined += f'typedef {v.scalar_type.stdint_type} {v.scalar_type};\n'

	#
	f.source += ''.join(f'#include "{n.shorthand}.hpp"\n' for n in forward_decl_vecs)

	f.source += f'\nnamespace {namespace} {{\n'
	
	f.source += '//// forward declarations\n// typedef these because the _t suffix is kinda unwieldy when using these types often\n\n'

	for v in forward_decl_vecs:
		if v.scalar_type.stdint_type:
			f.source += f'typedef {v.scalar_type.stdint_type} {v.scalar_type};\n'

	
	f.header += f'''
		struct {V} {{
			union {{ // Union with named members and array members to allow vector[] operator, not 100% sure that this is not undefined behavoir, but I think all compilers definitely don't screw up this use case
				struct {{
					{T}	{', '.join(dims)};
				}};
				{T}		arr[{size}];
			}};
		\n'''
		
	f.method(V, f'{T}&', 'operator[]', 'int i', 'return arr[i];',
		  comment='Component indexing operator')
	f.method(V, f'{T} const&', 'operator[]', 'int i', 'return arr[i];', const=True,
		  comment='Component indexing operator')

	f += '\n'
	
	f.constructor(f'{V}', args='', comment='uninitialized constructor', defaulted=True)
	f.constructor(f'{V}', args=f'{T} all',						init_list=', '.join(f'{d}{{all}}' for d in dims),
		comment=
		'''sets all components to one value
		implicit constructor -> float3(x,y,z) * 5 will be turned into float3(x,y,z) * float3(5) by to compiler to be able to execute operator*(float3, float3), which is desirable
		and short initialization like float3 a = 0; works''')
	f.constructor(f'{V}', args=', '.join(f'{T} {d}' for d in dims), init_list=', '.join(f'{d}{{{d}}}' for d in dims),
		comment='supply all components')
	
	for vsz in range(2,size):
		U = get_type(T, vsz)
		u = ''.join(dims[:vsz])

		if U in vectors:
			f.constructor(f'{V}', comment='extend vector',
						 args=', '.join([f'{U} {u}'] + [f'{T} {d}' for d in dims[vsz:]]),
						 init_list=', '.join([f'{d}{{{u}.{d}}}' for d in dims[:vsz]] + [f'{d}{{{d}}}' for d in dims[vsz:]]))

	for vsz in range(size+1,vec_sizes[-1]+1):
		U = get_type(T, vsz)
		
		if U in vectors:
			f.constructor(f'{V}', comment='truncate vector',
						 args=f'{U} v',
						 init_list=', '.join(f'{d}{{v.{d}}}' for d in dims[:size]))
		
	f += '\n//// Truncating cast operators\n\n'
	
	for vsz in range(2,size):
		U = get_type(T, vsz)
		vdims = dims[:vsz]
		
		if U in vectors:
			f.method(f'{V}', '', f'operator {U}', '', f'return {U}(%s);' % ', '.join(vdims), const=True, explicit=True, comment="truncating cast operator")
		
	f += '\n//// Type cast operators\n\n'
	for to_vec in other_type_vecs:
		if to_vec in vectors:
			casting_op(to_vec, "type cast operator")
			
	if str(T) != 'bool':
		if BV in vectors:
			f += '\n'

			for op in ('+', '-', '*', '/'):
				compound_binary_op(op, "componentwise arithmetic operator")
		

	f.header += '};\n'
	
	if str(T) == 'bool':
		f += '\n//// reducing ops\n\n'
		
		f.function(f'{T}', 'all', f'{V} v', 'return %s;' % ' && '.join(f'v.{d}' for d in dims), comment='all components are true')
		f.function(f'{T}', 'any', f'{V} v', 'return %s;' % ' || '.join(f'v.{d}' for d in dims), comment='any component is true')
		
		f += '\n//// boolean ops\n\n'
		
		unary_op('!')
		binary_op('&&')
		binary_op('||')
		
		f += '\n//// comparison ops\n\n'
	else:
		f += '\n//// arthmethic ops\n\n'
		unary_op('+')
		if T not in uints:
			unary_op('-')

		binary_op('+')
		binary_op('-')
		binary_op('*')
		binary_op('/')
		
		f += '\n//// bitwise ops\n\n'
		
		if T not in floats:
			unary_op('~')
			binary_op('&')
			binary_op('|')
			binary_op('^')
			binary_op('<<')
			binary_op('>>')
		
		if BV in vectors:
			f += '\n//// comparison ops\n\n'
			comparison_op('<', comment="componentwise comparison returns a bool vector")
			comparison_op('<=', comment="componentwise comparison returns a bool vector")
			comparison_op('>', comment="componentwise comparison returns a bool vector")
			comparison_op('>=', comment="componentwise comparison returns a bool vector")
			
	if BV in vectors:
		comparison_func('equal', '==', comment="componentwise equality comparison, returns a bool vector")
		comparison_func('nequal', '!=', comment="componentwise inequality comparison, returns a bool vector")
		
		f.function(f'bool', 'operator==', f'{V} l, {V} r', f'return %s;' % ' && '.join(f'(l.{d} == r.{d})' for d in dims), comment='full equality comparison, returns true only if all components are equal')
		f.function(f'bool', 'operator!=', f'{V} l, {V} r', f'return %s;' % ' || '.join(f'(l.{d} != r.{d})' for d in dims), comment='full inequality comparison, returns true if any components are inequal')
		
		f.function(f'{V}', 'select', f'{BV} c, {V} l, {V} r', f'return {V}(%s);' % ', '.join(f'c.{d} ? l.{d} : r.{d}' for d in dims),
			comment='componentwise ternary (c ? l : r)')
	
	if str(T) != 'bool':
		f += '\n//// misc ops\n'
		
		if T not in uints:
			unary_func('abs', comment="componentwise absolute")
		
		nary_func('min', ('l', 'r'), comment="componentwise minimum")
		nary_func('max', ('l', 'r'), comment="componentwise maximum")

		f.function(f'{V}', 'clamp', f'{V} x, {V} a, {V} b', f'return min(max(x,a), b);', comment="componentwise clamp into range [a,b]")
		f.function(f'{V}', 'clamp', f'{V} x', f'return min(max(x, {T}(0)), {T}(1));', comment="componentwise clamp into range [0,1] also known as saturate in hlsl")
		
		def minmax_component(mode):
			op = {'min':'<=', 'max':'>='}[mode]
			f.function(f'{T}', f'{mode}_component', f'{V} v, int* {mode}_index=nullptr', f'''
				int index = 0;
				{T} {mode}_val = v.x;	
				for (int i=1; i<{size}; ++i) {{
					if (v.arr[i] {op} {mode}_val) {{
						index = i;
						{mode}_val = v.arr[i];
					}}
				}}
				if ({mode}_index) *{mode}_index = index;
				return {mode}_val;
			''', comment=f'get {mode}imum component of vector, optionally get component index via {mode}_index')

		minmax_component('min')
		minmax_component('max')
		
		f += '\n'
		
		if T in floats:
			unary_func('floor', comment="componentwise floor")
			unary_func('ceil', comment="componentwise ceil")
			unary_func('round', comment="componentwise round")
			
			if IV in vectors:
				unary_func('floori', ret=IV, comment="componentwise floor to int")
				unary_func('ceili', ret=IV, comment="componentwise ceil to int")
				unary_func('roundi', ret=IV, comment="componentwise round to int")

			nary_func('pow', ('v','e'), comment="componentwise pow")

		nary_func('wrap', ('v','range'), comment="componentwise wrap")
		nary_func('wrap', ('v','a','b'), comment="componentwise wrap")

		f += '\n'

		generic_math(V, f)

		f += '\n//// Vector math\n\n'
		
		if T not in uints:
			if FT in scalars:
				f.function(f'{FT}', 'length', f'{V} v',				f'return sqrt(({FT})(%s));' % ' + '.join(f'v.{d} * v.{d}' for d in dims), comment='magnitude of vector')
			
			f.function(f'{T}', 'length_sqr', f'{V} v',			f'return %s;' % ' + '.join(f'v.{d} * v.{d}' for d in dims), comment='squared magnitude of vector, cheaper than length() because it avoids the sqrt(), some algorithms only need the squared magnitude')
			
			if FT in scalars:
				f.function(f'{FT}', 'distance', f'{V} a, {V} b',	f'return length(a - b);', comment='distance between points, equivalent to length(a - b)')
			if FV in vectors:
				f.function(f'{FV}', 'normalize', f'{V} v',			f'return {FV}(v) / length(v);', comment='normalize vector so that it has length() = 1, undefined for zero vector')
			if FV in vectors:
				f.function(f'{FV}', 'normalizesafe', f'{V} v', f'''
					{FT} len = length(v);
					if (len == {FT}(0)) {{
						return {FT}(0);
					}}
					return {FV}(v) / {FV}(len);
				''', comment='normalize vector so that it has length() = 1, returns zero vector if vector was zero vector')
		
			f.function(f'{T}', 'dot', f'{V} l, {V} r', f'return %s;' % ' + '.join(f'l.{d} * r.{d}' for d in dims), comment='dot product')
		
			if size == 3:
				f.function(f'{V}', 'cross', f'{V} l, {V} r', comment='3d cross product', body=f'''
					return {V}(
						l.y * r.z - l.z * r.y,
						l.z * r.x - l.x * r.z,
						l.x * r.y - l.y * r.x);
				''')
				
			elif size == 2:
				f.function(f'{T}', 'cross', f'{V} l, {V} r', 'return l.x * r.y - l.y * r.x;',
					comment='''2d cross product hack for convenient 2d stuff
					same as cross({T.name[:-2]}3(l, 0), {T.name[:-2]}3(r, 0)).z,
					ie. the cross product of the 2d vectors on the z=0 plane in 3d space and then return the z coord of that (signed mag of cross product)''')
			
				f.function(f'{V}', 'rotate90', f'{V} v', f'return {V}(-v.y, v.x);',
					comment=f'rotate 2d vector counterclockwise 90 deg, ie. {V}(-y, x) which is fast')

	f += '}\n'

def gen_matrix(M, f):
	
	T = M.scalar_type
	size = M.size

	mpass = 'const&'

	# standard math way of writing matrix size:
	# size[0] = rows	ie. height
	# size[1] = columns	ie. width

	V = get_type(T, size[0]) # column vectors
	RV = get_type(T, size[1]) # row vectors

	other_size_mats = [m for m in matricies if m.size[0] != size[0] or m.size[1] != size[1] if m.scalar_type == T]
	other_type_mats = [m for m in matricies if m.size[0] == size[0] and m.size[1] == size[1] if m.scalar_type != T]

	forward_decl_vecs = other_size_mats + other_type_mats
	
	for v in set((V, RV)):
		f.header += f'#include "{v.shorthand}.hpp"\n'
		
	f.header += f'\nnamespace {namespace} {{\n\n'

	f.header += '//// matrix forward declarations\n'
	f.header += ''.join(f'struct {n};\n' for n in forward_decl_vecs)

	#
	f.inlined += ''.join(f'#include "{n.shorthand}.hpp"\n' for n in forward_decl_vecs)

	f.inlined += f'\nnamespace {namespace} {{\n\n'

	f.source += ''.join(f'#include "{n.shorthand}.hpp"\n' for n in forward_decl_vecs)

	f.source += f'\nnamespace {namespace} {{\n\n'

	def row_major(cell_format):	return ',\n'.join(', '.join(cell_format.format(c=c,r=r) for c in range(size[1])) for r in range(size[0]))
	def col_major(cell_format):	return ',\n'.join(', '.join(cell_format.format(c=c,r=r) for r in range(size[0])) for c in range(size[1]))
	
	def row_vec_cells(cell_format):	return ',\n'.join(f'{RV}(%s)' % ', '.join(cell_format.format(c=c,r=r) for c in range(size[1])) for r in range(size[0]))
	def col_vec_cells(cell_format):	return ',\n'.join(f'{V}(%s)' % ', '.join(cell_format.format(c=c,r=r) for r in range(size[0])) for c in range(size[1]))

	def row_vecs(vec_format):	return ', '.join(vec_format.format(r=r) for r in range(size[0]))
	def col_vecs(vec_format):	return ', '.join(vec_format.format(c=c) for c in range(size[1]))

	def componentwise(op_format):
		return f'return {M}(%s);' % row_major(op_format)
	
	################
	f.header += f'''
		struct {M} {{
			{V} arr[{size[1]}]; // column major for compatibility with OpenGL
		\n'''
		
	f += '//// Accessors\n\n'
	#src += method(f, f'{M}', f'{T}&', 'get', F'int r, int c', 'return arr[c][r];', comment='get cell with r,c indecies (r=row, c=column)')
	f.method(f'{M}', f'{T} const&', 'get', F'int r, int c', 'return arr[c][r];', const=True, comment='get cell with row, column indecies')

	#f.method(f'{M}', f'{V}&', 'get_column', F'int indx', 'return arr[indx];', comment='get matrix column')
	f.method(f'{M}', f'{V} const&', 'get_column', F'int indx', 'return arr[indx];', const=True, comment='get matrix column')
	
	f.method(f'{M}', f'{RV}', 'get_row', F'int indx', f'return {RV}(%s);' % ', '.join(f'arr[{c}][indx]' for c in range(size[1])),
		const=True, comment='get matrix row')
	
	f += '\n//// Constructors\n\n'
	f.constructor(f'{M}', '', comment='uninitialized constructor', defaulted=True)

	f.constructor(f'{M}', args=f'{T} all',						explicit=True, init_list='\narr{%s}' % col_vec_cells('all'),		comment='supply one value for all cells')
	f.constructor(f'{M}', args=row_major(str(T) +' c{r}{c}'),	explicit=True, init_list='\narr{%s}' % col_vec_cells('c{r}{c}'),	comment='supply all cells, in row major order for readability -> c<row><column>')
	
	f += '\n// static rows() and columns() methods are preferred over constructors, to avoid confusion if column or row vectors are supplied to the constructor\n'
	
	f.static_method(f'{M}', f'{M}', 'rows',	args=row_vecs(str(RV) +' row{r}'), body=f'return {M}(%s);' % row_major('row{r}[{c}]'),
		comment='supply all row vectors')
	f.static_method(f'{M}', f'{M}', 'rows',	args=row_major(str(T) +' c{r}{c}'), body=f'return {M}(%s);' % row_major('c{r}{c}'),
		comment='supply all cells in row major order')

	f.static_method(f'{M}', f'{M}', 'columns',	args=col_vecs(str(V) +' col{c}'), body=f'return {M}(%s);' % row_major('col{c}[{r}]'),
		comment='supply all column vectors')
	f.static_method(f'{M}', f'{M}', 'columns',	args=col_major(str(T) +' c{r}{c}'), body=f'return {M}(%s);' % row_major('c{r}{c}'),
		comment='supply all cells in column major order')

	f += '\n'
	f.static_method(f'{M}', f'{M}', 'identity', args='',
		comment='identity matrix',
		body=f'return {M}(%s);' % ',\n'.join(','.join('1' if x==y else '0' for x in range(size[1])) for y in range(size[0])))
	
	f += '\n// Casting operators\n\n'
	for m in other_size_mats:
		def cell(r,c):
			if r<size[1] and c<size[0]:
				return f'arr[{r}][{c}]'
			else:
				return '        1' if r == c else '        0'

		f.method(f'{M}', '', f'operator {m.name}', '', explicit=True, const=True,
			comment='extend/truncate matrix of other size',
			body=f'return {m.name}(%s);' % ',\n'.join(', '.join(cell(c,r) for c in range(m.size[1])) for r in range(m.size[0])))

	for m in other_type_mats:
		f.method(f'{M}', '', f'operator {m.name}', '', explicit=True, const=True,
			comment='typecast',
			body=f'return {m.name}(%s);' % ',\n'.join(', '.join(f'({m.scalar_type})arr[{r}][{c}]' for c in range(m.size[1])) for r in range(m.size[0])))
	
	f += '\n// Componentwise operators; These might be useful in some cases\n\n'

	f.method(f'{M}', f'{M}&', f'operator+=', f'{T} r', f'*this = *this + r;\nreturn *this;', comment='add scalar to all matrix cells')
	f.method(f'{M}', f'{M}&', f'operator-=', f'{T} r', f'*this = *this - r;\nreturn *this;', comment='substract scalar from all matrix cells')
	f.method(f'{M}', f'{M}&', f'operator*=', f'{T} r', f'*this = *this * r;\nreturn *this;', comment='multiply scalar with all matrix cells')
	f.method(f'{M}', f'{M}&', f'operator/=', f'{T} r', f'*this = *this / r;\nreturn *this;', comment='divide all matrix cells by scalar')
		
	f += '\n// Matrix multiplication\n\n'

	f.method(f'{M}', f'{M}&', 'operator*=', f'{M} {mpass} r', '*this = *this * r;\nreturn *this;', comment='matrix-matrix muliplication')
		
	f.header += '};\n'

	f += '\n// Componentwise operators; These might be useful in some cases\n\n'
	
	# These are probably not needed
	#f.function(f'{M}', 'operator+', f'{M} {mpass} m', componentwise('+m.arr[{c}][{r}]'), comment='all cells positive\n(componentwise unary positive)')
	#f.function(f'{M}', 'operator-', f'{M} {mpass} m', componentwise('-m.arr[{c}][{r}]'), comment='negate all cells\n(componentwise unary negative)')
		
	for op in ['+', '-', '*', '/']:
		f += '\n'

		name = f'operator{op}'
		
		if op == '*':	name = 'mul_componentwise'
		elif op == '/':	name = 'div_componentwise'

		f.function(f'{M}', name, f'{M} {mpass} l, {M} {mpass} r', componentwise(f'l.arr[{{c}}][{{r}}] {op} r.arr[{{c}}][{{r}}]'), comment=f'componentwise matrix_cell {op} matrix_cell')
		f.function(f'{M}', f'operator{op}', f'{M} {mpass} l, {T} r', componentwise(f'l.arr[{{c}}][{{r}}] {op} r'), comment=f'componentwise matrix_cell {op} scalar')
		f.function(f'{M}', f'operator{op}', f'{T} l, {M} {mpass} r', componentwise(f'l {op} r.arr[{{c}}][{{r}}]'), comment=f'componentwise scalar {op} matrix_cell')
	
	f += '\n// Matrix ops\n\n'

	dims = ['x', 'y', 'z', 'w']
	def _mm(l, r):
		nonlocal f
		if not type_exist(T, (l.size[0], r.size[1])):
			f += f'// {l} * {r} -> {l.size[0]}x{r.size[1]} ; matrix not implemented\n'
			return

		ret = get_type(T, (l.size[0], r.size[1])).name
		args = f'{l} {mpass} l, {r} {mpass} r'
		body = f'{ret} ret;\n%s\nreturn ret;' % '\n'.join(f'ret.arr[{c}] = l * r.arr[{c}];' for c in range(r.size[1]))
		comment='matrix-matrix multiply'
		f.function(ret, 'operator*', args, body, comment=comment, constexpr=False)
	def matmul(op):
		nonlocal f
		if op == 'mm':
			if size[0] == size[1]:
				_mm(M, M) # square matricies just implement their own multiplications
			elif size[0] == size[1]-1:
				L = get_type(T, (size[0], size[0]))
				R = get_type(T, (size[1], size[1]))

				# 3x4 implements 3x3 * 3x4 and 3x4 * 4x4
				if L in matricies: _mm(L, M)
				if R in matricies: _mm(M, R)
						
		elif op == 'mv':
			ret = f'{V}'
			args = f'{M} {mpass} l, {RV} r'
			body = f'{V} ret;\n%s\nreturn ret;' % '\n'.join(f'ret[{r}] = %s;' % ' + '.join(f'l.arr[{c}].{dims[r]} * r.{dims[c]}' for c in range(size[1])) for r in range(size[0]))
			comment='matrix-vector multiply'
			f.function(ret, 'operator*', args, body, comment=comment, constexpr=False)
		elif op == 'vm':
			ret = f'{RV}'
			args = f'{V} l, {M} {mpass} r'
			body = f'{RV} ret;\n%s\nreturn ret;' % '\n'.join(f'ret[{c}] = %s;' % ' + '.join(f'l.{dims[r]} * r.arr[{c}].{dims[r]}' for r in range(size[0])) for c in range(size[1]))
			comment='vector-matrix multiply'
			f.function(ret, 'operator*', args, body, comment=comment, constexpr=False)
	def matmul_shortform(op, r):
		nonlocal f
		if r == None:
			return
		if op == 'mm':
			sqr = get_type(T, (size[1],size[1]))
			
			if sqr in matricies:
				f.function(f'{M}', 'operator*', f'{M} {mpass} l, {r} {mpass} r', f'''
					return l * ({sqr})r;
				''', comment=f'shortform for {M} * ({sqr}){r}', constexpr=False)
		elif op == 'mv':
			v = get_type(T, size[1])
		
			if v in vectors:
				f.function(f'{r}', 'operator*', f'{M} {mpass} l, {r} r', f'''
					return l * {v}(r, 1);
				''', comment=f'shortform for {M} * {v}({r}, 1)', constexpr=False)
	
	matmul('mm')
	matmul('mv')
	matmul('vm')
	
	if size[0] == size[1]-1:
		f += f'\n// Matrix operation shortforms so that you can treat a {size[0]}x{size[0]+1} matrix as a {size[0]}x{size[0]} matrix plus translation\n\n'

		QM = get_type(T, (size[0],size[0]))
		if QM in matricies:
			matmul_shortform('mm', QM)

		matmul_shortform('mm', M)

		matmul_shortform('mv', V)

	if type_exist(T, (size[1], size[0])):
		m = get_type(T, (size[1], size[0]))
		f.function(m, 'transpose', f'{M} {mpass} m', f'return {m}::rows(%s);' % ', '.join( f'm.arr[{c}]' for c in range(size[1]) ))

	if size[0] == size[1]:
		f += '\n'

		import matrix_stuff as ms

		f.inlined += ms.define_letterify(T, size[0]) + '\n'
		f.source += ms.define_letterify(T, size[0]) + '\n'

		f.function(f'{T}', 'determinant', f'{M} {mpass} mat', ms.gen_determinant_code(T, size[0]), constexpr=False)
		f.function(f'{M}', 'inverse', f'{M} {mpass} mat', ms.gen_inverse_code(M, T, size[0]), constexpr=False)
		
		f.inlined += '\n#undef LETTERIFY\n\n'
		f.source += '\n#undef LETTERIFY\n\n'

	f += '}\n'

def transform2():
	fM = get_type('float', (2,2))
	fHM = get_type('float', (2,3))
	dM = get_type('double', (2,2))
	dHM = get_type('double', (2,3))
	if fM not in matricies and fHM not in matricies and dM not in matricies and dHM not in matricies:
		return

	f = gen.add_file('transform2d')

	for t in ['float', 'double']:
		V = get_type(t, 2)
		M = get_type(t, (2,2))
		HM = get_type(t, (2,3))
		
		if V in vectors:    f.header += f'#include "{V.shorthand}.hpp"\n'
		if M in matricies:  f.header += f'#include "{M.shorthand}.hpp"\n'
		if HM in matricies: f.header += f'#include "{HM.shorthand}.hpp"\n\n'
		
	f.source += '#include <cmath>\n\n'
	
	f += f'namespace {namespace} {{\n\n'
	
	for t in ['float', 'double']:
		T = get_type(t)
		V = get_type(t, 2)
		M = get_type(t, (2,2))
		HM = get_type(t, (2,3))

		if M in matricies:
			f.function(f'{M}', 'rotate2', f'{T} ang', f'''
				{T} s = std::sin(ang), c = std::cos(ang);
				return {M}(
					 c, -s,
					 s,  c
				);
			''')
			f.function(f'{M}', 'scale', f'{V} v', f'''
				return {M}(
					v.x,   0,
					  0, v.y
				);
			''')
		if HM in matricies:
			f.function(f'{HM}', 'translate', f'{V} v', f'''
				return {HM}(
					1, 0, v.x,
					0, 1, v.y
				);
			''')

		f += '\n'

	f += '}\n'
	
def transform3():
	fM = get_type('float', (3,3))
	fHM = get_type('float', (3,4))
	dM = get_type('double', (3,3))
	dHM = get_type('double', (3,4))
	if fM not in matricies and fHM not in matricies and dM not in matricies and dHM not in matricies:
		return
	
	f = gen.add_file('transform3d')

	for t in ['float', 'double']:
		V = get_type(t, 3)
		M = get_type(t, (3,3))
		HM = get_type(t, (3,4))
		
		if V in vectors:    f.header += f'#include "{V.shorthand}.hpp"\n'
		if M in matricies:  f.header += f'#include "{M.shorthand}.hpp"\n'
		if HM in matricies: f.header += f'#include "{HM.shorthand}.hpp"\n\n'
		
	f.source += '#include <cmath>\n\n'

	f += f'namespace {namespace} {{\n\n'
	
	for t in ['float', 'double']:
		T = get_type(t)
		V = get_type(t, 3)
		M = get_type(t, (3,3))
		HM = get_type(t, (3,4))
	
		if M in matricies: 
			f.function(f'{M}', 'rotate3_X', f'{T} ang', f'''
				{T} s = std::sin(ang), c = std::cos(ang);
				return {M}(
					 1,  0,  0,
					 0,  c, -s,
					 0,  s,  c
				);
			''')
			f.function(f'{M}', 'rotate3_Y', f'{T} ang', f'''
				{T} s = std::sin(ang), c = std::cos(ang);
				return {M}(
					 c,  0,  s,
					 0,  1,  0,
					-s,  0,  c
				);
			''')
			f.function(f'{M}', 'rotate3_Z', f'{T} ang', f'''
				{T} s = std::sin(ang), c = std::cos(ang);
				return {M}(
					 c, -s,  0,
					 s,  c,  0,
					 0,  0,  1
				);
			''')
			f.function(f'{M}', 'scale', f'{V} v', f'''
				return {M}(
					v.x,   0,   0,
					  0, v.y,   0,
					  0,   0, v.z
				);
			''')
		if HM in matricies:  
			f.function(f'{HM}', 'translate', f'{V} v', f'''
				return {HM}(
					1, 0, 0, v.x,
					0, 1, 0, v.y,
					0, 0, 1, v.z
				);
			''')

		f += '\n'

	f += '}\n'
	
########## What we want to generate
#floats =	[get_type(t) for t in ['float', 'double']]
#ints =		[get_type(t) for t in ['int8_t', 'int16_t', 'int', 'int64_t']]
#uints =		[get_type(t) for t in ['uint8_t', 'uint16_t', 'unsigned int', 'uint64_t']]
floats =	[get_type(t) for t in ['float']]
ints =		[get_type(t) for t in ['int', 'int64_t']]
uints =		[get_type(t) for t in ['uint8_t']]

scalars = floats + ints + uints

vec_sizes = [2,3,4]

vectors = [
	[get_type(scalar.name, size) for size in vec_sizes] for scalar in [get_type('bool')] + scalars
]
vectors = list(chain.from_iterable(vectors))

mat_sizes = [(2,2), (3,3), (4,4), (2,3), (3,4)]

matricies = [
	[ get_type('float', s) for s in mat_sizes ],
	#[ get_type('double', s) for s in mat_sizes ],
]
matricies = list(chain.from_iterable(matricies))

all_types = scalars + vectors + matricies

####
output_types = all_types

import sys
if len(sys.argv) > 1:
	output_types = [_types_by_name[t] for t in sys.argv[1:]]
	
	deps = [t.get_deps() for t in output_types]
	deps = list(chain.from_iterable(deps))
	output_types = output_types + deps
	
	output_types = list(set(output_types))
	
	scalars   = [t for t in output_types if t.kind() == 'scalar']
	vectors   = [t for t in output_types if t.kind() == 'vector']
	matricies = [t for t in output_types if t.kind() == 'matrix']

########## Generate all files
import os

dir = os.path.join('output')
gen = srcgen.Generator(dir, default_constexpr=True, default_inline=True)

for s in scalars:
	scalar_math(s, gen.add_file(s.shorthand))

for v in vectors:
	gen_vector(v, gen.add_file(v.shorthand))

for m in matricies:
	gen_matrix(m, gen.add_file(m.shorthand))

transform2()
transform3()

gen.write_files('kissmath.py at <TODO: add github link>')
