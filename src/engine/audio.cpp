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
  if (g_musicActive)        { ma_sound_stop(&g_music);       ma_sound_uninit(&g_music);       }
  if (g_intermissionInited) { ma_sound_stop(&g_intermission);ma_sound_uninit(&g_intermission);}
  if (g_oneShotFullInited)  { ma_sound_stop(&g_oneShotFull); ma_sound_uninit(&g_oneShotFull); }
  ma_engine_uninit(&g_engine);
  g_ready = false;
}

void playOneShot(const std::string &path) {
  if (!g_ready) return;
  ma_engine_play_sound(&g_engine, path.c_str(), NULL);
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
