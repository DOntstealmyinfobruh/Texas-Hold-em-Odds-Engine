#include "holdem/simulator.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

#include "holdem/evaluator.hpp"
#include "holdem/rng.hpp"

namespace holdem {

namespace {

constexpr int kMaxOpponents = 9;

void validate(const SimConfig& cfg) {
  if (popcount(cfg.hero) != 2) throw std::invalid_argument("hero must hold exactly 2 cards");
  if (cfg.hero & ~kFullDeck || cfg.board & ~kFullDeck || cfg.dead & ~kFullDeck)
    throw std::invalid_argument("invalid card bits");
  const int board_cards = popcount(cfg.board);
  if (board_cards != 0 && (board_cards < 3 || board_cards > 5))
    throw std::invalid_argument("board must have 0, 3, 4 or 5 cards");
  if ((cfg.hero & cfg.board) || (cfg.hero & cfg.dead) || (cfg.board & cfg.dead))
    throw std::invalid_argument("hero, board and dead cards overlap");
  if (cfg.opponents < 1 || cfg.opponents > kMaxOpponents)
    throw std::invalid_argument("opponents must be between 1 and 9");
  const int remaining = kDeckSize - popcount(cfg.hero | cfg.board | cfg.dead);
  if (2 * cfg.opponents + (5 - board_cards) > remaining)
    throw std::invalid_argument("not enough cards left in the deck");
}

}  // namespace

SimResult simulate_single_thread(const SimConfig& cfg, std::uint64_t trials,
                                 std::uint64_t seed) {
  // Remaining deck as one-hot Hands so "deal a card" is just an OR.
  Hand deck[kDeckSize];
  int deck_size = 0;
  for (Hand rest = kFullDeck & ~(cfg.hero | cfg.board | cfg.dead); rest; rest &= rest - 1)
    deck[deck_size++] = rest & (~rest + 1);

  const int opponents = cfg.opponents;
  const int board_needed = 5 - popcount(cfg.board);
  const int to_deal = board_needed + 2 * opponents;

  Rng rng(seed);
  SimResult r;
  r.trials = trials;
  std::uint64_t ties = 0, losses = 0;
  double equity = 0.0;

  for (std::uint64_t t = 0; t < trials; ++t) {
    // Partial Fisher-Yates: deck[0 .. to_deal) becomes a uniform random draw.
    // The deck is left permuted, which is still a valid starting point for
    // the next trial, so it never needs resetting.
    for (int i = 0; i < to_deal; ++i) {
      const int j = i + static_cast<int>(rng.below(static_cast<std::uint32_t>(deck_size - i)));
      std::swap(deck[i], deck[j]);
    }

    Hand board = cfg.board;
    for (int i = 0; i < board_needed; ++i) board |= deck[i];

    const HandValue hero = evaluate(cfg.hero | board);
    HandValue best_opp = 0;
    int tied = 0;
    const Hand* hole = deck + board_needed;
    for (int o = 0; o < opponents; ++o, hole += 2) {
      const HandValue v = evaluate(hole[0] | hole[1] | board);
      if (v > best_opp) { best_opp = v; tied = (v == hero); }
      else if (v == best_opp && v == hero) { ++tied; }
    }

    if (hero > best_opp) {
      equity += 1.0;
    } else if (hero == best_opp) {
      ++ties;
      equity += 1.0 / (tied + 1);
    } else {
      ++losses;
    }
  }

  r.ties = ties;
  r.losses = losses;
  r.wins = trials - ties - losses;
  r.equity_sum = equity;
  r.threads_used = 1;
  return r;
}

SimResult simulate(const SimConfig& cfg) {
  validate(cfg);

  unsigned threads = cfg.threads ? cfg.threads : std::thread::hardware_concurrency();
  if (threads == 0) threads = 1;
  if (cfg.trials < 100'000) threads = 1;  // not worth spawning threads
  threads = static_cast<unsigned>(std::min<std::uint64_t>(threads, std::max<std::uint64_t>(cfg.trials, 1)));

  const auto start = std::chrono::steady_clock::now();

  std::vector<SimResult> parts(threads);
  std::uint64_t seed_state = cfg.seed;
  std::vector<std::uint64_t> seeds(threads);
  for (auto& s : seeds) s = Rng::splitmix64(seed_state);

  auto work = [&](unsigned i) {
    const std::uint64_t share = cfg.trials / threads + (i < cfg.trials % threads ? 1 : 0);
    parts[i] = simulate_single_thread(cfg, share, seeds[i]);
  };

  if (threads == 1) {
    work(0);
  } else {
    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (unsigned i = 0; i < threads; ++i) pool.emplace_back(work, i);
    for (auto& th : pool) th.join();
  }

  SimResult total;
  for (const auto& p : parts) {
    total.trials += p.trials;
    total.wins += p.wins;
    total.ties += p.ties;
    total.losses += p.losses;
    total.equity_sum += p.equity_sum;
  }
  total.threads_used = threads;
  total.seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  return total;
}

}  // namespace holdem
