#pragma once

#include <string>

#include "engine/game_object.h"
#include "game/app_state.h"

// =============================================================================
// UPGRADE SYSTEM
// =============================================================================
// Tabela de upgrades por tipo de tropa (níveis 1..kMaxTroopLevel) + janela
// ImGui que mostra stats atuais → próximos e oferece o botão "UPGRADE - Xg".

// Nível máximo de uma tropa (acima disso, sem upgrades disponíveis).
// Tem que casar com a quantidade de tiers no TroopDef de cada classe.
constexpr int kMaxTroopLevel = 5;

struct UpgradeData {
  int cost;
  int nextDamage;
  float nextRange;
  float nextFireRate;
  std::string description;
};

// Calcula os stats que a tropa terá após o próximo upgrade.
// Fallback seguro (cost = 9999, descrição genérica) se a combinação
// (troopType, currentLevel) não estiver na tabela.
UpgradeData calculateUpgrade(int troopType, int currentLevel);

// Desenha a janela de detalhes da tropa selecionada. Retorna true se o usuário
// clicou no botão de UPGRADE — main aplica a transição com troop.upgrade().
bool drawTroopDetailsHud(GameObject &troop, AppState &state, unsigned int placeholderIcon);
