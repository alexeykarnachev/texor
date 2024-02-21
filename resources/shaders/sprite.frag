in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 src;  // (x, y, w, h)

out vec4 finalColor;

void main() {
    vec2 tex_size = vec2(textureSize(texture0, 0));
    vec2 uv = fragTexCoord;
    uv.x = src.x / tex_size.x + uv.x * src.z / tex_size.x;
    uv.y = src.y / tex_size.y + uv.y * src.w / tex_size.y;
    uv *= tex_size;
    uv = vec2(floor(uv.x), ceil(uv.y)) + min(fract(uv) / fwidth(uv), 1.0) - 0.5;
    uv /= tex_size;

    vec4 color = texture(texture0, uv);
    if (color.a < 0.02) discard;
    finalColor = color * fragColor;
}
