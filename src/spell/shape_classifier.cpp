#include "spell/shape_classifier.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "stb_image.h"

// Inferência manual em CPU — sem dependência externa. Arquitetura hardcoded
// para o classificador 3-classes (circle, square, triangle), 50x50 grayscale:
//
//   conv 1->32  + conv 32->32  + maxpool   →  25x25x32
//   conv 32->64 + conv 64->64  + maxpool   →  12x12x64  (25→12: divisão inteira)
//   conv 64->128+ conv 128->128+ maxpool   →  6x6x128
//   global avg pool                        →  128
//   dense 128 ReLU                         →  128
//   dense 3 softmax                        →  3
//
// Total: 303 331 floats (~1.18 MB). Layout do .bin: pesos seguidos de bias
// para cada camada, na ordem acima. ~1 ms por inferência em CPU moderna.

namespace spell {

namespace {

constexpr int kInputH = 50;
constexpr int kInputW = 50;
constexpr int kNumClasses = 3;
constexpr size_t kExpectedFloats = 303331;
const char *kLabels[] = {"circle", "square", "triangle"};

// Conv2D 3x3 stride=1 padding=same, bias, ReLU opcional.
// in : (H, W, Cin)  row-major HWC
// out: (H, W, Cout) row-major HWC
// W  : (Cout, Cin, 3, 3) row-major
// b  : (Cout,)
void conv3x3Same(const float *in, int H, int W, int Cin,
                 const float *Wt, const float *bt, int Cout,
                 float *out, bool relu) {
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      for (int oc = 0; oc < Cout; ++oc) {
        float s = bt[oc];
        const float *w_oc = Wt + oc * Cin * 9;
        for (int ic = 0; ic < Cin; ++ic) {
          const float *w_ic = w_oc + ic * 9;
          for (int ky = 0; ky < 3; ++ky) {
            const int yy = y + ky - 1;
            if (yy < 0 || yy >= H) continue;
            for (int kx = 0; kx < 3; ++kx) {
              const int xx = x + kx - 1;
              if (xx < 0 || xx >= W) continue;
              s += in[(yy * W + xx) * Cin + ic] * w_ic[ky * 3 + kx];
            }
          }
        }
        if (relu && s < 0.0f) s = 0.0f;
        out[(y * W + x) * Cout + oc] = s;
      }
    }
  }
}

// MaxPool 2x2 stride 2, sem padding. Saída H/2 x W/2 x C (div. inteira).
void maxPool2(const float *in, int H, int W, int C, float *out) {
  const int Ho = H / 2, Wo = W / 2;
  for (int y = 0; y < Ho; ++y) {
    for (int x = 0; x < Wo; ++x) {
      for (int c = 0; c < C; ++c) {
        const int iy = y * 2, ix = x * 2;
        float m = in[(iy * W + ix) * C + c];
        m = std::max(m, in[(iy * W + (ix + 1)) * C + c]);
        m = std::max(m, in[((iy + 1) * W + ix) * C + c]);
        m = std::max(m, in[((iy + 1) * W + (ix + 1)) * C + c]);
        out[(y * Wo + x) * C + c] = m;
      }
    }
  }
}

}  // namespace

// Container dos pesos: um único blob contíguo + ponteiros fatiados.
struct ClassifierImpl {
  std::vector<float> blob;
  const float *W1a, *b1a, *W1b, *b1b;
  const float *W2a, *b2a, *W2b, *b2b;
  const float *W3a, *b3a, *W3b, *b3b;
  const float *Wh,  *bh,  *Wo,  *bo;
  bool loaded = false;
};

ShapeClassifier::ShapeClassifier() : impl_(new ClassifierImpl()) {}
ShapeClassifier::~ShapeClassifier() = default;

bool ShapeClassifier::load(const std::string &modelPath) {
  std::ifstream f(modelPath, std::ios::binary | std::ios::ate);
  if (!f) {
    std::cerr << "[ShapeClassifier] nao abriu '" << modelPath << "'" << std::endl;
    return false;
  }
  const size_t bytes = static_cast<size_t>(f.tellg());
  const size_t floats = bytes / sizeof(float);
  if (floats != kExpectedFloats) {
    std::cerr << "[ShapeClassifier] tamanho inesperado: " << bytes
              << " bytes (" << floats << " floats, esperado "
              << kExpectedFloats << ")" << std::endl;
    return false;
  }
  f.seekg(0);
  impl_->blob.resize(floats);
  f.read(reinterpret_cast<char *>(impl_->blob.data()), bytes);

  // Fatia o blob na ordem fixa: pesos da camada, depois bias da mesma.
  const float *p = impl_->blob.data();
  auto take = [&](size_t n) { const float *r = p; p += n; return r; };

  impl_->W1a = take(32 * 1   * 9); impl_->b1a = take(32);
  impl_->W1b = take(32 * 32  * 9); impl_->b1b = take(32);
  impl_->W2a = take(64 * 32  * 9); impl_->b2a = take(64);
  impl_->W2b = take(64 * 64  * 9); impl_->b2b = take(64);
  impl_->W3a = take(128 * 64 * 9); impl_->b3a = take(128);
  impl_->W3b = take(128 * 128 * 9); impl_->b3b = take(128);
  impl_->Wh  = take(128 * 128);    impl_->bh  = take(128);
  impl_->Wo  = take(3 * 128);      impl_->bo  = take(3);

  if (p != impl_->blob.data() + impl_->blob.size()) {
    std::cerr << "[ShapeClassifier] descompactacao do blob inconsistente" << std::endl;
    impl_->blob.clear();
    return false;
  }

  impl_->loaded = true;
  // std::cout << "[ShapeClassifier] carregado: " << modelPath
  //           << " (" << floats << " floats)" << std::endl;
  return true;
}

bool ShapeClassifier::isLoaded() const { return impl_->loaded; }

ClassificationResult ShapeClassifier::classify(const Canvas &canvas, float threshold) {
  ClassificationResult r{};
  if (!isLoaded()) return r;

  // Canvas in: background=1, traço=0. Modelo espera o oposto (white-on-black).
  // image[i] = 1 - canvas[i] inverte para o formato do treino.
  std::vector<float> image(kInputH * kInputW);
  for (int i = 0; i < kInputH * kInputW; ++i) image[i] = 1.0f - canvas.pixels[i];

  // Block 1
  std::vector<float> a(50 * 50 * 32);
  std::vector<float> b(50 * 50 * 32);
  conv3x3Same(image.data(), 50, 50, 1,  impl_->W1a, impl_->b1a, 32, a.data(), true);
  conv3x3Same(a.data(),     50, 50, 32, impl_->W1b, impl_->b1b, 32, b.data(), true);
  std::vector<float> p1(25 * 25 * 32);
  maxPool2(b.data(), 50, 50, 32, p1.data());

  // Block 2
  std::vector<float> c(25 * 25 * 64);
  std::vector<float> d(25 * 25 * 64);
  conv3x3Same(p1.data(), 25, 25, 32, impl_->W2a, impl_->b2a, 64, c.data(), true);
  conv3x3Same(c.data(),  25, 25, 64, impl_->W2b, impl_->b2b, 64, d.data(), true);
  std::vector<float> p2(12 * 12 * 64);
  maxPool2(d.data(), 25, 25, 64, p2.data());

  // Block 3
  std::vector<float> e(12 * 12 * 128);
  std::vector<float> g(12 * 12 * 128);
  conv3x3Same(p2.data(), 12, 12, 64,  impl_->W3a, impl_->b3a, 128, e.data(), true);
  conv3x3Same(e.data(),  12, 12, 128, impl_->W3b, impl_->b3b, 128, g.data(), true);
  std::vector<float> p3(6 * 6 * 128);
  maxPool2(g.data(), 12, 12, 128, p3.data());

  // Global Average Pool 6x6 → 128
  float gap[128] = {0};
  for (int y = 0; y < 6; ++y)
    for (int x = 0; x < 6; ++x)
      for (int ch = 0; ch < 128; ++ch)
        gap[ch] += p3[(y * 6 + x) * 128 + ch];
  for (int ch = 0; ch < 128; ++ch) gap[ch] /= 36.0f;

  // Dense hidden 128→128 + ReLU
  float h[128];
  for (int o = 0; o < 128; ++o) {
    float s = impl_->bh[o];
    const float *w = impl_->Wh + o * 128;
    for (int i = 0; i < 128; ++i) s += w[i] * gap[i];
    h[o] = s > 0.0f ? s : 0.0f;
  }

  // Dense out 128→3 + Softmax
  float logits[kNumClasses];
  for (int o = 0; o < kNumClasses; ++o) {
    float s = impl_->bo[o];
    const float *w = impl_->Wo + o * 128;
    for (int i = 0; i < 128; ++i) s += w[i] * h[i];
    logits[o] = s;
  }
  const float m = std::max({logits[0], logits[1], logits[2]});
  float e0 = std::exp(logits[0] - m);
  float e1 = std::exp(logits[1] - m);
  float e2 = std::exp(logits[2] - m);
  const float sum = e0 + e1 + e2;
  float probs[kNumClasses] = {e0 / sum, e1 / sum, e2 / sum};

  for (int i = 0; i < kNumClasses; ++i) r.probs[i] = probs[i];

  int best = 0;
  for (int i = 1; i < kNumClasses; ++i) if (probs[i] > probs[best]) best = i;
  r.confidence = probs[best];
  if (r.confidence >= threshold) {
    r.classIndex = best;
    r.label = kLabels[best];
  }
  return r;
}

bool ShapeClassifier::selfTest(const std::string &samplesDir,
                               const std::string &manifestPath) {
  if (!isLoaded()) {
    std::cerr << "[selfTest] classifier nao carregado" << std::endl;
    return false;
  }
  std::ifstream mf(manifestPath);
  if (!mf) {
    std::cerr << "[selfTest] nao abriu manifest: " << manifestPath << std::endl;
    return false;
  }

  std::ofstream log("selftest.log");
  log << "===== self-test do classifier =====" << std::endl;
  int passed = 0, total = 0;
  float maxDiff = 0.0f;

  std::string line;
  while (std::getline(mf, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::stringstream ss(line);
    std::string filename, trueIdxStr, trueName, pcStr, psStr, ptStr;
    std::getline(ss, filename, ',');
    std::getline(ss, trueIdxStr, ',');
    std::getline(ss, trueName, ',');
    std::getline(ss, pcStr, ',');
    std::getline(ss, psStr, ',');
    std::getline(ss, ptStr, ',');
    float expected[3] = {std::stof(pcStr), std::stof(psStr), std::stof(ptStr)};

    std::string fullPath = samplesDir + "/" + filename;
    int w, h, ch;
    unsigned char *pixels = stbi_load(fullPath.c_str(), &w, &h, &ch, 1);
    if (!pixels) {
      std::cerr << "[selfTest] falha ao abrir " << fullPath << std::endl;
      continue;
    }
    if (w != 50 || h != 50) {
      std::cerr << "[selfTest] " << filename << " nao eh 50x50" << std::endl;
      stbi_image_free(pixels);
      continue;
    }

    // PNG vem como preto-em-branco (treino: bg=255, traço=0).
    // Canvas no nosso código tem a mesma convenção (bg=1.0, traço=0.0):
    // basta dividir por 255. A inversão pro modelo acontece dentro de classify().
    Canvas c;
    for (int i = 0; i < kCanvasPixels; ++i) {
      c.pixels[i] = pixels[i] / 255.0f;
    }
    stbi_image_free(pixels);

    // threshold 0 pra forçar retornar a classe (sem rejeitar)
    ClassificationResult r = classify(c, 0.0f);
    float diff = 0.0f;
    for (int i = 0; i < 3; ++i) diff = std::max(diff, std::abs(r.probs[i] - expected[i]));
    maxDiff = std::max(maxDiff, diff);
    bool ok = (diff < 1e-3f);
    log << "  " << filename
        << "  exp=[" << expected[0] << "," << expected[1] << "," << expected[2] << "]"
        << "  got=[" << r.probs[0] << "," << r.probs[1] << "," << r.probs[2] << "]"
        << "  diff=" << diff << (ok ? "  OK" : "  FAIL")
        << std::endl;
    if (ok) ++passed;
    ++total;
  }

  log << "===== self-test: " << passed << "/" << total
      << "  max diff=" << maxDiff << " =====" << std::endl;
  log.close();
  return passed == total;
}

}  // namespace spell
