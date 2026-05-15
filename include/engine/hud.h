#pragma once
#include <GLFW/glfw3.h>
#include "app_state.h"

//estrutura para os icones da hud
struct HudTextures {
    unsigned int topBackground;
    unsigned int goldIcon;
    unsigned int healthIcon;
    unsigned int archerIcon; 
};

class Hud {
public:
    Hud();
    ~Hud();

    void Init(GLFWwindow* window);
    void SetTextures(const HudTextures& textures);
    void Render(const AppState& state, float fps);
    void Shutdown();

private:
    void SetupStyle();
    HudTextures m_textures;
};