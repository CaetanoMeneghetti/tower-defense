#include "game/troop_placement.h"

#include <cstdlib>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include "game/collisions.h"
#include "game/game_constants.h"
#include "input/camera_controller.h"

void handleTroopPlacement(const TroopPlacementContext &ctx, AppState &state, float deltaTime) {
  using namespace game_constants;

  // Ignora o "click" enquanto o botão segue pressionado desde o clique na HUD.
  static bool waitingForRelease = true;
  if (waitingForRelease) {
    if (glfwGetMouseButton(ctx.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
      waitingForRelease = false;
    }
  }

  // 1. Coordenada 3D do chão baseada no mouse
  Vector<3> groundPos = getMouseGroundPosition(
      ctx.window, ctx.cam, ctx.cameraPosition, state.fbWidth, state.fbHeight);

  // 2 + 3. Validação de posicionamento (via colisões centralizadas)
  auto placement = collisions::checkPlacement(
      ctx.curvePoints, groundPos[0], groundPos[2],
      kPathHalfWidth, kTroopPathClearance, kTroopMaxDistanceFromPath);
  bool isInvalidPlacement = placement.invalid();

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
  const TroopDef *previewDef;
  if (state.selectedTroopType == defender_types::kArcher)
    previewDef = &ctx.archerClass;
  else if (state.selectedTroopType == defender_types::kArquebus)
    previewDef = &ctx.arquebusClass;
  else if (state.selectedTroopType == defender_types::kKnight)
    previewDef = &ctx.knightClass;
  else
    previewDef = &ctx.cannonClass;
  GameObject previewGhost(previewDef, groundPos);
  if (state.selectedTroopType == defender_types::kKnight)
    previewGhost.setAnimation("knightIdle");
  else
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
      newArcher.damage   = kArcherArrowDamage;
      newArcher.range    = kArcherRange;
      newArcher.fireRate = 2.0f;
      newArcher.setIdleAnimations({"idle1"});
      ctx.defenders.push_back(newArcher);
      ctx.defenderShoots.push_back(DefenderShoot{});
    } else if (state.selectedTroopType == defender_types::kArquebus) {
      state.gold -= kArquebusCost;
      GameObject newArquebus(&ctx.arquebusClass, groundPos);
      newArquebus.damage   = kArquebusBulletDamage;
      newArquebus.range    = kArquebusRange;
      newArquebus.fireRate = 2.0f;
      newArquebus.setIdleAnimations({"idle1"});
      ctx.defenders.push_back(newArquebus);
      ctx.defenderShoots.push_back(DefenderShoot{});
    } else if (state.selectedTroopType == defender_types::kCannon) {
      state.gold -= kCannonCost;
      GameObject newCannon(&ctx.cannonClass, groundPos);
      newCannon.damage   = kCannonDamage;
      newCannon.range    = kCannonRange;
      newCannon.fireRate = kCannonBaseReload;  // recarga base (cooldown entre ciclos)
      newCannon.setIdleAnimations({"idle1"});
      ctx.defenders.push_back(newCannon);
      ctx.defenderShoots.push_back(DefenderShoot{});
    } else if (state.selectedTroopType == defender_types::kKnight) {
      state.gold -= kKnightCost;
      GameObject newKnight(&ctx.knightClass, groundPos);
      newKnight.damage   = 1;    // invocações por comando
      newKnight.range    = 0.0f;
      newKnight.fireRate = game_constants::kKnightCommandInterval;  // intervalo base nível 1
      newKnight.setIdleAnimations({"knightIdle"});
      ctx.defenders.push_back(newKnight);
      DefenderShoot knightShoot{};
      // Offset aleatório para que comandantes colocados em momentos diferentes não disparem em sincronia
      knightShoot.shootTimer = static_cast<float>(std::rand() % 7);
      ctx.defenderShoots.push_back(knightShoot);
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
