#pragma once
// Multithreaded Monte Carlo equity simulator.

#include <cstdint>
#include <string>

#include "holdem/cards.hpp"

namespace holdem {

struct SimConfig {
  Hand hero = 0;                 // exactly 2 cards
  Hand board = 0;                // 0, 3, 4 or 5 known community cards
  Hand dead = 0;                 // cards known to be out of play
  int opponents = 1;             // 1..9 opponents holding random cards
  std::uint64_t trials = 10'000'000;
  unsigned threads = 0;          // 0 = std::thread::hardware_concurrency()
  std::uint64_t seed = 0x5EED5EED5EEDULL;
};

struct SimResult {
  std::uint64_t trials = 0;
  std::uint64_t wins = 0;        // hero strictly best
  std::uint64_t ties = 0;        // hero shares the best hand
  std::uint64_t losses = 0;
  double equity_sum = 0.0;       // wins + fractional pot shares from ties
  double seconds = 0.0;
  unsigned threads_used = 0;

  double win_rate() const { return trials ? double(wins) / double(trials) : 0.0; }
  double tie_rate() const { return trials ? double(ties) / double(trials) : 0.0; }
  double loss_rate() const { return trials ? double(losses) / double(trials) : 0.0; }
  double equity() const { return trials ? equity_sum / double(trials) : 0.0; }
  double hands_per_second() const { return seconds > 0 ? double(trials) / seconds : 0.0; }
};

// Throws std::invalid_argument on an impossible configuration (wrong card
// counts, overlapping cards, too many opponents for the remaining deck).
SimResult simulate(const SimConfig& config);

// Single-threaded worker, exposed for testing and for callers that manage
// their own threads.
SimResult simulate_single_thread(const SimConfig& config, std::uint64_t trials,
                                 std::uint64_t seed);

}  // namespace holdem
