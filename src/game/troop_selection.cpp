#include "game/troop_selection.h"

#include <imgui.h>

#include <cmath>

#include "engine/audio.h"
#include "game/collisions.h"
#include "input/camera_controller.h"

namespace troop_selection {

int update(GLFWwindow *window,
           const Camera &cam,
           const Vector<3> &cameraPosition,
           const std::vector<GameObject> &defenders,
           AppState &state,
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
    float bestDist = kSelectionRadius;
    for (size_t i = 0; i < defenders.size(); ++i) {
      const float dx = defenders[i].position[0] - clickPos[0];
      const float dz = defenders[i].position[2] - clickPos[2];
      if (!collisions::inRange(defenders[i].position[0], defenders[i].position[2],
                                clickPos[0], clickPos[2], bestDist)) continue;
      bestDist = std::sqrt(dx * dx + dz * dz);
      selected = static_cast<int>(i);
    }

    if (selected >= 0) {
      state.cameraMode = CameraMode::Orbital;
      state.orbitTarget = defenders[selected].position;
      audio::playOneShot("data/audio/selectionsound.mp3");
    }
  }
  leftMouseWasDown = leftMouseIsDown;

  // Botão direito limpa a seleção e sai da câmera orbital.
  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
    selected = -1;
    if (state.cameraMode == CameraMode::Orbital) {
      state.cameraMode = CameraMode::Free;
      state.firstMouse = true;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
  }

  return selected;
}

}  // namespace troop_selection
