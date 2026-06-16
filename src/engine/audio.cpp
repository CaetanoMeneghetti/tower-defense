#include "engine/audio.h"
#include "miniaudio.h"
#include <iostream>

static ma_engine g_engine;
static ma_sound  g_music;         // faixa de batalha
static ma_sound  g_intermission;  // faixa de intermission (posição preservada)
static ma_sound  g_oneShotFull;   // one-shot com volume controlado
static bool g_ready              = false;
static bool g_musicActive        = false;
static bool g_intermissionInited = false;
static bool g_oneShotFullInited  = false;

// ---- Pool de SFX pré-decodificados ----
// Cada arquivo vira um clipe com N vozes decodificadas, para disparos sobrepostos
// (vários arqueiros atirando). Realocar g_sfx é seguro: o std::vector<ma_sound>
// move só o ponteiro do buffer, preservando o endereço de cada ma_sound.
namespace {
constexpr int kVoicesPerClip = 6;
struct SfxClip {
  std::string path;
  std::vector<ma_sound> voices;
  int  next   = 0;
  bool inited = false;
};
std::vector<SfxClip> g_sfx;

SfxClip *findClip(const std::string &path) {
  for (auto &c : g_sfx)
    if (c.path == path) return &c;
  return nullptr;
}
}  // namespace

namespace audio {

bool init() {
  if (ma_engine_init(NULL, &g_engine) != MA_SUCCESS) {
    std::cout << "Falha ao iniciar audio\n";
    return false;
  }
  ma_engine_set_volume(&g_engine, 0.20f);
  g_ready = true;
  return true;
}

void shutdown() {
  if (!g_ready) return;
  for (auto &clip : g_sfx)
    for (auto &v : clip.voices) ma_sound_uninit(&v);
  g_sfx.clear();
  if (g_musicActive)        { ma_sound_stop(&g_music);       ma_sound_uninit(&g_music);       }
  if (g_intermissionInited) { ma_sound_stop(&g_intermission);ma_sound_uninit(&g_intermission);}
  if (g_oneShotFullInited)  { ma_sound_stop(&g_oneShotFull); ma_sound_uninit(&g_oneShotFull); }
  ma_engine_uninit(&g_engine);
  g_ready = false;
}

void preloadOneShots(const std::vector<std::string> &paths) {
  if (!g_ready) return;
  for (const auto &p : paths) {
    if (findClip(p)) continue;  // já carregado
    g_sfx.emplace_back();
    SfxClip &clip = g_sfx.back();
    clip.path = p;
    clip.voices.resize(kVoicesPerClip);
    bool ok = true;
    for (int i = 0; i < kVoicesPerClip; ++i) {
      // DECODE: decodifica em memória (síncrono); vozes seguintes reaproveitam
      // o buffer cacheado pelo resource manager.
      if (ma_sound_init_from_file(&g_engine, p.c_str(), MA_SOUND_FLAG_DECODE,
                                  NULL, NULL, &clip.voices[i]) != MA_SUCCESS) {
        for (int j = 0; j < i; ++j) ma_sound_uninit(&clip.voices[j]);
        ok = false;
        break;
      }
    }
    if (ok) {
      clip.inited = true;
    } else {
      std::cout << "Falha ao pre-carregar SFX: " << p << "\n";
      g_sfx.pop_back();
    }
  }
}

void playOneShot(const std::string &path) {
  if (!g_ready) return;
  SfxClip *clip = findClip(path);
  if (!clip) {
    // Não pré-carregado: caminho antigo (carrega/decodifica na hora).
    ma_engine_play_sound(&g_engine, path.c_str(), NULL);
    return;
  }
  // Escolhe uma voz ociosa; se todas tocando, rouba a próxima (round-robin).
  ma_sound *chosen = nullptr;
  for (auto &v : clip->voices)
    if (!ma_sound_is_playing(&v)) { chosen = &v; break; }
  if (!chosen) {
    chosen = &clip->voices[clip->next];
    clip->next = (clip->next + 1) % (int)clip->voices.size();
    ma_sound_stop(chosen);
  }
  ma_sound_seek_to_pcm_frame(chosen, 0);
  ma_sound_start(chosen);
}

void playOneShotAt(const std::string &path, float volume) {
  if (!g_ready) return;
  if (g_oneShotFullInited) {
    ma_sound_stop(&g_oneShotFull);
    ma_sound_uninit(&g_oneShotFull);
    g_oneShotFullInited = false;
  }
  if (ma_sound_init_from_file(&g_engine, path.c_str(),
      MA_SOUND_FLAG_DECODE, NULL, NULL, &g_oneShotFull) == MA_SUCCESS) {
    ma_sound_set_volume(&g_oneShotFull, volume);
    ma_sound_start(&g_oneShotFull);
    g_oneShotFullInited = true;
  }
}

void playMusic(const std::string &path, float volume) {
  if (!g_ready) return;
  if (g_musicActive) { ma_sound_stop(&g_music); ma_sound_uninit(&g_music); g_musicActive = false; }
  if (ma_sound_init_from_file(&g_engine, path.c_str(),
      MA_SOUND_FLAG_STREAM, NULL, NULL, &g_music) == MA_SUCCESS) {
    ma_sound_set_looping(&g_music, MA_TRUE);
    ma_sound_set_volume(&g_music, volume);
    ma_sound_start(&g_music);
    g_musicActive = true;
  }
}

void pauseBattleMusic() {
  if (!g_ready || !g_musicActive) return;
  ma_sound_stop(&g_music);  // preserva cursor
}

void resumeBattleMusic() {
  if (!g_ready || !g_musicActive) return;
  if (!ma_sound_is_playing(&g_music))
    ma_sound_start(&g_music);
}

void stopMusic() {
  if (!g_ready || !g_musicActive) return;
  ma_sound_stop(&g_music);
  ma_sound_uninit(&g_music);
  g_musicActive = false;
}

void startIntermissionMusic(const std::string &path, float volume) {
  if (!g_ready) return;
  if (g_intermissionInited) {
    ma_sound_stop(&g_intermission);
    ma_sound_uninit(&g_intermission);
    g_intermissionInited = false;
  }
  if (ma_sound_init_from_file(&g_engine, path.c_str(),
      MA_SOUND_FLAG_STREAM, NULL, NULL, &g_intermission) == MA_SUCCESS) {
    ma_sound_set_looping(&g_intermission, MA_TRUE);
    ma_sound_set_volume(&g_intermission, volume);
    ma_sound_start(&g_intermission);
    g_intermissionInited = true;
  }
}

void pauseIntermissionMusic() {
  // ma_sound_stop preserva a posição — ma_sound_start retoma de onde parou
  if (!g_ready || !g_intermissionInited) return;
  ma_sound_stop(&g_intermission);
}

void resumeIntermissionMusic() {
  if (!g_ready || !g_intermissionInited) return;
  if (!ma_sound_is_playing(&g_intermission))
    ma_sound_start(&g_intermission);
}

void stopIntermissionMusic() {
  if (!g_ready || !g_intermissionInited) return;
  ma_sound_stop(&g_intermission);
  ma_sound_uninit(&g_intermission);
  g_intermissionInited = false;
}

}  // namespace audio
