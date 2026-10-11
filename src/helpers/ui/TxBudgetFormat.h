#pragma once
#include <stdint.h>

// remaining_ms / max_budget_ms as 0-100. max_budget_ms must be the real computed
// maximum (Dispatcher.cpp's tx_budget_ms formula), not the raw duty-cycle window.
inline uint8_t txBudgetPercent(unsigned long remaining_ms, unsigned long max_budget_ms) {
  if (max_budget_ms == 0) return 0;
  if (remaining_ms >= max_budget_ms) return 100;
  return (uint8_t)((remaining_ms * 100UL) / max_budget_ms);
}
