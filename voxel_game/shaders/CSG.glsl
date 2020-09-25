
// Based on https://www.iquilezles.org/www/articles/smin/smin.htm:
// polynomial smooth min (k = 0.1);
float smin (float a, float b, float k)	{
	float h = max(k - abs(a - b), 0.0) / k;
	return min(a, b) - h*h*k * 0.25;
}
float smax (float a, float b, float k)	{
	float h = max(k - abs(a - b), 0.0) / k;
	return max(a, b) + h*h*k * 0.25;
}

float sphere (vec3 p, vec3 center, float r) {
	p -= center;
	return length(p) - r;
}
float ellipse (vec3 p, vec3 center, vec3 r) {
	// https://www.iquilezles.org/www/articles/ellipsoids/ellipsoids.htm
	p -= center;
	p /= r;
	return (length(p) - 1.0) * min(min(r.x,r.y),r.z);
}

float capsule (vec3 p, vec3 base, vec3 dir, float h, float r) {
	p -= base;
	float t = dot(p, dir);
	t = clamp(t, 0.0, h);

	vec3 proj = dir * t;
	return length(p - proj) - r;
}
float capsule2 (vec3 p, vec3 a, vec3 b, float r) {
	return capsule(p, a, normalize(b-a), length(b-a), r);
}


float sphereGrad (vec3 p, vec3 center, float r, out vec3 grad) {
	p -= center;
	grad = normalize(p);
	return length(p) - r;
}