#pragma once
#include <GLFW/glfw3.h>
#include "app_state.h"

class Hud {
public:
    Hud();
    ~Hud();

    void Init(GLFWwindow* window);
    void Render(const AppState& state, float fps);
    void Shutdown();

private:
    void SetupStyle();
};