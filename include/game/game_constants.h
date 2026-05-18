#pragma once

#include <glm/glm.hpp>

#include "math/constants.h"

// =============================================================================
// CONSTANTES DE GAMEPLAY
// =============================================================================
// Ajustes de jogabilidade (custos, ranges, tempos). Centralizadas aqui para
// facilitar balanceamento sem caçar valores no meio do código.
namespace game_constants {

// ---- Câmera ----
constexpr float kMouseSensitivity = 0.005f;
constexpr float kFreeCameraSpeed = 5.0f;
constexpr float kOrbitalAngularSpeed = 2.0f;
constexpr float kMaxPitchRad = 89.0f * math_constants::kDegToRad;
constexpr float kMinOrbitalRadius = 1.0f;
constexpr float kMaxOrbitalRadius = 50.0f;

// ---- Mapa / Path ----
constexpr float kMapHalfSize = 200.0f;
constexpr float kPathHalfWidth = 2.0f;

// ---- Árvores ----
constexpr float kTreeBorderMargin = 12.0f;
constexpr float kTreeMinSpacing = 6.0f;
constexpr float kTreePathBuffer = 6.0f;
constexpr float kTreeBaseScale = 1.0f;
constexpr int kTreeCount = 1000;
constexpr int kMaxTreeAttempts = kTreeCount * 40;

// ---- Lanternas ----
constexpr int kLanternCount = 8;
constexpr float kLanternScale = 0.01f;
// Distância do centro do path; path tem meia-largura 2.0, então fica na grama.
constexpr float kLanternPathOffset = 2.8f;
// Altura aproximada do bulbo/chama em world units (já escalado).
// BBox do OBJ: Y ∈ [-5, 85]cm; bulbo no meio ~ y=45cm → 0.9 world units.
constexpr float kLanternLightHeight = 0.9f;
// Laranja quente; valores > 1 para a luz "estourar" sobre o ambient noturno.
inline const glm::vec3 kLanternLightColor = glm::vec3(1.8f, 1.05f, 0.40f);

// ---- Combate ----
constexpr float kEnemyRespawnDelay = 2.0f;
constexpr float kEnemyHitFlashDuration = 0.15f;
constexpr float kArcherRange = 10.0f;
constexpr float kArcherShootInterval = 1.0f;
constexpr int kArcherArrowDamage = 10;

// ---- Custos de tropas ----
constexpr int kArcherCost = 50;
constexpr int kArquebusCost = 75;

// ---- Posicionamento de tropas ----
// Folga adicional sobre o path para o overlay vermelho de inválido.
constexpr float kTroopPathClearance = 0.5f;
// Distância máxima do caminho onde tropas podem ser colocadas.
constexpr float kTroopMaxDistanceFromPath = 12.0f;

// ---- Loop ----
constexpr double kTargetFps = 60.0;
constexpr double kFrameDelay = 1.0 / kTargetFps;

}  // namespace game_constants
