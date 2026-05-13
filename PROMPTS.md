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
