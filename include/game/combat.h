#pragma once

#include <vector>

#include "game/enemy_system.h"

// =============================================================================
// COMBATE — efeitos de dano reutilizáveis
// =============================================================================
// Funções de dano compartilhadas entre defensores e (futuramente) feitiços.

namespace combat {

// Aplica `damage` a todos os inimigos vivos dentro de `radius` de (cx,cz) no XZ,
// marca o flash de acerto e mantém hp >= 0. `radius` é parâmetro pois varia com a
// origem do golpe (canhão, feitiço...). ticks = posições do frame atual.
// Retorna a quantidade de inimigos atingidos.
int applyAreaDamage(std::vector<EnemyInstance> &enemies,
                    const std::vector<EnemyTickResult> &ticks,
                    float cx, float cz, float radius, int damage);

}  // namespace combat
