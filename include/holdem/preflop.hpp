#pragma once
// Precomputed preflop odds for all 169 distinct starting hands.
//
// The 1,326 possible two-card holdings collapse to 169 strategically distinct
// hands: 13 pairs, 78 suited and 78 offsuit combinations. They are laid out on
// the usual 13x13 grid (A first):
//
//     row = 12 - high rank, col = 12 - low rank      suited  (above diagonal)
//     row = 12 - low rank,  col = 12 - high rank     offsuit (below diagonal)
//     row == col                                     pairs
//
//     index = row * 13 + col   (0 = AA, 1 = AKs, 13 = AKo, 168 = 22)
//
// A lookup is a few bit operations on the 64-bit hand plus one array read.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "holdem/cards.hpp"

namespace holdem {

inline constexpr int kNumStartingHands = 169;
inline constexpr int kMaxTableOpponents = 9;

struct PreflopOdds {
  float win;     // probability hero wins outright
  float tie;     // probability hero splits the pot
  float equity;  // win + fractional share of split pots
};

// Index 0..168 for a two-card hand. Returns -1 if `hole` isn't exactly 2 cards.
int starting_hand_index(Hand hole);

// "AA", "AKs", "T9o" ... for an index.
std::string starting_hand_name(int index);

// Parse "AKs", "ako", "TT", "72o". Returns nullopt on bad input.
std::optional<int> starting_hand_index_from_name(std::string_view name);

// A concrete two-card hand belonging to the class (used to run simulations).
Hand representative_hand(int index);

// Number of the 1,326 card combos in this class: 6 (pair), 4 (suited), 12 (offsuit).
int combos_in_class(int index);

// O(1) table lookup. `opponents` in 1..9.
const PreflopOdds& preflop_odds(int index, int opponents = 1);
const PreflopOdds& preflop_odds(Hand hole, int opponents = 1);

// Metadata of the generated table.
std::uint64_t preflop_table_trials(int opponents);

}  // namespace holdem
