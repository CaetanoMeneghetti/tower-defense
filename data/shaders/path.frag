#version 330 core

out vec4 fragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 texCoord;

uniform sampler2D dirt;
uniform sampler2D noise;
uniform vec3 viewPos;

// Luz direcional (sol/lua)
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight light;

const float UV_SCALE_X = 2.0;
const float UV_SCALE_Y = 0.5;

const float NOISE_SCALE_Y = 0.1;

const float DIRT_LAYER2_SCALE  = 1.67;
const vec2  DIRT_LAYER2_OFFSET = vec2(0.3, 0.7);

const float CENTER_OFFSET = 0.5;
const float CENTER_SCALE  = 2.0;

const float EDGE_INNER = 0.68;
const float EDGE_OUTER = 1.00;

const float EDGE_NOISE_SCALE     = 4.5;
const vec2  EDGE_NOISE_OFFSET    = vec2(2.1, 4.7);
const float EDGE_NOISE_AMPLITUDE = 0.50;
const float NOISE_ACTIVATION     = 0.25;

// Segunda frequência de noise para micro-detalhe na borda
const float EDGE_NOISE2_SCALE  = 9.0;
const vec2  EDGE_NOISE2_OFFSET = vec2(5.3, 1.2);
const float EDGE_NOISE2_WEIGHT = 0.25;

const float ALPHA_CUTOFF = 0.05;
const float SHININESS    = 4.0;

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

void main() {
  // --- Textura de terra (duas camadas misturadas) ---
  vec2 uvBase    = vec2(texCoord.x * UV_SCALE_X, texCoord.y * UV_SCALE_Y);
  float noiseVal = texture(noise, vec2(texCoord.x, texCoord.y * NOISE_SCALE_Y)).r;
  vec3 dirt1     = texture(dirt, uvBase).rgb;
  vec3 dirt2     = texture(dirt, uvBase * DIRT_LAYER2_SCALE + DIRT_LAYER2_OFFSET).rgb;
  vec3 baseColor = mix(dirt1, dirt2, noiseVal);

  // --- Borda: solo compactado — escurece onde terra encontra grama ---
  float centerDistance = abs(texCoord.x - CENTER_OFFSET) * CENTER_SCALE;
  float edgeBlend      = smoothstep(0.18, 0.82, centerDistance);
  baseColor = mix(baseColor, baseColor * vec3(0.42, 0.40, 0.38), edgeBlend * 0.72);

  // --- Fade de opacidade granulado (duas frequências de noise) ---
  float edgeNoise    = texture(noise, texCoord * EDGE_NOISE_SCALE  + EDGE_NOISE_OFFSET).r;
  float edgeNoise2   = texture(noise, texCoord * EDGE_NOISE2_SCALE + EDGE_NOISE2_OFFSET).r;
  float combinedNoise = mix(edgeNoise, edgeNoise2, EDGE_NOISE2_WEIGHT);
  float edgeFactor    = smoothstep(NOISE_ACTIVATION, 1.0, centerDistance);
  float perturbedDist = centerDistance + (combinedNoise - 0.5) * edgeFactor * EDGE_NOISE_AMPLITUDE;
  float opacity       = 1.0 - smoothstep(EDGE_INNER, EDGE_OUTER, perturbedDist);

  if (opacity < ALPHA_CUTOFF) discard;

  vec3 norm    = normalize(Normal);
  vec3 viewDir = normalize(viewPos - FragPos);
  fragColor = vec4(calcDirLight(norm, viewDir, baseColor), opacity);
}
