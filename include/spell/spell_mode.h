#pragma once

#include <GLFW/glfw3.h>

#include <utility>
#include <vector>

#include "game/app_state.h"
#include "spell/canvas.h"
#include "spell/shape_classifier.h"

namespace spell {

// =============================================================================
// MODO DESENHO DE FEITIÇOS
// =============================================================================
// Tecla F entra/sai. Dentro do modo, o jogador desenha LIVREMENTE em qualquer
// lugar da tela (LMB segurado = um traço; soltar e pressionar de novo = traço
// novo). ENTER classifica.
//
// Ao classificar: calculamos o bounding box de todos os pontos, expandimos pra
// ser quadrado + 10% de padding, e rasterizamos no canvas 50×50 final.
// O canvas debug é mostrado no canto inferior direito (o que a CNN vê).

struct ScreenPoint { float x; float y; };
using Stroke = std::vector<ScreenPoint>;

struct SpellMode {
  bool fKeyDown = false;
  bool enterKeyDown = false;
  bool lmbDown = false;

  std::vector<Stroke> strokes;  // todos os traços do desenho atual

  Canvas lastCanvas;            // último canvas 50×50 que foi pra inferência
  bool hasCanvas = false;       // se já houve uma classificação
  ClassificationResult lastResult;
  float lastProbs[3] = {0, 0, 0};
};

void init(SpellMode &s);

// Por frame: trata F/ENTER/LMB. Mantém state.isDrawingSpell sincronizado.
void update(SpellMode &s,
            AppState &state,
            GLFWwindow *window,
            ShapeClassifier &classifier);

// Render via ImGui (drawlist do foreground + janela de resultado + debug crop).
// Chamar entre ImGui::NewFrame() e ImGui::Render().
void render(const SpellMode &s, const AppState &state);

}  // namespace spell
