#include "game/spell_effects.h"

#include "game/game_constants.h"

namespace spell_effects {

// QUADRADO (azul): fixa a velocidade em 75% da original. Permanente — nunca é
// restaurado (só some quando o inimigo morre/renasce, via clearSpellEffects).
void applySlow(EnemyInstance &enemy) {
  enemy.speedMultiplier = game_constants::kSpellSlowMultiplier;
}

// TRIÂNGULO (laranja): tira metade da vida ATUAL. Usa divisão inteira no dano
// (hp - hp/2), o que mantém pelo menos 1 de vida em inimigos com hp ímpar.
void applyHalfLife(EnemyInstance &enemy) {
  enemy.hp -= enemy.hp / 2;
  enemy.hitFlashTime = game_constants::kEnemyHitFlashDuration;
}

// CÍRCULO (verde): arma o veneno. O dano em si (10% da vida máx/s) é descontado
// frame a frame em enemy_system.cpp enquanto poisonTimer > 0.
void applyPoison(EnemyInstance &enemy) {
  enemy.poisonTimer = game_constants::kSpellPoisonDuration;
}

}  // namespace spell_effects
