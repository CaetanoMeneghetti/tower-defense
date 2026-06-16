#pragma once
#include <string>
#include <vector>

namespace audio {
  bool init();
  void shutdown();

  // Pré-decodifica os SFX em memória (pool de vozes por arquivo). Sem isso, o
  // 1º play de cada som decodifica do disco e gera latência. Chamar uma vez no init.
  void preloadOneShots(const std::vector<std::string> &paths);

  // SFX fire-and-forget. Se pré-carregado, dispara uma voz já decodificada.
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
