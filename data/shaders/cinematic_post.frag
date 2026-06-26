#version 330 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D screenTex;
uniform float time;
uniform float fadeAlpha;
uniform float screenAspect;  // window W/H

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = vUV;
    vec2 fromCenter = uv - 0.5;

    // ---- Chromatic aberration (increases toward edges) ----
    float aberr = 0.0035 * length(fromCenter * 2.0);
    float r = texture(screenTex, uv - fromCenter * aberr).r;
    float g = texture(screenTex, uv).g;
    float b = texture(screenTex, uv + fromCenter * aberr).b;
    vec3 col = vec3(r, g, b);

    // ---- Exposure ----
    col *= 1.08;

    // ---- Lift / Gain color grade ----
    // Lift: push shadows slightly blue-teal (moonlit night)
    vec3 lift = vec3(0.005, 0.010, 0.045);
    // Gain: warm bright areas (firelight/gold highlights)
    vec3 gain = vec3(1.12, 1.00, 0.82);
    col = col * gain + lift;

    // ---- Desaturate toward horror/grim palette ----
    float luma = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(luma), col, 0.78);

    // ---- Contrast S-curve ----
    col = clamp(col, 0.0, 1.0);
    col = col * col * (3.0 - 2.0 * col);

    // ---- Vignette ----
    vec2 vigUV = fromCenter * 2.0;
    float vig = 1.0 - smoothstep(0.45, 1.35, length(vigUV));
    col *= mix(0.03, 1.0, vig);

    // ---- Film grain (animated) ----
    float noise = rand(uv + vec2(fract(time * 0.031), fract(time * 0.057)));
    float grain = (noise - 0.5) * 0.048 * (0.6 + 0.4 * vig);
    col += grain;

    // ---- Letterbox 2.35:1 (black bars top/bottom) ----
    // barFrac = half of the height consumed by bars
    float barFrac = max(0.0, 0.5 * (1.0 - screenAspect / 2.35));
    if (uv.y < barFrac || uv.y > 1.0 - barFrac) col = vec3(0.0);

    // ---- Fade to/from black ----
    col = mix(col, vec3(0.0), clamp(fadeAlpha, 0.0, 1.0));

    FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
