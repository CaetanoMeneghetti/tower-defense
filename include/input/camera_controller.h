#pragma once

#include <GLFW/glfw3.h>

#include "engine/camera.h"
#include "game/app_state.h"
#include "math/vector.h"

// =============================================================================
// CONTROLE DE CÂMERA — ORBITAL + RAYCAST DE MOUSE PARA O CHÃO
// =============================================================================

// Atualiza cameraPos em coordenadas esféricas baseadas em (orbitYaw, orbitPitch, orbitRadius).
void updateOrbitalCameraPosition(const AppState &s, Vector<3> &cameraPos);

// Raycasting do cursor contra o plano Y = 0 (chão), em world space.
// Retorna (0,0,0) quando o raio aponta para cima (não cruza o chão).
Vector<3> getMouseGroundPosition(GLFWwindow *window,
                                 const Camera &cam,
                                 const Vector<3> &camPos,
                                 int width,
                                 int height);
