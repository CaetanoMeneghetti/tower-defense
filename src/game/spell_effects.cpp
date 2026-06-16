#include "game/spell_effects.h"

#include "game/game_constants.h"

namespace spell_effects {

// QUADRADO (azul): fixa a velocidade em 75% da original. Permanente — só some
// quando o inimigo morre/renasce (clearSpellEffects).
void applySlow(EnemyInstance &enemy) {
  enemy.speedMultiplier = game_constants::kSpellSlowMultiplier;
  // Não causa dano, mas pisca pra confirmar que o feitiço pegou.
  enemy.hitFlashTime = game_constants::kEnemyHitFlashDuration;
}

// TRIÂNGULO (laranja): tira metade da vida atual. Divisão inteira (hp - hp/2)
// mantém >= 1 de vida em hp ímpar.
void applyHalfLife(EnemyInstance &enemy) {
  enemy.hp -= enemy.hp / 2;
  enemy.hitFlashTime = game_constants::kEnemyHitFlashDuration;
}

// CÍRCULO (verde): arma o veneno; o dano (10% da vida máx/s) é descontado frame a
// frame em enemy_system.cpp enquanto poisonTimer > 0.
void applyPoison(EnemyInstance &enemy) {
  enemy.poisonTimer = game_constants::kSpellPoisonDuration;
}

}  // namespace spell_effects
