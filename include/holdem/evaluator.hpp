#pragma once
// Bitwise hand evaluator for 5, 6 or 7 cards stored as a 64-bit Hand.
//
// Returns a 32-bit HandValue where a larger value is a stronger hand, so
// comparing two hands is a single integer comparison:
//
//     bits 26..29  category (0 = high card ... 8 = straight flush)
//     bits 13..25  primary rank bits   (e.g. the pair / trips / quads rank)
//     bits  0..12  secondary rank bits (kickers)
//
// Rank bits are 13-bit masks; for two masks with the same number of set bits
// a numeric comparison is exactly the poker "compare highest card first" rule.
//
// Everything is computed from the four 13-bit suit lanes with a handful of
// ANDs/ORs plus five 8 KB lookup tables built at compile time. There is no
// sorting, no per-card loop and no data-dependent memory beyond the tables.

#include <cstdint>

#include "holdem/cards.hpp"

namespace holdem {

using HandValue = std::uint32_t;

enum Category : int {
  kHighCard = 0,
  kOnePair = 1,
  kTwoPair = 2,
  kThreeOfAKind = 3,
  kStraight = 4,
  kFlush = 5,
  kFullHouse = 6,
  kFourOfAKind = 7,
  kStraightFlush = 8,
};

constexpr Category category_of(HandValue v) { return static_cast<Category>(v >> 26); }
const char* category_name(Category c);

namespace detail {

struct Tables {
  std::uint16_t top1[8192];          // highest set bit only
  std::uint16_t top2[8192];          // two highest set bits
  std::uint16_t top3[8192];
  std::uint16_t top5[8192];
  std::int8_t straight_high[8192];   // rank of the straight's top card, -1 if none
};

constexpr unsigned keep_top(unsigned mask, int n) {
  unsigned out = 0;
  for (int r = 12; r >= 0 && n > 0; --r) {
    if (mask & (1u << r)) {
      out |= 1u << r;
      --n;
    }
  }
  return out;
}

constexpr int find_straight(unsigned mask) {
  for (int hi = 12; hi >= 4; --hi) {
    const unsigned run = 0x1Fu << (hi - 4);
    if ((mask & run) == run) return hi;
  }
  if ((mask & 0x100Fu) == 0x100Fu) return 3;  // A-2-3-4-5 "wheel": 5-high
  return -1;
}

constexpr Tables build_tables() {
  Tables t{};
  for (unsigned m = 0; m < 8192; ++m) {
    t.top1[m] = static_cast<std::uint16_t>(keep_top(m, 1));
    t.top2[m] = static_cast<std::uint16_t>(keep_top(m, 2));
    t.top3[m] = static_cast<std::uint16_t>(keep_top(m, 3));
    t.top5[m] = static_cast<std::uint16_t>(keep_top(m, 5));
    t.straight_high[m] = static_cast<std::int8_t>(find_straight(m));
  }
  return t;
}

inline constexpr Tables kTables = build_tables();

constexpr HandValue make_value(int category, unsigned primary, unsigned secondary) {
  return (static_cast<HandValue>(category) << 26) | (primary << 13) | secondary;
}

}  // namespace detail

// Evaluate a hand of 5 to 7 cards.
inline HandValue evaluate(Hand h) {
  using detail::kTables;
  using detail::make_value;

  const unsigned c = suit_ranks(h, 0);
  const unsigned d = suit_ranks(h, 1);
  const unsigned e = suit_ranks(h, 2);  // hearts
  const unsigned s = suit_ranks(h, 3);

  // Flush. With at most 7 cards a flush rules out quads and full houses,
  // so it can be resolved before anything else.
  unsigned flush = 0;
  if (popcount(c) >= 5) flush = c;
  else if (popcount(d) >= 5) flush = d;
  else if (popcount(e) >= 5) flush = e;
  else if (popcount(s) >= 5) flush = s;
  if (flush) {
    const int sf = kTables.straight_high[flush];
    if (sf >= 0) return make_value(kStraightFlush, 1u << sf, 0);
    return make_value(kFlush, kTables.top5[flush], 0);
  }

  // Ranks held in >= 1, >= 2, >= 3 and 4 suits.
  const unsigned any = c | d | e | s;
  const unsigned two_plus = (c & d) | (e & s) | ((c | d) & (e | s));
  const unsigned three_plus = (c & d & (e | s)) | (e & s & (c | d));
  const unsigned four = c & d & e & s;

  if (four) return make_value(kFourOfAKind, four, kTables.top1[any & ~four]);

  if (three_plus) {
    const unsigned trips = kTables.top1[three_plus];
    const unsigned pair = two_plus & ~trips;  // includes a second set of trips
    if (pair) return make_value(kFullHouse, trips, kTables.top1[pair]);
  }

  const int st = kTables.straight_high[any];
  if (st >= 0) return make_value(kStraight, 1u << st, 0);

  if (three_plus) {
    const unsigned trips = kTables.top1[three_plus];
    return make_value(kThreeOfAKind, trips, kTables.top2[any & ~trips]);
  }

  if (two_plus) {
    if (two_plus & (two_plus - 1)) {  // two or three pairs: keep the best two
      const unsigned pairs = kTables.top2[two_plus];
      return make_value(kTwoPair, pairs, kTables.top1[any & ~pairs]);
    }
    return make_value(kOnePair, two_plus, kTables.top3[any & ~two_plus]);
  }

  return make_value(kHighCard, kTables.top5[any], 0);
}

}  // namespace holdem
