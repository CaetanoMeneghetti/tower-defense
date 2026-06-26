#version 330 core

in vec2 vTexCoords;
in vec3 vFragPos;
in vec3 vNormal;

uniform sampler2D tex;
uniform vec3  lightDir;
uniform vec3  lightAmbient;
uniform vec3  lightDiffuse;
uniform vec3  fogColor;
uniform float fogStart;
uniform float fogEnd;
uniform vec3  viewPos;

struct PointLight {
    vec3 position;
    vec3 color;
};
const int MAX_POINT_LIGHTS = 20;
uniform int        numPointLights;
uniform PointLight pointLights[MAX_POINT_LIGHTS];

const float PL_KC = 1.0;
const float PL_KL = 0.14;
const float PL_KQ = 0.07;

out vec4 FragColor;

void main() {
    vec4 texColor = texture(tex, vTexCoords);
    if (texColor.a < 0.5) discard;

    vec3 norm  = normalize(vNormal);
    float diff = max(dot(norm, normalize(lightDir)), 0.0);
    vec3 color = texColor.rgb * (lightAmbient + lightDiffuse * diff);

    for (int i = 0; i < numPointLights; ++i) {
        vec3  toLight = pointLights[i].position - vFragPos;
        float dist    = length(toLight);
        vec3  lDir    = toLight / max(dist, 0.0001);
        float att     = 1.0 / (PL_KC + PL_KL * dist + PL_KQ * dist * dist);
        // Grama é double-sided: usa abs para iluminar dos dois lados
        float pDiff   = abs(dot(norm, lDir));
        color += texColor.rgb * pointLights[i].color * pDiff * att;
    }

    float dist = length(viewPos - vFragPos);
    float fog  = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
    color      = mix(color, fogColor, fog);

    FragColor = vec4(color, 1.0);
}
