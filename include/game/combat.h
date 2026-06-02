#pragma once

#include <vector>

#include "game/enemy_system.h"

// =============================================================================
// COMBATE — efeitos de dano reutilizáveis
// =============================================================================
// Funções de dano compartilhadas entre defensores e (futuramente) feitiços.

namespace combat {

// Aplica `damage` a TODOS os inimigos vivos dentro de `radius` de (cx, cz) no
// plano XZ. Marca o flash de acerto e mantém hp >= 0. O raio varia conforme a
// origem do golpe (canhão, feitiço, etc.), por isso é parâmetro.
//
// enemies = slots de inimigos; ticks = posições do frame atual (alinhado).
// Retorna a quantidade de inimigos atingidos.
int applyAreaDamage(std::vector<EnemyInstance> &enemies,
                    const std::vector<EnemyTickResult> &ticks,
                    float cx, float cz, float radius, int damage);

}  // namespace combat
