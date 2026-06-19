#pragma once

#include <array>
#include <GLFW/glfw3.h>

#include "game/enemy_system.h"

// =============================================================================
// SISTEMA DE WAVES — 10 ondas configuráveis
// =============================================================================
// Para editar a composição de cada onda, abra src/game/wave_system.cpp
// e modifique o array kWaves.

enum class WavePhase {
  Intermission,  // intervalo antes da onda — mostra contagem regressiva
  Starting,      // waveStart.mp3 tocando; wave começa após 5 s
  Active,        // onda em andamento — inimigos sendo spawnados
  Victory,       // todas as 10 ondas concluídas
};

// Definição completa de uma onda. Edite em wave_system.cpp.
struct WaveDef {
  const char* label;
  int         enemyCount;
  float       spawnInterval;
  EnemyStats  enemyStats;
  float       intermissionSecs;
  int         armoredCount    = 0;
  EnemyStats  armoredStats    = {};
  int         chargerCount    = 0;
  EnemyStats  chargerStats    = {};
  int         necroCount      = 0;
  EnemyStats  necroStats      = {};
};

// Estado em tempo de execução.
struct WaveState {
  int       currentWave  = 0;
  WavePhase phase        = WavePhase::Intermission;
  float     timer        = 0.0f;         // countdown (intermission) ou próximo spawn (active)
  float     startingTimer = 0.0f;        // countdown da fase Starting (5 s)
  int       spawnedCount = 0;            // inimigos spawnados nesta onda
  int       killedCount  = 0;            // inimigos eliminados/chegados nesta onda
  bool      yWasPressed  = false;        // detecção de borda para Y (pular)
};

// As 10 ondas — definidas e documentadas em wave_system.cpp. Edite lá.
extern const std::array<WaveDef, 10> kWaves;

// Cria estado inicial (intermission da onda 0, timer já carregado).
WaveState makeWaveState();

// Atualiza timers e transições de fase. Chame uma vez por frame.
// `deathsThisFrame` = quantos inimigos morreram ou chegaram ao castelo neste frame.
// `allSlotsFull`    = true se não há nenhum slot livre para spawnar.
// Retorna o tipo a spawnar (enemy_types::kNormal ou kArmored), ou -1 para não spawnar.
int updateWave(WaveState &ws, GLFWwindow *window, float deltaTime,
               int deathsThisFrame, bool allSlotsFull);
