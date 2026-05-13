#version 330 core

out vec4 fragColor;

in vec2 texCoord;
in vec3 FragPos;

uniform sampler2D grass;
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

const vec3  TINT_SECO  = vec3(0.96, 0.94, 0.83);
const vec3  TINT_VIVO  = vec3(0.94, 1.02, 0.92);
const float MACRO_MIN  = 0.35;
const float MACRO_MAX  = 0.65;

const float SHININESS = 8.0;

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

  // Normal fixa pra cima — plano horizontal sem normal no VBO
  vec3 norm    = vec3(0.0, 1.0, 0.0);
  vec3 viewDir = normalize(viewPos - FragPos);

  fragColor = vec4(calcDirLight(norm, viewDir, baseColor), 1.0);
}
