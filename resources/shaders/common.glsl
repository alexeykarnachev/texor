struct AmbientLight {
    vec3 color;
    float intensity;
};

struct DirectionalLight {
    vec3 color;
    float intensity;

    vec3 direction;

    mat4 vp;
};

struct PointLight {
    vec3 color;
    float intensity;

    vec3 position;
    vec3 attenuation;
};

uniform int n_ambient_lights;
uniform AmbientLight ambient_lights[16];

uniform int n_directional_lights;
uniform DirectionalLight directional_lights[16];
uniform sampler2D directional_lights_shadow_map[16];

uniform int n_point_lights;
uniform PointLight point_lights[16];

