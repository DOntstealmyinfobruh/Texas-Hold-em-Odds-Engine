// Benchmarks the three claims of the project:
//   1. 10 million simulated hands in under 3 seconds
//   2. O(1) lookups of precomputed odds for all 169 starting hands
//   3. 64-bit hands vs card-list storage

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#include "holdem/cards.hpp"
#include "holdem/evaluator.hpp"
#include "holdem/preflop.hpp"
#include "holdem/rng.hpp"
#include "holdem/simulator.hpp"

using namespace holdem;
using Clock = std::chrono::steady_clock;

namespace {

double seconds_since(Clock::time_point t) {
  return std::chrono::duration<double>(Clock::now() - t).count();
}

// Typical "list of cards" representations used for comparison.
struct CardInt { std::int32_t value; };                    // e.g. Cactus Kev 32-bit card
struct CardRS { std::int32_t rank; std::int32_t suit; };   // naive struct

}  // namespace

int main(int argc, char** argv) {
  std::uint64_t trials = 10'000'000;
  unsigned threads = 0;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "-n") && i + 1 < argc) trials = std::strtoull(argv[++i], nullptr, 10);
    else if (!std::strcmp(argv[i], "-t") && i + 1 < argc) threads = static_cast<unsigned>(std::atoi(argv[++i]));
  }
  const unsigned hw = std::thread::hardware_concurrency();

  std::printf("== Texas Hold'em Odds Engine benchmark ==\n");
  std::printf("hardware threads: %u\n\n", hw);

  // ---------------------------------------------------------------- 1. simulation
  std::printf("[1] Monte Carlo: %llu heads-up hands\n", static_cast<unsigned long long>(trials));
  const char* hands[] = {"As Kd", "Qh Qc", "7s 2h"};
  for (const char* text : hands) {
    SimConfig cfg;
    cfg.hero = *parse_hand(text);
    cfg.trials = trials;
    cfg.threads = threads;
    const SimResult r = simulate(cfg);
    std::printf("    %-6s equity %6.2f%%   %.3fs  %5.1fM hands/s  (%u thread%s)  %s\n", text,
                100 * r.equity(), r.seconds, r.hands_per_second() / 1e6, r.threads_used,
                r.threads_used == 1 ? " " : "s", r.seconds < 3.0 ? "PASS <3s" : "SLOW");
  }
  {
    SimConfig cfg;
    cfg.hero = *parse_hand("As Kd");
    cfg.trials = trials;
    cfg.threads = 1;
    const SimResult r = simulate(cfg);
    std::printf("    single thread: %.3fs  (%.1fM hands/s)\n", r.seconds, r.hands_per_second() / 1e6);
  }

  // ---------------------------------------------------------------- raw evaluator
  {
    Rng rng(42);
    std::vector<Hand> sample(1 << 20);
    for (auto& h : sample) {
      h = 0;
      while (popcount(h) < 7) {
        const int r = static_cast<int>(rng.below(13)), s = static_cast<int>(rng.below(4));
        h |= card(r, s);
      }
    }
    const int reps = 20;
    HandValue sink = 0;
    const auto t = Clock::now();
    for (int k = 0; k < reps; ++k)
      for (Hand h : sample) sink ^= evaluate(h);
    const double s = seconds_since(t);
    const double n = double(reps) * sample.size();
    std::printf("\n    7-card evaluator: %.1fM evals/s, %.2f ns/eval  (checksum %u)\n", n / s / 1e6,
                s / n * 1e9, sink);
  }

  // ---------------------------------------------------------------- 2. lookups
  {
    std::printf("\n[2] Precomputed table lookup (169 hands x 9 table sizes)\n");
    Rng rng(7);
    std::vector<Hand> holes(1 << 16);
    for (auto& h : holes) {
      const Hand a = Hand{1} << (rng.below(4) * 16 + rng.below(13));
      Hand b;
      do b = Hand{1} << (rng.below(4) * 16 + rng.below(13)); while (b == a);
      h = a | b;
    }
    const std::uint64_t lookups = 50'000'000;
    double sink = 0;
    const auto t = Clock::now();
    for (std::uint64_t i = 0; i < lookups; ++i)
      sink += preflop_odds(holes[i & 0xFFFF], 1 + static_cast<int>(i % 9)).equity;
    const double s = seconds_since(t);
    std::printf("    %llu lookups in %.3fs: %.1f ns/lookup  (checksum %.1f)\n",
                static_cast<unsigned long long>(lookups), s, s / lookups * 1e9, sink);
    std::printf("    table size: %zu bytes\n",
                sizeof(PreflopOdds) * kNumStartingHands * kMaxTableOpponents);

    SimConfig cfg;
    cfg.hero = *parse_hand("As Kd");
    cfg.trials = trials;
    cfg.threads = threads;
    const double sim_s = simulate(cfg).seconds;
    std::printf("    vs. simulating on demand: %.0fx faster\n", sim_s / (s / lookups));
  }

  // ---------------------------------------------------------------- 3. memory
  std::printf("\n[3] Memory per hand\n");
  const double bit = sizeof(Hand);
  auto row = [&](const char* name, double bytes5, double bytes7) {
    std::printf("    %-34s %5.0f B  %5.0f B   64-bit saves %3.0f%% / %3.0f%%\n", name, bytes5, bytes7,
                100 * (1 - bit / bytes5), 100 * (1 - bit / bytes7));
  };
  std::printf("    %-34s %7s %7s\n", "representation", "5 cards", "7 cards");
  std::printf("    %-34s %5.0f B  %5.0f B\n", "uint64_t bitmask (this project)", bit, bit);
  row("int32 card list (Cactus Kev style)", 5 * sizeof(CardInt), 7 * sizeof(CardInt));
  row("struct {int rank; int suit;} list", 5 * sizeof(CardRS), 7 * sizeof(CardRS));
  row("std::vector<int32> (24 B + heap)", sizeof(std::vector<int>) + 5 * 4.0,
      sizeof(std::vector<int>) + 7 * 4.0);
  return 0;
}
