#pragma once

#include <memory>
#include <string>

#include "spell/canvas.h"

namespace spell {

struct ClassifierImpl;  // PIMPL — pesos + ponteiros fatiados ficam no .cpp

struct ClassificationResult {
  int classIndex = -1;        // -1 = abaixo do threshold (ou erro)
  float confidence = 0.0f;    // sempre o max softmax, mesmo quando rejeitado
  std::string label;          // vazio se classIndex < 0
  float probs[3] = {0, 0, 0}; // todas as probs por classe (debug/UI)
};

// Classificador de formas geométricas (circle/square/triangle) com inferência
// manual em CPU. Lê pesos float32 little-endian do arquivo passado em load();
// arquitetura é fixa (hardcoded em shape_classifier.cpp).
class ShapeClassifier {
 public:
  ShapeClassifier();
  ~ShapeClassifier();

  ShapeClassifier(const ShapeClassifier &) = delete;
  ShapeClassifier &operator=(const ShapeClassifier &) = delete;

  // Carrega os pesos. Retorna false em erro (mensagem detalhada em stderr).
  bool load(const std::string &modelPath);

  // Roda regressão contra PNGs grayscale 50x50 (traço preto em fundo branco)
  // listados num manifest CSV. Printa diff por sample em stdout. Retorna true
  // se todas as predições baterem com o esperado (diff < 1e-3 por classe).
  // Use uma vez no startup para validar o pipeline.
  bool selfTest(const std::string &samplesDir, const std::string &manifestPath);

  bool isLoaded() const;

  // Roda inferência sobre o canvas atual.
  //   - canvas: black-on-white (background=1, traço=0). Invertemos aqui para
  //     atender ao formato do modelo (white-on-black).
  //   - threshold: confiança mínima do top-1; se abaixo, retorna classIndex=-1.
  ClassificationResult classify(const Canvas &canvas, float threshold = 0.70f);

 private:
  std::unique_ptr<ClassifierImpl> impl_;
};

}  // namespace spell
