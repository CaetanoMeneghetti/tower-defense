#pragma once

#include "math/constants.h"
#include "math/vector.h"
#include "render/render_constants.h"

// =============================================================================
// ESTADO GLOBAL DA APLICAÇÃO
// =============================================================================
// Compartilhado entre HUD, input e loop principal. Tudo que precisa persistir
// entre frames e ser visível por mais de um módulo fica aqui

// Três modos de câmera:
//  - Free:    livre estilo FPS (WASD + mouse). Default.
//  - Aerial:  look-at fixa do alto olhando direto para baixo.
//  - Orbital: orbita em torno de uma entidade clicada (HUD de unidade).
enum class CameraMode { Free, Aerial, Orbital };

struct AppState {
  // ---- Jogador ----
  int gold = 99999;
  int health = 100;

  // ---- Posicionamento de tropas ----
  bool isPlacingTroop = false;
  int selectedTroopType = 0;

  // ---- Desenho de feitiço ----
  // Quando true: câmera/seleção/placement ficam congelados; F sai.
  bool isDrawingSpell = false;

  // Cargas de feitiço possuídas (consumíveis). Índice = classe da CNN:
  // 0=círculo (veneno), 1=quadrado (lentidão), 2=triângulo (metade da vida).
  // Comprar no HUD incrementa; desenhar a forma decrementa. Só é possível
  // lançar um feitiço com carga > 0.
  int spellCharges[3] = {0, 0, 0};

  // ---- Câmera (modo) ----
  CameraMode cameraMode = CameraMode::Free;
  bool cPressed = false;
  bool showCurve = false;
  bool tPressed = false;

  // Alvo da câmera orbital (entidade focada). Atualizado ao clicar em uma
  // unidade; usado como centro da órbita.
  Vector<3> orbitTarget = {0.0f, 0.0f, 0.0f};

  // ---- Câmera livre (FPS) ----
  float yaw = -math_constants::kHalfPi;
  float pitch = 0.0f;
  float lastX = render_constants::kWindowWidth / 2.0f;
  float lastY = render_constants::kWindowHeight / 2.0f;
  bool firstMouse = true;

  // ---- Câmera orbital ----
  float orbitYaw = 0.0f;
  float orbitPitch = math_constants::kPi / 6.0f;
  float orbitRadius = 10.0f;

  // ---- Câmera aérea (pan no plano XZ; sem subir/descer) ----
  float aerialPanX = 0.0f;
  float aerialPanZ = 0.0f;

  // ---- Tamanho do framebuffer ----
  int fbWidth = render_constants::kWindowWidth;
  int fbHeight = render_constants::kWindowHeight;
};
