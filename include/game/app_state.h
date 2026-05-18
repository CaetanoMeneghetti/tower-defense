#pragma once

#include "math/constants.h"
#include "render/render_constants.h"

// =============================================================================
// ESTADO GLOBAL DA APLICAÇÃO
// =============================================================================
// Compartilhado entre HUD, input e loop principal. Tudo que precisa persistir
// entre frames e ser visível por mais de um módulo mora aqui.
struct AppState {
  // ---- Jogador ----
  int gold = 99999;
  int health = 100;

  // ---- Posicionamento de tropas ----
  bool isPlacingTroop = false;
  int selectedTroopType = 0;

  // ---- Câmera (modo) ----
  bool useFreeCamera = false;
  bool cPressed = false;
  bool showCurve = false;
  bool tPressed = false;

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

  // ---- Tamanho do framebuffer ----
  int fbWidth = render_constants::kWindowWidth;
  int fbHeight = render_constants::kWindowHeight;
};
