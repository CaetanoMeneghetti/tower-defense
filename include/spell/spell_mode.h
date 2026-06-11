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

// Cor de cada feitiço, indexada pela classe da CNN (0=círculo, 1=quadrado,
// 2=triângulo). RGB em [0,1]. Usada tanto na forma perfeita desenhada na tela
// quanto nos ícones do HUD de compra.
inline constexpr float kSpellColors[3][3] = {
    {0.35f, 0.90f, 0.40f},  // círculo  — verde
    {0.30f, 0.55f, 1.00f},  // quadrado — azul
    {1.00f, 0.60f, 0.15f},  // triângulo — laranja
};

// Conjuração ativa: a forma perfeita que aparece na tela após a CNN classificar.
// Fica centrada no centro do desenho (centerX/centerY em pixels) com "raio"
// radius (metade do lado do bbox do desenho). O efeito de gameplay é aplicado
// uma única vez (quando applied passa a true, no main) a todos os inimigos cujo
// ponto na tela cai dentro da forma; depois ela só faz fade-out (life -> 0).
struct SpellCast {
  int   shape;             // 0=círculo, 1=quadrado, 2=triângulo
  float centerX, centerY;  // centro na tela (px)
  float radius;            // metade do lado / raio na tela (px)
  float life;              // tempo restante do visual (s)
  float maxLife;           // duração total (para o fade)
  bool  applied = false;   // efeito de gameplay já aplicado?
};

struct SpellMode {
  bool fKeyDown = false;
  bool enterKeyDown = false;
  bool lmbDown = false;

  std::vector<Stroke> strokes;  // todos os traços do desenho atual

  Canvas lastCanvas;            // último canvas 50×50 que foi pra inferência
  bool hasCanvas = false;       // se já houve uma classificação
  ClassificationResult lastResult;
  float lastProbs[3] = {0, 0, 0};

  std::vector<SpellCast> casts;  // formas perfeitas ativas (efeito + fade)
};

// True se o ponto de tela (px,py) está dentro da forma perfeita da conjuração.
bool pointInCast(const SpellCast &c, float px, float py);

void init(SpellMode &s);

// Por frame: trata F/ENTER/LMB. Mantém state.isDrawingSpell sincronizado.
// deltaTime alimenta o fade-out das formas perfeitas conjuradas.
void update(SpellMode &s,
            AppState &state,
            GLFWwindow *window,
            ShapeClassifier &classifier,
            float deltaTime);

// Render via ImGui (drawlist do foreground + janela de resultado + debug crop).
// Chamar entre ImGui::NewFrame() e ImGui::Render().
void render(const SpellMode &s, const AppState &state);

}  // namespace spell
