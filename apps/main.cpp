// holdem_odds — command-line front end.
//
//   holdem_odds AsKd                       instant lookup from the 169-hand table
//   holdem_odds AKs -o 3                   lookup a hand class vs 3 opponents
//   holdem_odds AsKd --sim                 run a fresh 10M-hand simulation
//   holdem_odds AsKd -b "Ah 7c 2d" -o 2    simulate with a known flop
//   holdem_odds --grid [-o N]              print the 13x13 equity grid
//   holdem_odds --rank [-o N]              list all 169 hands, best first

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "holdem/cards.hpp"
#include "holdem/preflop.hpp"
#include "holdem/simulator.hpp"

using namespace holdem;

namespace {

void usage() {
  std::cout <<
      "usage: holdem_odds <hand> [options]\n"
      "       holdem_odds --grid | --rank [-o N]\n\n"
      "  <hand>              two cards (AsKd, \"Ah 7c\") or a class (AKs, T9o, QQ)\n"
      "  -o, --opponents N   random opponents, 1-9 (default 1)\n"
      "  -b, --board CARDS   known community cards (3, 4 or 5); implies --sim\n"
      "  -d, --dead CARDS    cards known to be out of play; implies --sim\n"
      "  -s, --sim           run a Monte Carlo simulation instead of a table lookup\n"
      "  -n, --trials N      simulation size (default 10000000)\n"
      "  -t, --threads N     worker threads (default: all cores)\n"
      "      --seed N        RNG seed\n"
      "      --grid          13x13 equity grid (suited above the diagonal)\n"
      "      --rank          all 169 starting hands ranked by equity\n";
}

void print_grid(int opponents) {
  std::printf("Preflop equity vs %d random opponent(s)  (suited upper-right, offsuit lower-left)\n\n     ",
              opponents);
  const char* ranks = "AKQJT98765432";
  for (int c = 0; c < 13; ++c) std::printf("  %c  ", ranks[c]);
  std::printf("\n");
  for (int r = 0; r < 13; ++r) {
    std::printf("  %c  ", ranks[r]);
    for (int c = 0; c < 13; ++c) std::printf("%4.1f ", 100.0 * preflop_odds(r * 13 + c, opponents).equity);
    std::printf("\n");
  }
}

void print_rank(int opponents) {
  std::vector<int> order(kNumStartingHands);
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    return preflop_odds(a, opponents).equity > preflop_odds(b, opponents).equity;
  });
  std::printf("rank  hand  equity   win     tie    (vs %d opponent%s)\n", opponents,
              opponents == 1 ? "" : "s");
  for (int i = 0; i < kNumStartingHands; ++i) {
    const auto& p = preflop_odds(order[i], opponents);
    std::printf("%4d  %-4s  %5.2f%%  %5.2f%%  %4.2f%%\n", i + 1, starting_hand_name(order[i]).c_str(),
                100.0 * p.equity, 100.0 * p.win, 100.0 * p.tie);
  }
}

}  // namespace

int main(int argc, char** argv) {
  std::string hand_text, board_text, dead_text;
  int opponents = 1;
  bool sim = false, grid = false, rank = false;
  SimConfig cfg;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) { std::cerr << a << " needs a value\n"; std::exit(2); }
      return argv[++i];
    };
    if (a == "-h" || a == "--help") { usage(); return 0; }
    else if (a == "-o" || a == "--opponents") opponents = std::atoi(value().c_str());
    else if (a == "-b" || a == "--board") { board_text = value(); sim = true; }
    else if (a == "-d" || a == "--dead") { dead_text = value(); sim = true; }
    else if (a == "-s" || a == "--sim") sim = true;
    else if (a == "-n" || a == "--trials") cfg.trials = std::strtoull(value().c_str(), nullptr, 10);
    else if (a == "-t" || a == "--threads") cfg.threads = static_cast<unsigned>(std::atoi(value().c_str()));
    else if (a == "--seed") cfg.seed = std::strtoull(value().c_str(), nullptr, 10);
    else if (a == "--grid") grid = true;
    else if (a == "--rank") rank = true;
    else if (!a.empty() && a[0] == '-') { std::cerr << "unknown option " << a << "\n"; usage(); return 2; }
    else hand_text += (hand_text.empty() ? "" : " ") + a;
  }

  if (opponents < 1 || opponents > 9) { std::cerr << "opponents must be 1-9\n"; return 2; }
  if (grid) { print_grid(opponents); return 0; }
  if (rank) { print_rank(opponents); return 0; }
  if (hand_text.empty()) { usage(); return 2; }

  // Accept either concrete cards ("AsKd") or a class name ("AKs").
  Hand hero = 0;
  int index = -1;
  if (auto h = parse_hand(hand_text); h && popcount(*h) == 2) {
    hero = *h;
    index = starting_hand_index(hero);
  } else if (auto idx = starting_hand_index_from_name(hand_text)) {
    index = *idx;
    hero = representative_hand(index);
  } else {
    std::cerr << "could not parse hand '" << hand_text << "' (try AsKd or AKs)\n";
    return 2;
  }

  const std::string name = starting_hand_name(index);

  if (!sim) {
    const auto& p = preflop_odds(index, opponents);
    std::printf("%s  vs %d random opponent%s  (precomputed, %llu trials)\n", name.c_str(), opponents,
                opponents == 1 ? "" : "s",
                static_cast<unsigned long long>(preflop_table_trials(opponents)));
    std::printf("  win %6.2f%%   tie %5.2f%%   equity %6.2f%%\n\n", 100.0 * p.win, 100.0 * p.tie,
                100.0 * p.equity);
    std::printf("  equity by table size:");
    for (int o = 1; o <= kMaxTableOpponents; ++o)
      std::printf("  %d:%4.1f%%", o, 100.0 * preflop_odds(index, o).equity);
    std::printf("\n");
    return 0;
  }

  if (!board_text.empty()) {
    auto b = parse_hand(board_text);
    if (!b) { std::cerr << "could not parse board '" << board_text << "'\n"; return 2; }
    cfg.board = *b;
  }
  if (!dead_text.empty()) {
    auto d = parse_hand(dead_text);
    if (!d) { std::cerr << "could not parse dead cards '" << dead_text << "'\n"; return 2; }
    cfg.dead = *d;
  }
  cfg.hero = hero;
  cfg.opponents = opponents;

  try {
    const SimResult r = simulate(cfg);
    std::printf("%s (%s)", hand_to_string(hero).c_str(), name.c_str());
    if (cfg.board) std::printf("  board: %s", hand_to_string(cfg.board).c_str());
    std::printf("  vs %d random opponent%s\n", opponents, opponents == 1 ? "" : "s");
    std::printf("  win %6.2f%%   tie %5.2f%%   loss %6.2f%%   equity %6.2f%%\n", 100.0 * r.win_rate(),
                100.0 * r.tie_rate(), 100.0 * r.loss_rate(), 100.0 * r.equity());
    std::printf("  %llu hands in %.3fs on %u thread%s  (%.1fM hands/s)\n",
                static_cast<unsigned long long>(r.trials), r.seconds, r.threads_used,
                r.threads_used == 1 ? "" : "s", r.hands_per_second() / 1e6);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 2;
  }
  return 0;
}
