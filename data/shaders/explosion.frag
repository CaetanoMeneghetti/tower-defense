#version 330 core

in  vec2 vUV;
out vec4 FragColor;

uniform sampler2D tex;
uniform float     alpha;  // fade nos últimos frames

void main() {
    vec4 color = texture(tex, vUV);
    if (color.a < 0.05) discard;
    FragColor = vec4(color.rgb, color.a * alpha);
}
