#pragma once

#define RZ_COMP_GCC				1
#define RZ_COMP_LLVM			2
#define RZ_COMP_MSVC			3
// Determining the compiler
#if !defined RZ_COMP
	#if _MSC_VER && !__INTELRZ_COMPILER && !__clan_
		#define RZ_COMP RZ_COMP_MSVC
	#elif __GNUC__ && !__clan_
		#define RZ_COMP RZ_COMP_GCC
	#elif __clan_
		#define RZ_COMP RZ_COMP_LLVM
	#else
		#warning Cannot determine compiler!.
	#endif
#endif

#define RZ_ARCH_X64				1
#define RZ_ARCH_ARM_CORTEX_M4	2
#define RZ_ARCH_ARM_V6_HF		3

#define RZ_PLATF_GENERIC_WIN	1
#define RZ_PLATF_GENERIC_UNIX	2
#define RZ_PLATF_NONE			3

#undef FORCEINLINE

#if RZ_COMP == RZ_COMP_MSVC
	#define FORCEINLINE						__forceinline
	#define NOINLINE						__declspec(noinline)
	#define DBGBREAK						__debugbreak()
	
	#define F32_INF							((float)(1e+300 * 1e+300))
	#define F64_INF							(1e+300 * 1e+300)
	#define F32_QNAN						std::numeric_limits<float>::quiet_NaN()
	#define F64_QNAN						std::numeric_limits<double>::quiet_NaN()
	
#elif RZ_COMP == RZ_COMP_LLVM
	#define FORCEINLINE						__attribute__((always_inline)) inline
	#define NOINLINE						__attribute__((noinline))
	#define DBGBREAK						do { asm volatile ("int3"); } while(0)
		
	#define F32_INF							(__builtin_inff())
	#define F64_INF							(__builtin_inf())
	#define F32_QNAN						((float)__builtin_nan("0"))
	#define F64_QNAN						(__builtin_nan("0"))
	
#elif RZ_COMP == RZ_COMP_GCC
	#define FORCEINLINE						__attribute__((always_inline)) inline
	#define NOINLINE						__attribute__((noinline))
	
	#if RZ_PLATF == RZ_PLATF_GENERIC_WIN
		#define DBGBREAK					do { __debugbreak(); } while(0)
	#elif RZ_PLATF == RZ_PLATF_GENERIC_UNIX
		#if RZ_ARCH == RZ_ARCH_ARM_V6_HF
			#define DBGBREAK				do { asm volatile ("bkpt #0"); } while(0)
		#endif
	#endif
	
	#define F32_INF							(__builtin_inff())
	#define F64_INF							(__builtin_inf())
	#define F32_QNAN						((float)__builtin_nan("0"))
	#define F64_QNAN						(__builtin_nan("0"))
	
#endif
