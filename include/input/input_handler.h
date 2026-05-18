#pragma once

#include <GLFW/glfw3.h>

#include "game/app_state.h"
#include "math/vector.h"

// =============================================================================
// CALLBACKS DO GLFW + PROCESSAMENTO DE INPUT POR FRAME
// =============================================================================
// O AppState fica acessível pelas callbacks via glfwSetWindowUserPointer.

// Helper para recuperar o AppState anexado à janela.
AppState &stateFromWindow(GLFWwindow *window);

// Registra as três callbacks (mouse move, scroll, framebuffer resize).
void registerInputCallbacks(GLFWwindow *window);

// Lê o teclado por frame: ESC, WASD/Space/Shift, toggles C e T.
void processInput(GLFWwindow *window, Vector<3> &cameraPosition, float deltaTime);
