#version 330 core

out vec4 fragColor;

in vec3 FragPos;
in vec2 texCoord;
// Normal é enviado pelo vertex shader compartilhado mas não usado aqui:
// o frag deriva a normal via TBN hardcoded + normal map.

uniform sampler2D dirt;
uniform sampler2D noise;
uniform sampler2D normalMap;
uniform sampler2D aoMap;
uniform sampler2D roughnessMap;
uniform sampler2D displacementMap;
uniform vec3 viewPos;

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

// --- amostragem em world-space: quebra padrões de UV tiling ---
const float DIRT_WORLD_SCALE = 0.55;   // 1 tile a cada ~1.8 unidades mundo

const mat2 ROT1 = mat2( 0.42, -0.91,  0.91,  0.42);  // 65°
const mat2 ROT2 = mat2(-0.71, -0.71,  0.71, -0.71);  // 135°

const float LAYER2_SCALE  = 1.43;
const float LAYER3_SCALE  = 1.87;
const vec2  LAYER2_OFFSET = vec2(0.23, 0.67);
const vec2  LAYER3_OFFSET = vec2(0.61, 0.14);

const float BLEND_NOISE_SCALE1  = 0.09;
const float BLEND_NOISE_SCALE2  = 0.15;
const vec2  BLEND_NOISE_OFFSET2 = vec2(0.35, 0.78);
const float MASK_MIN = 0.35;
const float MASK_MAX = 0.65;

// macro-variação de tonalidade (terra seca vs úmida)
const float MACRO_SCALE  = 0.035;
const vec2  MACRO_OFFSET = vec2(4.2, 1.7);
const vec3  TINT_SECO   = vec3(0.96, 0.92, 0.84);
const vec3  TINT_UMIDO  = vec3(0.84, 0.80, 0.74);
const float MACRO_MIN = 0.3;
const float MACRO_MAX = 0.7;

// --- borda ---
const float CENTER_OFFSET = 0.5;
const float CENTER_SCALE  = 2.0;

const float EDGE_INNER = 0.68;
const float EDGE_OUTER = 1.00;

const float EDGE_NOISE_SCALE     = 4.5;
const vec2  EDGE_NOISE_OFFSET    = vec2(2.1, 4.7);
const float EDGE_NOISE_AMPLITUDE = 0.50;
const float NOISE_ACTIVATION     = 0.25;

const float EDGE_NOISE2_SCALE  = 9.0;
const vec2  EDGE_NOISE2_OFFSET = vec2(5.3, 1.2);
const float EDGE_NOISE2_WEIGHT = 0.25;

const float ALPHA_CUTOFF = 0.05;

vec3 calcDirLight(vec3 norm, vec3 viewDir, vec3 matColor, float ao, float shininess) {
    vec3 lDir     = normalize(light.direction);
    vec3 ambient  = light.ambient * matColor * ao;
    float diff    = max(dot(norm, lDir), 0.0);
    vec3 diffuse  = light.diffuse * diff * matColor;
    vec3 halfDir  = normalize(lDir + viewDir);
    float spec    = pow(max(dot(norm, halfDir), 0.0), shininess);
    vec3 specular = light.specular * spec;
    return ambient + diffuse + specular;
}

void main() {
  // --- textura de terra: 3 camadas rotacionadas em world-space XZ ---
  vec2 wUV  = FragPos.xz * DIRT_WORLD_SCALE;
  vec2 wUV2 = ROT1 * wUV * LAYER2_SCALE + LAYER2_OFFSET;
  vec2 wUV3 = ROT2 * wUV * LAYER3_SCALE + LAYER3_OFFSET;

  float mask1 = smoothstep(MASK_MIN, MASK_MAX, texture(noise, wUV * BLEND_NOISE_SCALE1).r);
  float mask2 = smoothstep(MASK_MIN, MASK_MAX, texture(noise, wUV * BLEND_NOISE_SCALE2 + BLEND_NOISE_OFFSET2).r);

  vec3 dirt1     = texture(dirt, wUV).rgb;
  vec3 dirt2     = texture(dirt, wUV2).rgb;
  vec3 dirt3     = texture(dirt, wUV3).rgb;
  vec3 baseColor = mix(dirt1, dirt2, mask1);
  baseColor      = mix(baseColor, dirt3, mask2);

  // macro-variação de tonalidade
  float macroMask = smoothstep(MACRO_MIN, MACRO_MAX, texture(noise, wUV * MACRO_SCALE + MACRO_OFFSET).r);
  baseColor *= mix(TINT_SECO, TINT_UMIDO, macroMask);

  // --- borda: solo compactado — escurece onde terra encontra grama ---
  float centerDistance = abs(texCoord.x - CENTER_OFFSET) * CENTER_SCALE;
  float edgeBlend      = smoothstep(0.18, 0.82, centerDistance);
  baseColor = mix(baseColor, baseColor * vec3(0.42, 0.40, 0.38), edgeBlend * 0.72);

  // --- fade de opacidade granulado na borda ---
  float edgeNoise     = texture(noise, texCoord * EDGE_NOISE_SCALE  + EDGE_NOISE_OFFSET).r;
  float edgeNoise2    = texture(noise, texCoord * EDGE_NOISE2_SCALE + EDGE_NOISE2_OFFSET).r;
  float combinedNoise = mix(edgeNoise, edgeNoise2, EDGE_NOISE2_WEIGHT);
  float edgeFactor    = smoothstep(NOISE_ACTIVATION, 1.0, centerDistance);
  float perturbedDist = centerDistance + (combinedNoise - 0.5) * edgeFactor * EDGE_NOISE_AMPLITUDE;
  float opacity       = 1.0 - smoothstep(EDGE_INNER, EDGE_OUTER, perturbedDist);

  if (opacity < ALPHA_CUTOFF) discard;

  // --- mapas PBR (world-space) ---
  float height    = texture(displacementMap, wUV).r;
  baseColor *= mix(0.80, 1.06, height);

  float ao        = texture(aoMap, wUV).r;
  float roughness = texture(roughnessMap, wUV).r;
  float shininess = mix(8.0, 1.0, roughness);

  // normal map: plano horizontal, TBN hardcoded (T=+X, B=-Z, N=+Y)
  vec3 tn  = texture(normalMap, wUV).rgb * 2.0 - 1.0;
  mat3 TBN = mat3(
      vec3(1.0, 0.0,  0.0),
      vec3(0.0, 0.0, -1.0),
      vec3(0.0, 1.0,  0.0)
  );
  vec3 norm    = normalize(TBN * tn);
  vec3 viewDir = normalize(viewPos - FragPos);
  vec3 lit = calcDirLight(norm, viewDir, baseColor, ao, shininess)
           + calcPointLights(norm, FragPos, viewDir, baseColor, shininess);
  fragColor = vec4(lit, opacity);
}
