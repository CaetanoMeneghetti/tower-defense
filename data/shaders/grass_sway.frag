#version 330 core

in vec2 vTexCoords;
in vec3 vFragPos;
in vec3 vNormal;

uniform sampler2D tex;
uniform vec3  lightDir;      // direção da luz (aponta para a fonte)
uniform vec3  lightAmbient;
uniform vec3  lightDiffuse;
uniform vec3  fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform vec3  viewPos;

out vec4 FragColor;

void main() {
    vec4 texColor = texture(tex, vTexCoords);
    if (texColor.a < 0.1) discard;

    vec3 norm  = normalize(vNormal);
    float diff = max(dot(norm, normalize(lightDir)), 0.0);
    vec3 color = texColor.rgb * (lightAmbient + lightDiffuse * diff);

    float dist = length(viewPos - vFragPos);
    float fog  = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
    color      = mix(color, fogColor, fog);

    FragColor = vec4(color, texColor.a);
}
