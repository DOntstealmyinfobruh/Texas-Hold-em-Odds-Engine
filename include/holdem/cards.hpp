#pragma once
// Card and hand representation.
//
// A hand (any set of cards: hole cards, board, or all 7 together) is a single
// 64-bit integer with one bit per card. The layout is suit-major:
//
//     bit = suit * 16 + rank        rank: 0 = '2' ... 12 = 'A'
//                                    suit: 0 = clubs, 1 = diamonds,
//                                          2 = hearts, 3 = spades
//
//     63        48 47        32 31        16 15         0
//     [ spades   ][ hearts    ][ diamonds  ][ clubs     ]
//      ---AKQJT98765432 (13 bits used per 16-bit lane)
//
// Combining cards is a bitwise OR, checking for a card is an AND, and each
// suit's ranks fall out with a shift + mask, which is what makes the
// evaluator fast. A 5-card hand stored this way takes 8 bytes instead of
// 20 bytes as five 32-bit card ints (60% less), and a 7-card hand takes
// 8 bytes instead of 28 (71% less).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace holdem {

using Hand = std::uint64_t;

inline constexpr int kNumRanks = 13;
inline constexpr int kNumSuits = 4;
inline constexpr int kDeckSize = 52;

inline constexpr Hand kRankMask = 0x1FFF;  // 13 rank bits in one suit lane
inline constexpr Hand kFullDeck =
    kRankMask | (kRankMask << 16) | (kRankMask << 32) | (kRankMask << 48);

constexpr int card_bit_index(int rank, int suit) { return suit * 16 + rank; }
constexpr Hand card(int rank, int suit) { return Hand{1} << card_bit_index(rank, suit); }
constexpr int rank_of(int bit_index) { return bit_index & 15; }
constexpr int suit_of(int bit_index) { return bit_index >> 4; }

inline int popcount(Hand h) {
#if defined(_MSC_VER)
  return static_cast<int>(__popcnt64(h));
#else
  return __builtin_popcountll(h);
#endif
}

inline int lowest_bit(Hand h) {  // h must be non-zero
#if defined(_MSC_VER)
  unsigned long i;
  _BitScanForward64(&i, h);
  return static_cast<int>(i);
#else
  return __builtin_ctzll(h);
#endif
}

// Rank bits (13-bit mask) of one suit.
constexpr unsigned suit_ranks(Hand h, int suit) {
  return static_cast<unsigned>((h >> (suit * 16)) & kRankMask);
}

// Parse "As", "td", "Th", "10c" ... Returns the bit index.
std::optional<int> parse_card(std::string_view text);

// Parse a run of cards such as "AsKd", "As Kd", "as,kd,2c". Returns nullopt on
// any malformed or duplicated card.
std::optional<Hand> parse_hand(std::string_view text);

std::string card_to_string(int bit_index);
std::string hand_to_string(Hand h);  // e.g. "2c Td As"

}  // namespace holdem
