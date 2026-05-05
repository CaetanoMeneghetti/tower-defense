#include "world/path_generator.h"

int globalSeed = 42;

// Gera a malha 3D (Mesh) do caminho de terra com bordas irregulares
// Recebe uma lista de pontos centrais (a curva) e a largura base do caminho
Mesh generatePathMesh(const std::vector<Point> &centerPoints, float baseWidth) {
  std::vector<Vertex> meshVertices;

  // Um caminho precisa de pelo menos 2 pontos para formar uma reta/segmento
  if (centerPoints.size() < 2) {
    return Mesh(meshVertices);
  }

  const int LEFT_OFFSET = 123; // Diferenciador para o lado esquerdo
  const int RIGHT_OFFSET = 789; // Diferenciador para o lado direito
  const float Y_HEIGHT = 0.1f; // Elevação para evitar Z-Fighting

  // Estrutura temporária para armazenar os dados de cada "fatia" do caminho
  // antes de transformá-los em triângulos para o OpenGL.
  struct PathNode {
    Point leftVertex;   // Coordenada do lado esquerdo do caminho
    Point rightVertex;  // Coordenada do lado direito do caminho
    float v;            // Coordenada de textura V (distância percorrida)
  };

  std::vector<PathNode> nodes;
  float currentV = 0.0f;  // Acumulador da distância física percorrida

  // =========================================================================
  // CALCULAR VÉRTICES E LARGURA PROCEDURAL (Para cada ponto da curva)
  // =========================================================================
  for (size_t i = 0; i < centerPoints.size(); ++i) {
    Point current = centerPoints[i];
    Point dir;

    // 1.1 Calcula o vetor direção (Tangente) apontando para o próximo ponto
    if (i < centerPoints.size() - 1) {
      dir.x = centerPoints[i + 1].x - current.x;
      dir.y = centerPoints[i + 1].y - current.y;
    } else {
      // Se for o último ponto, mantém a direção do segmento anterior
      dir.x = current.x - centerPoints[i - 1].x;
      dir.y = current.y - centerPoints[i - 1].y;
    }

    // Normaliza o vetor direção para ter tamanho exatamente 1
    float len = dir.length();
    if (len > 0.0f) {
      dir.x /= len;
      dir.y /= len;
    }

    // Encontra o vetor Perpendicular (Normal)
    // Truque matemático 2D: Girar 90 graus é trocar (x, y) por (-y, x)
    Point normal = {-dir.y, dir.x};

    // Função geradora de aleatoriedade (Pseudo-Random Hash)
    // Transforma qualquer número inteiro (seed) em um valor caótico entre -1.0 e +1.0
    auto random1D = [&](int localSeed) -> float {
      float val = std::sin((localSeed + globalSeed) * 12.9898f) * 43758.5453f;
      return (val - std::floor(val)) * 2.0f - 1.0f;
    };

    // Configuração do ruído da borda (1D value noise)
    float variance = 0.4f;    // Força máxima que a borda pode inchar/espremer
    float noiseScale = 0.5f;  // Frequência: quanto menor, mais longas são as deformações
    float scaledV = currentV * noiseScale;  // Usa a distância como "tempo" do ruído

    // Descobre entre quais "pontos de controle do ruído" nós estamos
    int i0 = static_cast<int>(std::floor(scaledV));
    int i1 = i0 + 1;

    // 't' é a nossa posição atual entre esses dois pontos de controle (de 0.0 a 1.0)
    float t = scaledV - static_cast<float>(i0);

    // Suaviza 't' para a transição não ser reta/dura (matemática do Smoothstep)
    float smoothT = t * t * (3.0f - 2.0f * t);

    // Aplica o ruído para a esquerda
    // Sorteia um valor pro ponto de trás (i0), um pro da frente (i1) e interpola
    float rLeft0 =
        random1D(i0 + LEFT_OFFSET);  // Offset genérico (+123) para a esquerda ser única
    float rLeft1 = random1D(i1 + LEFT_OFFSET);
    float noiseLeft = (rLeft0 + (rLeft1 - rLeft0) * smoothT) * variance;

    // Aplica o ruído para a direita
    // Usa um offset diferente (+789) para a direita deformar de forma independente da
    // esquerda
    float rRight0 = random1D(i0 + RIGHT_OFFSET);
    float rRight1 = random1D(i1 + RIGHT_OFFSET);
    float noiseRight = (rRight0 + (rRight1 - rRight0) * smoothT) * variance;

    // Calcula a largura final baseada no ruído gerado
    float widthLeft = baseWidth + noiseLeft;
    float widthRight = baseWidth + noiseRight;

    // Cria o nó do caminho empurrando o ponto central para os lados usando a Normal
    PathNode node;
    node.leftVertex.x = current.x + (normal.x * widthLeft);
    node.leftVertex.y = current.y + (normal.y * widthLeft);

    node.rightVertex.x = current.x - (normal.x * widthRight);
    node.rightVertex.y = current.y - (normal.y * widthRight);

    // Atualiza e salva a distância percorrida (Coordenada V de textura)
    if (i > 0) {
      currentV += dist(current, centerPoints[i - 1]);
    }
    node.v = currentV;

    nodes.push_back(node);
  }

  // =========================================================================
  // CONSTRUIR A MALHA (Montar os Triângulos para o OpenGL)
  // =========================================================================

  // Percorre os nós de 2 em 2 para formar os quadrados (quads) que compõem o caminho
  for (size_t i = 0; i < nodes.size() - 1; ++i) {
    PathNode p1 = nodes[i];      // Fatias atuais
    PathNode p2 = nodes[i + 1];  // Próxima fatia à frente

    // Estrutura do Vertex: {{X, Y, Z}, {U, V}, {NormalX, NormalY, NormalZ}}
    // A Normal (0, 1, 0) diz para a iluminação que o chão está virado para cima

    // Vértices do nó atual (p1)
    Vertex vLeft1 = {
        {p1.leftVertex.x, Y_HEIGHT, p1.leftVertex.y}, {0.0f, p1.v}, {0.0f, 1.0f, 0.0f}};
    Vertex vRight1 = {
        {p1.rightVertex.x, Y_HEIGHT, p1.rightVertex.y}, {1.0f, p1.v}, {0.0f, 1.0f, 0.0f}};

    // Vértices do próximo nó (p2)
    Vertex vLeft2 = {
        {p2.leftVertex.x, Y_HEIGHT, p2.leftVertex.y}, {0.0f, p2.v}, {0.0f, 1.0f, 0.0f}};
    Vertex vRight2 = {
        {p2.rightVertex.x, Y_HEIGHT, p2.rightVertex.y}, {1.0f, p2.v}, {0.0f, 1.0f, 0.0f}};

    // Monta o primeiro triângulo do segmento (sentido anti-horário)
    meshVertices.push_back(vLeft1);
    meshVertices.push_back(vRight1);
    meshVertices.push_back(vLeft2);

    // Monta o segundo triângulo do segmento (sentido anti-horário)
    meshVertices.push_back(vRight1);
    meshVertices.push_back(vRight2);
    meshVertices.push_back(vLeft2);
  }

  return Mesh(meshVertices);
}
