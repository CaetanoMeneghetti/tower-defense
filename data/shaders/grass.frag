#version 330 core

out vec4 fragColor;

in vec2 texCoord;
in vec3 FragPos;

uniform sampler2D grass;
uniform sampler2D noise;
uniform sampler2D normalMap;
uniform sampler2D aoMap;
uniform sampler2D roughnessMap;
uniform sampler2D displacementMap;
uniform vec3 viewPos;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

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

const float PL_KC = 1.0;
const float PL_KL = 0.22;
const float PL_KQ = 0.20;

vec3 calcPointLights(vec3 norm, vec3 fragPos, vec3 viewDir, vec3 matColor, float shininess) {
    vec3 result = vec3(0.0);
    for (int i = 0; i < numPointLights; ++i) {
        vec3 toLight = pointLights[i].position - fragPos;
        float dist = length(toLight);
        vec3 lDir  = toLight / max(dist, 0.0001);
        float att  = 1.0 / (PL_KC + PL_KL * dist + PL_KQ * dist * dist);
        float diff = max(dot(norm, lDir), 0.0);
        vec3 halfDir = normalize(lDir + viewDir);
        float spec = pow(max(dot(norm, halfDir), 0.0), shininess);
        result += (diff * matColor + spec * vec3(0.2)) * pointLights[i].color * att;
    }
    return result;
}

// --- blending de múltiplas camadas de grama ---
const mat2 ROT1 = mat2(0.39, -0.92, 0.92, 0.39);
const mat2 ROT2 = mat2(-0.65, -0.75, 0.75, -0.65);

const float UV_SCALE_1 = 1.31;
const float UV_SCALE_2 = 1.73;

const vec2 UV_OFFSET_1 = vec2(0.17, 0.82);
const vec2 UV_OFFSET_2 = vec2(0.53, 0.29);

const float NOISE_SCALE_1  = 0.04;
const float NOISE_SCALE_2  = 0.07;
const vec2  NOISE_OFFSET_2 = vec2(0.4, 0.1);

const float MASK_MIN = 0.3;
const float MASK_MAX = 0.7;

const float MACRO_NOISE_SCALE  = 0.013;
const vec2  MACRO_NOISE_OFFSET = vec2(7.3, 3.1);

const vec3  TINT_SECO = vec3(0.96, 0.94, 0.83);
const vec3  TINT_VIVO = vec3(0.94, 1.02, 0.92);
const float MACRO_MIN = 0.35;
const float MACRO_MAX = 0.65;

void main() {
  // --- cor base: blending de 3 camadas com noise ---
  vec3 grass1 = texture(grass, texCoord).rgb;

  vec2 uv2    = ROT1 * texCoord * UV_SCALE_1 + UV_OFFSET_1;
  vec3 grass2 = texture(grass, uv2).rgb;

  vec2 uv3    = ROT2 * texCoord * UV_SCALE_2 + UV_OFFSET_2;
  vec3 grass3 = texture(grass, uv3).rgb;

  float mask1 = smoothstep(MASK_MIN, MASK_MAX, texture(noise, texCoord * NOISE_SCALE_1).r);
  float mask2 = smoothstep(MASK_MIN, MASK_MAX, texture(noise, texCoord * NOISE_SCALE_2 + NOISE_OFFSET_2).r);

  vec3 baseColor = mix(grass1, grass2, mask1);
  baseColor      = mix(baseColor, grass3, mask2);

  float macroMask = smoothstep(MACRO_MIN, MACRO_MAX,
      texture(noise, texCoord * MACRO_NOISE_SCALE + MACRO_NOISE_OFFSET).r);
  baseColor *= mix(TINT_SECO, TINT_VIVO, macroMask);

  // --- mapas PBR ---

  // Displacement: pontas da grama (height=1) mais claras, base (height=0) mais escura
  float height = texture(displacementMap, texCoord).r;
  baseColor *= mix(0.78, 1.08, height);

  float ao        = texture(aoMap, texCoord).r;
  float roughness = texture(roughnessMap, texCoord).r;
  float shininess = mix(16.0, 1.0, roughness);

  // --- normal map: espaço tangente → espaço mundo ---
  // Plano horizontal: T aponta +X (direção U), B aponta -Z (direção V), N aponta +Y
  vec3 tn  = texture(normalMap, texCoord).rgb * 2.0 - 1.0;
  mat3 TBN = mat3(
      vec3(1.0, 0.0,  0.0),   // tangente
      vec3(0.0, 0.0, -1.0),   // bitangente
      vec3(0.0, 1.0,  0.0)    // normal
  );
  vec3 norm    = normalize(TBN * tn);
  vec3 viewDir = normalize(viewPos - FragPos);

  // --- Blinn-Phong com AO só no ambiente ---
  vec3 lDir     = normalize(light.direction);
  vec3 ambient  = light.ambient * baseColor * ao;  // oclusão só afeta luz indireta
  float diff    = max(dot(norm, lDir), 0.0);
  vec3 diffuse  = light.diffuse * diff * baseColor;
  vec3 halfDir  = normalize(lDir + viewDir);
  float spec    = pow(max(dot(norm, halfDir), 0.0), shininess);
  vec3 specular = light.specular * spec;

    vec3 litColor = ambient + diffuse + specular
                  + calcPointLights(norm, FragPos, viewDir, baseColor, shininess);
    float dist = length(viewPos - FragPos);
    float fogFactor = clamp((fogEnd - dist) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 finalColor = mix(fogColor, litColor, fogFactor);
    fragColor = vec4(finalColor, 1.0);
}
