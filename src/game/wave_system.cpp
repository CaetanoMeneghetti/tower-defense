#include "game/wave_system.h"

#include <GLFW/glfw3.h>

#include "game/enemy_system.h"

// =============================================================================
// DEFINIÇÃO DAS 10 ONDAS DO JOGO
// =============================================================================
// Campos:
//   label            – nome exibido na HUD durante o intervalo
//   enemyCount       – total de inimigos na onda (normais + blindados)
//   spawnInterval    – segundos entre cada spawn
//   enemyStats       – stats dos zumbis normais { maxHp, speed, dano, type=0 }
//   intermissionSecs – duração do intervalo antes desta onda
//   armoredCount     – quantos dos enemyCount são zumbis blindados
//   armoredStats     – stats dos blindados { maxHp, speed, dano, type=1 }
//
// Os blindados são intercalados uniformemente com os normais (algoritmo Bresenham).

const std::array<WaveDef, 10> kWaves = {{
//   label       count  spawnInt   { hp   spd  dano  t }   intermission  armored  { hp   spd  dano  t }
  { "Onda  1",    6,    1.2f,     {  50, 1.5f,  10, 0 },   20.0f,        0,       {   0, 0.0f,  0, 1 } },
  { "Onda  2",    8,    1.0f,     {  55, 1.8f,  10, 0 },   22.0f,        2,       {  80, 1.2f, 15, 1 } },
  { "Onda  3",   10,    0.9f,     {  60, 2.0f,  12, 0 },   25.0f,        3,       {  90, 1.3f, 18, 1 } },
  { "Onda  4",   12,    0.8f,     {  65, 2.0f,  12, 0 },   28.0f,        5,       { 100, 1.4f, 20, 1 } },
  { "Onda  5",   14,    0.7f,     {  70, 2.2f,  15, 0 },   32.0f,        6,       { 110, 1.5f, 22, 1 } },
  { "Onda  6",   14,    0.8f,     {  75, 2.0f,  15, 0 },   35.0f,        8,       { 120, 1.5f, 22, 1 } },
  { "Onda  7",   16,    0.7f,     {  80, 2.2f,  18, 0 },   38.0f,       10,       { 130, 1.6f, 25, 1 } },
  { "Onda  8",   18,    0.6f,     {  85, 2.2f,  18, 0 },   42.0f,       12,       { 150, 1.7f, 28, 1 } },
  { "Onda  9",   18,    0.5f,     {  90, 2.5f,  20, 0 },   48.0f,       14,       { 170, 1.8f, 30, 1 } },
  { "Onda 10",   20,    0.5f,     { 100, 2.5f,  20, 0 },   55.0f,       16,       { 200, 2.0f, 35, 1 } },
}};

WaveState makeWaveState() {
  WaveState ws;
  ws.currentWave   = 0;
  ws.phase         = WavePhase::Intermission;
  ws.timer         = kWaves[0].intermissionSecs;
  ws.startingTimer = 0.0f;
  ws.spawnedCount  = 0;
  ws.killedCount   = 0;
  ws.yWasPressed   = false;
  return ws;
}

int updateWave(WaveState &ws, GLFWwindow *window, float deltaTime,
               int deathsThisFrame, bool allSlotsFull) {
  if (ws.phase == WavePhase::Victory) return -1;

  bool yDown        = glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS;
  bool yJustPressed = yDown && !ws.yWasPressed;
  ws.yWasPressed    = yDown;

  // ---- Intermission ----
  if (ws.phase == WavePhase::Intermission) {
    ws.timer -= deltaTime;
    if (ws.timer <= 0.0f || yJustPressed) {
      ws.phase         = WavePhase::Starting;
      ws.startingTimer = 5.0f;
    }
    return -1;
  }

  // ---- Starting ----
  if (ws.phase == WavePhase::Starting) {
    ws.startingTimer -= deltaTime;
    if (ws.startingTimer <= 0.0f) {
      ws.phase        = WavePhase::Active;
      ws.timer        = 0.0f;
      ws.spawnedCount = 0;
      ws.killedCount  = 0;
    }
    return -1;
  }

  // ---- Active ----
  const WaveDef &def = kWaves[ws.currentWave];

  ws.killedCount += deathsThisFrame;

  if (ws.spawnedCount >= def.enemyCount && ws.killedCount >= def.enemyCount) {
    int next = ws.currentWave + 1;
    if (next >= 10) {
      ws.phase = WavePhase::Victory;
    } else {
      ws.currentWave = next;
      ws.phase       = WavePhase::Intermission;
      ws.timer       = kWaves[next].intermissionSecs;
    }
    return -1;
  }

  if (ws.spawnedCount >= def.enemyCount) return -1;
  if (allSlotsFull) return -1;

  ws.timer -= deltaTime;
  if (ws.timer > 0.0f) return -1;

  // Determina tipo do próximo spawn por interleaving (Bresenham)
  int i = ws.spawnedCount;
  int spawnType = enemy_types::kNormal;
  if (def.armoredCount > 0 && def.enemyCount > 0) {
    int armoredBefore = i * def.armoredCount / def.enemyCount;
    int armoredAfter  = (i + 1) * def.armoredCount / def.enemyCount;
    if (armoredAfter > armoredBefore) spawnType = enemy_types::kArmored;
  }

  ws.spawnedCount++;
  ws.timer = def.spawnInterval;
  return spawnType;
}
