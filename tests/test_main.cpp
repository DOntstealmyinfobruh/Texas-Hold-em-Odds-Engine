// Unit and correctness tests. No external framework: each CHECK prints the
// failing expression and the process exits non-zero if anything failed.

#include <cmath>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>

#include "holdem/cards.hpp"
#include "holdem/evaluator.hpp"
#include "holdem/preflop.hpp"
#include "holdem/simulator.hpp"

using namespace holdem;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                              \
  do {                                                                           \
    ++g_checks;                                                                  \
    if (!(cond)) {                                                               \
      ++g_failures;                                                              \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
    }                                                                            \
  } while (0)

static Hand H(const char* s) {
  auto h = parse_hand(s);
  if (!h) {
    std::printf("  bad test hand: %s\n", s);
    ++g_failures;
    return 0;
  }
  return *h;
}
static HandValue E(const char* s) { return evaluate(H(s)); }

static void test_parsing() {
  std::printf("parsing\n");
  CHECK(parse_card("As") == card_bit_index(12, 3));
  CHECK(parse_card("2c") == card_bit_index(0, 0));
  CHECK(parse_card("td") == card_bit_index(8, 1));
  CHECK(parse_card("10h") == card_bit_index(8, 2));
  CHECK(!parse_card("Xs"));
  CHECK(!parse_card("Ax"));
  CHECK(!parse_card("A"));
  CHECK(popcount(H("AsKd")) == 2);
  CHECK(H("As Kd") == H("as,kd"));
  CHECK(!parse_hand("AsAs"));                // duplicate
  CHECK(!parse_hand("AsK"));                 // truncated
  CHECK(hand_to_string(H("Kd As 2c")) == "2c Kd As");
  CHECK(popcount(kFullDeck) == 52);
}

static void test_categories() {
  std::printf("hand categories\n");
  CHECK(category_of(E("As Ks Qs Js Ts 2d 3c")) == kStraightFlush);
  CHECK(category_of(E("5h 4h 3h 2h Ah Kd Kc")) == kStraightFlush);  // steel wheel
  CHECK(category_of(E("9c 9d 9h 9s 2d 3c 4h")) == kFourOfAKind);
  CHECK(category_of(E("9c 9d 9h 2s 2d 3c 4h")) == kFullHouse);
  CHECK(category_of(E("9c 9d 9h 2s 2d 2c 4h")) == kFullHouse);      // two trips
  CHECK(category_of(E("Ah 9h 7h 4h 2h Kd Kc")) == kFlush);
  CHECK(category_of(E("Ah 2c 3d 4s 5h Kd Kc")) == kStraight);       // wheel
  CHECK(category_of(E("9c 9d 9h 2s Kd 3c 5h")) == kThreeOfAKind);
  CHECK(category_of(E("9c 9d 2h 2s Kd 3c 5h")) == kTwoPair);
  CHECK(category_of(E("9c 9d Ah 2s Kd 3c 5h")) == kOnePair);
  CHECK(category_of(E("9c Td Ah 2s Kd 3c 5h")) == kHighCard);
  CHECK(category_of(E("As Ks Qs Js Ts")) == kStraightFlush);         // 5-card input
  CHECK(category_of(E("As Ks Qs Js 9s 2c")) == kFlush);              // 6-card input
}

static void test_ordering() {
  std::printf("hand ordering and kickers\n");
  // category ladder
  CHECK(E("As Ks Qs Js Ts 2d 3c") > E("9c 9d 9h 9s 2d 3c 4h"));
  CHECK(E("9c 9d 9h 9s 2d 3c 4h") > E("9c 9d 9h 2s 2d 3c 4h"));
  CHECK(E("2c 2d 2h 3s 3d 7c 8h") > E("Ah 9h 7h 4h 2h Kd Qc"));
  CHECK(E("Ah 9h 7h 4h 2h Kd Kc") > E("Ts 9c 8d 7s 6h 2c 2d"));
  // straights: wheel is the lowest straight, broadway the highest
  CHECK(E("6c 5d 4h 3s 2c Kd Jc") > E("Ah 2c 3d 4s 5h Kd Jc"));
  CHECK(E("Ac Kd Qh Js Tc 2d 3c") > E("Kd Qh Js Tc 9d 2c 3h"));
  CHECK(E("5h 4h 3h 2h Ah Kd Kc") < E("6h 5h 4h 3h 2h Kd Kc"));
  // kickers
  CHECK(E("Ac Ad Kh 9s 7c 3d 2h") > E("Ac Ad Qh Js Tc 3d 2h"));
  CHECK(E("Ac Ad Kh Qs 7c 3d 2h") == E("Ac Ad Kh Qs 7d 3s 2c"));  // only 5 cards count
  CHECK(E("Ac Ad Kh Qs 8c 3d 2h") > E("Ac Ad Kh Qs 7c 3d 2h"));
  // two pair: with three pairs only the best two count, third pair can be kicker
  CHECK(E("Ac Ad Kh Ks 2c 2d Qh") > E("Ac Ad Kh Ks 3c 3d Jh"));
  CHECK(E("Ac Ad Kh Ks 2c 2d 3h") == E("Ac Ad Kh Ks 3c 3d 2h"));
  // full house: trips rank first, then pair
  CHECK(E("3c 3d 3h 2s 2d 9c 8h") > E("2c 2d 2h As Ad 9c 8h"));
  CHECK(E("Ac Ad Ah 2s 2d 9c 8h") < E("Ac Ad Ah Ks Kd 9c 8h"));
  CHECK(E("9c 9d 9h 8s 8d 8c 2h") > E("9c 9d 9h 7s 7d 7c Ah"));
  // quads kicker
  CHECK(E("9c 9d 9h 9s Ad 3c 4h") > E("9c 9d 9h 9s Kd Qc Jh"));
  // flush compares all five cards
  CHECK(E("Ah Kh 7h 4h 3h") > E("Ah Kh 7h 4h 2h"));
  CHECK(E("Ah Kh Qh 4h 3h 2h 9h") == E("Ah Kh Qh 9h 4h"));
}

static void test_five_card_enumeration() {
  std::printf("all 2,598,960 five-card hands\n");
  long long counts[9] = {};
  int idx[52];
  for (int i = 0, k = 0; i < 64; ++i) if ((kFullDeck >> i) & 1) idx[k++] = i;
  for (int a = 0; a < 52; ++a)
    for (int b = a + 1; b < 52; ++b)
      for (int c = b + 1; c < 52; ++c)
        for (int d = c + 1; d < 52; ++d)
          for (int e = d + 1; e < 52; ++e) {
            Hand h = (Hand{1} << idx[a]) | (Hand{1} << idx[b]) | (Hand{1} << idx[c]) |
                     (Hand{1} << idx[d]) | (Hand{1} << idx[e]);
            ++counts[category_of(evaluate(h))];
          }
  const long long expected[9] = {1302540, 1098240, 123552, 54912, 10200, 5108, 3744, 624, 40};
  for (int i = 0; i < 9; ++i) {
    if (counts[i] != expected[i])
      std::printf("  %s: got %lld expected %lld\n", category_name(Category(i)), counts[i], expected[i]);
    CHECK(counts[i] == expected[i]);
  }
}

static void test_distinct_five_card_values() {
  // There are exactly 7,462 distinct 5-card hand strengths.
  std::printf("7,462 distinct five-card hand values\n");
  std::set<HandValue> values;
  int idx[52];
  for (int i = 0, k = 0; i < 64; ++i) if ((kFullDeck >> i) & 1) idx[k++] = i;
  for (int a = 0; a < 52; ++a)
    for (int b = a + 1; b < 52; ++b)
      for (int c = b + 1; c < 52; ++c)
        for (int d = c + 1; d < 52; ++d)
          for (int e = d + 1; e < 52; ++e)
            values.insert(evaluate((Hand{1} << idx[a]) | (Hand{1} << idx[b]) | (Hand{1} << idx[c]) |
                                   (Hand{1} << idx[d]) | (Hand{1} << idx[e])));
  CHECK(values.size() == 7462);
}

static void test_starting_hands() {
  std::printf("169 starting hands\n");
  std::set<std::string> names;
  for (int i = 0; i < kNumStartingHands; ++i) {
    const std::string n = starting_hand_name(i);
    names.insert(n);
    CHECK(starting_hand_index_from_name(n) == i);
    CHECK(starting_hand_index(representative_hand(i)) == i);
  }
  CHECK(names.size() == 169);
  CHECK(starting_hand_name(0) == "AA");
  CHECK(starting_hand_name(1) == "AKs");
  CHECK(starting_hand_name(13) == "AKo");
  CHECK(starting_hand_name(168) == "22");

  // Every one of the 1,326 two-card combos maps into a class with the right count.
  int per_class[kNumStartingHands] = {};
  for (int a = 0; a < 64; ++a)
    for (int b = a + 1; b < 64; ++b) {
      const Hand h = (Hand{1} << a) | (Hand{1} << b);
      if ((h & kFullDeck) != h) continue;
      ++per_class[starting_hand_index(h)];
    }
  int total = 0;
  for (int i = 0; i < kNumStartingHands; ++i) {
    CHECK(per_class[i] == combos_in_class(i));
    total += per_class[i];
  }
  CHECK(total == 1326);
  CHECK(starting_hand_index(H("Kd As")) == starting_hand_index(H("Ah Kc")));  // order/suit-free
  CHECK(starting_hand_index(H("Ks As")) == 1);
}

static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static void test_simulation() {
  std::printf("Monte Carlo vs known exact equities\n");
  // Exact heads-up all-in preflop equities vs a random hand.
  struct Known { const char* hand; double equity; };
  const Known known[] = {{"AsAh", 0.8520}, {"KsKh", 0.8240}, {"AsKs", 0.6704},
                         {"AsKh", 0.6531}, {"7s2h", 0.3459}, {"3s2h", 0.3229}};
  for (const auto& k : known) {
    SimConfig cfg;
    cfg.hero = H(k.hand);
    cfg.trials = 2'000'000;
    const SimResult r = simulate(cfg);
    std::printf("  %s  %.2f%%  (exact %.2f%%)\n", k.hand, 100 * r.equity(), 100 * k.equity);
    CHECK(near(r.equity(), k.equity, 0.003));
    CHECK(r.wins + r.ties + r.losses == r.trials);
  }

  // Board fully known: the result is deterministic.
  SimConfig cfg;
  cfg.hero = H("AsAh");
  cfg.board = H("Ad Ac 2s 7h 9d");  // quad aces; only a straight flush could beat it and none is possible
  cfg.trials = 200'000;
  CHECK(simulate(cfg).win_rate() == 1.0);

  // Board makes a royal flush for everyone: always a split.
  cfg.hero = H("2c 3d");
  cfg.board = H("As Ks Qs Js Ts");
  SimResult r = simulate(cfg);
  CHECK(r.tie_rate() == 1.0);
  CHECK(near(r.equity(), 0.5, 1e-9));
  cfg.opponents = 3;
  r = simulate(cfg);
  CHECK(near(r.equity(), 0.25, 1e-9));

  // Same seed + single thread is reproducible.
  SimConfig a;
  a.hero = H("QsJs");
  a.trials = 50'000;
  a.threads = 1;
  CHECK(simulate(a).wins == simulate(a).wins);

  // Input validation.
  bool threw = false;
  try { SimConfig bad; bad.hero = H("As"); simulate(bad); } catch (const std::invalid_argument&) { threw = true; }
  CHECK(threw);
  threw = false;
  try { SimConfig bad; bad.hero = H("AsKs"); bad.board = H("As 2c 3d"); simulate(bad); }
  catch (const std::invalid_argument&) { threw = true; }
  CHECK(threw);
}

static void test_preflop_table() {
  std::printf("precomputed table\n");
  const int aa = *starting_hand_index_from_name("AA");
  const int k32 = *starting_hand_index_from_name("32o");
  for (int o = 1; o <= kMaxTableOpponents; ++o) {
    double weighted = 0;
    int best = 0;
    for (int i = 0; i < kNumStartingHands; ++i) {
      const auto& p = preflop_odds(i, o);
      CHECK(p.equity > 0.0f && p.equity < 1.0f);
      CHECK(p.win <= p.equity + 1e-6f);
      weighted += combos_in_class(i) * p.equity;
      if (p.equity > preflop_odds(best, o).equity) best = i;
    }
    CHECK(best == aa);  // AA is the best hand at every table size
    // Averaged over all 1,326 combos, a seat's equity is exactly 1/(opponents+1).
    CHECK(near(weighted / 1326.0, 1.0 / (o + 1), 0.002));
  }
  CHECK(near(preflop_odds(aa, 1).equity, 0.8520, 0.002));
  CHECK(near(preflop_odds(k32, 1).equity, 0.3229, 0.002));
  CHECK(&preflop_odds(H("Ah Ad")) == &preflop_odds(aa));
}

int main() {
  test_parsing();
  test_categories();
  test_ordering();
  test_five_card_enumeration();
  test_distinct_five_card_values();
  test_starting_hands();
  test_simulation();
  test_preflop_table();
  std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
  return g_failures ? 1 : 0;
}
