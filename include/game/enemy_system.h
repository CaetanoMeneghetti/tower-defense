#pragma once

#include <vector>

#include "engine/catmull_rom.h"
#include "engine/game_object.h"
#include "game/app_state.h"
#include "game/path_navigation.h"
#include "math/vector.h"

// =============================================================================
// INIMIGOS — STATS + ESTADO RUNTIME + UPDATE
// =============================================================================


namespace enemy_types {
  constexpr int kNormal  = 0;
  constexpr int kArmored = 1;
}

struct EnemyStats {
  int   maxHp;
  float speed;
  int   damage;
  int   type = enemy_types::kNormal;
};

struct EnemyInstance {
  EnemyStats stats;
  int hp;
  float pathDistance;
  bool alive;
  float respawnTimer;
  float hitFlashTime;
  // Quando true, o respawn automático é suprimido — o wave system controla.
  bool waveControlled = false;

  // ---- Efeitos de feitiço (ver game/spell_effects.h) ----
  // Multiplicador de velocidade do feitiço quadrado (lentidão permanente).
  float speedMultiplier = 1.0f;
  // Veneno do feitiço círculo: segundos restantes + acumulador fracionário de
  // dano (hp é int; o veneno tira 10% da vida máx por segundo).
  float poisonTimer = 0.0f;
  float poisonAccum = 0.0f;
};

// Zera os efeitos de feitiço (lentidão/veneno) — chamado ao (re)nascer.
void clearSpellEffects(EnemyInstance &enemy);

// Stats pré-definidas (declaração em .cpp).
extern const EnemyStats kZombieStats;

// Inicializa uma instância no início do path.
EnemyInstance makeEnemy(const EnemyStats &stats);

// Resultado do tick do inimigo.
struct EnemyTickResult {
  Vector<3> position;
  float angle;
  bool reachedEnd;
  bool diedThisFrame;  // morreu (hp=0) ou chegou ao castelo neste frame
};

// Avança o inimigo no path, decrementa respawn timer e atualiza flash.
// Caso o inimigo alcance o castelo, decrementa state.health e respawna.
EnemyTickResult updateEnemy(EnemyInstance &enemy,
                            GameObject &enemyModel,
                            const std::vector<Point> &curvePoints,
                            const PathCache &curveCache,
                            AppState &state,
                            float deltaTime);
