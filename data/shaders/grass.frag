#version 330 core

out vec4 fragColor;

in vec2 texCoord;

uniform sampler2D grass;
uniform sampler2D noise;

// Rotação (valores já em cos/sin)
const mat2 ROT1 = mat2(0.39, -0.92, 0.92, 0.39);
const mat2 ROT2 = mat2(-0.65, -0.75, 0.75, -0.65);

// Escalas UV
const float UV_SCALE_1 = 1.31;
const float UV_SCALE_2 = 1.73;

// Offsets UV
const vec2 UV_OFFSET_1 = vec2(0.17, 0.82);
const vec2 UV_OFFSET_2 = vec2(0.53, 0.29);

// Noise das máscaras de mistura
const float NOISE_SCALE_1 = 0.04;
const float NOISE_SCALE_2 = 0.07;
const vec2  NOISE_OFFSET_2 = vec2(0.4, 0.1);

// Smoothstep das máscaras
const float MASK_MIN = 0.3;
const float MASK_MAX = 0.7;

// Variação macro (tint regional)
// Frequência baixa = manchas grandes que cobrem várias copias do tile
// Offset grande descorrelaciona das máscaras de mistura acima
const float MACRO_NOISE_SCALE = 0.013;
const vec2  MACRO_NOISE_OFFSET = vec2(7.3, 3.1);

// Tints suaves: ambos próximos de (1, 1, 1) para multiplicação leve
const vec3 TINT_SECO  = vec3(0.96, 0.94, 0.83);  // ligeiramente amarelado/seco
const vec3 TINT_VIVO  = vec3(0.94, 1.02, 0.92);  // ligeiramente mais verde
const float MACRO_MIN = 0.35;
const float MACRO_MAX = 0.65;

void main() {
  // Grama base
  vec3 grass1 = texture(grass, texCoord).rgb;

  // Segunda camada
  vec2 uv2 = ROT1 * texCoord * UV_SCALE_1 + UV_OFFSET_1;
  vec3 grass2 = texture(grass, uv2).rgb;

  // Terceira camada
  vec2 uv3 = ROT2 * texCoord * UV_SCALE_2 + UV_OFFSET_2;
  vec3 grass3 = texture(grass, uv3).rgb;

  // Máscaras de mistura
  float mask1 = texture(noise, texCoord * NOISE_SCALE_1).r;
  float mask2 = texture(noise, texCoord * NOISE_SCALE_2 + NOISE_OFFSET_2).r;
  mask1 = smoothstep(MASK_MIN, MASK_MAX, mask1);
  mask2 = smoothstep(MASK_MIN, MASK_MAX, mask2);

  // Mistura das 3 camadas
  vec3 finalColor = mix(grass1, grass2, mask1);
  finalColor = mix(finalColor, grass3, mask2);

  // Variação macro de cor: tint regional baseado em noise grande
  // Frequência (0.013) e offset (7.3, 3.1) escolhidos para nao
  // correlacionar com mask1/2 — evita reforço de padrão
  float macroMask = texture(noise, texCoord * MACRO_NOISE_SCALE + MACRO_NOISE_OFFSET).r;
  macroMask = smoothstep(MACRO_MIN, MACRO_MAX, macroMask);
  vec3 tint = mix(TINT_SECO, TINT_VIVO, macroMask);
  finalColor *= tint;

  fragColor = vec4(finalColor, 1.0);
}
