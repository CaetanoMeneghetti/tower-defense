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
  if (state.isPlacingTroop || state.isDrawingSpell || state.showBuyMenu ||
      io.WantCaptureMouse) {
    return selected;
  }

  // Detecta a borda de subida do botão esquerdo (clique único).
  static bool leftMouseWasDown = false;
  const bool leftMouseIsDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

  if (leftMouseIsDown && !leftMouseWasDown) {
    Vector<3> clickPos = getMouseGroundPosition(
        window, cam, cameraPosition, state.fbWidth, state.fbHeight);

    int hit = -1;
    float bestDist = kSelectionRadius;
    for (size_t i = 0; i < defenders.size(); ++i) {
      const float dx = defenders[i].position[0] - clickPos[0];
      const float dz = defenders[i].position[2] - clickPos[2];
      if (!collisions::inRange(defenders[i].position[0], defenders[i].position[2],
                                clickPos[0], clickPos[2], bestDist)) continue;
      bestDist = std::sqrt(dx * dx + dz * dz);
      hit = static_cast<int>(i);
    }
    if (hit >= 0) {
      selected = hit;
      audio::playOneShot("data/audio/selection_sound.mp3");
      state.cameraMode  = CameraMode::Orbital;
      state.orbitTarget = defenders[hit].position;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
  }
  leftMouseWasDown = leftMouseIsDown;

  // Botão direito: desseleciona e sai da câmera orbital.
  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
    if (selected >= 0 && state.cameraMode == CameraMode::Orbital) {
      state.cameraMode = CameraMode::Free;
      state.firstMouse = true;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    selected = -1;
  }

  return selected;
}

}  // namespace troop_selection
