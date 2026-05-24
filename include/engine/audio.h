#pragma once
#include <string>

namespace audio {
  bool init();
  void shutdown();

  // SFX fire-and-forget (toca no volume do engine)
  void playOneShot(const std::string &path);

  // One-shot com volume próprio (ex.: fanfare waveStart)
  void playOneShotAt(const std::string &path, float volume);

  // Música de batalha (onda ativa) — posição preservada entre pausas
  void playMusic(const std::string &path, float volume = 0.7f);
  void pauseBattleMusic();   // stop sem uninit; cursor preservado para resumo
  void resumeBattleMusic();  // retoma de onde parou
  void stopMusic();          // para e destrói (Victory / cleanup)

  // Música de intermission — posição é preservada entre pausas
  void startIntermissionMusic(const std::string &path, float volume = 0.7f);
  void pauseIntermissionMusic();
  void resumeIntermissionMusic();
  void stopIntermissionMusic();
}
