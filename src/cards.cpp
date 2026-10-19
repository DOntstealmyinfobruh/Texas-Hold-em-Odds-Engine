#include "holdem/cards.hpp"

#include <cctype>

namespace holdem {

namespace {
constexpr char kRankChars[] = "23456789TJQKA";
constexpr char kSuitChars[] = "cdhs";

int rank_from_char(char c) {
  switch (std::toupper(static_cast<unsigned char>(c))) {
    case '2': return 0;  case '3': return 1;  case '4': return 2;
    case '5': return 3;  case '6': return 4;  case '7': return 5;
    case '8': return 6;  case '9': return 7;  case 'T': return 8;
    case 'J': return 9;  case 'Q': return 10; case 'K': return 11;
    case 'A': return 12;
    default: return -1;
  }
}

int suit_from_char(char c) {
  switch (std::tolower(static_cast<unsigned char>(c))) {
    case 'c': return 0; case 'd': return 1; case 'h': return 2; case 's': return 3;
    default: return -1;
  }
}

bool is_separator(char c) { return c == ' ' || c == ',' || c == '-' || c == '\t'; }
}  // namespace

std::optional<int> parse_card(std::string_view t) {
  int r, s;
  if (t.size() == 3 && t[0] == '1' && t[1] == '0') {  // "10h" == "Th"
    r = 8;
    s = suit_from_char(t[2]);
  } else if (t.size() == 2) {
    r = rank_from_char(t[0]);
    s = suit_from_char(t[1]);
  } else {
    return std::nullopt;
  }
  if (r < 0 || s < 0) return std::nullopt;
  return card_bit_index(r, s);
}

std::optional<Hand> parse_hand(std::string_view text) {
  Hand h = 0;
  std::size_t i = 0;
  while (i < text.size()) {
    if (is_separator(text[i])) { ++i; continue; }
    std::size_t len = (text[i] == '1' && i + 2 < text.size() && text[i + 1] == '0') ? 3 : 2;
    if (i + len > text.size()) return std::nullopt;
    auto c = parse_card(text.substr(i, len));
    if (!c) return std::nullopt;
    Hand bit = Hand{1} << *c;
    if (h & bit) return std::nullopt;  // duplicate card
    h |= bit;
    i += len;
  }
  return h;
}

std::string card_to_string(int bit_index) {
  return {kRankChars[rank_of(bit_index)], kSuitChars[suit_of(bit_index)]};
}

std::string hand_to_string(Hand h) {
  std::string out;
  while (h) {
    int b = lowest_bit(h);
    h &= h - 1;
    if (!out.empty()) out += ' ';
    out += card_to_string(b);
  }
  return out;
}

}  // namespace holdem
