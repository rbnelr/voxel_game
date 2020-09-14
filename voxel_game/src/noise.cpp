#include "noise.hpp"
#include "util/random.hpp"

namespace noise {
	static inline constexpr int HASH_COUNT = 256;
	static inline constexpr int HASH_MASK = 255;

	struct Tables {
		uint8_t hash [HASH_COUNT*2] = {
			251,14,37,129,68,103,87,59,88,80,50,32,236,26,125,12,84,172,252,187,200,74,234,111,17,62,161,238,226,213,192,25,
			114,232,216,156,0,104,127,36,15,7,219,72,130,243,9,167,137,254,183,21,49,109,61,195,6,60,170,94,171,138,163,78,
			199,134,255,16,75,45,112,159,11,140,58,181,93,162,86,148,85,208,191,102,229,22,91,228,105,34,169,193,96,247,250,227,
			246,198,205,76,153,99,48,124,212,40,217,135,196,47,180,144,244,33,241,5,55,230,203,220,29,249,8,54,18,63,133,2,
			113,158,43,101,82,10,136,209,214,30,28,154,95,142,83,67,197,19,70,77,123,31,215,117,131,164,186,23,132,221,69,146,
			106,35,143,239,151,149,160,44,242,1,65,177,121,139,27,157,118,20,57,222,173,24,231,165,175,126,4,240,64,150,189,184,
			122,46,116,235,207,188,237,79,51,52,210,100,253,223,66,218,178,97,42,39,53,81,38,98,248,56,119,90,152,41,92,245,
			206,13,73,179,155,110,224,115,168,145,201,176,147,202,120,194,225,71,107,233,190,108,182,211,166,185,174,128,3,89,141,204,
			
			251,14,37,129,68,103,87,59,88,80,50,32,236,26,125,12,84,172,252,187,200,74,234,111,17,62,161,238,226,213,192,25,
			114,232,216,156,0,104,127,36,15,7,219,72,130,243,9,167,137,254,183,21,49,109,61,195,6,60,170,94,171,138,163,78,
			199,134,255,16,75,45,112,159,11,140,58,181,93,162,86,148,85,208,191,102,229,22,91,228,105,34,169,193,96,247,250,227,
			246,198,205,76,153,99,48,124,212,40,217,135,196,47,180,144,244,33,241,5,55,230,203,220,29,249,8,54,18,63,133,2,
			113,158,43,101,82,10,136,209,214,30,28,154,95,142,83,67,197,19,70,77,123,31,215,117,131,164,186,23,132,221,69,146,
			106,35,143,239,151,149,160,44,242,1,65,177,121,139,27,157,118,20,57,222,173,24,231,165,175,126,4,240,64,150,189,184,
			122,46,116,235,207,188,237,79,51,52,210,100,253,223,66,218,178,97,42,39,53,81,38,98,248,56,119,90,152,41,92,245,
			206,13,73,179,155,110,224,115,168,145,201,176,147,202,120,194,225,71,107,233,190,108,182,211,166,185,174,128,3,89,141,204,
		};

		void generate_hash () {
			uint8_t hash [HASH_COUNT*2];

			for (int i=0; i<HASH_COUNT; ++i) {
				hash[i] = (uint8_t)i;
			}

			auto rand = Random(123456);

			for (int i=0; i<HASH_COUNT-1; ++i) {
				
				std::swap(hash[i], hash[rand.uniform(i+1, HASH_COUNT)]);
			}

			for (int i=0; i<HASH_COUNT; ++i) {
				hash[i + HASH_COUNT] = hash[i];
			}

			printf("uint8_t hash [HASH_COUNT*2] = {");
			for (int i=0; i<HASH_COUNT*2; ++i) {
				if (i % 32 == 0)
					printf("\n\t");
				printf("%d,", hash[i]);
			}
			printf("\n}\n");
		}

		Tables () {
			//generate_hash();
		}
	};

	Tables tbl;

	static inline float smooth (float t) {
		return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	}
	static inline float lerp (float a, float b, float t) {
		//return t*b + (1.0f - t)*a;
		return (b - a) * t + a; // faster?
	}

	static inline constexpr float scale = 1.0f / HASH_MASK * 2;
	static inline constexpr float offs = -1;

	float __vectorcall vnoise (float pos) {
		float fx = floor(pos);
		float tx = pos - fx;
		
		int ix0 = (int)fx;
		ix0 &= HASH_MASK;
		int ix1 = ix0 +1;
		
		int hx0 = tbl.hash[ix0];
		int hx1 = tbl.hash[ix1];

		return lerp((float)hx0, (float)hx1, smooth(tx)) * scale + offs;
	}

	float __vectorcall vnoise (float2 pos) {
		float fx = floor(pos.x);
		float fy = floor(pos.y);
		float tx = pos.x - fx;
		float ty = pos.y - fy;

		int ix0 = (int)fx;
		ix0 &= HASH_MASK;
		int ix1 = ix0 +1;

		int iy0 = (int)fy;
		iy0 &= HASH_MASK;
		int iy1 = iy0 +1;

		int h0 = tbl.hash[ix0];
		int h1 = tbl.hash[ix1];
		int h00 = tbl.hash[h0 + iy0];
		int h10 = tbl.hash[h1 + iy0];
		int h01 = tbl.hash[h0 + iy1];
		int h11 = tbl.hash[h1 + iy1];

		tx = smooth(tx);
		ty = smooth(ty);

		float v0 = lerp((float)h00, (float)h10, tx);
		float v1 = lerp((float)h01, (float)h11, tx);
		float v2 = lerp(v0, v1, ty);

		return v2 * scale + offs;
	}
}
