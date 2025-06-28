/*
 * OpenSimplex (Simplectic) Noise in C++
 * by Arthur Tombs
 *
 * Modified 2015-01-08
 *
 * This is a derivative work based on OpenSimplex by Kurt Spencer:
 *   https://gist.github.com/KdotJPG/b1270127455a94ac5d19
 *
 * Anyone is free to make use of this software in whatever way they want.
 * Attribution is appreciated, but not required.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include "open_simplex_noise.hpp"

namespace OSN {

	// Array of gradient values for 2D. They approximate the directions to the
	// vertices of a octagon from its center.
	// Gradient set 2014-10-06.
	const int Noise<2>::gradients [] = {
		5, 2,   2, 5,  -5, 2,  -2, 5,
		5,-2,   2,-5,  -5,-2,  -2,-5
	};

	// Array of gradient values for 3D. They approximate the directions to the
	// vertices of a rhombicuboctahedron from its center, skewed so that the
	// triangular and square facets can be inscribed in circles of the same radius.
	// New gradient set 2014-10-06.
	const int Noise<3>::gradients [] = {
		-11, 4, 4,  -4, 11, 4,  -4, 4, 11,   11, 4, 4,   4, 11, 4,   4, 4, 11,
		-11,-4, 4,  -4,-11, 4,  -4,-4, 11,   11,-4, 4,   4,-11, 4,   4,-4, 11,
		-11, 4,-4,  -4, 11,-4,  -4, 4,-11,   11, 4,-4,   4, 11,-4,   4, 4,-11,
		-11,-4,-4,  -4,-11,-4,  -4,-4,-11,   11,-4,-4,   4,-11,-4,   4,-4,-11
	};

	// Array of gradient values for 4D. They approximate the directions to the
	// vertices of a disprismatotesseractihexadecachoron from its center, skewed so that the
	// tetrahedral and cubic facets can be inscribed in spheres of the same radius.
	// Gradient set 2014-10-06.
	const int Noise<4>::gradients [] = {
		3, 1, 1, 1,   1, 3, 1, 1,   1, 1, 3, 1,   1, 1, 1, 3,
		-3, 1, 1, 1,  -1, 3, 1, 1,  -1, 1, 3, 1,  -1, 1, 1, 3,
		3,-1, 1, 1,   1,-3, 1, 1,   1,-1, 3, 1,   1,-1, 1, 3,
		-3,-1, 1, 1,  -1,-3, 1, 1,  -1,-1, 3, 1,  -1,-1, 1, 3,
		3, 1,-1, 1,   1, 3,-1, 1,   1, 1,-3, 1,   1, 1,-1, 3,
		-3, 1,-1, 1,  -1, 3,-1, 1,  -1, 1,-3, 1,  -1, 1,-1, 3,
		3,-1,-1, 1,   1,-3,-1, 1,   1,-1,-3, 1,   1,-1,-1, 3,
		-3,-1,-1, 1,  -1,-3,-1, 1,  -1,-1,-3, 1,  -1,-1,-1, 3,
		3, 1, 1,-1,   1, 3, 1,-1,   1, 1, 3,-1,   1, 1, 1,-3,
		-3, 1, 1,-1,  -1, 3, 1,-1,  -1, 1, 3,-1,  -1, 1, 1,-3,
		3,-1, 1,-1,   1,-3, 1,-1,   1,-1, 3,-1,   1,-1, 1,-3,
		-3,-1, 1,-1,  -1,-3, 1,-1,  -1,-1, 3,-1,  -1,-1, 1,-3,
		3, 1,-1,-1,   1, 3,-1,-1,   1, 1,-3,-1,   1, 1,-1,-3,
		-3, 1,-1,-1,  -1, 3,-1,-1,  -1, 1,-3,-1,  -1, 1,-1,-3,
		3,-1,-1,-1,   1,-3,-1,-1,   1,-1,-3,-1,   1,-1,-1,-3,
		-3,-1,-1,-1,  -1,-3,-1,-1,  -1,-1,-3,-1,  -1,-1,-1,-3
	};
}
