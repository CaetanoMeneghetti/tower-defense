#version 330 core

out vec4 fragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 texCoord;

uniform sampler2D colorMap;
uniform sampler2D normalMap;
uniform sampler2D roughnessMap;
uniform sampler2D metallicMap;
uniform sampler2D aoMap;
uniform sampler2D opacityMap;

uniform vec3 viewPos;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight light;

struct PointLight {
    vec3 position;
    vec3 color;
};
const int MAX_POINT_LIGHTS = 20;
uniform int numPointLights;
uniform PointLight pointLights[MAX_POINT_LIGHTS];

uniform float fogStart;
uniform float fogEnd;
uniform vec3 fogColor;

// Cutoff alto + filtro nearest em opacityMap dão alpha-test estável em mipmaps.
const float ALPHA_CUTOFF = 0.5;

const float PL_KC = 1.0;
const float PL_KL = 0.22;
const float PL_KQ = 0.20;

vec3 calcPointLights(vec3 norm, vec3 fragPos, vec3 viewDir, vec3 matColor,
                     float metallic, float shininess) {
    vec3 result  = vec3(0.0);
    vec3 specTint = mix(vec3(1.0), matColor, metallic);
    for (int i = 0; i < numPointLights; ++i) {
        vec3 toLight = pointLights[i].position - fragPos;
        float dist = length(toLight);
        vec3 lDir  = toLight / max(dist, 0.0001);
        float att  = 1.0 / (PL_KC + PL_KL * dist + PL_KQ * dist * dist);

        float diff = max(dot(norm, lDir), 0.0);
        vec3 halfDir = normalize(lDir + viewDir);
        float spec = pow(max(dot(norm, halfDir), 0.0), shininess);

        vec3 d = diff * matColor * (1.0 - metallic);
        vec3 s = spec * specTint;
        result += (d + s) * pointLights[i].color * att;
    }
    return result;
}

void main() {
    vec4 baseColor = texture(colorMap, texCoord);
    float opacity  = texture(opacityMap, texCoord).r;
    if (opacity < ALPHA_CUTOFF) discard;

    // Sem normal map: a TBN screen-space desestabiliza em ângulos rasos /
    // UVs degeneradas no modelo, produzindo manchas. Vertex normal é robusto.
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);

    float roughness = texture(roughnessMap, texCoord).r;
    float metallic  = texture(metallicMap, texCoord).r;
    float ao        = texture(aoMap, texCoord).r;
    float shininess = mix(8.0, 128.0, 1.0 - roughness);

    vec3 lDir  = normalize(light.direction);
    vec3 H     = normalize(lDir + V);
    float diff = max(dot(N, lDir), 0.0);
    float spec = pow(max(dot(N, H), 0.0), shininess);

    vec3 specTint = mix(vec3(1.0), baseColor.rgb, metallic);
    vec3 ambient  = light.ambient  * baseColor.rgb * ao;
    vec3 diffuse  = light.diffuse  * diff * baseColor.rgb * (1.0 - metallic);
    vec3 specular = light.specular * spec * specTint;

    vec3 directional = ambient + diffuse + specular;
    vec3 fromLanterns = calcPointLights(N, FragPos, V, baseColor.rgb, metallic, shininess);
    vec3 resultColor = directional + fromLanterns;

    float dist = length(viewPos - FragPos);
    float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 finalColor = mix(resultColor, fogColor, fogFactor);

    fragColor = vec4(finalColor, opacity);
}
