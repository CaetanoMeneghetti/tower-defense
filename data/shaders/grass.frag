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

// Noise
const float NOISE_SCALE_1 = 0.04;
const float NOISE_SCALE_2 = 0.07;
const vec2  NOISE_OFFSET_2 = vec2(0.4, 0.1);

// Smoothstep
const float MASK_MIN = 0.3;
const float MASK_MAX = 0.7;

void main() {
  // Grama base
  vec3 grama1 = texture(grass, texCoord).rgb;

  // Segunda camada
  vec2 uv2 = ROT1 * texCoord * UV_SCALE_1 + UV_OFFSET_1;
  vec3 grama2 = texture(grass, uv2).rgb;

  // Terceira camada
  vec2 uv3 = ROT2 * texCoord * UV_SCALE_2 + UV_OFFSET_2;
  vec3 grama3 = texture(grass, uv3).rgb;


  // Noise
  float mascara1 = texture(noise, texCoord * NOISE_SCALE_1).r;
  float mascara2 = texture(noise, texCoord * NOISE_SCALE_2 + NOISE_OFFSET_2).r;

  // Contraste
  mascara1 = smoothstep(MASK_MIN, MASK_MAX, mascara1);
  mascara2 = smoothstep(MASK_MIN, MASK_MAX, mascara2);

  // Mistura
  vec3 corFinal = mix(grama1, grama2, mascara1);
  corFinal = mix(corFinal, grama3, mascara2);

  fragColor = vec4(corFinal, 1.0);
}
