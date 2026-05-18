#include "game/troop_selection.h"

#include <imgui.h>

#include <cmath>

#include "input/camera_controller.h"

namespace troop_selection {

int update(GLFWwindow *window,
           const Camera &cam,
           const Vector<3> &cameraPosition,
           const std::vector<GameObject> &defenders,
           const AppState &state,
           int currentSelectedIndex) {
  int selected = currentSelectedIndex;
  ImGuiIO &io = ImGui::GetIO();
  if (state.isPlacingTroop || io.WantCaptureMouse) {
    return selected;
  }

  // Detecta a borda de subida do botão esquerdo (clique único).
  static bool leftMouseWasDown = false;
  const bool leftMouseIsDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

  if (leftMouseIsDown && !leftMouseWasDown) {
    Vector<3> clickPos = getMouseGroundPosition(
        window, cam, cameraPosition, state.fbWidth, state.fbHeight);

    selected = -1;
    float minDist = kSelectionRadius;
    for (size_t i = 0; i < defenders.size(); ++i) {
      const float dx = defenders[i].position[0] - clickPos[0];
      const float dz = defenders[i].position[2] - clickPos[2];
      const float dist = std::sqrt(dx * dx + dz * dz);
      if (dist < minDist) {
        minDist = dist;
        selected = static_cast<int>(i);
      }
    }
  }
  leftMouseWasDown = leftMouseIsDown;

  // Botão direito limpa a seleção.
  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
    selected = -1;
  }

  return selected;
}

}  // namespace troop_selection
