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
  { "Onda  1",   6,    1.2f,    { 50, 1.5f,  10,  0 },  20.0f,   0,  {  0, 0.0f,  0,  1 },   0,  {   0, 0.0f,  0,  2 },   0,  {  0, 0.0f,  0,  3 } },
  { "Onda  2",   8,    1.0f,    { 55, 1.8f,  10,  0 },  22.0f,   2,  { 80, 1.2f, 15,  1 },   0,  {   0, 0.0f,  0,  2 },   0,  {  0, 0.0f,  0,  3 } },
  { "Onda  3",  11,    0.9f,    { 60, 2.0f,  12,  0 },  25.0f,   3,  { 90, 1.3f, 18,  1 },   2,  { 150, 2.2f, 25,  2 },   0,  {  0, 0.0f,  0,  3 } },
  { "Onda  4",  13,    0.8f,    { 65, 2.0f,  12,  0 },  28.0f,   5,  {100, 1.4f, 20,  1 },   3,  { 160, 2.2f, 28,  2 },   0,  {  0, 0.0f,  0,  3 } },
  { "Onda  5",  15,    0.7f,    { 70, 2.2f,  15,  0 },  32.0f,   5,  {110, 1.5f, 22,  1 },   4,  { 170, 2.3f, 30,  2 },   0,  {  0, 0.0f,  0,  3 } },
  { "Onda  6",  16,    0.8f,    { 75, 2.0f,  15,  0 },  35.0f,   6,  {120, 1.5f, 22,  1 },   4,  { 180, 2.3f, 30,  2 },   1,  { 60, 1.6f, 15,  3 } },
  { "Onda  7",  17,    0.7f,    { 80, 2.2f,  18,  0 },  38.0f,   7,  {130, 1.6f, 25,  1 },   5,  { 190, 2.4f, 32,  2 },   2,  { 65, 1.6f, 15,  3 } },
  { "Onda  8",  18,    0.6f,    { 85, 2.2f,  18,  0 },  42.0f,   8,  {150, 1.7f, 28,  1 },   5,  { 200, 2.4f, 35,  2 },   2,  { 70, 1.7f, 18,  3 } },
  { "Onda  9",  19,    0.5f,    { 90, 2.5f,  20,  0 },  48.0f,  10,  {170, 1.8f, 30,  1 },   5,  { 210, 2.5f, 38,  2 },   3,  { 75, 1.7f, 18,  3 } },
  { "Onda 10",  20,    0.5f,    {100, 2.5f,  20,  0 },  55.0f,  10,  {200, 2.0f, 35,  1 },   6,  { 220, 2.6f, 40,  2 },   4,  { 80, 1.8f, 20,  3 } },
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

  int spawnType = pickSpawnType(ws.spawnedCount, def);
  ws.spawnedCount++;
  ws.timer = def.spawnInterval;
  return spawnType;
}
