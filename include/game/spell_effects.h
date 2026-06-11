#pragma once

#include "game/enemy_system.h"

// =============================================================================
// FEITIÇOS — EFEITOS DE GAMEPLAY
// =============================================================================
// Três feitiços, um por forma geométrica desenhada e reconhecida pela CNN. O
// efeito é aplicado a cada inimigo cujo ponto projetado na tela cai dentro da
// forma perfeita renderizada (ver spell::pointInCast em spell/spell_mode.h).
//
// Mapeamento forma -> cor -> efeito:
//   - QUADRADO  (azul)    -> lentidão permanente (applySlow)
//   - TRIÂNGULO (laranja) -> tira metade da vida  (applyHalfLife)
//   - CÍRCULO   (verde)   -> veneno 10%/s         (applyPoison)
//
// As funções abaixo apenas SETAM o estado no inimigo; o tique contínuo (veneno,
// movimento reduzido) acontece em enemy_system.cpp::updateEnemy.
namespace spell_effects {

// QUADRADO (azul) — LENTIDÃO PERMANENTE.
// Multiplica a velocidade do inimigo por game_constants::kSpellSlowMultiplier
// (0.75 -> anda a 75% do normal) pelo resto da vida dele. Idempotente: lançar de
// novo não empilha além de 0.75.
void applySlow(EnemyInstance &enemy);

// TRIÂNGULO (laranja) — DANO INSTANTÂNEO.
// Remove metade da vida atual do inimigo de uma só vez (arredonda para cima o
// que é tirado, então hp acaba na metade inferior).
void applyHalfLife(EnemyInstance &enemy);

// CÍRCULO (verde) — VENENO (dano contínuo).
// Aplica veneno que tira game_constants::kSpellPoisonFraction (10%) da vida
// máxima por segundo, durante game_constants::kSpellPoisonDuration segundos.
// Reaplicar renova a duração para o valor cheio.
void applyPoison(EnemyInstance &enemy);

}  // namespace spell_effects
