// file was generated by kissmath.py at <TODO: add github link>
#pragma once

////// Forward declarations

#include "float3.hpp"

namespace kissmath {
	
	//// matrix forward declarations
	struct float2x2;
	struct float4x4;
	struct float2x3;
	struct float3x4;
	
	struct float3x3 {
		float3 arr[3]; // column major for compatibility with OpenGL
		
		//// Accessors
		
		// get cell with row, column indecies
		inline constexpr float const& get (int r, int c) const;
		
		// get matrix column
		inline constexpr float3 const& get_column (int indx) const;
		
		// get matrix row
		inline constexpr float3 get_row (int indx) const;
		
		
		//// Constructors
		
		// uninitialized constructor
		inline float3x3 () = default;
		
		// supply one value for all cells
		inline constexpr explicit float3x3 (float all);
		
		// supply all cells, in row major order for readability -> c<row><column>
		inline constexpr explicit float3x3 (float c00, float c01, float c02,
											float c10, float c11, float c12,
											float c20, float c21, float c22);
		
		
		// static rows() and columns() methods are preferred over constructors, to avoid confusion if column or row vectors are supplied to the constructor
		// supply all row vectors
		static inline constexpr float3x3 rows (float3 row0, float3 row1, float3 row2);
		
		// supply all cells in row major order
		static inline constexpr float3x3 rows (float c00, float c01, float c02,
											   float c10, float c11, float c12,
											   float c20, float c21, float c22);
		
		// supply all column vectors
		static inline constexpr float3x3 columns (float3 col0, float3 col1, float3 col2);
		
		// supply all cells in column major order
		static inline constexpr float3x3 columns (float c00, float c10, float c20,
												  float c01, float c11, float c21,
												  float c02, float c12, float c22);
		
		
		// identity matrix
		static inline constexpr float3x3 identity ();
		
		
		// Casting operators
		
		// extend/truncate matrix of other size
		inline constexpr explicit operator float2x2 () const;
		
		// extend/truncate matrix of other size
		inline constexpr explicit operator float4x4 () const;
		
		// extend/truncate matrix of other size
		inline constexpr explicit operator float2x3 () const;
		
		// extend/truncate matrix of other size
		inline constexpr explicit operator float3x4 () const;
		
		
		// Componentwise operators; These might be useful in some cases
		
		// add scalar to all matrix cells
		inline float3x3& operator+= (float r);
		
		// substract scalar from all matrix cells
		inline float3x3& operator-= (float r);
		
		// multiply scalar with all matrix cells
		inline float3x3& operator*= (float r);
		
		// divide all matrix cells by scalar
		inline float3x3& operator/= (float r);
		
		
		// Matrix multiplication
		
		// matrix-matrix muliplication
		inline float3x3& operator*= (float3x3 const& r);
		
	};
	
	// Componentwise operators; These might be useful in some cases
	
	
	// componentwise matrix_cell + matrix_cell
	inline constexpr float3x3 operator+ (float3x3 const& l, float3x3 const& r);
	
	// componentwise matrix_cell + scalar
	inline constexpr float3x3 operator+ (float3x3 const& l, float r);
	
	// componentwise scalar + matrix_cell
	inline constexpr float3x3 operator+ (float l, float3x3 const& r);
	
	
	// componentwise matrix_cell - matrix_cell
	inline constexpr float3x3 operator- (float3x3 const& l, float3x3 const& r);
	
	// componentwise matrix_cell - scalar
	inline constexpr float3x3 operator- (float3x3 const& l, float r);
	
	// componentwise scalar - matrix_cell
	inline constexpr float3x3 operator- (float l, float3x3 const& r);
	
	
	// componentwise matrix_cell * matrix_cell
	inline constexpr float3x3 mul_componentwise (float3x3 const& l, float3x3 const& r);
	
	// componentwise matrix_cell * scalar
	inline constexpr float3x3 operator* (float3x3 const& l, float r);
	
	// componentwise scalar * matrix_cell
	inline constexpr float3x3 operator* (float l, float3x3 const& r);
	
	
	// componentwise matrix_cell / matrix_cell
	inline constexpr float3x3 div_componentwise (float3x3 const& l, float3x3 const& r);
	
	// componentwise matrix_cell / scalar
	inline constexpr float3x3 operator/ (float3x3 const& l, float r);
	
	// componentwise scalar / matrix_cell
	inline constexpr float3x3 operator/ (float l, float3x3 const& r);
	
	
	// Matrix ops
	
	// matrix-matrix multiply
	inline float3x3 operator* (float3x3 const& l, float3x3 const& r);
	
	// matrix-vector multiply
	inline float3 operator* (float3x3 const& l, float3 r);
	
	// vector-matrix multiply
	inline float3 operator* (float3 l, float3x3 const& r);
	
	inline constexpr float3x3 transpose (float3x3 const& m);
	
	
	inline float determinant (float3x3 const& mat);
	
	inline float3x3 inverse (float3x3 const& mat);
	
}


#include "float3x3.inl"