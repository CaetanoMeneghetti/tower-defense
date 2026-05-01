#version 330 core

out vec4 fragColor;

in vec3 objNormal;
in vec2 texCoord;

uniform sampler2D dirt;
uniform sampler2D noise;

// Escala da textura base
const float UV_SCALE_X = 2.0;
const float UV_SCALE_Y = 0.5;

// Noise
const float NOISE_SCALE_Y = 0.1;

// Segunda camada da terra
const float DIRT_LAYER2_SCALE = 1.67;
const vec2  DIRT_LAYER2_OFFSET = vec2(0.3, 0.7);

// Distância do centro
const float CENTER_OFFSET = 0.5;
const float CENTER_SCALE = 2.0;

// Opacidade
const float OPACITY_BASE = 1.0;
const float NOISE_INFLUENCE = 0.6;

// Smoothstep (borda)
const float EDGE_MIN = 0.0;
const float EDGE_MAX = 0.2;

// Corte (discard)
const float ALPHA_CUTOFF = 0.05;

void main() {
  // UV base
  vec2 uvBase = vec2(texCoord.x * UV_SCALE_X,
                      texCoord.y * UV_SCALE_Y);

  // Noise
  float valorRuido = texture(noise,
      vec2(texCoord.x, texCoord.y * NOISE_SCALE_Y)).r;

  // Terra base
  vec3 terra1 = texture(dirt, uvBase).rgb;

  // Segunda camada
  vec2 uvDeslocado = uvBase * DIRT_LAYER2_SCALE + DIRT_LAYER2_OFFSET;
  vec3 terra2 = texture(dirt, uvDeslocado).rgb;

  // Mistura
  vec3 corFinalTerra = mix(terra1, terra2, valorRuido);

  // Distância do centro
  float distDoCentro = abs(texCoord.x - CENTER_OFFSET) * CENTER_SCALE;

  // Opacidade base
  float opacidade = OPACITY_BASE - distDoCentro;

  // Ruído na borda
  opacidade -= (valorRuido * NOISE_INFLUENCE);

  // Suavização
  opacidade = smoothstep(EDGE_MIN, EDGE_MAX, opacidade);

  // Corte
  if (opacidade < ALPHA_CUTOFF) {
      discard;
  }

  fragColor = vec4(corFinalTerra, opacidade);
}
