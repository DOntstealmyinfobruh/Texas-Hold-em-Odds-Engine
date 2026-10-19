#include "holdem/evaluator.hpp"

namespace holdem {

const char* category_name(Category c) {
  switch (c) {
    case kHighCard: return "High Card";
    case kOnePair: return "One Pair";
    case kTwoPair: return "Two Pair";
    case kThreeOfAKind: return "Three of a Kind";
    case kStraight: return "Straight";
    case kFlush: return "Flush";
    case kFullHouse: return "Full House";
    case kFourOfAKind: return "Four of a Kind";
    case kStraightFlush: return "Straight Flush";
  }
  return "?";
}

}  // namespace holdem
