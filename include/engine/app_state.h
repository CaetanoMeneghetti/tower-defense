#pragma once
#include "math/constants.h"

struct AppState {
    //info do jogador
    int gold = 250;
    int health = 20;

    //status da camera
    bool useFreeCamera = false;
    bool cPressed = false;
    bool showCurve = false;
    bool tPressed = false;

    float yaw = -math_constants::kHalfPi;
    float pitch = 0.0f;
    float lastX = 1920 / 2.0f;
    float lastY = 1080 / 2.0f;
    bool firstMouse = true;

    float orbitYaw = 0.0f;
    float orbitPitch = math_constants::kPi / 6.0f;
    float orbitRadius = 10.0f;

    int fbWidth = 1920;
    int fbHeight = 1080;
};