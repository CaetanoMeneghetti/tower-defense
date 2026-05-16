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
