// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Input uniform values
uniform mat4 mvp;
uniform mat4 matModel;

// Output vertex attributes (to fragment shader)
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;
out vec4 directional_lights_pos[16];

void main() {
    // Send vertex attributes to fragment shader
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPosition = vertexPosition;

    mat3 normalMatrix = transpose(inverse(mat3(matModel)));
    fragNormal = normalMatrix * vertexNormal;

    // Calculate final vertex position
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
