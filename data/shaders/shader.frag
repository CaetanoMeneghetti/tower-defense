#version 330 core

out vec4 fragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 texCoord;

uniform sampler2D tex;
uniform vec3 viewPos;

// Luz direcional (sol/lua)
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight light;

// Point lights (lanternas)
struct PointLight {
    vec3 position;
    vec3 color;
};
const int MAX_POINT_LIGHTS = 20;
uniform int numPointLights;
uniform PointLight pointLights[MAX_POINT_LIGHTS];

// Fog uniforms
uniform float fogStart;
uniform float fogEnd;
uniform vec3 fogColor;

const float SHININESS = 32.0;

const float PL_KC = 1.0;
const float PL_KL = 0.22;
const float PL_KQ = 0.20;

vec3 calcDirLight(vec3 norm, vec3 viewDir, vec3 matColor) {
    vec3 lDir     = normalize(light.direction);
    vec3 ambient  = light.ambient * matColor;
    float diff    = max(dot(norm, lDir), 0.0);
    vec3 diffuse  = light.diffuse * diff * matColor;
    vec3 halfDir  = normalize(lDir + viewDir);
    float spec    = pow(max(dot(norm, halfDir), 0.0), SHININESS);
    vec3 specular = light.specular * spec;
    return ambient + diffuse + specular;
}

vec3 calcPointLights(vec3 norm, vec3 fragPos, vec3 viewDir, vec3 matColor) {
    vec3 result = vec3(0.0);
    for (int i = 0; i < numPointLights; ++i) {
        vec3 toLight = pointLights[i].position - fragPos;
        float dist = length(toLight);
        vec3 lDir  = toLight / max(dist, 0.0001);
        float att  = 1.0 / (PL_KC + PL_KL * dist + PL_KQ * dist * dist);
        float diff = max(dot(norm, lDir), 0.0);
        vec3 halfDir = normalize(lDir + viewDir);
        float spec = pow(max(dot(norm, halfDir), 0.0), SHININESS);
        result += (diff * matColor + spec * vec3(0.25)) * pointLights[i].color * att;
    }
    return result;
}

void main() {
    vec4 texColor = texture(tex, texCoord);
    vec3 norm     = normalize(Normal);
    vec3 viewDir  = normalize(viewPos - FragPos);

    vec3 resultColor = calcDirLight(norm, viewDir, texColor.rgb)
                     + calcPointLights(norm, FragPos, viewDir, texColor.rgb);

    // Alpha test para as folhas
    if (texColor.a < 0.1) discard;

    // Cálculo do fog
    float dist = length(viewPos - FragPos);
    float fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 finalColor = mix(resultColor, fogColor, fogFactor);

    fragColor = vec4(finalColor, texColor.a);
}
