#pragma once

#include <vector>

#include "engine/game_object.h"
#include "game/enemy_system.h"
#include "math/vector.h"

// =============================================================================
// DEFENSORES — ESTADO DE TIRO + UPDATE
// =============================================================================
// Defenders compartilham a entidade visual (GameObject). O estado de tiro vive
// em vetor paralelo para manter os GameObjects independentes da lógica de
// combate.

struct DefenderShoot {
  float shootTimer;
  bool aiming;
};

// Tipos de defensor. Espelha GameObject::type. kNone (0) = nenhum selecionado.
namespace defender_types {
constexpr int kNone = 0;
constexpr int kArcher = 1;
constexpr int kArquebus = 2;
}  // namespace defender_types

// Atualiza animações, mira e dano sobre o inimigo. Mantém defenderShoots
// alinhado com defenders se um foi recém adicionado.
void updateDefenders(std::vector<GameObject> &defenders,
                     std::vector<DefenderShoot> &defenderShoots,
                     EnemyInstance &enemy,
                     const Vector<3> &enemyPos,
                     float deltaTime);
