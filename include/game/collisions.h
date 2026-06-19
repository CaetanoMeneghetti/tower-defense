#pragma once

#include <vector>

#include "engine/catmull_rom.h"
#include "game/enemy_system.h"

// =============================================================================
// COLISÕES — testes geométricos centralizados
// =============================================================================
// Primitivas puras: sem estado, sem efeitos colaterais.
// Todos os testes de proximidade/alcance do jogo passam por aqui.

namespace collisions {

// Retorna true se dois pontos no plano XZ estão a no máximo `range` entre si.
bool inRange(float ax, float az, float bx, float bz, float range);

// Retorna true se duas bounding-boxes alinhadas aos eixos (AABB) no plano XZ se
// sobrepõem. Cada caixa é dada pelo seu centro (x, z) e sua semi-extensão
// (metade do lado). Usado na colisão de combate corpo-a-corpo.
bool aabbOverlap(float ax, float az, float aHalf,
                 float bx, float bz, float bHalf);

// Índice do inimigo vivo mais próximo de (ox, oz).
// `outDistSq` recebe a distância quadrática; FLT_MAX se nenhum vivo.
// Retorna -1 se não houver nenhum inimigo vivo.
int closestAliveEnemy(const std::vector<EnemyInstance> &enemies,
                      const std::vector<EnemyTickResult> &ticks,
                      float ox, float oz,
                      float &outDistSq);

// Resultado de uma verificação de posicionamento de tropa.
struct PlacementResult {
  bool onPath;            // ponto está muito próximo do caminho
  bool tooFar;            // ponto está longe demais do caminho
  bool invalid() const { return onPath || tooFar; }
};

// Verifica se (px, pz) é uma posição válida para colocar uma tropa.
PlacementResult checkPlacement(const std::vector<Point> &curvePoints,
                                float px, float pz,
                                float pathHalfWidth, float pathClearance,
                                float maxDistance);

}  // namespace collisions
