#version 330 core

out vec4 fragColor;

in vec3 FragPos;
in vec2 texCoord;

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

struct PointLight {  // lanternas
    vec3 position;
    vec3 color;
};
const int MAX_POINT_LIGHTS = 20;
uniform int numPointLights;
uniform PointLight pointLights[MAX_POINT_LIGHTS];

// ----------------------------------------------------------------------------
// Parâmetros
// ----------------------------------------------------------------------------
// Atenuação e brilho das point lights (constante, linear, quadrática).
const float PL_KC = 1.0;
const float PL_KL = 0.22;
const float PL_KQ = 0.20;
const float PL_SPEC = 0.2;  // intensidade do specular das lanternas

// Detiling: a terra é amostrada 3x em world-space XZ com rotações/escalas
// distintas e misturada por ruído. Nenhuma camada é alinhada aos eixos, então
// não aparece linha reta de repetição do tile.
const float DIRT_WORLD_SCALE = 0.55;   // 1 tile a cada ~1.8 unidades de mundo

const mat2 ROT0 = mat2( 0.94, -0.34,  0.34,  0.94);  // 20°
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

// Macro-variação de tonalidade (terra seca vs. úmida).
const float MACRO_SCALE = 0.035;
const vec2  MACRO_OFFSET = vec2(4.2, 1.7);
const vec3  TINT_SECO   = vec3(0.96, 0.92, 0.84);
const vec3  TINT_UMIDO  = vec3(0.84, 0.80, 0.74);
const float MACRO_MIN = 0.3;
const float MACRO_MAX = 0.7;

// Borda. centerDist = distância ao eixo do caminho (0 no centro, 1 na borda).
const float EDGE_DARKEN_MIN = 0.18;        // escurecimento de solo compactado
const float EDGE_DARKEN_MAX = 0.82;
const float EDGE_DARKEN_STRENGTH = 0.72;
const vec3  EDGE_DARKEN_TINT = vec3(0.42, 0.40, 0.38);

const float EDGE_INNER = 0.68;             // fade de opacidade terra -> grama
const float EDGE_OUTER = 1.00;
const float EDGE_NOISE_SCALE     = 4.5;    // granulado da borda
const vec2  EDGE_NOISE_OFFSET    = vec2(2.1, 4.7);
const float EDGE_NOISE_AMPLITUDE = 0.50;
const float NOISE_ACTIVATION     = 0.25;
const float EDGE_NOISE2_SCALE  = 9.0;
const vec2  EDGE_NOISE2_OFFSET = vec2(5.3, 1.2);
const float EDGE_NOISE2_WEIGHT = 0.25;
const float ALPHA_CUTOFF = 0.05;

// PBR.
const float HEIGHT_DARK   = 0.80;  // brilho nos vales / cristas do displacement
const float HEIGHT_BRIGHT = 1.06;
const float SHININESS_ROUGH  = 1.0;
const float SHININESS_SMOOTH = 8.0;

// ----------------------------------------------------------------------------

vec3 calcDirLight(vec3 norm, vec3 viewDir, vec3 matColor, float ao, float shininess) {
    vec3  lDir    = normalize(light.direction);
    vec3  halfDir = normalize(lDir + viewDir);
    float diff    = max(dot(norm, lDir), 0.0);
    float spec    = pow(max(dot(norm, halfDir), 0.0), shininess);
    vec3  ambient = light.ambient * matColor * ao;
    return ambient + light.diffuse * diff * matColor + light.specular * spec;
}

vec3 calcPointLights(vec3 norm, vec3 fragPos, vec3 viewDir, vec3 matColor, float shininess) {
    vec3 result = vec3(0.0);
    for (int i = 0; i < numPointLights; ++i) {
        vec3  toLight = pointLights[i].position - fragPos;
        float dist    = length(toLight);
        vec3  lDir    = toLight / max(dist, 0.0001);
        float att     = 1.0 / (PL_KC + PL_KL * dist + PL_KQ * dist * dist);
        vec3  halfDir = normalize(lDir + viewDir);
        float diff    = max(dot(norm, lDir), 0.0);
        float spec    = pow(max(dot(norm, halfDir), 0.0), shininess);
        result += (diff * matColor + spec * vec3(PL_SPEC)) * pointLights[i].color * att;
    }
    return result;
}

void main() {
  // --- terra: 3 camadas rotacionadas em world-space, misturadas por ruído ---
  vec2 wUV  = FragPos.xz * DIRT_WORLD_SCALE;
  vec2 wUV1 = ROT0 * wUV;
  vec2 wUV2 = ROT1 * wUV * LAYER2_SCALE + LAYER2_OFFSET;
  vec2 wUV3 = ROT2 * wUV * LAYER3_SCALE + LAYER3_OFFSET;

  float mask1 = smoothstep(MASK_MIN, MASK_MAX, texture(noise, wUV * BLEND_NOISE_SCALE1).r);
  float mask2 = smoothstep(MASK_MIN, MASK_MAX, texture(noise, wUV * BLEND_NOISE_SCALE2 + BLEND_NOISE_OFFSET2).r);

  vec3 baseColor = mix(texture(dirt, wUV1).rgb, texture(dirt, wUV2).rgb, mask1);
  baseColor      = mix(baseColor, texture(dirt, wUV3).rgb, mask2);

  // macro-variação de tonalidade
  float macroMask = smoothstep(MACRO_MIN, MACRO_MAX, texture(noise, wUV * MACRO_SCALE + MACRO_OFFSET).r);
  baseColor *= mix(TINT_SECO, TINT_UMIDO, macroMask);

  // --- borda: solo compactado escurece onde a terra encontra a grama ---
  float centerDist = abs(texCoord.x - 0.5) * 2.0;
  float darken     = smoothstep(EDGE_DARKEN_MIN, EDGE_DARKEN_MAX, centerDist);
  baseColor = mix(baseColor, baseColor * EDGE_DARKEN_TINT, darken * EDGE_DARKEN_STRENGTH);

  // --- fade de opacidade granulado na borda ---
  float edgeNoise     = texture(noise, texCoord * EDGE_NOISE_SCALE  + EDGE_NOISE_OFFSET).r;
  float edgeNoise2    = texture(noise, texCoord * EDGE_NOISE2_SCALE + EDGE_NOISE2_OFFSET).r;
  float combinedNoise = mix(edgeNoise, edgeNoise2, EDGE_NOISE2_WEIGHT);
  float edgeFactor    = smoothstep(NOISE_ACTIVATION, 1.0, centerDist);
  float perturbedDist = centerDist + (combinedNoise - 0.5) * edgeFactor * EDGE_NOISE_AMPLITUDE;
  float opacity       = 1.0 - smoothstep(EDGE_INNER, EDGE_OUTER, perturbedDist);

  if (opacity < ALPHA_CUTOFF) discard;

  // --- mapas PBR (world-space) ---
  float height    = texture(displacementMap, wUV).r;
  float ao        = texture(aoMap, wUV).r;
  float roughness = texture(roughnessMap, wUV).r;
  float shininess = mix(SHININESS_SMOOTH, SHININESS_ROUGH, roughness);
  baseColor *= mix(HEIGHT_DARK, HEIGHT_BRIGHT, height);

  // normal map num plano horizontal: TBN fixo (T=+X, B=-Z, N=+Y) aplicado
  // direto como swizzle, equivalente a mat3(T,B,N) * tn.
  vec3 tn   = texture(normalMap, wUV).rgb * 2.0 - 1.0;
  vec3 norm = normalize(vec3(tn.x, tn.z, -tn.y));

  vec3 viewDir = normalize(viewPos - FragPos);
  vec3 lit = calcDirLight(norm, viewDir, baseColor, ao, shininess)
           + calcPointLights(norm, FragPos, viewDir, baseColor, shininess);
  fragColor = vec4(lit, opacity);
}
