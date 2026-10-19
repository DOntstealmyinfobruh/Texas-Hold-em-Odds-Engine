// Starting-hand indexing (no table dependency, so the precompute tool can use it).
#include <cctype>
#include <stdexcept>

#include "holdem/preflop.hpp"

namespace holdem {

namespace {
constexpr char kRankChars[] = "23456789TJQKA";

int rank_from_char(char c) {
  const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  for (int r = 0; r < kNumRanks; ++r)
    if (kRankChars[r] == u) return r;
  return -1;
}

int index_of(int high, int low, bool suited) {
  if (high == low) return (12 - high) * 13 + (12 - high);
  if (suited) return (12 - high) * 13 + (12 - low);
  return (12 - low) * 13 + (12 - high);
}
}  // namespace

int starting_hand_index(Hand hole) {
  if (popcount(hole) != 2) return -1;
  const int a = lowest_bit(hole);
  const int b = lowest_bit(hole & (hole - 1));
  const int ra = rank_of(a), rb = rank_of(b);
  const int high = ra > rb ? ra : rb;
  const int low = ra > rb ? rb : ra;
  return index_of(high, low, suit_of(a) == suit_of(b));
}

std::string starting_hand_name(int index) {
  if (index < 0 || index >= kNumStartingHands) throw std::out_of_range("starting hand index");
  const int row = index / 13, col = index % 13;
  if (row == col) return {kRankChars[12 - row], kRankChars[12 - row]};
  if (row < col) return {kRankChars[12 - row], kRankChars[12 - col], 's'};
  return {kRankChars[12 - col], kRankChars[12 - row], 'o'};
}

std::optional<int> starting_hand_index_from_name(std::string_view name) {
  if (name.size() < 2 || name.size() > 3) return std::nullopt;
  int r1 = rank_from_char(name[0]);
  int r2 = rank_from_char(name[1]);
  if (r1 < 0 || r2 < 0) return std::nullopt;
  const int high = r1 > r2 ? r1 : r2, low = r1 > r2 ? r2 : r1;
  if (high == low) {
    if (name.size() == 3) return std::nullopt;
    return index_of(high, low, false);
  }
  if (name.size() != 3) return std::nullopt;
  const char kind = static_cast<char>(std::tolower(static_cast<unsigned char>(name[2])));
  if (kind != 's' && kind != 'o') return std::nullopt;
  return index_of(high, low, kind == 's');
}

Hand representative_hand(int index) {
  if (index < 0 || index >= kNumStartingHands) throw std::out_of_range("starting hand index");
  const int row = index / 13, col = index % 13;
  if (row == col) return card(12 - row, 3) | card(12 - row, 2);           // pair: spades + hearts
  if (row < col) return card(12 - row, 3) | card(12 - col, 3);            // suited: both spades
  return card(12 - col, 3) | card(12 - row, 2);                           // offsuit
}

int combos_in_class(int index) {
  const int row = index / 13, col = index % 13;
  return row == col ? 6 : (row < col ? 4 : 12);
}

}  // namespace holdem
