
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
