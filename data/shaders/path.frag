#version 330 core

out vec4 fragColor;

in vec3 objNormal;
in vec2 texCoord;

uniform sampler2D dirt;
uniform sampler2D noise;

// Escala da textura base
const float UV_SCALE_X = 2.0;
const float UV_SCALE_Y = 0.5;

// Noise (mistura entre camadas de terra)
const float NOISE_SCALE_Y = 0.1;

// Segunda camada da terra
const float DIRT_LAYER2_SCALE = 1.67;
const vec2  DIRT_LAYER2_OFFSET = vec2(0.3, 0.7);

// Distância do centro
const float CENTER_OFFSET = 0.5;
const float CENTER_SCALE = 2.0;

// Faixa de transição
// distDoCentro vai de 0 (centro) a 1 (borda da geometria irregular).
// EDGE_INNER..EDGE_OUTER define onde a opacidade vai de 1 → 0
const float EDGE_INNER = 0.70;  // Centro fica 100% opaco até EDGE_INNER (%) do caminho
const float EDGE_OUTER = 1.00;  // Borda totalmente transparente

// Noise que granula a borda
// Frequência alta = grãos finos (sem virar manchas grandes)
const float EDGE_NOISE_SCALE = 3.0; // Tamanho dos grãos
const vec2  EDGE_NOISE_OFFSET = vec2(2.1, 4.7);
const float EDGE_NOISE_AMPLITUDE = 0.4; // Força da granulação

// Onde o noise começa a ter efeito (um pouco antes (0.05, talvez) de EDGE_INNER
// para entrada suave da granulação)
const float NOISE_ACTIVATION = 0.30;

const float ALPHA_CUTOFF = 0.05;

void main() {
  // UV base
  vec2 uvBase = vec2(texCoord.x * UV_SCALE_X, texCoord.y * UV_SCALE_Y);

  // Noise (só para mistura entre as camadas de terra)
  float noiseValue = texture(noise, vec2(texCoord.x, texCoord.y * NOISE_SCALE_Y)).r;

  // Terra base
  vec3 dirt1 = texture(dirt, uvBase).rgb;

  // Segunda camada
  vec2 uvOffset = uvBase * DIRT_LAYER2_SCALE + DIRT_LAYER2_OFFSET;
  vec3 dirt2 = texture(dirt, uvOffset).rgb;

  // Mistura
  vec3 dirtFinalColor = mix(dirt1, dirt2, noiseValue);

  // ------------------------------------------------------------------
  // Transição granular grama ↔ terra
  //
  // 1. centerDistance: 0 (centro do caminho) → 1 (borda da geometria).
  //    A geometria já é irregular (path_generator.cpp), então este "1"
  //    está em um lugar diferente em cada fatia do caminho.
  //
  // 2. edgeFactor: 0 no centro, 1 perto da borda.
  //    Garante que o noise NÃO afete o miolo do caminho.
  //
  // 3. perturbedDistance: deslocamos a distância em ±EDGE_NOISE_AMPLITUDE/2
  //    apenas onde edgeFactor > 0. Pixels que estavam "fora" entram,
  //    pixels que estavam "dentro" saem → granula o contorno.
  //
  // 4. smoothstep limpo na distância perturbada gera o alpha final.
  // ------------------------------------------------------------------
  float centerDistance = abs(texCoord.x - CENTER_OFFSET) * CENTER_SCALE;

  float edgeNoise = texture(noise, texCoord * EDGE_NOISE_SCALE + EDGE_NOISE_OFFSET).r;

  float edgeFactor = smoothstep(NOISE_ACTIVATION, 1.0, centerDistance);
  float perturbedDistance = centerDistance + (edgeNoise - 0.5) * edgeFactor * EDGE_NOISE_AMPLITUDE;

  float opcaity = 1.0 - smoothstep(EDGE_INNER, EDGE_OUTER, perturbedDistance);

  if (opcaity < ALPHA_CUTOFF) {
    discard;
  }

  fragColor = vec4(dirtFinalColor, opcaity);
}
