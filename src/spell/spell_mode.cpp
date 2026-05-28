#include "spell/spell_mode.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace spell {

namespace {

constexpr float kStrokeThicknessPx = 4.0f;  // grossura da linha branca na tela
// Padding em torno do bbox. O modelo atual foi treinado com RandomZoom
// (-0.6, 0.1) — tolera formas grandes (~75-125% do canvas). 0.15 deixa o
// desenho ocupar ~77% do canvas, fica visível e dentro da distribuição.
constexpr float kBBoxPadding       = 0.15f;
constexpr int   kDebugCropPx       = 150;   // tamanho do crop de debug na tela

// Constrói o canvas 50x50 a partir dos traços coletados em coords de tela.
// Faz bbox-fit (quadrado + padding) para o desenho ocupar quase todo o quadro,
// equiparando ao que a CNN viu no treino (desenhos centralizados em 50x50).
void rasterizeStrokes(const std::vector<Stroke> &strokes, Canvas &out) {
  clear(out);
  if (strokes.empty()) return;

  float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
  bool hasPoint = false;
  for (const auto &s : strokes) {
    for (const auto &p : s) {
      minX = std::min(minX, p.x); minY = std::min(minY, p.y);
      maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y);
      hasPoint = true;
    }
  }
  if (!hasPoint) return;

  // Quadrado em torno do bbox + folga.
  float w = maxX - minX;
  float h = maxY - minY;
  float side = std::max(w, h);
  if (side < 1.0f) side = 1.0f;  // ponto único — evita div/0
  side *= (1.0f + 2.0f * kBBoxPadding);
  float cx = 0.5f * (minX + maxX);
  float cy = 0.5f * (minY + maxY);
  float originX = cx - 0.5f * side;
  float originY = cy - 0.5f * side;

  auto toCanvas = [&](float sx, float sy, int &outX, int &outY) {
    float u = (sx - originX) / side;  // [0, 1]
    float v = (sy - originY) / side;
    int ix = static_cast<int>(u * kCanvasSize);
    int iy = static_cast<int>(v * kCanvasSize);
    if (ix < 0) ix = 0;
    if (iy < 0) iy = 0;
    if (ix >= kCanvasSize) ix = kCanvasSize - 1;
    if (iy >= kCanvasSize) iy = kCanvasSize - 1;
    outX = ix;
    outY = iy;
  };

  for (const auto &stroke : strokes) {
    if (stroke.empty()) continue;
    int prevX, prevY;
    toCanvas(stroke[0].x, stroke[0].y, prevX, prevY);
    plot(out, prevX, prevY);
    for (size_t i = 1; i < stroke.size(); ++i) {
      int x, y;
      toCanvas(stroke[i].x, stroke[i].y, x, y);
      drawLine(out, prevX, prevY, x, y);
      prevX = x; prevY = y;
    }
  }
}

}  // namespace

void init(SpellMode &s) {
  clear(s.lastCanvas);
}

void update(SpellMode &s,
            AppState &state,
            GLFWwindow *window,
            ShapeClassifier &classifier) {
  // F (borda de subida) toggla o modo.
  bool fNow = (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS);
  if (fNow && !s.fKeyDown) {
    state.isDrawingSpell = !state.isDrawingSpell;
    if (state.isDrawingSpell) {
      s.strokes.clear();
      s.hasCanvas = false;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    } else {
      // Restaura cursor se a câmera Livre estiver ativa.
      if (state.cameraMode == CameraMode::Free) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        state.firstMouse = true;
      }
    }
  }
  s.fKeyDown = fNow;

  if (!state.isDrawingSpell) return;

  // ENTER (borda de subida): rasteriza + classifica.
  bool enterNow = (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS);
  if (enterNow && !s.enterKeyDown && !s.strokes.empty()) {
    rasterizeStrokes(s.strokes, s.lastCanvas);
    s.lastResult = classifier.classify(s.lastCanvas);
    for (int i = 0; i < 3; ++i) s.lastProbs[i] = s.lastResult.probs[i];
    s.hasCanvas = true;

    // ---- Salva o canvas 50x50 como PGM em captures/ ----
    // Formato igual ao dataset original: traço preto (0), fundo branco (255).
    // Útil pra rodar o mesmo desenho pela CNN Keras via script Python e
    // confirmar se o erro está no C++ ou no modelo.
    {
      namespace fs = std::filesystem;
      fs::path dir = "captures";
      fs::create_directories(dir);
      static int counter = 0;
      // Encontra próximo índice livre só na primeira chamada.
      if (counter == 0) {
        for (const auto &e : fs::directory_iterator(dir)) {
          if (e.path().extension() == ".pgm") ++counter;
        }
      }
      char name[64];
      std::snprintf(name, sizeof(name), "capture_%03d.pgm", counter++);
      std::ofstream out(dir / name, std::ios::binary);
      out << "P5\n50 50\n255\n";
      for (int i = 0; i < kCanvasPixels; ++i) {
        unsigned char v = static_cast<unsigned char>(s.lastCanvas.pixels[i] * 255.0f);
        out.put(static_cast<char>(v));
      }
    }

    s.strokes.clear();
  }
  s.enterKeyDown = enterNow;

  // Acumula pontos enquanto LMB segurado.
  bool lmbNow = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
  if (lmbNow) {
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    ScreenPoint p{static_cast<float>(mx), static_cast<float>(my)};
    if (!s.lmbDown || s.strokes.empty()) {
      // Início de novo traço.
      s.strokes.emplace_back();
      s.strokes.back().push_back(p);
    } else {
      // Continua o traço atual — descarta repetidos pra evitar bloat.
      auto &cur = s.strokes.back();
      const auto &last = cur.back();
      if (std::abs(p.x - last.x) > 0.5f || std::abs(p.y - last.y) > 0.5f) {
        cur.push_back(p);
      }
    }
  }
  s.lmbDown = lmbNow;
}

void render(const SpellMode &s, const AppState &state) {
  ImDrawList *dl = ImGui::GetForegroundDrawList();

  // ---- Traços em desenho (linhas brancas grossas em cima da cena) ----
  if (state.isDrawingSpell) {
    const ImU32 stroke = IM_COL32(255, 255, 255, 255);
    for (const auto &str : s.strokes) {
      if (str.size() < 2) {
        if (!str.empty()) {
          dl->AddCircleFilled(ImVec2(str[0].x, str[0].y),
                               kStrokeThicknessPx * 0.5f, stroke);
        }
        continue;
      }
      // Polyline encadeada com cantos round.
      std::vector<ImVec2> pts;
      pts.reserve(str.size());
      for (const auto &p : str) pts.emplace_back(p.x, p.y);
      dl->AddPolyline(pts.data(), static_cast<int>(pts.size()),
                      stroke, ImDrawFlags_None, kStrokeThicknessPx);
    }

    // Dica no canto superior.
    dl->AddText(ImVec2(10.0f, 10.0f), IM_COL32(255, 255, 255, 230),
                "Desenhe com LMB | ENTER classifica | F sai");
  }

  // ---- Janela de resultado + debug crop ----
  // Aparece quando há resultado, OU quando estamos em modo desenho (pra ver o
  // crop debug do último desenho mesmo antes de classificar de novo).
  if (s.hasCanvas) {
    ImGui::SetNextWindowPos(ImVec2(10.0f, 200.0f), ImGuiCond_Always);
    ImGuiWindowFlags wflags = ImGuiWindowFlags_AlwaysAutoResize |
                              ImGuiWindowFlags_NoSavedSettings |
                              ImGuiWindowFlags_NoCollapse |
                              ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("Spell Classifier", nullptr, wflags)) {
      if (s.lastResult.classIndex >= 0) {
        ImGui::Text("Classe: %s", s.lastResult.label.c_str());
      } else {
        ImGui::Text("Classe: <abaixo do threshold>");
      }
      ImGui::Text("Confianca: %.1f%%", s.lastResult.confidence * 100.0f);
      ImGui::Separator();
      ImGui::Text("circle   %.3f", s.lastProbs[0]);
      ImGui::Text("square   %.3f", s.lastProbs[1]);
      ImGui::Text("triangle %.3f", s.lastProbs[2]);
    }
    ImGui::End();

    // Crop 50x50 no canto inferior direito (mostra o que a CNN realmente viu —
    // pós-inversão, ou seja, traço branco em fundo preto).
    const int cropSize = kDebugCropPx;
    ImVec2 cropP0(float(state.fbWidth - cropSize - 10),
                  float(state.fbHeight - cropSize - 10));
    ImVec2 cropP1(cropP0.x + cropSize, cropP0.y + cropSize);
    dl->AddRectFilled(cropP0, cropP1, IM_COL32(0, 0, 0, 230));
    dl->AddRect(cropP0, cropP1, IM_COL32(180, 180, 180, 255), 0.0f, 0, 1.5f);

    const float cell = float(cropSize) / float(kCanvasSize);
    for (int y = 0; y < kCanvasSize; ++y) {
      for (int x = 0; x < kCanvasSize; ++x) {
        // Inversão: aqui canvas tem bg=1, traço=0, então 1-v = o que a CNN vê.
        float v = 1.0f - s.lastCanvas.pixels[y * kCanvasSize + x];
        if (v <= 0.05f) continue;
        unsigned char g = (unsigned char)(v * 255.0f);
        ImU32 col = IM_COL32(g, g, g, 255);
        ImVec2 a(cropP0.x + x * cell, cropP0.y + y * cell);
        ImVec2 b(a.x + cell + 1.0f, a.y + cell + 1.0f);
        dl->AddRectFilled(a, b, col);
      }
    }
    
    dl->AddText(ImVec2(cropP0.x, cropP0.y - 16.0f),
                IM_COL32(220, 220, 220, 255),
                "Input da CNN (50x50)");
  }
}

}  // namespace spell
