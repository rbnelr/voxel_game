
#ifdef NOISE_USE_TIME
	uniform float time;

	float seed = time + 1.0;
#else
	float seed = 1.0;
#endif

//// https://stackoverflow.com/questions/4200224/random-noise-functions-for-glsl
// Gold Noise ©2015 dcerisano@standard3d.com
// - based on the Golden Ratio
// - uniform normalized distribution
// - fastest static noise generator function (also runs at low precision)

float PHI = 1.61803398874989484820459;  // Φ = Golden Ratio

float gold_noise(in vec2 xy, in float seed){
	return fract(tan(distance(xy*PHI, xy)*seed)*xy.x);
}
///

float rand () {
	return gold_noise(gl_FragCoord.xy, seed++);
}
vec2 rand2 () {
	return vec2(rand(), rand());
}
vec3 rand3 () {
	return vec3(rand(), rand(), rand());
}
