#include "game/wave_system.h"

#include <GLFW/glfw3.h>

#include "game/enemy_system.h"

// =============================================================================
// DEFINIÇÃO DAS 10 ONDAS DO JOGO
// =============================================================================
// chargerStats/necroStats só precisam de { hp, speed, damage, type } correto.
// Os campos omitidos ficam em zero/default.

const std::array<WaveDef, 10> kWaves = {{
//   label      count  spawnInt  { hp   spd   dano  t }  interv  arm  { hp   spd  dano  t }  chg  { hp    spd  dano  t }  nec  { hp   spd  dano  t }
  { "Onda  1",   10,    1.2f,    { 50, 1.5f,  10,  0 },  20.0f,   0,  {  0, 0.0f,  0,  1 },   0,  {   0, 0.0f,  0,  2 },   0,  {  0, 0.0f,  0,  3 } },
  { "Onda  2",   15,    1.0f,    { 55, 1.8f,  10,  0 },  22.0f,   3,  { 100, 1.2f, 15,  1 },   1,  {   0, 0.0f,  0,  2 },   0,  {  0, 0.0f,  0,  3 } },
  { "Onda  3",  20,    0.9f,    { 60, 2.0f,  12,  0 },  25.0f,   5,  { 100, 1.3f, 18,  1 },   3,  { 150, 2.2f, 25,  2 },   0,  {  0, 0.0f,  0,  3 } },
  { "Onda  4",  25,    0.8f,    { 65, 2.0f,  12,  0 },  28.0f,   10,  {150, 1.4f, 20,  1 },   5,  { 200, 2.2f, 28,  2 },   0,  {  0, 0.0f,  0,  3 } },
  { "Onda  5",  30,    0.7f,    { 70, 2.2f,  15,  0 },  32.0f,   12,  {150, 1.5f, 22,  1 },   10,  { 200, 2.3f, 30,  2 },   1,  { 400, 0.0f,  0,  3 } },
  { "Onda  6",  35,    0.8f,    { 90, 2.0f,  15,  0 },  35.0f,   15,  {150, 1.5f, 22,  1 },   10,  { 200, 2.3f, 30,  2 },   3,  { 400, 1.6f, 15,  3 } },
  { "Onda  7",  45,    0.7f,    { 110, 2.2f,  18,  0 },  38.0f,   20,  {200, 1.6f, 25,  1 },   15,  { 300, 2.4f, 32,  2 },   3,  { 600, 1.6f, 15,  3 } },
  { "Onda  8",  50,    0.6f,    { 130, 2.2f,  18,  0 },  42.0f,   25,  {250, 1.7f, 28,  1 },   25,  { 350, 2.4f, 35,  2 },   5,  { 600, 1.7f, 18,  3 } },
  { "Onda  9",  75,    0.5f,    { 150, 2.5f,  20,  0 },  48.0f,  30,  {350, 1.8f, 30,  1 },   28,  { 400, 2.5f, 38,  2 },   10,  { 700, 1.7f, 18,  3 } },
  { "Onda 10",  100,    0.5f,    {200, 2.5f,  20,  0 },  55.0f,  50,  {500, 2.0f, 35,  1 },   30,  { 400, 2.6f, 40,  2 },   15,  { 800, 1.8f, 20,  3 } },
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

// Determina o tipo do spawn número `i` na onda por interleaving (Bresenham).
// Ordem de prioridade: necromancer > charger > armored > normal.
static int pickSpawnType(int i, const WaveDef &def) {
  const int total = def.enemyCount;

  auto bresenham = [&](int count) -> bool {
    if (count <= 0) return false;
    return (i + 1) * count / total > i * count / total;
  };

  if (bresenham(def.necroCount))   return enemy_types::kNecromancer;
  if (bresenham(def.chargerCount)) return enemy_types::kCharger;
  if (bresenham(def.armoredCount)) return enemy_types::kArmored;
  return enemy_types::kNormal;
}

int updateWave(WaveState &ws, GLFWwindow *window, float deltaTime,
               int deathsThisFrame, int aliveCount, bool allSlotsFull) {
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

  if (ws.spawnedCount >= def.enemyCount && aliveCount == 0) {
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

  int spawnType = pickSpawnType(ws.spawnedCount, def);
  ws.spawnedCount++;
  ws.timer = def.spawnInterval;
  return spawnType;
}
