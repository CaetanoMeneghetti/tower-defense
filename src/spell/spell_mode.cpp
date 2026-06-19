#include "spell/spell_mode.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "game/game_constants.h"

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

// Centro (px) e "raio" (metade do maior lado do bbox, px) dos traços na tela.
// Usado para posicionar a forma perfeita no mesmo centro do desenho.
bool strokesScreenBounds(const std::vector<Stroke> &strokes,
                         float &cx, float &cy, float &radius) {
  float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
  bool hasPoint = false;
  for (const auto &s : strokes) {
    for (const auto &p : s) {
      minX = std::min(minX, p.x); minY = std::min(minY, p.y);
      maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y);
      hasPoint = true;
    }
  }
  if (!hasPoint) return false;
  cx = 0.5f * (minX + maxX);
  cy = 0.5f * (minY + maxY);
  radius = 0.5f * std::max(maxX - minX, maxY - minY);
  if (radius < 8.0f) radius = 8.0f;  // mínimo para formas/clique muito pequenos
  return true;
}

// Vértices da forma perfeita na tela. Para o triângulo, um equilátero apontando
// para cima centrado em (cx,cy) com circunraio r (y cresce para baixo na tela).
void triangleVertices(float cx, float cy, float r, ImVec2 v[3]) {
  v[0] = ImVec2(cx,                      cy - r);          // topo
  v[1] = ImVec2(cx + 0.8660254f * r,     cy + 0.5f * r);   // base direita
  v[2] = ImVec2(cx - 0.8660254f * r,     cy + 0.5f * r);   // base esquerda
}

}  // namespace

// Teste ponto-dentro-da-forma por tipo (0=círculo, 1=quadrado, 2=triângulo).
bool pointInCast(const SpellCast &c, float px, float py) {
  const float dx = px - c.centerX;
  const float dy = py - c.centerY;
  if (c.shape == 1) {  // quadrado: axis-aligned, meio-lado = radius
    return std::abs(dx) <= c.radius && std::abs(dy) <= c.radius;
  }
  if (c.shape == 2) {  // triângulo: teste por sinais das três arestas
    ImVec2 v[3];
    triangleVertices(c.centerX, c.centerY, c.radius, v);
    auto edge = [](const ImVec2 &a, const ImVec2 &b, float px, float py) {
      return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
    };
    float d0 = edge(v[0], v[1], px, py);
    float d1 = edge(v[1], v[2], px, py);
    float d2 = edge(v[2], v[0], px, py);
    bool hasNeg = (d0 < 0) || (d1 < 0) || (d2 < 0);
    bool hasPos = (d0 > 0) || (d1 > 0) || (d2 > 0);
    return !(hasNeg && hasPos);
  }
  // círculo (0): distância <= raio
  return dx * dx + dy * dy <= c.radius * c.radius;
}

void init(SpellMode &s) {
  clear(s.lastCanvas);
}

void update(SpellMode &s,
            AppState &state,
            GLFWwindow *window,
            ShapeClassifier &classifier,
            float deltaTime) {
  // F (borda de subida) toggla o modo. O desenho só pode ser INICIADO na câmera
  // aérea (vista de cima do mapa); sair é sempre permitido.
  bool fNow = (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS);
  if (fNow && !s.fKeyDown) {
    if (!state.isDrawingSpell && state.cameraMode != CameraMode::Aerial) {
      // Fora da câmera aérea, F não faz nada.
    } else {
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
  }
  s.fKeyDown = fNow;

  // Decai as formas perfeitas ativas (fade) e remove as que expiraram.
  for (auto &c : s.casts) c.life -= deltaTime;
  s.casts.erase(std::remove_if(s.casts.begin(), s.casts.end(),
                               [](const SpellCast &c) { return c.life <= 0.0f; }),
                s.casts.end());

  if (!state.isDrawingSpell) return;

  // ENTER (borda de subida): rasteriza + classifica.
  bool enterNow = (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS);
  if (enterNow && !s.enterKeyDown && !s.strokes.empty()) {
    // Centro/raio do desenho na tela ANTES de limpar os traços — a forma
    // perfeita nascerá centrada no mesmo ponto do desenho.
    float drawCx = 0.0f, drawCy = 0.0f, drawR = 0.0f;
    bool hasBounds = strokesScreenBounds(s.strokes, drawCx, drawCy, drawR);

    rasterizeStrokes(s.strokes, s.lastCanvas);
    s.lastResult = classifier.classify(s.lastCanvas);
    for (int i = 0; i < 3; ++i) s.lastProbs[i] = s.lastResult.probs[i];
    s.hasCanvas = true;

    // Lança o feitiço SOMENTE se a forma foi reconhecida e o jogador tem carga
    // daquele feitiço. Sem carga (ou abaixo do threshold) -> não acontece nada.
    const int cls = s.lastResult.classIndex;
    if (hasBounds && cls >= 0 && cls < 3 && state.spellCharges[cls] > 0) {
      state.spellCharges[cls]--;
      SpellCast cast;
      cast.shape   = cls;
      cast.centerX = drawCx;
      cast.centerY = drawCy;
      cast.radius  = drawR;
      cast.life    = game_constants::kSpellVisualDuration;
      cast.maxLife = game_constants::kSpellVisualDuration;
      cast.applied = false;  // main aplica o efeito aos inimigos neste frame
      s.casts.push_back(cast);
    }

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

  // ---- Formas perfeitas conjuradas (com fade-out) ----
  // Após a CNN reconhecer o desenho, mostramos a forma geométrica PERFEITA na
  // cor do feitiço, centrada no centro do desenho. Some suavemente em maxLife.
  for (const auto &c : s.casts) {
    float t = (c.maxLife > 0.0f) ? (c.life / c.maxLife) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    const float *rgb = kSpellColors[c.shape];
    int a    = static_cast<int>(200.0f * t);   // contorno
    int aFill = static_cast<int>(70.0f * t);    // preenchimento
    ImU32 line = IM_COL32(int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255), a);
    ImU32 fill = IM_COL32(int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255), aFill);
    const float th = 4.0f;
    if (c.shape == 1) {  // quadrado
      ImVec2 p0(c.centerX - c.radius, c.centerY - c.radius);
      ImVec2 p1(c.centerX + c.radius, c.centerY + c.radius);
      dl->AddRectFilled(p0, p1, fill);
      dl->AddRect(p0, p1, line, 0.0f, 0, th);
    } else if (c.shape == 2) {  // triângulo
      ImVec2 v[3];
      triangleVertices(c.centerX, c.centerY, c.radius, v);
      dl->AddTriangleFilled(v[0], v[1], v[2], fill);
      dl->AddTriangle(v[0], v[1], v[2], line, th);
    } else {  // círculo
      dl->AddCircleFilled(ImVec2(c.centerX, c.centerY), c.radius, fill, 48);
      dl->AddCircle(ImVec2(c.centerX, c.centerY), c.radius, line, 48, th);
    }
  }

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

    // ---- Painel do modo feitiço (topo, centralizado) ----
    // Mostra as 3 formas disponíveis (cor + nome + cargas) e as instruções, em
    // vez de só um texto solto. Formas sem carga aparecem esmaecidas.
    const float panelW = 560.0f, panelH = 88.0f;
    const float panelX = (state.fbWidth - panelW) * 0.5f;
    const float panelY = 12.0f;
    dl->AddRectFilled(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH),
                      IM_COL32(20, 20, 25, 236), 10.0f);
    dl->AddRect(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH),
                IM_COL32(150, 110, 35, 255), 10.0f, 0, 2.0f);

    const char *title = "MODO FEITI\xc3\x87O";
    dl->AddText(ImVec2(panelX + (panelW - ImGui::CalcTextSize(title).x) * 0.5f, panelY + 8.0f),
                IM_COL32(255, 216, 70, 255), title);

    struct SpellHint { int cls; const char *name; };
    const SpellHint hints[3] = {{1, "Lentid\xc3\xa3o"}, {2, "Dano"}, {0, "Veneno"}};
    const float colW = panelW / 3.0f;
    const float iconY = panelY + 48.0f;
    for (int i = 0; i < 3; ++i) {
      const int cls = hints[i].cls;
      const int charges = state.spellCharges[cls];
      const bool has = charges > 0;
      const float *rgb = kSpellColors[cls];
      const ImU32 col = IM_COL32(int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255),
                                 has ? 255 : 80);
      const float cx = panelX + colW * i + 24.0f;
      const float r = 11.0f;
      if (cls == 1) {
        dl->AddRectFilled(ImVec2(cx - r, iconY - r), ImVec2(cx + r, iconY + r), col, 2.0f);
      } else if (cls == 2) {
        ImVec2 v[3];
        triangleVertices(cx, iconY, r, v);
        dl->AddTriangleFilled(v[0], v[1], v[2], col);
      } else {
        dl->AddCircleFilled(ImVec2(cx, iconY), r, col, 24);
      }
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%s  x%d", hints[i].name, charges);
      dl->AddText(ImVec2(cx + r + 8.0f, iconY - 7.0f),
                  has ? IM_COL32(235, 228, 210, 255) : IM_COL32(120, 115, 105, 255), buf);
    }

    const char *instr = "Desenhe a forma    \xe2\x80\xa2    ENTER confirma    \xe2\x80\xa2    F sai";
    dl->AddText(ImVec2(panelX + (panelW - ImGui::CalcTextSize(instr).x) * 0.5f,
                       panelY + panelH - 20.0f),
                IM_COL32(170, 162, 148, 255), instr);
  }

  // ---- Janela de resultado + debug crop ----
  // Aparece quando há resultado, OU quando estamos em modo desenho (pra ver o
  // crop debug do último desenho mesmo antes de classificar de novo).
  if (s.hasCanvas) {
    // Abaixo da janela de Debug (que cresce a partir de y=120) para não sobrepor.
    ImGui::SetNextWindowPos(ImVec2(10.0f, 300.0f), ImGuiCond_Always);
    ImGuiWindowFlags wflags = ImGuiWindowFlags_AlwaysAutoResize |
                              ImGuiWindowFlags_NoSavedSettings |
                              ImGuiWindowFlags_NoCollapse |
                              ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("Feiti\xc3\xa7o", nullptr, wflags)) {
      const ImVec4 gold(1.0f, 0.85f, 0.20f, 1.0f);
      const ImVec4 green(0.40f, 0.90f, 0.40f, 1.0f);
      const ImVec4 muted(0.65f, 0.60f, 0.50f, 1.0f);

      ImGui::TextColored(gold, "RESULTADO");
      ImGui::Separator();

      if (s.lastResult.classIndex >= 0) {
        ImGui::SetWindowFontScale(1.25f);
        ImGui::TextColored(gold, "%s", s.lastResult.label.c_str());
        ImGui::SetWindowFontScale(1.0f);
      } else {
        ImGui::TextColored(muted, "Abaixo do limiar");
      }
      ImGui::TextColored(s.lastResult.confidence >= 0.5f ? green : muted,
                         "Confian\xc3\xa7""a: %.1f%%", s.lastResult.confidence * 100.0f);
      ImGui::Spacing();

      // Barras de probabilidade por forma, cada uma na cor do feitiço.
      const char *names[3] = {"C\xc3\xadrculo", "Quadrado", "Tri\xc3\xa2ngulo"};
      for (int i = 0; i < 3; ++i) {
        const float *rgb = kSpellColors[i];
        const ImVec4 col(rgb[0], rgb[1], rgb[2], 1.0f);
        ImGui::TextColored(col, "%s", names[i]);
        ImGui::SameLine(90.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
        ImGui::ProgressBar(s.lastProbs[i], ImVec2(130.0f, 14.0f));
        ImGui::PopStyleColor();
      }
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
