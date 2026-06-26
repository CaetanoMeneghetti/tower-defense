#version 330 core

out vec4 FragColor;
in vec3 localPos;

uniform sampler2D skyTexture;

const float PI = 3.14159265359;
const vec3 SKY_TINT = vec3(0.03, 0.05, 0.12);

void main() {
    vec3 d = normalize(localPos);

    // Mapeamento esférico
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;
    float v = asin(d.y) / PI + 0.5;

    vec3 col = texture(skyTexture, vec2(u, v)).rgb;
    float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
    float starMask = smoothstep(0.55, 0.90, lum);

    col *= mix(0.45, 0.18, starMask);
    col = mix(col, SKY_TINT, 0.55);

    FragColor = vec4(col, 1.0);
}
