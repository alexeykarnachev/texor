in vec3 fragPosition;

out vec4 fragColor;

uniform vec2 u_light_pos;
uniform float u_radius;

#define BRICK_WIDTH  0.25
#define BRICK_HEIGHT 0.08
#define MORTAR_THICKNESS 0.05
#define BM_WIDTH  (BRICK_WIDTH + MORTAR_THICKNESS)
#define BM_HEIGHT (BRICK_HEIGHT + MORTAR_THICKNESS)
#define MWF (MORTAR_THICKNESS * 0.5 / BM_WIDTH)
#define MHF (MORTAR_THICKNESS * 0.5 / BM_HEIGHT)
#define BRICK_COLOR vec3(0.5, 0.5, 0.14)
#define MORTAR_COLOR vec3(0.4, 0.5, 0.3)
#define PI 3.141592654


vec3 LIGHT_POS = vec3(0.0, 0.0, 2.0);
const vec3 LIGHT_COLOR = vec3(0.6, 0.6, 0.5);
const vec3 AMBIENT_COLOR = vec3(0.0, 0.0, 0.0);

vec3 CalculatePos(vec3 pos, vec3 normal, vec2 uv);
vec3 CalculateNormal(vec3 pos, vec3 normal);
vec3 Shader(vec3 pos, vec3 normal, vec3 diffuse_color);
float Hash(vec2 p);
float Noise(vec2 p);
float FractalSum(vec2 uv);

vec3 CalculatePos(vec3 pos, vec3 normal, vec2 uv) {
    float s = uv.x;
    float t = uv.y;
    float sbump = smoothstep(0.0, MWF, s) - smoothstep(1.0-MWF, 1.0, s);
    float tbump = smoothstep(0.0, MHF, t) - smoothstep(1.0-MHF, 1.0, t);
    float stbump = sbump * tbump;
    return pos + normal * stbump + stbump * normal * FractalSum(uv) * 0.25;
}

vec3 CalculateNormal(vec3 pos, vec3 normal) {
    vec3 dx = dFdx(pos);
    vec3 dy = dFdy(pos);
    return normalize(cross(dx, dy));
}

vec3 Shader(vec3 pos, vec3 normal, vec3 diffuse_color) {
    vec3 dir = normalize(LIGHT_POS - pos);
    vec3 diffuse = abs(dot(dir, normal)) * LIGHT_COLOR * diffuse_color;
    vec3 color = AMBIENT_COLOR + diffuse;
    return color;
}

float Hash(vec2 p) {
    float h = dot(p, vec2(17.1, 311.7));
    return -1.0 + 2.0 * fract(sin(h) * 4358.5453);
}

float Noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    
    return mix(mix(Hash(i + vec2(0.0, 0.0)),
                   Hash(i + vec2(1.0, 0.0)), u.x),
               mix(Hash(i + vec2(0.0, 1.0)),
                   Hash(i + vec2(1.0, 1.0)), u.x), u.y);
 
}

float FractalSum(vec2 uv) {
    const int octaves = 1;
    float amplitude = 1.0;
    float f = 0.0;
    
    uv *= 25.0;
    mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
    for (int i = 0; i < octaves; ++ i) {
        f += abs(amplitude * Noise(uv));
        uv = m * uv;
        amplitude *= 0.5;
    }
    return f;
}

void main() {
	vec2 uv = fragPosition.xy / (1.2 * u_radius);
    vec2 pos_xy = fragPosition.xy;
    vec3 pos = vec3(pos_xy , 0.0);
    vec3 normal = vec3(0.0, 0.0, -1.0);
 
    LIGHT_POS += vec3(u_light_pos.x, u_light_pos.y, 0.0);
    
    float brick_u = uv.x / BM_WIDTH;
    float brick_v = uv.y / BM_HEIGHT;
    
    if (mod(brick_v, 2.0) > 1.0)
        brick_u += 0.5;
    
    brick_u -= floor(brick_u);
    brick_v -= floor(brick_v);
    brick_u += FractalSum(uv) * 0.005;
    brick_v += FractalSum(vec2(brick_u, brick_v)) * 0.005;

    float w = step(MWF, brick_u) - step(1.0-MWF, brick_u);
    float th = step(MHF, brick_v) - step(1.0-MHF, brick_v);
    vec3 color = mix(MORTAR_COLOR, BRICK_COLOR, w * th);
    
    vec3 replacement_pos = CalculatePos(pos, normal, vec2(brick_u, brick_v));
    vec3 replacement_normal = CalculateNormal(replacement_pos, normal);
    float shadow = 1.0 - smoothstep(0.85, 1.0, length(pos_xy) / u_radius);
    fragColor = shadow * vec4(Shader(replacement_pos, replacement_normal, color), 1.0);
}
