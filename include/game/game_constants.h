#pragma once

#include <glm/glm.hpp>

#include "math/constants.h"

// =============================================================================
// CONSTANTES DE GAMEPLAY
// =============================================================================

namespace game_constants {

// ---- Câmera ----
constexpr float kMouseSensitivity = 0.005f;
constexpr float kFreeCameraSpeed = 8.0f;
constexpr float kOrbitalAngularSpeed = 2.0f;
constexpr float kMaxPitchRad = 89.0f * math_constants::kDegToRad;
constexpr float kMinOrbitalRadius = 1.0f;
constexpr float kMaxOrbitalRadius = 50.0f;
// Câmera aérea: altura fixa e deslocamento (pan) no plano XZ.
constexpr float kAerialCameraHeight = 40.0f;   // altura da câmera olhando para baixo
constexpr float kAerialPanSpeed     = 18.0f;   // velocidade do pan (unidades/seg)
constexpr float kAerialPanLimit     = 30.0f;   // deslocamento máx. a partir do centro

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
// Distância de arco entre duas lanternas consecutivas no mesmo lado.
// A contagem total é derivada de (totalArcLength / kLanternSpacing) * 2,
// cappada em (kMaxPointLights - 4) para reservar slots para tochas de canhão.
constexpr float kLanternSpacing = 12.0f;
constexpr float kLanternScale = 0.01f;
// Distância do centro do path; path tem meia-largura 2.0 + ruído máx 0.4 = 2.4.
constexpr float kLanternPathOffset = 3.2f;
// Altura aproximada do bulbo/chama em world units (já escalado).
// BBox do OBJ: Y ∈ [-5, 85]cm; bulbo no meio ~ y=45cm → 0.9 world units.
constexpr float kLanternLightHeight = 0.9f;
// Laranja quente; valores > 1 para a luz "estourar" sobre o ambient noturno.
inline const glm::vec3 kLanternLightColor = glm::vec3(1.8f, 1.05f, 0.40f);

// ---- Combate ----
//Arqueiro
constexpr float kEnemyRespawnDelay = 2.0f;
constexpr float kEnemyHitFlashDuration = 0.15f;
constexpr float kArcherRange = 10.0f;
constexpr float kArcherShootInterval = 1.0f;
constexpr int kArcherArrowDamage = 10;

//Arcabuz
constexpr float kArquebusRange          = 10.0f;
constexpr float kArquebusFireDuration   = 0.7f;   // duração da animação de disparo
constexpr float kArquebusReloadDuration = 1.4f;   // duração da animação de reload
constexpr int   kArquebusBulletDamage   = 20;
// ---- Canhão ----
constexpr float kCannonRange         = 12.0f;
constexpr float kCannonFireDuration  = 1.2f;   // duração da animação de disparo
constexpr float kCannonCowerDuration = 2.0f;   // duração da animação de encolhimento
constexpr int   kCannonDamage        = 40;
constexpr float kCannonBaseReload    = 3.0f;   // recarga base no nível 1 (cooldown entre ciclos, em s)
constexpr float kCannonSplashRadius  = 3.0f;   // raio do dano em área do disparo do canhão
constexpr float kCannonBarrelOffset  = 1.0f;   // origem do mesh cannon.obj à frente do canhoneiro
constexpr float kCannonBarrelScale   = 0.45f;  // escala do mesh cannon.obj
constexpr float kCannonerBehindOffset = 0.7f;  // quanto o canhoneiro recua do ponto lógico

// ---- Custos de tropas ----
constexpr int kArcherCost   = 50;
constexpr int kArquebusCost = 75;
constexpr int kCannonCost   = 100;
constexpr int kKnightCost   = 250;

// ---- Feitiços (desenho + CNN) ----
// Cada feitiço é consumível: comprar adiciona 1 carga; desenhar a forma gasta a
// carga correspondente. Índice de classe da CNN: 0=círculo, 1=quadrado, 2=triângulo.
constexpr float kSpellSlowMultiplier = 0.75f;  // quadrado: anda a 75% da velocidade
constexpr float kSpellPoisonFraction = 0.10f;  // círculo: 10% da vida máx por segundo
constexpr float kSpellPoisonDuration = 8.0f;   // círculo: duração do veneno (s)
constexpr int   kSpellCircleCost     = 90;     // veneno
constexpr int   kSpellSquareCost     = 60;     // lentidão
constexpr int   kSpellTriangleCost   = 120;    // metade da vida
// Tempo que a forma perfeita fica visível na tela após o lançamento (fade-out).
constexpr float kSpellVisualDuration = 1.2f;

// --- Espadachim (path walker)
constexpr int   kKnightMaxHp = 80;
constexpr float kKnightSpeed = 1.5f;
constexpr int   kKnightSlashDamage = 10;
constexpr float kKnightSlashInterval = 0.8f;
constexpr float kKnightCollisionRadius = 1.2f;
// Semi-extensões (AABB no plano XZ) da colisão de combate corpo-a-corpo. A soma
// das duas reproduz o antigo raio de contato (kKnightCollisionRadius).
constexpr float kKnightMeleeHalfExtent = kKnightCollisionRadius * 0.5f;
constexpr float kEnemyMeleeHalfExtent  = kKnightCollisionRadius * 0.5f;

// --- Comandante (defender colocável)
constexpr int   kKnightCommanderCost         = 80;
constexpr float kKnightCommandInterval       = 10.0f;  // segundos entre invocações
constexpr float kKnightCommandAnimDuration   = 4.0f;   // duração da animação de comando
// ---- Posicionamento de tropas ----
// Folga adicional sobre o path para o overlay vermelho de inválido.
constexpr float kTroopPathClearance = 0.5f;
// Distância máxima do caminho onde tropas podem ser colocadas.
constexpr float kTroopMaxDistanceFromPath = 12.0f;

// ---- Loop ----
constexpr double kTargetFps = 60.0;
constexpr double kFrameDelay = 1.0 / kTargetFps;

}  // namespace game_constants
