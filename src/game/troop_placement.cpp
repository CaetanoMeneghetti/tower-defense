#include "game/troop_placement.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include "game/game_constants.h"
#include "game/path_navigation.h"
#include "input/camera_controller.h"

void handleTroopPlacement(const TroopPlacementContext &ctx, AppState &state, float deltaTime) {
  using namespace game_constants;

  // Bloqueia o disparo do "click" enquanto o botão do mouse ainda está
  // pressionado desde o clique no botão da HUD.
  static bool waitingForRelease = true;
  if (waitingForRelease) {
    if (glfwGetMouseButton(ctx.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
      waitingForRelease = false;
    }
  }

  // 1. Coordenada 3D do chão baseada no mouse
  Vector<3> groundPos = getMouseGroundPosition(
      ctx.window, ctx.cam, ctx.cameraPosition, state.fbWidth, state.fbHeight);

  // 2. Distância exata do mouse até a linha central do caminho
  float distToPath = distanceToPath(ctx.curvePoints, groundPos[0], groundPos[2]);

  // 3. Validações: em cima do path? muito longe?
  bool isOnPath = (distToPath < (kPathHalfWidth + kTroopPathClearance));
  bool isTooFar = (distToPath > kTroopMaxDistanceFromPath);
  bool isInvalidPlacement = isOnPath || isTooFar;

  // 4. Cor do holograma
  glm::vec4 hColor =
      isInvalidPlacement
          ? glm::vec4(1.0f, 0.2f, 0.2f, 0.5f)   // Vermelho translúcido
          : glm::vec4(0.2f, 0.7f, 1.0f, 0.5f);  // Azul ciano translúcido

  // 5. Setup blending
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // 6. Shader + uniforms do preview
  glUseProgram(ctx.previewShader);
  glUniformMatrix4fv(glGetUniformLocation(ctx.previewShader, "view"), 1, GL_FALSE, ctx.glView);
  glUniformMatrix4fv(
      glGetUniformLocation(ctx.previewShader, "projection"), 1, GL_FALSE, ctx.glProj);
  glUniform4fv(
      glGetUniformLocation(ctx.previewShader, "previewColor"), 1, glm::value_ptr(hColor));

  // 7. Desenha o "fantasma" na posição do mouse
  const TroopDef *previewDef = (state.selectedTroopType == defender_types::kArcher)
                                   ? &ctx.archerClass
                                   : &ctx.arquebusClass;
  GameObject previewGhost(previewDef, groundPos);
  previewGhost.setAnimation("idle1");
  previewGhost.update(deltaTime);
  previewGhost.draw(ctx.previewShader);

  glDisable(GL_BLEND);

  // 8. Confirmação do clique esquerdo
  ImGuiIO &io = ImGui::GetIO();
  if (!waitingForRelease && !isInvalidPlacement &&
      glfwGetMouseButton(ctx.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
      !io.WantCaptureMouse) {
    if (state.selectedTroopType == defender_types::kArcher) {
      state.gold -= kArcherCost;
      GameObject newArcher(&ctx.archerClass, groundPos);
      newArcher.setIdleAnimations({"idle1"});
      ctx.defenders.push_back(newArcher);
      ctx.defenderShoots.push_back(DefenderShoot{0.0f, false});
    } else if (state.selectedTroopType == defender_types::kArquebus) {
      state.gold -= kArquebusCost;
      GameObject newArquebus(&ctx.arquebusClass, groundPos);
      newArquebus.setIdleAnimations({"idle1"});
      ctx.defenders.push_back(newArquebus);
      ctx.defenderShoots.push_back(DefenderShoot{0.0f, false});
    }

    state.isPlacingTroop = false;
    waitingForRelease = true;
  }

  // 9. Cancelar com botão direito
  if (glfwGetMouseButton(ctx.window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
    state.isPlacingTroop = false;
    waitingForRelease = true;
  }
}
