Os prompts não mencionados aqui são aqueles em que o código gerado pela ferramenta de IA foi significativamente minoritário na tarefa ou quando foi documentado no commit (que contém a palavra "PROMPT"). Nesse caso, o prompt está destacado no corpo do commit.

---

**PROMPT**: Preciso definir o caminho que os inimigos vão percorrer usando curvas de Catmull-Rom. Também quero usar essas curvas para gerar um caminho de terra, que seguirá essa curva e formará um certo espaço em volta dela. Esse espaço não deve ser fixo (ou seja, a largura do caminho de terra não deve ser exatamente a mesma ao longo da curva, quero que tenha ruído para ficar mais natural). Crie uma função que constrói a malha 3D desse caminho a partir de uma lista de pontos de controle (estruturados como {float x, float y}). O ruído deve ser aleatório, talvez seguindo alguma distribuição -- não use seno e cosseno para criar o ruído --, e não precisa (nem deve) ser o mesmo para os dois lados.

Resultado (que foi comentado e levemente adaptado): função **generatePathMesh**, em *path_generator.cpp*.

---

**PROMPT**: Crie um shader que lê uma textura de grama para meu mapa. Ele também recebe uma textura de ruído. Utilize técnicas (misturas, ruídos, transformações, múltiplas camadas) para replicar a grama sem que ela tenha forme padrões/repetições. Quero algo mais realista.

Resultado (que foi comentado e levemente adaptado): arquivo *grass.frag*.

---

**PROMPT**: Corrija minha implementação de Blinn-Phong. Termos ambiente + difuso (Lambert) + especular. Também modifique a main para já usar a versão que fiz, e sugira parâmetros iniciais para eu ajustar. Coloque comentários nos parâmetros, para facilitar o uso deles.

Resultado: mudanças leves na implementação do Blinn-Phong em cada shader .vert (**calcDirLight**) e aplicação do modelo em **main_cpp**.

---

**PROMPT**: De que forma posso implementar uma neblina para esconder o fim do mapa, fazendo com que seja mais difícil de enxergar a borda dele?

Resultado: Ajustes (corrigiu a antiga e gerou a linha atual) nas variáveis `fogFactor`, `litColor` e `finalColor` em *grass.frag*.

---

**PROMPT**: Preciso que você implemente o carregamento de modelos .glb usando a biblioteca Assimp. A ideia é criar uma classe AnimatedModel que consiga ler a hierarquia dos ossos e as animações do arquivo. O código precisa calcular a interpolação entre os frames (movimento e rotação com quaternions) para que a animação fique fluida. No final, o código deve me entregar um array de matrizes pronto para eu enviar para o meu shader de skinning no OpenGL.

Resultado: extração dos canais de animação (aiNodeAnim) e um vector mat4 com as matrizes de ossos interpoladas do arquivo.

---

**PROMPT**: Implemente uma lógica na classe GameObject para eu conseguir "grudar" um objeto (tipo uma arma ou um item) na mão do personagem. Eu preciso de uma função que pegue a posição global de um osso específico do modelo animado e combine isso com a posição do personagem no mundo. O objetivo é que, se o personagem estiver se movendo e a animação da mão balançar, o item acompanhe exatamente esse movimento.

Resultado: O método GetBoneWorldTransform que multiplica a matriz de transformação global do GameObject (Posição/Rotação/Escala no mundo) pela matriz global do nó do osso extraída da hierarquia do Assimp.

---

**PROMPT**: Preciso criar um HUD para o meu jogo de Tower Defense usando Dear ImGui (com GLFW e OpenGL3). O desafio é que eu não quero que a barra superior pareça uma janela padrão do ImGui; ela precisa ficar colada no topo, ter uma textura de fundo mapeada na tela toda e ter elementos desenhados por cima (como slots de torres). Nessa barra, quero botões de imagem para selecionar o Arqueiro ou o Arcabuz. Se o jogador clicar e tiver ouro suficiente, o UI precisa forçar a alteração do estado do jogo para entrar no modo de "posicionar tropa". Por fim, preciso de uma janelinha de debug separada logo abaixo da barra mostrando o FPS e o status da câmera.

Resultado: A classe Hud com o método Render que cria uma janela "TopBar" usando ImGuiWindowFlags_NoDecoration e fundo transparente. Ela utiliza o ImDrawList::AddImage e AddRect para desenhar o fundo e os slots manualmente, ignorando o layout padrão. O clique nas tropas é feito com ImGui::ImageButton e a alteração do estado do jogo (que vem como const AppState&)

---

**PROMPT**: Preciso de um fragment shader que calcule iluminação usando o modelo Blinn-Phong para uma luz direcional (sol/lua) e suporte um array de até 20 point lights (lanternas) com atenuação. O material deve ter textura difusa (fazendo um discard se o alpha for menor que 0.1) e suporte a Normal Map controlado por uma variável vHasTangent. O detalhe mais crítico é que a matriz TBN interpolada que vem do Vertex Shader está ficando distorcida em malhas complexas. Para consertar isso, preciso que você aplique o processo de ortogonalização de Gram-Schmidt por pixel (per-pixel) diretamente no fragment shader para reconstruir uma matriz TBN perfeita. Calcule o Bitangente ideal e inverta o sinal dele se a UV estiver espelhada.

Resultado: Um fragment shader que separa o cálculo das luzes nas funções calcDirLight e calcPointLights, ambas utilizando a otimização de Blinn-Phong. O bloco if (vHasTangent > 0.5) do código extrai as colunas da matriz TBN e aplica matemática de Gram-Schmidt para subtrair a projeção indesejada do Tangente, garantindo que ele fique perfeitamente perpendicular ao Normal. Ele recria o Bitangente com um cross(N, T) e usa o dot para checar a direção (UVs espelhadas), montando a perfectTBN final para aplicar no normal map.

---

**PROMPT**: Preciso de um sistema de upgrade para as tropas do meu Tower Defense implementado com Dear ImGui. Quero uma janela de "Detalhes da Tropa" que fique travada no canto inferior direito da tela. Essa janela deve ler as informações de um GameObject (nível, dano, alcance, recarga) e exibir um comparativo com os atributos do próximo nível (ex: "Dano: 12 -> 16" com o valor novo em verde), além de uma descrição do upgrade. O nível máximo é 5; chegando lá, a interface deve travar e avisar que está no máximo. O botão de upgrade deve exibir o custo em ouro e ficar cinza/desabilitado se o jogador não tiver saldo suficiente no AppState. Quando a compra for feita, a função deve atualizar os status da tropa, descontar o ouro e retornar um bool (true) para avisar o loop principal que um upgrade acabou de acontecer.

Resultado: A função DrawTroopDetailsHUD monta a janela travada com ImGuiWindowFlags. Ela usa uma função lambda auxiliar chamada drawStatRow para formatar perfeitamente a linha de comparação dos atributos com as cores corretas. Além disso, também foi gerado função CalculateUpgrade que funciona como um banco de dados usando switch/case para retornar os status exatos do próximo nível com base no tipo da tropa (1 para Arqueiro, 2 para Arcabuzeiro) e seu nível atual.


---

**PROMPT**: Preciso converter as coordenadas 2D do mouse na tela em uma posição 3D exata no meu mundo de jogo. Não quero usar funções prontas como o glm::unProject. Preciso de uma função que pegue a posição atual do cursor do mouse via GLFW, transforme para Coordenadas Normalizadas de Dispositivo (NDC) entre $[-1, 1]$, extraia os vetores de direção (Right, Up, Backward) diretamente do topo da Matriz de Visão da câmera e use a tangente do FOV para calcular um raio no espaço do mundo. No final, a função deve calcular a interseção desse raio com o plano do chão onde $Y = 0$ e retornar esse ponto tridimensional.

Resultado: A função GetMouseGroundPos. Ela faz a matemática de projeção reversa manualmente. Primeiro calcula o aspecto da tela e o tanHalfFov. Depois, reconstrói o vetor do raio multiplicando os componentes NDC pelos eixos extraídos da matriz de visualização (V(0,0), V(1,0), etc.)

---

**PROMPT**: Quero implementar um sistema de "Preview" para o posicionamento de torres no meu jogo. Quando o jogador escolher uma tropa na interface, o jogo deve projetar um modelo "fantasma" translúcido na posição do mouse no chão. Esse fantasma precisa de validação de área: se o mouse estiver em cima da estrada de terra (raio menor que a largura do caminho) OU se estiver longe demais da estrada (onde os defensores não alcançariam os inimigos), o holograma deve ficar vermelho translúcido e bloquear o clique de construção. Se a posição for válida, ele deve ficar azul ciano translúcido e, ao clicar com o botão esquerdo, descontar o ouro do jogador e instanciar o GameObject definitivo naquela coordenada.

Resultado: O bloco de lógica condicional if (state.isPlacingTroop) dentro do loop principal. Ele usa a função distanceToPath contra os pontos da curva Catmull-Rom para definir as flags isOnPath e isTooFar. Com base nisso, preenche o uniform previewColor do previewShader com um vec4 contendo o canal Alpha em 0.5f (transparência).

---

**PROMPT**: No meu loop de renderização do OpenGL, preciso desenhar as armas (como arcos e arcabuzes) nas mãos dos meus personagens animados. As armas são modelos .obj estáticos e separados, enquanto os personagens são modelos .glb com animação esquelética. Preciso que o pipeline percorra as minhas tropas ativas, recupere a matriz de transformação global da animação do osso da mão esquerda (mixamorig:LeftHand) e combine essa matriz com uma matriz de matriz de offset personalizada (ajustes locais de escala, rotação e translação que fiz para alinhar a arma perfeitamente nos dedos do modelo). O resultado transformado deve ser enviado para o shader de objetos estáticos antes de desenhar a malha da arma.

Resultado: O trecho do loop principal que renderiza os equipamentos das tropas usando o objShader. Para cada unidade dentro do vetor defenders, o código verifica se o tier atual possui uma malha de arma configurada (currentTier.weaponMesh). Ele puxa a transformação da animação chamando unit.GetBoneWorldTransform("mixamorig:LeftHand") (retornando uma glm::mat4) e faz uma multiplicação direta de matrizes por currentTier.weaponOffset.

---

**PROMPT**: Preciso implementar o coração do sistema de animação esquelética usando a árvore de nós do Assimp. O motor precisa percorrer a hierarquia de ossos de forma recursiva, acumulando as matrizes de transformação dos pais para gerar a posição global de cada nó. No final, para os nós que realmente influenciam os vértices, preciso calcular a matriz final de skinning multiplicando a transformação global pelo offset do osso (matriz inversa da pose de descanso em T) e corrigindo o espaço com a inversa global do modelo.

Resultado: O método recursivo CalculateBoneTransform. Ele recebe o nó atual e a transformação acumulada do pai (parentTransform). Ele interpola as chaves de animação locais (translação, rotação e escala) daquele nó para formar a nodeTransform.

---

**PROMPT**: Quero carregar um modelo 3D estático ou com uma pose base (pose T) de um arquivo .glb/.fbx, mas preciso conseguir injetar arquivos de animação separados dinamicamente (tipo um arquivo só com a animação de correr, outro com o ataque, etc.) e associá-los a um nome/ID em string. O problema é que o Assimp desaloca a memória da cena (aiScene) se o seu respectivo objeto Assimp::Importer sair de escopo. Preciso de uma estrutura em C++ que gerencie o ciclo de vida desses arquivos de animação externos sem deixar a memória corromper.

Resultado: O método LoadAnimation + dicionários internos da classe. A solução armazena as animações usando um mapa de ponteiros  m_AnimImporters. Ao carregar uma animação o escopo do leitor do Assimp fica travado na memória dentro da classe pelo tempo que for necessário.
A cena gerada é salva em m_Animations[name] = scene, permitindo que o método GetTransformsAtTime faça buscas ultra rápidas por strings no frame-rate do jogo.
