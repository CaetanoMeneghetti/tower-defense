#pragma once

#include <GLFW/glfw3.h>

#include <vector>

#include "engine/camera.h"
#include "engine/game_object.h"
#include "game/app_state.h"
#include "math/vector.h"

// =============================================================================
// SELEÇÃO DE TROPA (clique no mundo)
// =============================================================================
// Clique esquerdo perto de um defensor (raio < kSelectionRadius) seleciona ele
// e trava a câmera orbital no alvo. Clique direito desseleciona e volta a
// câmera para o modo Livre. ImGui captura precedência sobre o clique.
// Não dispara quando state.isPlacingTroop é true (modo placement já consome).
namespace troop_selection {

constexpr float kSelectionRadius = 2.5f;

// Atualiza `selectedIndex` baseado no clique deste frame e mexe em
// `state.cameraMode` / `state.orbitTarget` quando um defensor é (des)selecionado.
// Retorna o índice atualizado (-1 se nenhum). Mantém estado interno via static
// para detectar a borda de subida do botão.
int update(GLFWwindow *window,
           const Camera &cam,
           const Vector<3> &cameraPosition,
           const std::vector<GameObject> &defenders,
           AppState &state,
           int currentSelectedIndex);

}  // namespace troop_selection
