#pragma once

#include <glm/glm.hpp>

#include "math/constants.h"

// =============================================================================
// CONSTANTES DE RENDERIZAÇÃO E JANELA
// =============================================================================
namespace render_constants {

constexpr int kWindowWidth = 1920;
constexpr int kWindowHeight = 1080;
constexpr char kWindowTitle[] = "1346AD: Iron & Blood";

constexpr float kFovDegrees = 45.0f;
constexpr float kNearPlane = 0.1f;
constexpr float kFarPlane = 100.0f;

// Fog noturna combinada com o ambient azulado da lua.
inline const glm::vec3 kFogColor = glm::vec3(0.02f, 0.03f, 0.07f);
constexpr float kFogStart = 60.0f;
constexpr float kFogEnd = 160.0f;

}  // namespace render_constants
