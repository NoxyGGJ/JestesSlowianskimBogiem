shader_type canvas_item;
render_mode blend_premul_alpha;

void vertex() { }

float hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float value_noise(vec2 uv) {
    ivec2 i = ivec2(floor(uv));
    vec2 f = fract(uv);

    // Four corners
    float a = hash(vec2(i));
    float b = hash(vec2(i + ivec2(1, 0)));
    float c = hash(vec2(i + ivec2(0, 1)));
    float d = hash(vec2(i + ivec2(1, 1)));

    // Smooth interpolation (Perlin-style fade)
    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(a, b, u.x),
        mix(c, d, u.x),
        u.y
    );
}
float calc_component(vec2 uv, vec2 motion)
{
    vec2 scale = vec2(8.0, 4.0);
    float n = value_noise(uv * scale + motion);
    return smoothstep(0.0, 1.0, n);
}
void fragment() {
	vec3 val = vec3(
		calc_component(SCREEN_UV, vec2(TIME * 0.25, TIME * 0.20)),
		calc_component(SCREEN_UV, vec2(TIME * 0.23, -TIME * 0.25)),
		calc_component(SCREEN_UV, vec2(-TIME * 0.22, TIME * 0.19)));
    vec3 color = val * val * val;
    COLOR = vec4(color * 0.25, 0.0);
}
